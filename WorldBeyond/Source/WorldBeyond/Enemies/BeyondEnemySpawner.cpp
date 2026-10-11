// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemies/BeyondEnemySpawner.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AI/BeyondEnemyController.h"
#include "Components/BillboardComponent.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "EngineUtils.h"
#include "Game/BeyondCombatSubsystem.h"
#include "TimerManager.h"
#include "World/BeyondWorldSubsystem.h"

ABeyondEnemySpawner::ABeyondEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	// Logic: it keeps track of its camp while the art around it streams in and out
	bIsSpatiallyLoaded = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(GetRootComponent());
		Sprite->bIsScreenSizeScaled = true;
	}
#endif
}

void ABeyondEnemySpawner::BeginPlay()
{
	Super::BeginPlay();
	BuildSlots();

	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		PartyWipedHandle = Combat->OnPartyWiped.AddUObject(this, &ThisClass::HandlePartyWiped);
	}
	if (UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this))
	{
		WorldSubsystem->OnPartyRested.AddUniqueDynamic(this, &ThisClass::HandlePartyRested);
	}

	if (ActivationRadius <= 0.0f)
	{
		Activate();
	}
	else
	{
		GetWorldTimerManager().SetTimer(ActivationTimer, this, &ThisClass::CheckActivation, 0.5f, true, FMath::FRandRange(0.1f, 0.5f));
	}
}

void ABeyondEnemySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		Combat->OnPartyWiped.Remove(PartyWipedHandle);
	}
	if (UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this))
	{
		WorldSubsystem->OnPartyRested.RemoveDynamic(this, &ThisClass::HandlePartyRested);
	}
	// Streamed out (it normally never is): don't leave its camp behind to be spawned twice
	if (EndPlayReason == EEndPlayReason::RemovedFromWorld)
	{
		PutAwayIdleEnemies();
	}
	Super::EndPlay(EndPlayReason);
}

void ABeyondEnemySpawner::BuildSlots()
{
	Slots.Reset();
	int32 Total = 0;
	for (const FBeyondSpawnEntry& Entry : Entries)
	{
		Total += Entry.Enemy ? (Entry.Count > 0 ? Entry.Count : Entry.Enemy->PackSize) : 0;
	}

	// Spread evenly on a circle (a single enemy stands on the spawner)
	int32 Index = 0;
	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FBeyondSpawnEntry& Entry = Entries[EntryIndex];
		if (!Entry.Enemy)
		{
			continue;
		}
		const int32 Count = Entry.Count > 0 ? Entry.Count : Entry.Enemy->PackSize;
		for (int32 Copy = 0; Copy < Count; ++Copy, ++Index)
		{
			FSlot& Slot = Slots.AddDefaulted_GetRef();
			Slot.EntryIndex = EntryIndex;
			const float Angle = 2.0f * PI * Index / FMath::Max(Total, 1);
			const float Distance = Total > 1 ? SpawnRadius * FMath::FRandRange(0.55f, 1.0f) : 0.0f;
			Slot.Location = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;
		}
	}
}

bool ABeyondEnemySpawner::IsPartyWithin(float Radius) const
{
	TArray<ABeyondCharacterBase*> Party = UBeyondWorldSubsystem::GetPartyMembers(this);
	if (Party.IsEmpty())
	{
		for (TActorIterator<ABeyondCharacterBase> It(GetWorld()); It; ++It)
		{
			if (It->TeamAffiliation == EBeyondTeam::Player)
			{
				Party.Add(*It);
			}
		}
	}
	const float RadiusSq = FMath::Square(Radius);
	for (const ABeyondCharacterBase* Member : Party)
	{
		if (Member && !UBeyondCombatLibrary::IsActorDead(Member) && FVector::DistSquared(Member->GetActorLocation(), GetActorLocation()) <= RadiusSq)
		{
			return true;
		}
	}
	return false;
}

bool ABeyondEnemySpawner::IsAnyEnemyFighting() const
{
	for (const FSlot& Slot : Slots)
	{
		const ABeyondEnemyCharacter* Enemy = Slot.Enemy.Get();
		const ABeyondEnemyController* Brain = Enemy ? Cast<ABeyondEnemyController>(Enemy->GetController()) : nullptr;
		if (Brain && !UBeyondCombatLibrary::IsActorDead(Enemy) && Brain->GetAIState() == EBeyondEnemyAIState::Combat)
		{
			return true;
		}
	}
	return false;
}

void ABeyondEnemySpawner::CheckActivation()
{
	if (!bActivated)
	{
		if (IsPartyWithin(ActivationRadius))
		{
			Activate();
		}
		return;
	}
	if (DeactivationRadius > 0.0f && !IsPartyWithin(FMath::Max(DeactivationRadius, ActivationRadius)) && !IsAnyEnemyFighting())
	{
		Deactivate();
		return;
	}
	// Slots that had no ground under them yet
	SpawnWaiting();
}

void ABeyondEnemySpawner::Activate()
{
	bActivated = true;
	if (Respawn == EBeyondRespawnRule::AfterDelay)
	{
		// The ones whose time came while the party was away
		const double Now = GetWorld()->GetTimeSeconds();
		for (FSlot& Slot : Slots)
		{
			if (Slot.bDefeated && Now - Slot.DefeatedTime >= RespawnDelay)
			{
				Slot.bDefeated = false;
			}
		}
	}
	SpawnWaiting();
}

void ABeyondEnemySpawner::Deactivate()
{
	bActivated = false;
	PutAwayIdleEnemies();
}

