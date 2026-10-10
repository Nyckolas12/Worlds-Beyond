// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BeyondEnemyController.h"
#include "WorldBeyond.h"
#include "AI/BeyondAIAbilityUtils.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "BeyondGameplayTags.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

namespace
{
	const FBeyondEnemyAIConfig& DefaultEnemyAIConfig()
	{
		static const FBeyondEnemyAIConfig Config;
		return Config;
	}

	constexpr float EnemyReturnArrivedDistance = 150.0f;
	constexpr float EnemyReturnTimeout = 12.0f;
	constexpr float EnemyAggroSoundCooldown = 12.0f;
}

ABeyondEnemyController::ABeyondEnemyController()
{
	bAttachToPawn = true;
	bWantsPlayerState = false;
}

ABeyondEnemyCharacter* ABeyondEnemyController::GetEnemy() const
{
	return Cast<ABeyondEnemyCharacter>(GetPawn());
}

const FBeyondEnemyAIConfig& ABeyondEnemyController::GetConfig() const
{
	const ABeyondEnemyCharacter* Enemy = GetEnemy();
	const UBeyondEnemyDefinition* Definition = Enemy ? Enemy->GetDefinition() : nullptr;
	return Definition ? Definition->AI : DefaultEnemyAIConfig();
}

float ABeyondEnemyController::GetCombatSpeed() const
{
	const ABeyondEnemyCharacter* Enemy = GetEnemy();
	return (CombatSpeed > 0.0f ? CombatSpeed : 380.0f) * (Enemy ? Enemy->CombatSpeedScale : 1.0f);
}

void ABeyondEnemyController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ABeyondEnemyCharacter* Enemy = Cast<ABeyondEnemyCharacter>(InPawn);
	if (!Enemy)
	{
		return;
	}

	Enemy->OnCharacterHitTaken.AddUniqueDynamic(this, &ThisClass::HandleHitTaken);
	Enemy->OnCharacterKilled.AddUniqueDynamic(this, &ThisClass::HandleKilled);
	if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
	{
		AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
	}

	if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
	{
		// Path following otherwise sets velocity directly and acceleration stays zero: Paragon anim Blueprints idle while sliding
		Movement->GetNavMovementProperties()->bUseAccelerationForPaths = true;
		CombatSpeed = Movement->MaxWalkSpeed;
	}

	StrafeSign = FMath::RandBool() ? 1.0f : -1.0f;
	const float Interval = GetConfig().ThinkInterval;
	GetWorldTimerManager().SetTimer(ThinkTimer, this, &ThisClass::Think, Interval, true, FMath::FRandRange(0.05f, Interval));
	SetState(EBeyondEnemyAIState::Idle);
	NextWanderTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(1.0f, 4.0f);
}

void ABeyondEnemyController::OnUnPossess()
{
	if (ABeyondEnemyCharacter* Enemy = GetEnemy())
	{
		Enemy->OnCharacterHitTaken.RemoveDynamic(this, &ThisClass::HandleHitTaken);
		Enemy->OnCharacterKilled.RemoveDynamic(this, &ThisClass::HandleKilled);
		if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
		{
			ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
		}
	}
	ReleaseTokens();
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	Super::OnUnPossess();
}

void ABeyondEnemyController::SetBrainEnabled(bool bEnabled)
{
	bBrainEnabled = bEnabled;
	if (!bEnabled)
	{
		ReleaseTokens();
		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
	}
}

