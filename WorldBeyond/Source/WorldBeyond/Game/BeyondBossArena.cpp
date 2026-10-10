// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/BeyondBossArena.h"
#include "WorldBeyond.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AI/BeyondCompanionController.h"
#include "AI/BeyondEnemyController.h"
#include "BeyondGameplayTags.h"
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Enemies/BeyondBossCharacter.h"
#include "Enemies/BeyondBossDefinition.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "EngineUtils.h"
#include "Game/BeyondCombatSubsystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "TimerManager.h"

namespace
{
	// The seal walls are 80 uu thick, centred on Arena Radius
	constexpr float SealWallHalfThickness = 40.0f;

	float GetCapsuleRadiusOf(const ABeyondCharacterBase* Character)
	{
		return Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 50.0f;
	}
}

ABeyondBossArena::ABeyondBossArena()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	PartyRush.Speed = 2400.0f;
	PartyRush.StopDistance = 60.0f;
	PartyRush.MaxRushDistance = 1500.0f;
	PartyRush.Timeout = 1.0f;

	BossSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("BossSpawnPoint"));
	BossSpawnPoint->SetupAttachment(GetRootComponent());

	DarknessVolume = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Darkness"));
	DarknessVolume->SetupAttachment(GetRootComponent());
	DarknessVolume->bUnbound = true;
	DarknessVolume->BlendWeight = 0.0f;
	DarknessVolume->Settings.bOverride_AutoExposureBias = true;
	DarknessVolume->Settings.AutoExposureBias = -2.5f;
	DarknessVolume->Settings.bOverride_VignetteIntensity = true;
	DarknessVolume->Settings.VignetteIntensity = 1.1f;
	DarknessVolume->Settings.bOverride_ColorGain = true;
	DarknessVolume->Settings.ColorGain = FVector4(0.55f, 0.45f, 0.8f, 1.0f);

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(GetRootComponent());
		Sprite->bIsScreenSizeScaled = true;
	}
#endif
}

void ABeyondBossArena::BeginPlay()
{
	Super::BeginPlay();
	BuildSeal();

	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		PartyWipedHandle = Combat->OnPartyWiped.AddUObject(this, &ThisClass::HandlePartyWiped);
	}

	// After the party has formed and loaded its save (which says whether this boss is already beaten)
	FTimerHandle Start;
	GetWorldTimerManager().SetTimer(Start, this, &ThisClass::SpawnBoss, 0.5f, false);
	GetWorldTimerManager().SetTimer(EngageTimer, this, &ThisClass::CheckEngage, 0.25f, true, 0.75f);
}

void ABeyondBossArena::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		Combat->OnPartyWiped.Remove(PartyWipedHandle);
	}
	CancelPull();
	Super::EndPlay(EndPlayReason);
}

bool ABeyondBossArena::IsInside(const FVector& Location, float Margin) const
{
	return FVector::Dist2D(Location, GetActorLocation()) <= ArenaRadius - SealWallHalfThickness - Margin;
}

FVector ABeyondBossArena::ClampInside(const FVector& Point, float Margin) const
{
	const FVector Centre = GetActorLocation();
	const float MaxDistance = FMath::Max(ArenaRadius - SealWallHalfThickness - Margin, 0.0f);
	FVector Offset = Point - Centre;
	Offset.Z = 0.0f;
	if (Offset.Size() <= MaxDistance)
	{
		return Point;
	}
	const FVector Clamped = Centre + Offset.GetSafeNormal() * MaxDistance;
	return FVector(Clamped.X, Clamped.Y, Point.Z);
}

ABeyondBossArena* ABeyondBossArena::FindSealedArenaAt(const UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ABeyondBossArena> It(World); It; ++It)
	{
		if (It->IsSealed() && It->IsInside(Location))
		{
			return *It;
		}
	}
	return nullptr;
}

TArray<ABeyondCharacterBase*> ABeyondBossArena::GetParty() const
{
	if (const ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetWorld()->GetFirstPlayerController()); PC && PC->PartyComponent)
	{
		return PC->PartyComponent->GetMembers();
	}
	TArray<ABeyondCharacterBase*> Party;
	for (TActorIterator<ABeyondCharacterBase> It(GetWorld()); It; ++It)
	{
		if (It->TeamAffiliation == EBeyondTeam::Player)
		{
			Party.Add(*It);
		}
	}
	return Party;
}

