// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BeyondCompanionController.h"
#include "AI/BeyondAIAbilityUtils.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Characters/BeyondCharacterBase.h"
#include "EngineUtils.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

ABeyondCompanionController::ABeyondCompanionController()
{
	bAttachToPawn = true;
}

FPathFollowingRequestResult ABeyondCompanionController::MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath)
{
	if (!bIssuingOwnMove)
	{
		FPathFollowingRequestResult Rejected;
		Rejected.Code = EPathFollowingRequestResult::Failed;
		return Rejected;
	}
	return Super::MoveTo(MoveRequest, OutPath);
}

void ABeyondCompanionController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (ABeyondCharacterBase* PossessedCharacter = Cast<ABeyondCharacterBase>(InPawn))
	{
		PossessedCharacter->OnCharacterHitTaken.AddUniqueDynamic(this, &ThisClass::HandleSelfHitTaken);
	}

	FollowSide = FMath::RandBool() ? 1.0f : -1.0f;
	GetWorldTimerManager().SetTimer(ThinkTimer, this, &ThisClass::Think, ThinkInterval, true, FMath::FRandRange(0.0f, ThinkInterval));
}

void ABeyondCompanionController::OnUnPossess()
{
	if (ABeyondCharacterBase* PossessedCharacter = Cast<ABeyondCharacterBase>(GetPawn()))
	{
		PossessedCharacter->OnCharacterHitTaken.RemoveDynamic(this, &ThisClass::HandleSelfHitTaken);
	}

	GetWorldTimerManager().ClearTimer(ThinkTimer);
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	CombatTarget.Reset();
	SelfAttacker.Reset();

	Super::OnUnPossess();
}

void ABeyondCompanionController::SetLeader(ABeyondCharacterBase* NewLeader)
{
	if (ABeyondCharacterBase* OldLeader = Leader.Get())
	{
		OldLeader->OnCharacterHitTaken.RemoveDynamic(this, &ThisClass::HandleLeaderHitTaken);
	}

	Leader = NewLeader;
	LeaderAttacker.Reset();

	if (NewLeader)
	{
		NewLeader->OnCharacterHitTaken.AddUniqueDynamic(this, &ThisClass::HandleLeaderHitTaken);
	}
}

void ABeyondCompanionController::HandleLeaderHitTaken(ABeyondCharacterBase* HitCharacter, AActor* DamageInstigator, float Damage, FGameplayTag HitResponse)
{
	if (Damage > 0.0f && UBeyondCombatLibrary::AreHostile(GetPawn(), DamageInstigator))
	{
		LeaderAttacker = DamageInstigator;
	}
}

void ABeyondCompanionController::HandleSelfHitTaken(ABeyondCharacterBase* HitCharacter, AActor* DamageInstigator, float Damage, FGameplayTag HitResponse)
{
	if (Damage > 0.0f && UBeyondCombatLibrary::AreHostile(GetPawn(), DamageInstigator))
	{
		SelfAttacker = DamageInstigator;
	}
}

bool ABeyondCompanionController::IsValidTarget(const AActor* Target) const
{
	const ABeyondCharacterBase* LeaderCharacter = Leader.Get();
	return Target && LeaderCharacter
		&& !UBeyondCombatLibrary::IsActorDead(Target)
		&& UBeyondCombatLibrary::AreHostile(GetPawn(), Target)
		&& FVector::Dist(Target->GetActorLocation(), LeaderCharacter->GetActorLocation()) <= DisengageRadius;
}

