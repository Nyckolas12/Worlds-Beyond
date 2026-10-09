// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemies/BeyondEnemySpawner.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "Components/BillboardComponent.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "EngineUtils.h"
#include "Game/BeyondCombatSubsystem.h"
#include "TimerManager.h"

ABeyondEnemySpawner::ABeyondEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
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

	if (ActivationRadius <= 0.0f)
	{
		bActivated = true;
		SpawnMissing();
	}
	else
	{
		GetWorldTimerManager().SetTimer(ActivationTimer, this, &ThisClass::CheckActivation, 0.5f, true, 0.5f);
	}
}

void ABeyondEnemySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		Combat->OnPartyWiped.Remove(PartyWipedHandle);
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

void ABeyondEnemySpawner::CheckActivation()
{
	for (TActorIterator<ABeyondCharacterBase> It(GetWorld()); It; ++It)
	{
		if (It->TeamAffiliation == EBeyondTeam::Player && !UBeyondCombatLibrary::IsActorDead(*It)
			&& FVector::Dist(It->GetActorLocation(), GetActorLocation()) <= ActivationRadius)
		{
			GetWorldTimerManager().ClearTimer(ActivationTimer);
			bActivated = true;
			SpawnMissing();
			return;
		}
	}
}

void ABeyondEnemySpawner::SpawnSlot(FSlot& Slot)
{
	UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(this);
	const FBeyondSpawnEntry& Entry = Entries.IsValidIndex(Slot.EntryIndex) ? Entries[Slot.EntryIndex] : FBeyondSpawnEntry();
	if (!Enemies || !Entry.Enemy)
	{
		return;
	}

	FBeyondEnemySpawnParams Params;
	Params.Level = Entry.Level;
	Params.bElite = Entry.Enemy->bCanBeElite && FMath::FRand() < EliteChance;
	// Facing outward from the camp
	const FVector Outward = (Slot.Location - GetActorLocation()).GetSafeNormal2D();
	const float Yaw = Outward.IsNearlyZero() ? GetActorRotation().Yaw : Outward.Rotation().Yaw;
	if (ABeyondEnemyCharacter* Enemy = Enemies->SpawnEnemy(Entry.Enemy, FTransform(FRotator(0.0f, Yaw, 0.0f), Slot.Location), Params))
	{
		Slot.Enemy = Enemy;
		Slot.bEverSpawned = true;
		Enemy->OnCharacterKilled.AddUniqueDynamic(this, &ThisClass::HandleEnemyKilled);
	}
}

void ABeyondEnemySpawner::SpawnMissing()
{
	for (FSlot& Slot : Slots)
	{
		if (!Slot.Enemy.IsValid() || UBeyondCombatLibrary::IsActorDead(Slot.Enemy.Get()))
		{
			SpawnSlot(Slot);
		}
	}
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
	if (Respawn != EBeyondRespawnRule::AfterDelay)
	{
		return;
	}
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].Enemy.Get() == Character)
		{
			FTimerHandle Timer;
			GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Index]()
			{
				if (Slots.IsValidIndex(Index) && (!Slots[Index].Enemy.IsValid() || UBeyondCombatLibrary::IsActorDead(Slots[Index].Enemy.Get())))
				{
					SpawnSlot(Slots[Index]);
				}
			}), RespawnDelay, false);
			return;
		}
	}
}

void ABeyondEnemySpawner::HandlePartyWiped()
{
	if (bActivated && Respawn == EBeyondRespawnRule::OnPartyWipe)
	{
		// After the party is back on its feet at the checkpoint
		FTimerHandle Timer;
		GetWorldTimerManager().SetTimer(Timer, this, &ThisClass::SpawnMissing, 3.5f, false);
	}
}