FVector ABeyondBossArena::GetEntryPoint(const ABeyondCharacterBase* Member, const ABeyondCharacterBase* Anchor, int32 Index) const
{
	const FVector Centre = GetActorLocation();
	const float Radius = GetCapsuleRadiusOf(Member);
	const float Margin = Radius + PullMargin;

	FVector Point;
	if (Anchor && Anchor != Member)
	{
		// Beside the demigod already inside
		FVector Out = (Anchor->GetActorLocation() - Centre).GetSafeNormal2D();
		if (Out.IsNearlyZero())
		{
			Out = -Anchor->GetActorForwardVector().GetSafeNormal2D();
		}
		const FVector Side = FVector(-Out.Y, Out.X, 0.0f) * (Index % 2 == 0 ? 1.0f : -1.0f);
		Point = Anchor->GetActorLocation() + Side * 220.0f + Out * 60.0f;
	}
	else
	{
		// Just inside the wall, on the side they came from
		FVector Out = (Member->GetActorLocation() - Centre).GetSafeNormal2D();
		if (Out.IsNearlyZero())
		{
			Out = GetActorForwardVector().GetSafeNormal2D();
		}
		const FVector Side(-Out.Y, Out.X, 0.0f);
		Point = Centre + Out * (ArenaRadius - SealWallHalfThickness - Margin) + Side * 200.0f * Index;
	}
	Point = ClampInside(Point, Margin);

	// Not on top of the boss
	if (const ABeyondBossCharacter* BossCharacter = SpawnedBoss.Get())
	{
		const float Clearance = GetCapsuleRadiusOf(BossCharacter) + Radius + 150.0f;
		FVector FromBoss = Point - BossCharacter->GetActorLocation();
		FromBoss.Z = 0.0f;
		if (FromBoss.Size() < Clearance)
		{
			FVector Away = FromBoss.GetSafeNormal();
			if (Away.IsNearlyZero())
			{
				Away = (Point - Centre).GetSafeNormal2D();
			}
			Point = ClampInside(BossCharacter->GetActorLocation() + Away * Clearance, Margin);
		}
	}

	Point.Z = Member->GetActorLocation().Z;
	FVector Ground;
	if (UBeyondEnemySubsystem::FindGroundPoint(GetWorld(), Point, Ground))
	{
		const float HalfHeight = Member->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.0f;
		// The navmesh snap may slide toward the wall; keep the spot inside and only take the ground height then
		Point = IsInside(Ground, Radius + SealWallHalfThickness) ? Ground + FVector(0.0f, 0.0f, HalfHeight) : FVector(Point.X, Point.Y, Ground.Z + HalfHeight);
	}
	return Point;
}

