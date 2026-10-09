// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/BeyondBossArena.h"
#include "WorldBeyond.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AI/BeyondEnemyController.h"
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
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

ABeyondBossArena::ABeyondBossArena()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

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
	Super::EndPlay(EndPlayReason);
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
	SetSealed(bSealDuringFight);

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
		Wall->SetBoxExtent(FVector(Segment * 0.55f, 40.0f, SealHeight * 0.5f));
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
