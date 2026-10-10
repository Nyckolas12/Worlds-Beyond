// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/BeyondRushComponent.h"
#include "WorldBeyond.h"
#include "AbilitySystem/Abilities/BeyondGA_Dash.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BeyondGameplayTags.h"
#include "Characters/BeyondCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Particles/ParticleSystemComponent.h"

namespace
{
	// Rushes slower than this progress for this long count as blocked
	constexpr float RushStuckSeconds = 0.3f;
	constexpr float RushMinProgress = 15.0f;
	// Close enough to a target point
	constexpr float RushPointTolerance = 50.0f;

	float GetCollisionRadius(const AActor* Actor)
	{
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		if (Actor)
		{
			Actor->GetSimpleCollisionCylinder(Radius, HalfHeight);
		}
		return Radius;
	}
}

UBeyondRushComponent::UBeyondRushComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// Before the character moves, so the force aims at where the target is this frame
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

UBeyondRushComponent* UBeyondRushComponent::FindRush(const AActor* Who)
{
	return Who ? Who->FindComponentByClass<UBeyondRushComponent>() : nullptr;
}

UBeyondRushComponent* UBeyondRushComponent::Rush(ABeyondCharacterBase* Who, AActor* Target, const FVector& Point, const FBeyondRushSettings& RushSettings,
	FOnRushFinished FinishedCallback)
{
	if (!Who)
	{
		return nullptr;
	}
	if (UBeyondRushComponent* Running = FindRush(Who))
	{
		Running->Cancel();
	}

	UBeyondRushComponent* NewRush = NewObject<UBeyondRushComponent>(Who, MakeUniqueObjectName(Who, StaticClass(), TEXT("Rush")));
	NewRush->RegisterComponent();
	NewRush->Begin(Target, Point, RushSettings, MoveTemp(FinishedCallback));
	// Already there, or it blinked straight away
	return NewRush->IsRushing() ? NewRush : nullptr;
}

FVector UBeyondRushComponent::GetStopPoint(const ABeyondCharacterBase* Who, const AActor* Target, float StopDistance)
{
	if (!Who || !Target)
	{
		return Who ? Who->GetActorLocation() : FVector::ZeroVector;
	}
	const FVector From = Who->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	FVector Direction = (TargetLocation - From).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Who->GetActorForwardVector().GetSafeNormal2D();
	}
	const float Reach = GetCollisionRadius(Who) + GetCollisionRadius(Target) + StopDistance;
	FVector Point = TargetLocation - Direction * Reach;
	Point.Z = From.Z;
	return Point;
}

void UBeyondRushComponent::Blink(ABeyondCharacterBase* Who, const FVector& Destination, const FRotator& Facing, const FBeyondRushSettings& RushSettings)
{
	if (!Who)
	{
		return;
	}
	FVector Point = Destination;
	FVector Ground;
	if (UBeyondEnemySubsystem::FindGroundPoint(Who->GetWorld(), Destination, Ground))
	{
		Point = Ground + FVector(0.0f, 0.0f, Who->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.0f);
	}

	BeyondFX::SpawnAtLocation(Who, RushSettings.BlinkDepartFX, Who->GetActorLocation());
	if (AAIController* AI = Cast<AAIController>(Who->GetController()))
	{
		AI->StopMovement();
	}
	if (!Who->TeleportTo(Point, Facing))
	{
		Who->SetActorLocationAndRotation(Point, Facing, false, nullptr, ETeleportType::TeleportPhysics);
	}
	Who->GetCharacterMovement()->StopMovementImmediately();
	BeyondFX::SpawnAtLocation(Who, RushSettings.BlinkArriveFX, Who->GetActorLocation());
	UE_LOG(LogBeyond, Verbose, TEXT("%s: blinked to %s"), *Who->GetName(), *Who->GetActorLocation().ToCompactString());
}

ABeyondCharacterBase* UBeyondRushComponent::GetCharacter() const
{
	return Cast<ABeyondCharacterBase>(GetOwner());
}

FVector UBeyondRushComponent::GetDestination() const
{
	if (const AActor* Target = TargetActor.Get())
	{
		return GetStopPoint(GetCharacter(), Target, Settings.StopDistance);
	}
	return TargetPoint;
}

bool UBeyondRushComponent::HasArrived() const
{
	const ABeyondCharacterBase* Character = GetCharacter();
	if (!Character)
	{
		return true;
	}
	if (const AActor* Target = TargetActor.Get())
	{
		const float Reach = GetCollisionRadius(Character) + GetCollisionRadius(Target) + Settings.StopDistance + RushPointTolerance;
		return FVector::Dist2D(Character->GetActorLocation(), Target->GetActorLocation()) <= Reach;
	}
	return FVector::Dist2D(Character->GetActorLocation(), TargetPoint) <= FMath::Max(Settings.StopDistance, RushPointTolerance);
}