bool ABeyondBossArena::PullPartyInside(AActor* Trigger)
{
	const TArray<ABeyondCharacterBase*> Party = GetParty();
	auto IsWellInside = [this](const ABeyondCharacterBase* Character)
	{
		return Character && IsInside(Character->GetActorLocation(), GetCapsuleRadiusOf(Character) + PullMargin * 0.5f);
	};

	// Whoever is already in (the one who walked in, if alive) is where the others gather
	const ABeyondCharacterBase* Anchor = nullptr;
	if (const ABeyondCharacterBase* TriggerCharacter = Cast<ABeyondCharacterBase>(Trigger);
		TriggerCharacter && !UBeyondCombatLibrary::IsActorDead(TriggerCharacter) && IsWellInside(TriggerCharacter))
	{
		Anchor = TriggerCharacter;
	}
	for (const ABeyondCharacterBase* Member : Party)
	{
		if (!Anchor && Member && !UBeyondCombatLibrary::IsActorDead(Member) && IsWellInside(Member))
		{
			Anchor = Member;
		}
	}

	TGuardValue<bool> Starting(bStartingPull, true);
	int32 Index = 0;
	for (ABeyondCharacterBase* Member : Party)
	{
		if (!Member || IsWellInside(Member))
		{
			continue;
		}
		const FVector Destination = GetEntryPoint(Member, Anchor, Index++);
		const FVector ToCentre = GetActorLocation() - Destination;
		const FRotator Facing(0.0f, ToCentre.Rotation().Yaw, 0.0f);

		// A fallen demigod is carried in, body and all, so it can still be revived
		if (UBeyondCombatLibrary::IsActorDead(Member))
		{
			const FVector Delta = Destination - Member->GetActorLocation();
			TArray<USkeletalMeshComponent*> Meshes;
			Member->GetComponents(Meshes);
			Member->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
			for (USkeletalMeshComponent* Mesh : Meshes)
			{
				if (Mesh->IsSimulatingPhysics())
				{
					Mesh->SetWorldLocation(Mesh->GetComponentLocation() + Delta, false, nullptr, ETeleportType::TeleportPhysics);
				}
			}
			UE_LOG(LogBeyond, Log, TEXT("Arena %s: carried %s's body inside"), *GetName(), *Member->GetName());
			continue;
		}

		// The duo move keeps going (it moves them itself); anything else they were doing stops
		UAbilitySystemComponent* ASC = Member->GetAbilitySystemComponent();
		if (ASC && ASC->HasMatchingGameplayTag(BeyondTags::State_Duo))
		{
			UBeyondRushComponent::Blink(Member, Destination, Facing, PartyRush);
			continue;
		}
		if (ASC)
		{
			ASC->CancelAllAbilities();
		}

		Incoming.Add(Member, Destination);
		TWeakObjectPtr<ABeyondBossArena> WeakThis(this);
		TWeakObjectPtr<ABeyondCharacterBase> WeakMember(Member);
		UE_LOG(LogBeyond, Log, TEXT("Arena %s: %s comes in (%.0f uu)"), *GetName(), *Member->GetName(), FVector::Dist2D(Member->GetActorLocation(), Destination));
		UBeyondRushComponent::Rush(Member, nullptr, Destination, PartyRush, [WeakThis, WeakMember](bool bArrived)
		{
			if (ABeyondBossArena* Arena = WeakThis.Get())
			{
				Arena->HandleMemberArrived(WeakMember.Get());
			}
		});
	}

	if (Incoming.IsEmpty())
	{
		return false;
	}
	GetWorldTimerManager().SetTimer(PullTimer, this, &ThisClass::FinishPull, PullTimeout, false);
	return true;
}

void ABeyondBossArena::HandleMemberArrived(ABeyondCharacterBase* Member)
{
	Incoming.Remove(Member);
	if (ABeyondCompanionController* Companion = Member ? Cast<ABeyondCompanionController>(Member->GetController()) : nullptr)
	{
		Companion->ResetEngagement();
	}
	if (!bStartingPull && Incoming.IsEmpty())
	{
		FinishPull();
	}
}

void ABeyondBossArena::FinishPull()
{
	GetWorldTimerManager().ClearTimer(PullTimer);
	// Out of time: whoever is still on the way blinks the rest of it
	TMap<TWeakObjectPtr<ABeyondCharacterBase>, FVector> Late = MoveTemp(Incoming);
	Incoming.Reset();
	for (const TPair<TWeakObjectPtr<ABeyondCharacterBase>, FVector>& Entry : Late)
	{
		if (ABeyondCharacterBase* Member = Entry.Key.Get())
		{
			if (UBeyondRushComponent* Rush = UBeyondRushComponent::FindRush(Member))
			{
				Rush->Cancel();
			}
			const FVector ToCentre = GetActorLocation() - Entry.Value;
			UBeyondRushComponent::Blink(Member, Entry.Value, FRotator(0.0f, ToCentre.Rotation().Yaw, 0.0f), PartyRush);
			if (ABeyondCompanionController* Companion = Cast<ABeyondCompanionController>(Member->GetController()))
			{
				Companion->ResetEngagement();
			}
		}
	}
	if (State == EBeyondArenaState::Fighting)
	{
		SetSealed(bSealDuringFight);
	}
}

