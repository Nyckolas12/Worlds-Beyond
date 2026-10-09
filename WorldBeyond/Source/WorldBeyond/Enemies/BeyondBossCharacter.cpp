// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemies/BeyondBossCharacter.h"
#include "WorldBeyond.h"
#include "AbilitySystem/BeyondAreaStrike.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AI/BeyondEnemyController.h"
#include "Animation/AnimMontage.h"
#include "BeyondGameplayTags.h"
#include "Camera/CameraComponent.h"
#include "CharacterAttributeSet.h"
#include "Enemies/BeyondBossDefinition.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "EngineUtils.h"
#include "Game/BeyondBossArena.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/IConsoleManager.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

namespace
{
	FGameplayTag BossPhaseTag(int32 Index)
	{
		switch (Index)
		{
		case 0: return BeyondTags::Boss_Phase_1;
		case 1: return BeyondTags::Boss_Phase_2;
		case 2: return BeyondTags::Boss_Phase_3;
		case 3: return BeyondTags::Boss_Phase_4;
		default: return FGameplayTag();
		}
	}

	constexpr float BossAnnouncementDuration = 3.5f;

	FAutoConsoleCommandWithWorldAndArgs GBossPhaseCommand(
		TEXT("Beyond.BossPhase"),
		TEXT("Moves the nearest living boss to a phase: Beyond.BossPhase <1..n>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (!Pawn || Args.IsEmpty() || !Args[0].IsNumeric())
			{
				UE_LOG(LogBeyond, Display, TEXT("Beyond.BossPhase <1..n> (needs a player pawn and a living boss)"));
				return;
			}
			ABeyondBossCharacter* Nearest = nullptr;
			float NearestDistance = TNumericLimits<float>::Max();
			for (TActorIterator<ABeyondBossCharacter> It(World); It; ++It)
			{
				const float Distance = FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation());
				if (!UBeyondCombatLibrary::IsActorDead(*It) && !It->bClone && Distance < NearestDistance)
				{
					Nearest = *It;
					NearestDistance = Distance;
				}
			}
			if (Nearest)
			{
				Nearest->ForcePhase(FCString::Atoi(*Args[0]) - 1);
			}
		}));
}

ABeyondBossCharacter::ABeyondBossCharacter()
{
	DestroyDelayAfterDeath = 20.0f;
}

void ABeyondBossCharacter::SetArena(ABeyondBossArena* InArena)
{
	Arena = InArena;
}

UBeyondBossDefinition* ABeyondBossCharacter::GetBossDefinition() const
{
	return Cast<UBeyondBossDefinition>(Definition);
}

int32 ABeyondBossCharacter::GetPhaseCount() const
{
	const UBeyondBossDefinition* Boss = GetBossDefinition();
	return Boss ? Boss->Phases.Num() : 0;
}

TArray<float> ABeyondBossCharacter::GetPhaseThresholds() const
{
	TArray<float> Result;
	if (const UBeyondBossDefinition* Boss = GetBossDefinition())
	{
		for (int32 Index = 1; Index < Boss->Phases.Num(); ++Index)
		{
			Result.Add(Boss->Phases[Index].HealthThreshold);
		}
	}
	return Result;
}

FText ABeyondBossCharacter::GetBossTitle() const
{
	const UBeyondBossDefinition* Boss = GetBossDefinition();
	return Boss ? Boss->Title : FText::GetEmpty();
}

bool ABeyondBossCharacter::GetAnnouncement(FText& OutText, float& OutAge) const
{
	OutAge = GetWorld()->GetTimeSeconds() - AnnouncementTime;
	OutText = Announcement;
	return !Announcement.IsEmpty() && OutAge <= BossAnnouncementDuration;
}

bool ABeyondBossCharacter::RunsPhases() const
{
	return !bClone && !bSummoned && GetPhaseCount() > 0;
}

void ABeyondBossCharacter::BeginPlay()
{
	Super::BeginPlay();

	// The Paragon hero Blueprints were made for players: an AI boss doesn't need their camera
	if (!IsPlayerControlled())
	{
		TInlineComponentArray<UCameraComponent*> Cameras(this);
		for (UCameraComponent* Camera : Cameras)
		{
			Camera->DestroyComponent();
		}
		TInlineComponentArray<USpringArmComponent*> Arms(this);
		for (USpringArmComponent* Arm : Arms)
		{
			Arm->DestroyComponent();
		}
	}

	if (!RunsPhases())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetCurrentHealthAttribute())
			.AddUObject(this, &ThisClass::HandleHealthChanged);
	}
	EnterPhase(0, false);
	bPhasesStarted = true;
}

void ABeyondBossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearFight(false);
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetCurrentHealthAttribute()).Remove(HealthChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

float ABeyondBossCharacter::GetHealthFloor() const
{
	const UBeyondBossDefinition* Boss = GetBossDefinition();
	if (!RunsPhases() || !Boss || !bPhasesStarted)
	{
		return 0.0f;
	}
	const int32 Next = PhaseIndex + 1;
	return Boss->Phases.IsValidIndex(Next) ? UBeyondCombatLibrary::GetActorMaxHealth(this) * Boss->Phases[Next].HealthThreshold : 0.0f;
}

float ABeyondBossCharacter::ModifyDamageTaken(float Damage, AActor* DamageInstigator, const FGameplayTagContainer& DamageTags) const
{
	Damage = Super::ModifyDamageTaken(Damage, DamageInstigator, DamageTags);
	if (SummonShield < 1.0f && CountLiveSummons() > 0)
	{
		Damage *= SummonShield;
	}
	return Damage;
}

void ABeyondBossCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	const UBeyondBossDefinition* Boss = GetBossDefinition();
	const int32 Next = PhaseIndex + 1;
	if (!Boss || bTransitioning || bPhasePending || !Boss->Phases.IsValidIndex(Next) || UBeyondCombatLibrary::IsActorDead(this))
	{
		return;
	}

	const float Threshold = UBeyondCombatLibrary::GetActorMaxHealth(this) * Boss->Phases[Next].HealthThreshold;
	if (Data.NewValue <= Threshold + 0.5f)
	{
		// Next tick: not from inside the damage callback
		bPhasePending = true;
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Next]()
		{
			bPhasePending = false;
			if (!UBeyondCombatLibrary::IsActorDead(this) && PhaseIndex + 1 == Next)
			{
				EnterPhase(Next, true);
			}
		}));
	}
}

void ABeyondBossCharacter::ForcePhase(int32 NewPhaseIndex)
{
	const UBeyondBossDefinition* Boss = GetBossDefinition();
	if (!RunsPhases() || !Boss || !Boss->Phases.IsValidIndex(NewPhaseIndex) || NewPhaseIndex == PhaseIndex || UBeyondCombatLibrary::IsActorDead(this))
	{
		return;
	}
	// Down to the phase's threshold, so the health bar and the floor agree
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		const float Health = UBeyondCombatLibrary::GetActorMaxHealth(this) * Boss->Phases[NewPhaseIndex].HealthThreshold;
		if (Health < UBeyondCombatLibrary::GetActorHealth(this))
		{
			ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), FMath::Max(Health, 1.0f));
		}
	}
	if (bTransitioning)
	{
		EndTransition();
	}
	EnterPhase(NewPhaseIndex, true);
}

void ABeyondBossCharacter::EnterPhase(int32 NewPhaseIndex, bool bTransition)
{
	const UBeyondBossDefinition* Boss = GetBossDefinition();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!Boss || !ASC || !Boss->Phases.IsValidIndex(NewPhaseIndex))
	{
		return;
	}
	const FBeyondBossPhase& Phase = Boss->Phases[NewPhaseIndex];
	PhaseIndex = NewPhaseIndex;

	// Phase tag: abilities can require it (ActivationRequiredTags)
	for (int32 Index = 0; Index < 4; ++Index)
	{
		ASC->SetLooseGameplayTagCount(BossPhaseTag(Index), Index == NewPhaseIndex ? 1 : 0);
	}

	if (Phase.Abilities && HasAuthority())
	{
		Phase.Abilities->GiveToAbilitySystem(ASC, this, &PhaseAbilityHandles);
	}

	if (bTransition && Phase.TransitionDuration > 0.0f)
	{
		bTransitioning = true;
		ASC->AddLooseGameplayTag(BeyondTags::State_Invincible);
		ASC->CancelAllAbilities();
		if (ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(GetController()))
		{
			AI->SetBrainEnabled(false);
		}
		GetCharacterMovement()->StopMovementImmediately();
		if (Phase.TransitionMontage)
		{
			PlayAnimMontage(Phase.TransitionMontage);
		}
		BeyondFX::SpawnAttached(Phase.TransitionFX, GetRootComponent());
		GetWorldTimerManager().SetTimer(TransitionTimer, this, &ThisClass::EndTransition, Phase.TransitionDuration, false);
	}

	for (const FBeyondBossTwist& Twist : Phase.Twists)
	{
		RunTwist(Twist);
	}

	UE_LOG(LogBeyond, Log, TEXT("Boss %s: phase %d (%s)"), *GetEnemyName().ToString(), NewPhaseIndex + 1, *Phase.Name.ToString());
	OnPhaseChanged.Broadcast(this, NewPhaseIndex);
}