void UBeyondRushComponent::FillFromDashAbility()
{
	if (Settings.DashMontage && Settings.TrailFX.IsSet())
	{
		return;
	}
	const ABeyondCharacterBase* Character = GetCharacter();
	const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (const UBeyondGA_Dash* Dash = Cast<UBeyondGA_Dash>(Spec.Ability))
		{
			if (!Settings.DashMontage)
			{
				Settings.DashMontage = Dash->DashMontage;
				Settings.MontagePlayRate = Dash->MontagePlayRate;
			}
			if (!Settings.TrailFX.IsSet())
			{
				Settings.TrailFX = Dash->TrailFX;
				Settings.TrailSocket = Dash->TrailSocket;
			}
			return;
		}
	}
}

void UBeyondRushComponent::Begin(AActor* Target, const FVector& InTargetPoint, const FBeyondRushSettings& InSettings, FOnRushFinished InOnFinished)
{
	ABeyondCharacterBase* Character = GetCharacter();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	Settings = InSettings;
	OnFinished = MoveTemp(InOnFinished);
	TargetActor = Target;
	TargetPoint = Target ? Target->GetActorLocation() : InTargetPoint;
	if (!Character || !Movement)
	{
		Finish(false);
		return;
	}
	FillFromDashAbility();

	const FVector From = Character->GetActorLocation();
	const FVector Destination = GetDestination();
	const FVector Facing = (Target ? Target->GetActorLocation() : Destination) - From;
	const FRotator FacingRotation(0.0f, Facing.IsNearlyZero() ? Character->GetActorRotation().Yaw : Facing.Rotation().Yaw, 0.0f);
	if (HasArrived())
	{
		Character->SetActorRotation(FacingRotation);
		Finish(true);
		return;
	}

	const float Distance = FVector::Dist2D(From, Destination);
	if (Settings.MaxRushDistance > 0.0f && Distance > Settings.MaxRushDistance)
	{
		Blink(Character, Destination, FacingRotation, Settings);
		Finish(false);
		return;
	}

	bRushing = true;
	Elapsed = 0.0f;
	LastProgressTime = 0.0f;
	BestDistance = Distance;
	Duration = FMath::Clamp(Distance / FMath::Max(Settings.Speed, 100.0f), 0.1f, Settings.Timeout);
	Character->SetActorRotation(FacingRotation);

	if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		ASC->AddLooseGameplayTag(BeyondTags::State_Dashing);
		bAddedInvincible = Settings.bInvincible;
		if (bAddedInvincible)
		{
			ASC->AddLooseGameplayTag(BeyondTags::State_Invincible);
		}
	}

	if (Settings.bPassThroughPawns)
	{
		UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		PreviousPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

		// Character meshes and held weapons can use other channels; ignore those actors outright (as UBeyondGA_Dash)
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondRushIgnore), false, Character);
		GetWorld()->OverlapMultiByObjectType(Overlaps, (From + Destination) * 0.5f, FQuat::Identity,
			FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects), FCollisionShape::MakeSphere(Distance * 0.5f + 300.0f), Params);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Other = Overlap.GetActor();
			const APawn* OtherPawn = Cast<APawn>(Other);
			if (!OtherPawn && Other && Other->GetAttachParentActor())
			{
				OtherPawn = Cast<APawn>(Other->GetAttachParentActor());
			}
			if (Other && OtherPawn && !IgnoredActors.Contains(Other))
			{
				Capsule->IgnoreActorWhenMoving(Other, true);
				IgnoredActors.Add(Other);
			}
		}
	}

	USkeletalMeshComponent* Mesh = Character->GetCombatMesh();
	Trail = BeyondFX::SpawnAttached(Settings.TrailFX, Mesh ? static_cast<USceneComponent*>(Mesh) : Character->GetRootComponent(), Settings.TrailSocket);

	// Montage root motion would fight the force; MetaHumans animate a child mesh, so turn it off there (as UBeyondGA_Dash)
	if (UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr)
	{
		PreviousRootMotionMode = AnimInstance->RootMotionMode;
		AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		bChangedRootMotionMode = true;
		if (Settings.DashMontage)
		{
			AnimInstance->Montage_Play(Settings.DashMontage, Settings.MontagePlayRate);
		}
		else if (Settings.LoopMontage)
		{
			AnimInstance->Montage_Play(Settings.LoopMontage, Settings.MontagePlayRate);
		}
	}

	if (AAIController* AI = Cast<AAIController>(Character->GetController()))
	{
		AI->StopMovement();
	}

	TSharedPtr<FRootMotionSource_MoveToDynamicForce> Force = MakeShared<FRootMotionSource_MoveToDynamicForce>();
	Force->InstanceName = TEXT("BeyondRush");
	Force->AccumulateMode = ERootMotionAccumulateMode::Override;
	Force->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
	Force->Priority = 900;
	Force->StartLocation = From;
	Force->InitialTargetLocation = Destination;
	Force->TargetLocation = Destination;
	Force->Duration = Duration;
	Force->bRestrictSpeedToExpected = false;
	Force->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Force->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	RootMotionSourceID = Movement->ApplyRootMotionSource(Force);

	UE_LOG(LogBeyond, Verbose, TEXT("%s: rushing %.0f uu to %s over %.2f s"), *Character->GetName(), Distance,
		Target ? *Target->GetName() : *Destination.ToCompactString(), Duration);
}

void UBeyondRushComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bRushing)
	{
		return;
	}

	ABeyondCharacterBase* Character = GetCharacter();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || UBeyondCombatLibrary::IsActorDead(Character))
	{
		Cancel();
		return;
	}

	Elapsed += DeltaTime;

	// A target that died or vanished: finish the run to where it was
	if (TargetActor.IsValid())
	{
		TargetPoint = TargetActor->GetActorLocation();
	}
	const FVector Destination = GetDestination();
	const TSharedPtr<FRootMotionSource> Source = Movement->GetRootMotionSourceByID(RootMotionSourceID);
	if (Source.IsValid() && Source->GetScriptStruct() == FRootMotionSource_MoveToDynamicForce::StaticStruct())
	{
		static_cast<FRootMotionSource_MoveToDynamicForce*>(Source.Get())->SetTargetLocation(Destination);
	}

	if (HasArrived())
	{
		Finish(true);
		return;
	}

	const float Distance = FVector::Dist2D(Character->GetActorLocation(), Destination);
	if (Distance < BestDistance - RushMinProgress)
	{
		BestDistance = Distance;
		LastProgressTime = Elapsed;
	}
	const bool bStuck = Elapsed - LastProgressTime > RushStuckSeconds;
	const bool bForceDone = !Source.IsValid() && Elapsed > Duration + 0.15f;
	if (bStuck || bForceDone || Elapsed >= Settings.Timeout)
	{
		const FVector Facing = TargetPoint - Destination;
		Blink(Character, Destination, FRotator(0.0f, Facing.IsNearlyZero() ? Character->GetActorRotation().Yaw : Facing.Rotation().Yaw, 0.0f), Settings);
		Finish(false);
		return;
	}

	UpdateMontages();
}

void UBeyondRushComponent::UpdateMontages()
{
	const ABeyondCharacterBase* Character = GetCharacter();
	USkeletalMeshComponent* Mesh = Character ? Character->GetCombatMesh() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance || !Settings.LoopMontage)
	{
		return;
	}
	// The dash start plays once, then the loop keeps the run going
	const bool bStartPlaying = Settings.DashMontage && AnimInstance->Montage_IsPlaying(Settings.DashMontage);
	if (!bStartPlaying && !AnimInstance->Montage_IsPlaying(Settings.LoopMontage))
	{
		AnimInstance->Montage_Play(Settings.LoopMontage, Settings.MontagePlayRate);
	}
}

void UBeyondRushComponent::Restore()
{
	ABeyondCharacterBase* Character = GetCharacter();
	if (!Character)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement(); Movement && RootMotionSourceID != 0)
	{
		Movement->RemoveRootMotionSourceByID(RootMotionSourceID);
	}
	RootMotionSourceID = 0;

	if (UFXSystemComponent* TrailComponent = Trail.Get())
	{
		TrailComponent->Deactivate();
	}
	Trail.Reset();

	if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(BeyondTags::State_Dashing))
		{
			ASC->RemoveLooseGameplayTag(BeyondTags::State_Dashing);
		}
		if (bAddedInvincible)
		{
			ASC->RemoveLooseGameplayTag(BeyondTags::State_Invincible);
		}
	}
	bAddedInvincible = false;

	if (Settings.bPassThroughPawns)
	{
		UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, PreviousPawnResponse);
		for (const TWeakObjectPtr<AActor>& Other : IgnoredActors)
		{
			if (Other.IsValid())
			{
				Capsule->IgnoreActorWhenMoving(Other.Get(), false);
			}
		}
		IgnoredActors.Reset();
	}

	USkeletalMeshComponent* Mesh = Character->GetCombatMesh();
	if (UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr)
	{
		for (UAnimMontage* Montage : { Settings.DashMontage.Get(), Settings.LoopMontage.Get() })
		{
			if (Montage && AnimInstance->Montage_IsPlaying(Montage))
			{
				AnimInstance->Montage_Stop(0.2f, Montage);
			}
		}
		if (bChangedRootMotionMode)
		{
			AnimInstance->SetRootMotionMode(PreviousRootMotionMode);
		}
	}
	bChangedRootMotionMode = false;
}

void UBeyondRushComponent::Finish(bool bArrived)
{
	if (bRushing)
	{
		bRushing = false;
		Restore();
	}
	if (const ABeyondCharacterBase* Character = GetCharacter())
	{
		UE_LOG(LogBeyond, Verbose, TEXT("%s: rush %s after %.2f s"), *Character->GetName(), bArrived ? TEXT("arrived") : TEXT("ended in a blink"), Elapsed);
	}
	FOnRushFinished Callback = MoveTemp(OnFinished);
	OnFinished = nullptr;
	DestroyComponent();
	if (Callback)
	{
		Callback(bArrived);
	}
}

void UBeyondRushComponent::Cancel()
{
	if (bRushing)
	{
		bRushing = false;
		Restore();
	}
	OnFinished = nullptr;
	if (!IsBeingDestroyed())
	{
		DestroyComponent();
	}
}

void UBeyondRushComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bRushing)
	{
		bRushing = false;
		Restore();
	}
	OnFinished = nullptr;
	Super::EndPlay(EndPlayReason);
}