void ABeyondBossArena::CancelPull()
{
	GetWorldTimerManager().ClearTimer(PullTimer);
	for (const TPair<TWeakObjectPtr<ABeyondCharacterBase>, FVector>& Entry : Incoming)
	{
		if (UBeyondRushComponent* Rush = UBeyondRushComponent::FindRush(Entry.Key.Get()))
		{
			Rush->Cancel();
		}
	}
	Incoming.Reset();
}

bool ABeyondBossArena::IsBeaten() const
{
	const ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetWorld()->GetFirstPlayerController());
	return Boss && Boss->bStoryBoss && PC && PC->PartyComponent && PC->PartyComponent->IsBossDefeated(Boss->BossId);
}

void ABeyondBossArena::SpawnBoss()
{
	if (SpawnedBoss.IsValid() && !UBeyondCombatLibrary::IsActorDead(SpawnedBoss.Get()))
	{
		return;
	}
	if (IsBeaten())
	{
		State = EBeyondArenaState::Defeated;
		UE_LOG(LogBeyond, Log, TEXT("Arena %s: %s is already beaten"), *GetName(), *GetNameSafe(Boss));
		return;
	}

	UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(this);
	if (!Enemies || !Boss)
	{
		return;
	}
	FBeyondEnemySpawnParams Params;
	Params.Level = BossLevel;
	ABeyondEnemyCharacter* Spawned = Enemies->SpawnEnemy(Boss, BossSpawnPoint->GetComponentTransform(), Params);
	ABeyondBossCharacter* BossCharacter = Cast<ABeyondBossCharacter>(Spawned);
	if (!BossCharacter)
	{
		UE_LOG(LogBeyond, Warning, TEXT("Arena %s: %s didn't spawn as an ABeyondBossCharacter (check its Character Class)"), *GetName(), *GetNameSafe(Boss));
		if (Spawned)
		{
			Spawned->Destroy();
		}
		return;
	}

	BossCharacter->SetArena(this);
	if (ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(BossCharacter->GetController()))
	{
		AI->SetBrainEnabled(false);
	}
	BossCharacter->OnCharacterKilled.AddUniqueDynamic(this, &ThisClass::HandleBossKilled);
	SpawnedBoss = BossCharacter;
	State = EBeyondArenaState::Dormant;
}

void ABeyondBossArena::CheckEngage()
{
	ABeyondBossCharacter* BossCharacter = SpawnedBoss.Get();
	if (State != EBeyondArenaState::Dormant || !BossCharacter || UBeyondCombatLibrary::IsActorDead(BossCharacter))
	{
		return;
	}

	for (TActorIterator<ABeyondCharacterBase> It(GetWorld()); It; ++It)
	{
		if (It->TeamAffiliation == EBeyondTeam::Player && !UBeyondCombatLibrary::IsActorDead(*It)
			&& FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= EngageRadius)
		{
			Engage(*It);
			return;
		}
	}

	// Hit from outside the engage radius (a long-range spell): that starts it too
	if (UBeyondCombatLibrary::GetActorHealth(BossCharacter) < UBeyondCombatLibrary::GetActorMaxHealth(BossCharacter))
	{
		Engage(nullptr);
	}
}

void ABeyondBossArena::Engage(AActor* Target)
{
	ABeyondBossCharacter* BossCharacter = SpawnedBoss.Get();
	if (State != EBeyondArenaState::Dormant || !BossCharacter || UBeyondCombatLibrary::IsActorDead(BossCharacter))
	{
		return;
	}
	State = EBeyondArenaState::Fighting;
	// The whole party comes inside first (whichever demigod walked in); the walls rise once they're all in
	if (!bSealDuringFight || !bPullPartyInside || !PullPartyInside(Target))
	{
		SetSealed(bSealDuringFight);
	}

	if (!Target)
	{
		float Best = TNumericLimits<float>::Max();
		for (TActorIterator<ABeyondCharacterBase> It(GetWorld()); It; ++It)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), BossCharacter->GetActorLocation());
			if (It->TeamAffiliation == EBeyondTeam::Player && !UBeyondCombatLibrary::IsActorDead(*It) && Distance < Best)
			{
				Target = *It;
				Best = Distance;
			}
		}
	}
	if (ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(BossCharacter->GetController()))
	{
		AI->SetBrainEnabled(true);
		if (Target)
		{
			AI->EngageTarget(Target, false);
		}
	}
	UE_LOG(LogBeyond, Log, TEXT("Arena %s: the fight with %s begins"), *GetName(), *BossCharacter->GetEnemyName().ToString());
}