void ABeyondBossCharacter::EndTransition()
{
	GetWorldTimerManager().ClearTimer(TransitionTimer);
	if (!bTransitioning)
	{
		return;
	}
	bTransitioning = false;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->RemoveLooseGameplayTag(BeyondTags::State_Invincible);
	}
	if (ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(GetController()); AI && !UBeyondCombatLibrary::IsActorDead(this))
	{
		AI->SetBrainEnabled(true);
		if (AActor* Target = FindFightTarget())
		{
			AI->EngageTarget(Target, false);
		}
	}
}

AActor* ABeyondBossCharacter::FindFightTarget() const
{
	if (const ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(GetController()); AI && AI->GetCombatTarget()
		&& !UBeyondCombatLibrary::IsActorDead(AI->GetCombatTarget()))
	{
		return AI->GetCombatTarget();
	}
	AActor* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<ABeyondCharacterBase> It(GetWorld()); It; ++It)
	{
		if (UBeyondCombatLibrary::AreHostile(this, *It) && !UBeyondCombatLibrary::IsActorDead(*It))
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), GetActorLocation());
			if (Distance < BestDistance)
			{
				Best = *It;
				BestDistance = Distance;
			}
		}
	}
	return Best;
}

FVector ABeyondBossCharacter::GetFightCentre() const
{
	if (const ABeyondBossArena* FightArena = Arena.Get())
	{
		return FightArena->GetActorLocation();
	}
	return GetHomeTransform().GetLocation();
}

float ABeyondBossCharacter::GetFightRadius() const
{
	const ABeyondBossArena* FightArena = Arena.Get();
	return FightArena ? FightArena->ArenaRadius : 2000.0f;
}

void ABeyondBossCharacter::RunTwist(const FBeyondBossTwist& Twist)
{
	if (!Twist.Announcement.IsEmpty())
	{
		Announcement = Twist.Announcement;
		AnnouncementTime = GetWorld()->GetTimeSeconds();
	}

	switch (Twist.Type)
	{
	case EBeyondTwistType::Hazards:
		SpawnHazards(Twist);
		break;
	case EBeyondTwistType::Summon:
		SpawnSummons(Twist);
		break;
	case EBeyondTwistType::Enrage:
		Enrage(Twist);
		break;
	case EBeyondTwistType::ShadowClones:
		SpawnClones(Twist);
		break;
	case EBeyondTwistType::Darkness:
		if (ABeyondBossArena* FightArena = Arena.Get())
		{
			FightArena->SetDarkness(Twist.Darkness);
		}
		break;
	}

	// Hazards and summons keep coming
	const bool bRepeats = Twist.RepeatInterval > 0.0f && (Twist.Type == EBeyondTwistType::Hazards || Twist.Type == EBeyondTwistType::Summon
		|| Twist.Type == EBeyondTwistType::ShadowClones);
	if (bRepeats)
	{
		FBeyondBossTwist Repeat = Twist;
		Repeat.RepeatInterval = 0.0f;
		Repeat.Announcement = FText::GetEmpty();
		FTimerHandle& Timer = TwistTimers.AddDefaulted_GetRef();
		GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Repeat]()
		{
			if (!UBeyondCombatLibrary::IsActorDead(this) && !bTransitioning)
			{
				RunTwist(Repeat);
			}
		}), Twist.RepeatInterval, true);
	}
}