void ABeyondEnemyController::SetState(EBeyondEnemyAIState NewState)
{
	State = NewState;
	StateStartTime = GetWorld()->GetTimeSeconds();
	bWaitingForToken = false;

	ABeyondEnemyCharacter* Enemy = GetEnemy();
	UCharacterMovementComponent* Movement = Enemy ? Enemy->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	// In a fight it faces its target while moving (strafing); otherwise it faces where it walks
	const bool bFighting = NewState == EBeyondEnemyAIState::Combat;
	Movement->bOrientRotationToMovement = !bFighting;
	Movement->bUseControllerDesiredRotation = bFighting;
	switch (NewState)
	{
	case EBeyondEnemyAIState::Idle:
		Movement->MaxWalkSpeed = GetCombatSpeed() * GetConfig().WanderSpeedScale;
		break;
	case EBeyondEnemyAIState::Combat:
		Movement->MaxWalkSpeed = GetCombatSpeed();
		break;
	case EBeyondEnemyAIState::Returning:
		Movement->MaxWalkSpeed = GetCombatSpeed() * 1.15f;
		break;
	}
}

bool ABeyondEnemyController::IsValidTarget(const AActor* Target) const
{
	const APawn* Self = GetPawn();
	return Target && Self && !UBeyondCombatLibrary::IsActorDead(Target) && UBeyondCombatLibrary::AreHostile(Self, Target)
		&& FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) <= GetConfig().SightRadius * 2.0f;
}

AActor* ABeyondEnemyController::LookForTarget() const
{
	const APawn* Self = GetPawn();
	if (!Self)
	{
		return nullptr;
	}

	const FBeyondEnemyAIConfig& Config = GetConfig();
	const FVector Location = Self->GetActorLocation();
	const FVector Forward = Self->GetActorForwardVector().GetSafeNormal2D();
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(Config.SightHalfAngle));

	AActor* Best = nullptr;
	float BestDistanceSq = FMath::Square(Config.SightRadius);
	for (TActorIterator<ABeyondCharacterBase> It(GetWorld()); It; ++It)
	{
		ABeyondCharacterBase* Candidate = *It;
		if (Candidate == Self || !UBeyondCombatLibrary::AreHostile(Self, Candidate) || UBeyondCombatLibrary::IsActorDead(Candidate)
			|| UBeyondCombatLibrary::IsInCutscene(Candidate))
		{
			continue;
		}

		const FVector ToCandidate = Candidate->GetActorLocation() - Location;
		const float DistanceSq = ToCandidate.SizeSquared();
		if (DistanceSq > BestDistanceSq)
		{
			continue;
		}
		const bool bClose = DistanceSq <= FMath::Square(Config.CloseSenseRadius);
		const bool bInCone = FVector::DotProduct(ToCandidate.GetSafeNormal2D(), Forward) >= CosHalfAngle;
		if ((bClose || bInCone) && LineOfSightTo(Candidate))
		{
			Best = Candidate;
			BestDistanceSq = DistanceSq;
		}
	}
	return Best;
}

void ABeyondEnemyController::EngageTarget(AActor* Target, bool bAlertOthers)
{
	ABeyondEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy || !Target || State == EBeyondEnemyAIState::Returning || UBeyondCombatLibrary::IsActorDead(Enemy)
		|| !UBeyondCombatLibrary::AreHostile(Enemy, Target))
	{
		return;
	}

	const bool bNewFight = State != EBeyondEnemyAIState::Combat;
	if (CombatTarget.Get() != Target)
	{
		ReleaseTokens();
	}
	CombatTarget = Target;
	if (bNewFight)
	{
		SetState(EBeyondEnemyAIState::Combat);
		StopMovement();

		const float Now = GetWorld()->GetTimeSeconds();
		const UBeyondEnemyDefinition* Definition = Enemy->GetDefinition();
		if (Definition && Definition->AggroSound && Now - LastAggroSoundTime > EnemyAggroSoundCooldown)
		{
			LastAggroSoundTime = Now;
			UGameplayStatics::PlaySoundAtLocation(this, Definition->AggroSound, Enemy->GetActorLocation());
		}
	}
	SetFocus(Target, EAIFocusPriority::Gameplay);

	if (bAlertOthers && bNewFight)
	{
		if (UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(this))
		{
			Enemies->AlertNearby(Enemy, Target, GetConfig().AlertRadius);
		}
	}
}