void ABeyondBossArena::HandleBossKilled(ABeyondCharacterBase* Character, AActor* Killer)
{
	State = EBeyondArenaState::Defeated;
	CancelPull();
	SetSealed(false);
	SetDarkness(0.0f);

	if (Boss && Boss->bStoryBoss)
	{
		if (const ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetWorld()->GetFirstPlayerController()); PC && PC->PartyComponent)
		{
			PC->PartyComponent->MarkBossDefeated(Boss->BossId);
		}
	}
	UE_LOG(LogBeyond, Log, TEXT("Arena %s: %s defeated"), *GetName(), *GetNameSafe(Boss));
}

void ABeyondBossArena::HandlePartyWiped()
{
	if (State == EBeyondArenaState::Fighting || State == EBeyondArenaState::Dormant)
	{
		ResetArena();
	}
}

void ABeyondBossArena::ResetArena()
{
	if (State == EBeyondArenaState::Defeated)
	{
		return;
	}
	if (ABeyondBossCharacter* BossCharacter = SpawnedBoss.Get())
	{
		// Its adds and hazards go with it (ABeyondBossCharacter::EndPlay)
		BossCharacter->Destroy();
	}
	SpawnedBoss.Reset();
	CancelPull();
	SetSealed(false);
	SetDarkness(0.0f);
	State = EBeyondArenaState::Dormant;
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &ThisClass::SpawnBoss, FMath::Max(RespawnDelay, 0.01f), false);
}

void ABeyondBossArena::BuildSeal()
{
	if (!SealWalls.IsEmpty() || SealSegments <= 0)
	{
		return;
	}
	const float Segment = 2.0f * PI * ArenaRadius / SealSegments;
	for (int32 Index = 0; Index < SealSegments; ++Index)
	{
		const float Angle = 2.0f * PI * Index / SealSegments;
		UBoxComponent* Wall = NewObject<UBoxComponent>(this, *FString::Printf(TEXT("SealWall%d"), Index));
		Wall->SetupAttachment(GetRootComponent());
		Wall->SetCanEverAffectNavigation(false);
		Wall->SetBoxExtent(FVector(Segment * 0.55f, SealWallHalfThickness, SealHeight * 0.5f));
		Wall->SetRelativeLocationAndRotation(FVector(FMath::Cos(Angle) * ArenaRadius, FMath::Sin(Angle) * ArenaRadius, SealHeight * 0.5f),
			FRotator(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f));
		Wall->SetCollisionObjectType(ECC_WorldDynamic);
		Wall->SetCollisionResponseToAllChannels(ECR_Ignore);
		Wall->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Wall->RegisterComponent();
		SealWalls.Add(Wall);
	}
}

void ABeyondBossArena::SetSealed(bool bNewSealed)
{
	if (bSealed == bNewSealed)
	{
		return;
	}
	bSealed = bNewSealed;
	for (UBoxComponent* Wall : SealWalls)
	{
		Wall->SetCollisionEnabled(bSealed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bSealed && SealFX.IsSet())
		{
			SealEffects.Add(BeyondFX::SpawnAttached(SealFX, Wall));
		}
	}
	if (!bSealed)
	{
		for (const TWeakObjectPtr<UFXSystemComponent>& Effect : SealEffects)
		{
			if (UFXSystemComponent* Component = Effect.Get())
			{
				Component->DestroyComponent();
			}
		}
		SealEffects.Reset();
	}
}

void ABeyondBossArena::SetDarkness(float Amount)
{
	Darkness = FMath::Clamp(Amount, 0.0f, 1.0f);
	DarknessVolume->BlendWeight = Darkness;
}