void ABeyondEnemySpawner::PutAwayIdleEnemies()
{
	for (FSlot& Slot : Slots)
	{
		ABeyondEnemyCharacter* Enemy = Slot.Enemy.Get();
		if (!Enemy)
		{
			continue;
		}
		const ABeyondEnemyController* Brain = Cast<ABeyondEnemyController>(Enemy->GetController());
		const bool bFighting = Brain && Brain->GetAIState() == EBeyondEnemyAIState::Combat && !UBeyondCombatLibrary::IsActorDead(Enemy);
		if (!bFighting)
		{
			Enemy->OnCharacterKilled.RemoveDynamic(this, &ThisClass::HandleEnemyKilled);
			Enemy->Destroy();
			Slot.Enemy.Reset();
		}
	}
}

int32 ABeyondEnemySpawner::GetSpawnLevel(int32 EntryIndex) const
{
	const int32 EntryLevel = Entries.IsValidIndex(EntryIndex) ? Entries[EntryIndex].Level : 1;
	if (EntryLevel > 0)
	{
		return EntryLevel;
	}
	int32 Min = 1;
	int32 Max = 1;
	if (const UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this); WorldSubsystem && WorldSubsystem->GetLevelBandAt(GetActorLocation(), Min, Max))
	{
		return FMath::Clamp(Min + LevelOffset, Min, Max);
	}
	return FMath::Max(1 + LevelOffset, 1);
}

int32 ABeyondEnemySpawner::GetDefeatedCount() const
{
	int32 Count = 0;
	for (const FSlot& Slot : Slots)
	{
		Count += Slot.bDefeated ? 1 : 0;
	}
	return Count;
}

void ABeyondEnemySpawner::SpawnSlot(FSlot& Slot)
{
	UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(this);
	const FBeyondSpawnEntry& Entry = Entries.IsValidIndex(Slot.EntryIndex) ? Entries[Slot.EntryIndex] : FBeyondSpawnEntry();
	if (!Enemies || !Entry.Enemy)
	{
		return;
	}
	// Nothing to stand on yet (the ground hasn't streamed in): try again on the next check
	FVector Ground;
	if (!UBeyondEnemySubsystem::FindGroundPoint(GetWorld(), Slot.Location, Ground))
	{
		return;
	}

	FBeyondEnemySpawnParams Params;
	Params.Level = GetSpawnLevel(Slot.EntryIndex);
	Params.bElite = Entry.Enemy->bCanBeElite && FMath::FRand() < EliteChance;
	// Facing outward from the camp
	const FVector Outward = (Slot.Location - GetActorLocation()).GetSafeNormal2D();
	const float Yaw = Outward.IsNearlyZero() ? GetActorRotation().Yaw : Outward.Rotation().Yaw;
	if (ABeyondEnemyCharacter* Enemy = Enemies->SpawnEnemy(Entry.Enemy, FTransform(FRotator(0.0f, Yaw, 0.0f), Slot.Location), Params))
	{
		Slot.Enemy = Enemy;
		Slot.bEverSpawned = true;
		Slot.bDefeated = false;
		Enemy->OnCharacterKilled.AddUniqueDynamic(this, &ThisClass::HandleEnemyKilled);
	}
}

void ABeyondEnemySpawner::SpawnWaiting()
{
	for (FSlot& Slot : Slots)
	{
		const bool bAlive = Slot.Enemy.IsValid() && !UBeyondCombatLibrary::IsActorDead(Slot.Enemy.Get());
		if (!bAlive && !Slot.bDefeated)
		{
			SpawnSlot(Slot);
		}
	}
}

void ABeyondEnemySpawner::ClearDefeated()
{
	for (FSlot& Slot : Slots)
	{
		Slot.bDefeated = false;
	}
}

void ABeyondEnemySpawner::SpawnMissing()
{
	ClearDefeated();
	SpawnWaiting();
}

TArray<ABeyondEnemyCharacter*> ABeyondEnemySpawner::GetSpawnedEnemies() const
{
	TArray<ABeyondEnemyCharacter*> Result;
	for (const FSlot& Slot : Slots)
	{
		if (ABeyondEnemyCharacter* Enemy = Slot.Enemy.Get())
		{
			Result.Add(Enemy);
		}
	}
	return Result;
}

void ABeyondEnemySpawner::HandleEnemyKilled(ABeyondCharacterBase* Character, AActor* Killer)
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].Enemy.Get() != Character)
		{
			continue;
		}
		Slots[Index].bDefeated = true;
		Slots[Index].DefeatedTime = GetWorld()->GetTimeSeconds();
		if (Respawn == EBeyondRespawnRule::AfterDelay)
		{
			FTimerHandle Timer;
			GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Index]()
			{
				if (bActivated && Slots.IsValidIndex(Index) && Slots[Index].bDefeated)
				{
					Slots[Index].bDefeated = false;
					SpawnSlot(Slots[Index]);
				}
			}), RespawnDelay, false);
		}
		return;
	}
}

void ABeyondEnemySpawner::HandlePartyWiped()
{
	if (Respawn == EBeyondRespawnRule::OnPartyWipe || Respawn == EBeyondRespawnRule::OnRest)
	{
		// After the party is back on its feet at the checkpoint
		FTimerHandle Timer;
		GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			ClearDefeated();
			if (bActivated)
			{
				SpawnWaiting();
			}
		}), 3.5f, false);
	}
}

void ABeyondEnemySpawner::HandlePartyRested()
{
	if (Respawn == EBeyondRespawnRule::OnRest)
	{
		ClearDefeated();
		if (bActivated)
		{
			SpawnWaiting();
		}
	}
}