void ABeyondBossCharacter::SpawnHazards(const FBeyondBossTwist& Twist)
{
	const FVector Centre = GetFightCentre();
	TArray<FVector> Points;
	switch (Twist.Placement)
	{
	case EBeyondHazardPlacement::ArenaCentre:
		for (int32 Index = 0; Index < Twist.HazardCount; ++Index)
		{
			Points.Add(Centre);
		}
		break;
	case EBeyondHazardPlacement::AroundArena:
		for (int32 Index = 0; Index < Twist.HazardCount; ++Index)
		{
			const float Angle = 2.0f * PI * Index / FMath::Max(Twist.HazardCount, 1) + FMath::FRandRange(-0.2f, 0.2f);
			Points.Add(Centre + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Twist.Spread);
		}
		break;
	case EBeyondHazardPlacement::AtTargets:
	{
		TArray<AActor*> Targets;
		for (TActorIterator<ABeyondCharacterBase> It(GetWorld()); It; ++It)
		{
			if (UBeyondCombatLibrary::AreHostile(this, *It) && !UBeyondCombatLibrary::IsActorDead(*It)
				&& FVector::Dist2D(It->GetActorLocation(), Centre) <= GetFightRadius() * 1.5f)
			{
				Targets.Add(*It);
			}
		}
		for (int32 Index = 0; Index < Twist.HazardCount && !Targets.IsEmpty(); ++Index)
		{
			const FVector2D Offset = Index < Targets.Num() ? FVector2D::ZeroVector : FMath::RandPointInCircle(Twist.Spread);
			Points.Add(Targets[Index % Targets.Num()]->GetActorLocation() + FVector(Offset.X, Offset.Y, 0.0f));
		}
		break;
	}
	}

	Hazards.RemoveAll([](const TWeakObjectPtr<ABeyondAreaStrike>& Hazard) { return !Hazard.IsValid(); });
	for (const FVector& Point : Points)
	{
		FVector Ground = Point;
		UBeyondEnemySubsystem::FindGroundPoint(GetWorld(), Point, Ground);
		if (ABeyondAreaStrike* Strike = ABeyondAreaStrike::SpawnStrike(this, this, Twist.Hazard, Ground, FMath::FRandRange(0.0f, 360.0f)))
		{
			// A lava floor outlasts a stumble of its maker; ClearFight removes it with the fight
			Strike->bCancelWithInstigator = false;
			Hazards.Add(Strike);
		}
	}
}

int32 ABeyondBossCharacter::CountLiveSummons() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ABeyondEnemyCharacter>& Summon : Summons)
	{
		Count += Summon.IsValid() && !UBeyondCombatLibrary::IsActorDead(Summon.Get()) ? 1 : 0;
	}
	return Count;
}

void ABeyondBossCharacter::SpawnSummons(const FBeyondBossTwist& Twist)
{
	UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(this);
	if (!Enemies || Twist.SummonIds.IsEmpty())
	{
		return;
	}
	SummonShield = FMath::Min(SummonShield, Twist.DamageTakenWhileSummonsLive);

	const int32 Wanted = FMath::Min(Twist.SummonCount, Twist.MaxAlive - CountLiveSummons());
	AActor* Target = FindFightTarget();
	for (int32 Index = 0; Index < Wanted; ++Index)
	{
		UBeyondEnemyDefinition* SummonDefinition = Enemies->FindDefinition(Twist.SummonIds[FMath::RandRange(0, Twist.SummonIds.Num() - 1)]);
		if (!SummonDefinition)
		{
			continue;
		}
		const float Angle = 2.0f * PI * Index / FMath::Max(Wanted, 1) + FMath::FRandRange(-0.3f, 0.3f);
		const FVector Location = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(450.0f, 700.0f);
		FBeyondEnemySpawnParams Params;
		Params.Level = GetCharacterLevel();
		Params.bSummoned = true;
		if (ABeyondEnemyCharacter* Summon = Enemies->SpawnEnemy(SummonDefinition, FTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f), Location), Params))
		{
			Summons.Add(Summon);
			Enemies->AlertEnemy(Summon, Target);
		}
	}
}