void ABeyondEnemyController::ReturnHome(bool bTeleport)
{
	ABeyondEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy || UBeyondCombatLibrary::IsActorDead(Enemy))
	{
		return;
	}

	ReleaseTokens();
	CombatTarget.Reset();
	ClearFocus(EAIFocusPriority::Gameplay);
	StopMovement();
	if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
	{
		ASC->CancelAllAbilities();
		ASC->SetLooseGameplayTagCount(BeyondTags::State_Resetting, 1);
	}
	SetState(EBeyondEnemyAIState::Returning);

	if (bTeleport)
	{
		const FTransform Home = Enemy->GetHomeTransform();
		Enemy->TeleportTo(Home.GetLocation(), Home.Rotator());
		FinishReturn();
		return;
	}
	IssueMoveTo(Enemy->GetHomeTransform().GetLocation(), EnemyReturnArrivedDistance * 0.5f);
}

void ABeyondEnemyController::FinishReturn()
{
	ABeyondEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy)
	{
		return;
	}
	if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
	{
		ASC->SetLooseGameplayTagCount(BeyondTags::State_Resetting, 0);
	}
	Enemy->RestoreToFull();
	SetState(EBeyondEnemyAIState::Idle);
	NextWanderTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(2.0f, 5.0f);
}

void ABeyondEnemyController::Think()
{
	ABeyondEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy || !bBrainEnabled || UBeyondCombatLibrary::IsActorDead(Enemy))
	{
		return;
	}

	// A cutscene has the world's attention
	if (UBeyondCombatLibrary::IsInCutscene(Enemy))
	{
		StopMovement();
		return;
	}

	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	if (ASC && ASC->HasMatchingGameplayTag(BeyondTags::State_Stunned))
	{
		StopMovement();
		return;
	}

	// Let a swing or a hit reaction play out
	if (State != EBeyondEnemyAIState::Returning && BeyondAI::IsUsingAbility(ASC))
	{
		return;
	}

	switch (State)
	{
	case EBeyondEnemyAIState::Idle:
		ThinkIdle();
		break;
	case EBeyondEnemyAIState::Combat:
		ThinkCombat();
		break;
	case EBeyondEnemyAIState::Returning:
		ThinkReturning();
		break;
	}
}

void ABeyondEnemyController::ThinkIdle()
{
	if (AActor* Seen = LookForTarget())
	{
		EngageTarget(Seen, true);
		return;
	}

	const ABeyondEnemyCharacter* Enemy = GetEnemy();
	const FBeyondEnemyAIConfig& Config = GetConfig();
	const float Now = GetWorld()->GetTimeSeconds();
	if (Config.WanderRadius <= 0.0f || Now < NextWanderTime)
	{
		return;
	}
	NextWanderTime = Now + FMath::FRandRange(4.0f, 9.0f);

	const FVector Home = Enemy->GetHomeTransform().GetLocation();
	FVector Point = Home;
	if (const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Random;
		if (NavSystem->GetRandomReachablePointInRadius(Home, Config.WanderRadius, Random))
		{
			Point = Random.Location;
		}
	}
	IssueMoveTo(Point, 50.0f);
}

void ABeyondEnemyController::ThinkCombat()
{
	ABeyondEnemyCharacter* Enemy = GetEnemy();
	const FBeyondEnemyAIConfig& Config = GetConfig();

	// Too far from home: give the fight up (no kiting enemies across the map)
	if (FVector::Dist(Enemy->GetActorLocation(), Enemy->GetHomeTransform().GetLocation()) > Config.LeashRadius)
	{
		ReturnHome(false);
		return;
	}

	AActor* Target = CombatTarget.Get();
	if (!IsValidTarget(Target))
	{
		ReleaseTokens();
		Target = LookForTarget();
		if (!Target)
		{
			ReturnHome(false);
			return;
		}
		CombatTarget = Target;
	}
	SetFocus(Target, EAIFocusPriority::Gameplay);

	if (GetWorld()->GetTimeSeconds() < NextActionTime)
	{
		return;
	}
	if (TryAttack(Target))
	{
		return;
	}
	Reposition(Target);
}