AActor* ABeyondCompanionController::PickTarget() const
{
	// Protect the leader first, then defend ourselves
	if (IsValidTarget(LeaderAttacker.Get()))
	{
		return LeaderAttacker.Get();
	}
	if (IsValidTarget(SelfAttacker.Get()))
	{
		return SelfAttacker.Get();
	}

	const APawn* Self = GetPawn();
	const FVector LeaderLocation = Leader->GetActorLocation();
	AActor* Best = nullptr;
	float BestDistanceSq = FMath::Square(EngageRadius);

	for (TActorIterator<ABeyondCharacterBase> It(GetWorld()); It; ++It)
	{
		ABeyondCharacterBase* Candidate = *It;
		if (Candidate == Self || !UBeyondCombatLibrary::AreHostile(Self, Candidate) || UBeyondCombatLibrary::IsActorDead(Candidate))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Candidate->GetActorLocation(), LeaderLocation);
		if (DistanceSq < BestDistanceSq && LineOfSightTo(Candidate))
		{
			BestDistanceSq = DistanceSq;
			Best = Candidate;
		}
	}
	return Best;
}

void ABeyondCompanionController::Think()
{
	APawn* Self = GetPawn();
	ABeyondCharacterBase* LeaderCharacter = Leader.Get();
	if (!Self || UBeyondCombatLibrary::IsActorDead(Self))
	{
		return;
	}
	if (!LeaderCharacter || LeaderCharacter == Self)
	{
		StopMovement();
		return;
	}

	// Fell far behind or got stuck: pop back next to the leader
	if (FVector::Dist(Self->GetActorLocation(), LeaderCharacter->GetActorLocation()) > RegroupDistance)
	{
		const FVector Offset = -LeaderCharacter->GetActorForwardVector() * FollowDistance + LeaderCharacter->GetActorRightVector() * FollowDistance * 0.5f * FollowSide;
		Self->TeleportTo(LeaderCharacter->GetActorLocation() + Offset, LeaderCharacter->GetActorRotation());
		StopMovement();
		CombatTarget.Reset();
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Self);

	// Let a running ability (montage, combo) finish before deciding again
	if (BeyondAI::IsUsingAbility(ASC))
	{
		return;
	}

	if (!IsValidTarget(CombatTarget.Get()))
	{
		CombatTarget = PickTarget();
	}
	AActor* Target = CombatTarget.Get();

	BeyondAI::FAbilityChoice Choice;
	if (BeyondAI::SelectAbility(ASC, Target, LeaderCharacter, Choice))
	{
		StopMovement();
		if (!Choice.bTargetsSelf && Target)
		{
			SetFocus(Target, EAIFocusPriority::Gameplay);
			FaceActor(Target);
		}
		ASC->TryActivateAbility(Choice.Handle);
		return;
	}

	if (Target)
	{
		SetFocus(Target, EAIFocusPriority::Gameplay);

		const float EngageRange = BeyondAI::GetPreferredEngageRange(ASC);
		const float DesiredRange = EngageRange > 0.0f ? EngageRange * 0.8f : FallbackAttackRange;
		if (FVector::Dist(Self->GetActorLocation(), Target->GetActorLocation()) > DesiredRange)
		{
			TGuardValue<bool> OwnMove(bIssuingOwnMove, true);
			MoveToActor(Target, DesiredRange * 0.9f);
		}
		return;
	}

	ClearFocus(EAIFocusPriority::Gameplay);
	FollowLeader();
}

void ABeyondCompanionController::FollowLeader()
{
	const APawn* Self = GetPawn();
	const ABeyondCharacterBase* LeaderCharacter = Leader.Get();

	const FVector FollowPoint = LeaderCharacter->GetActorLocation()
		- LeaderCharacter->GetActorForwardVector() * FollowDistance * 0.7f
		+ LeaderCharacter->GetActorRightVector() * FollowDistance * 0.7f * FollowSide;

	if (FVector::Dist2D(Self->GetActorLocation(), FollowPoint) > FollowAcceptanceRadius * 1.5f)
	{
		TGuardValue<bool> OwnMove(bIssuingOwnMove, true);
		MoveToLocation(FollowPoint, FollowAcceptanceRadius);
	}
}

void ABeyondCompanionController::FaceActor(const AActor* Target) const
{
	if (APawn* Self = GetPawn(); Self && Target)
	{
		const FVector ToTarget = Target->GetActorLocation() - Self->GetActorLocation();
		Self->SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	}
}