void ABeyondBossCharacter::SpawnClones(const FBeyondBossTwist& Twist)
{
	UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(this);
	if (!Enemies || !Definition)
	{
		return;
	}
	Clones.RemoveAll([](const TWeakObjectPtr<ABeyondEnemyCharacter>& Clone) { return !Clone.IsValid() || UBeyondCombatLibrary::IsActorDead(Clone.Get()); });
	AActor* Target = FindFightTarget();
	for (int32 Index = Clones.Num(); Index < Twist.CloneCount; ++Index)
	{
		const float Angle = 2.0f * PI * Index / FMath::Max(Twist.CloneCount, 1) + PI * 0.5f;
		const FVector Location = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 600.0f;
		FBeyondEnemySpawnParams Params;
		Params.Level = GetCharacterLevel();
		Params.bSummoned = true;
		Params.bClone = true;
		Params.HealthScale = Twist.CloneHealthFraction;
		Params.OutgoingDamageScale = Twist.CloneDamageFraction;
		Params.SizeScale = 0.92f;
		if (ABeyondEnemyCharacter* Clone = Enemies->SpawnEnemy(Definition, FTransform(GetActorRotation(), Location), Params))
		{
			Clone->SetTintColor(FLinearColor(0.12f, 0.02f, 0.25f, 0.85f));
			Clones.Add(Clone);
			Enemies->AlertEnemy(Clone, Target);
		}
	}
}

void ABeyondBossCharacter::Enrage(const FBeyondBossTwist& Twist)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (bEnraged || !ASC)
	{
		return;
	}
	bEnraged = true;
	ASC->AddLooseGameplayTag(BeyondTags::Boss_Enraged);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStrengthAttribute(),
		ASC->GetNumericAttributeBase(UCharacterAttributeSet::GetStrengthAttribute()) + Twist.EnrageDamageBonus);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetArcanaAttribute(),
		ASC->GetNumericAttributeBase(UCharacterAttributeSet::GetArcanaAttribute()) + Twist.EnrageDamageBonus);
	CombatSpeedScale *= Twist.EnrageSpeedMultiplier;
	GetCharacterMovement()->MaxWalkSpeed *= Twist.EnrageSpeedMultiplier;
	SetTintColor(Twist.EnrageTint);
	EnrageAura = BeyondFX::SpawnAttached(Twist.EnrageFX, GetRootComponent());
}

TArray<ABeyondEnemyCharacter*> ABeyondBossCharacter::GetMinions() const
{
	TArray<ABeyondEnemyCharacter*> Result;
	for (const TArray<TWeakObjectPtr<ABeyondEnemyCharacter>>* List : { &Summons, &Clones })
	{
		for (const TWeakObjectPtr<ABeyondEnemyCharacter>& Minion : *List)
		{
			if (ABeyondEnemyCharacter* Alive = Minion.Get(); Alive && !UBeyondCombatLibrary::IsActorDead(Alive))
			{
				Result.Add(Alive);
			}
		}
	}
	return Result;
}

TArray<ABeyondAreaStrike*> ABeyondBossCharacter::GetHazards() const
{
	TArray<ABeyondAreaStrike*> Result;
	for (const TWeakObjectPtr<ABeyondAreaStrike>& Hazard : Hazards)
	{
		if (ABeyondAreaStrike* Alive = Hazard.Get())
		{
			Result.Add(Alive);
		}
	}
	return Result;
}

void ABeyondBossCharacter::ClearFight(bool bKillMinions)
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Timer : TwistTimers)
		{
			World->GetTimerManager().ClearTimer(Timer);
		}
		World->GetTimerManager().ClearTimer(TransitionTimer);
	}
	TwistTimers.Reset();

	// Its adds fall with it (no loot, no EXP: they were summoned); on a reset they just vanish
	for (ABeyondEnemyCharacter* Minion : GetMinions())
	{
		if (bKillMinions)
		{
			UBeyondCombatLibrary::ApplyDamage(nullptr, Minion, 1.0e7f, BeyondTags::DamageType_Environment, FGameplayTag(), true, nullptr, true, true);
		}
		else
		{
			Minion->Destroy();
		}
	}
	Summons.Reset();
	Clones.Reset();

	for (ABeyondAreaStrike* Hazard : GetHazards())
	{
		Hazard->Destroy();
	}
	Hazards.Reset();

	if (UFXSystemComponent* Aura = EnrageAura.Get())
	{
		Aura->DestroyComponent();
	}
	if (ABeyondBossArena* FightArena = Arena.Get())
	{
		FightArena->SetDarkness(0.0f);
	}
}

void ABeyondBossCharacter::HandleDeath_Implementation()
{
	Super::HandleDeath_Implementation();
	if (RunsPhases())
	{
		ClearFight(true);
	}
}