void ABeyondEnemyController::ThinkReturning()
{
	ABeyondEnemyCharacter* Enemy = GetEnemy();
	const FVector Home = Enemy->GetHomeTransform().GetLocation();
	const bool bArrived = FVector::Dist2D(Enemy->GetActorLocation(), Home) <= EnemyReturnArrivedDistance;
	if (!bArrived && GetWorld()->GetTimeSeconds() - StateStartTime < EnemyReturnTimeout)
	{
		if (GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			IssueMoveTo(Home, EnemyReturnArrivedDistance * 0.5f);
		}
		return;
	}
	if (!bArrived)
	{
		// Stuck on the way: pop home
		Enemy->TeleportTo(Home, Enemy->GetHomeTransform().Rotator());
	}
	FinishReturn();
}

bool ABeyondEnemyController::TryAttack(AActor* Target)
{
	ABeyondEnemyCharacter* Enemy = GetEnemy();
	UAbilitySystemComponent* ASC = Enemy ? Enemy->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return false;
	}

	const FBeyondEnemyAIConfig& Config = GetConfig();
	ABeyondCharacterBase* TargetCharacter = Cast<ABeyondCharacterBase>(Target);
	bool bTokensBlocked = false;
	auto CanAfford = [&](const UBeyondGameplayAbility& Ability)
	{
		if (!Config.bUsesAttackTokens || Ability.AIAttackTokenCost <= 0 || !TargetCharacter)
		{
			return true;
		}
		const bool bAffordable = (TokenTarget.Get() == TargetCharacter && TokensHeld >= Ability.AIAttackTokenCost)
			|| TargetCharacter->GetAvailableAttackTokens() >= Ability.AIAttackTokenCost;
		bTokensBlocked |= !bAffordable;
		return bAffordable;
	};

	BeyondAI::FAbilityChoice Choice;
	if (!BeyondAI::SelectAbility(ASC, Target, nullptr, Choice, CanAfford))
	{
		bWaitingForToken = bTokensBlocked;
		return false;
	}
	bWaitingForToken = false;

	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Choice.Handle);
	const UBeyondGameplayAbility* Ability = Spec ? Cast<UBeyondGameplayAbility>(Spec->Ability) : nullptr;
	const int32 Cost = (Ability && Config.bUsesAttackTokens && TargetCharacter) ? Ability->AIAttackTokenCost : 0;
	if (Cost > 0 && !(TokenTarget.Get() == TargetCharacter && TokensHeld >= Cost))
	{
		ReleaseTokens();
		if (!TargetCharacter->TryReserveAttackTokens(Cost))
		{
			bWaitingForToken = true;
			return false;
		}
		TokenTarget = TargetCharacter;
		TokensHeld = Cost;
	}
	TokenAbility = Cost > 0 ? Choice.Handle : FGameplayAbilitySpecHandle();

	StopMovement();
	if (!Choice.bTargetsSelf)
	{
		SetFocus(Target, EAIFocusPriority::Gameplay);
		FaceActor(Target);
	}
	if (!ASC->TryActivateAbility(Choice.Handle))
	{
		ReleaseTokens();
		return false;
	}
	NextActionTime = GetWorld()->GetTimeSeconds() + Config.AttackRecovery;
	return true;
}

void ABeyondEnemyController::Reposition(AActor* Target)
{
	const ABeyondEnemyCharacter* Enemy = GetEnemy();
	const UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	const FBeyondEnemyAIConfig& Config = GetConfig();
	const FVector SelfLocation = Enemy->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const float Distance = FVector::Dist2D(SelfLocation, TargetLocation);
	const FVector Away = (SelfLocation - TargetLocation).GetSafeNormal2D();

	// Casters: too close, back off
	if (Config.KeepAwayDistance > 0.0f && Distance < Config.KeepAwayDistance)
	{
		IssueMoveTo(SelfLocation + Away * (Config.KeepAwayDistance - Distance + 250.0f), 60.0f);
		return;
	}

	// No token to swing with: circle the target until one frees up
	if (bWaitingForToken)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextStrafeFlipTime)
		{
			NextStrafeFlipTime = Now + FMath::FRandRange(2.5f, 5.0f);
			StrafeSign = FMath::RandBool() ? 1.0f : -1.0f;
		}
		const FVector Around = Away.IsNearlyZero() ? -Enemy->GetActorForwardVector() : Away;
		const FVector Orbit = Around.RotateAngleAxis(35.0f * StrafeSign, FVector::UpVector);
		IssueMoveTo(TargetLocation + Orbit * Config.StrafeRadius, 60.0f);
		return;
	}

	const float EngageRange = Config.PreferredRange > 0.0f ? Config.PreferredRange : BeyondAI::GetPreferredEngageRange(ASC) * 0.8f;
	const float DesiredRange = EngageRange > 0.0f ? EngageRange : 180.0f;
	if (Distance > DesiredRange)
	{
		FAIMoveRequest Request(Target);
		Request.SetAcceptanceRadius(DesiredRange * 0.85f);
		Request.SetReachTestIncludesAgentRadius(true);
		if (MoveTo(Request).Code == EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogBeyond, Verbose, TEXT("%s: no path to %s (navmesh?)"), *GetNameSafe(Enemy), *GetNameSafe(Target));
		}
	}
	else if (GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		StopMovement();
	}
}

void ABeyondEnemyController::IssueMoveTo(const FVector& Location, float AcceptanceRadius)
{
	FVector Destination = Location;
	if (const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Projected;
		if (NavSystem->ProjectPointToNavigation(Location, Projected, FVector(250.0f, 250.0f, 400.0f)))
		{
			Destination = Projected.Location;
		}
	}
	if (MoveToLocation(Destination, AcceptanceRadius) == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogBeyond, Verbose, TEXT("%s: no path to %s (navmesh?)"), *GetNameSafe(GetPawn()), *Destination.ToString());
	}
}

void ABeyondEnemyController::ReleaseTokens()
{
	if (ABeyondCharacterBase* Holder = TokenTarget.Get(); Holder && TokensHeld > 0)
	{
		Holder->ReleaseAttackTokens(TokensHeld);
	}
	TokenTarget.Reset();
	TokensHeld = 0;
	TokenAbility = FGameplayAbilitySpecHandle();
}

void ABeyondEnemyController::HandleAbilityEnded(const FAbilityEndedData& Data)
{
	if (TokensHeld > 0 && Data.AbilitySpecHandle == TokenAbility)
	{
		ReleaseTokens();
	}
}

void ABeyondEnemyController::HandleHitTaken(ABeyondCharacterBase* HitCharacter, AActor* DamageInstigator, float Damage, FGameplayTag HitResponse)
{
	if (Damage <= 0.0f || State == EBeyondEnemyAIState::Returning || !bBrainEnabled)
	{
		return;
	}
	// Whoever fired the projectile, not the projectile
	if (State != EBeyondEnemyAIState::Combat || !IsValidTarget(CombatTarget.Get()))
	{
		if (UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(this))
		{
			Enemies->AlertEnemy(GetEnemy(), DamageInstigator);
			if (AActor* Target = CombatTarget.Get())
			{
				Enemies->AlertNearby(GetEnemy(), Target, GetConfig().AlertRadius);
			}
		}
	}
}

void ABeyondEnemyController::HandleKilled(ABeyondCharacterBase* KilledCharacter, AActor* Killer)
{
	ReleaseTokens();
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	CombatTarget.Reset();
}

void ABeyondEnemyController::FaceActor(const AActor* Target) const
{
	if (APawn* Self = GetPawn(); Self && Target)
	{
		const FVector ToTarget = Target->GetActorLocation() - Self->GetActorLocation();
		Self->SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	}
}
