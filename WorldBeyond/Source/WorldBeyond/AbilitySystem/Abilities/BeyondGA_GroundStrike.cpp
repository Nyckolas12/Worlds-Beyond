// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_GroundStrike.h"
#include "WorldBeyond.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BeyondGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UBeyondGA_GroundStrike::UBeyondGA_GroundStrike()
{
	InputTag = BeyondTags::Ability_Input_E;
	DamageType = BeyondTags::DamageType_Explosion;
	HitResponse = BeyondTags::Event_Hit_Stagger;
	AIMinRange = 300.0f;
	AIMaxRange = 1500.0f;
	AIWeight = 2.0f;
}

void UBeyondGA_GroundStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bStruck = false;
	bImpactDone = false;
	bMontageDone = false;

	const APawn* Pawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
	const bool bPlayer = Pawn && Pawn->IsPlayerControlled();

	// Players aim with the reticle while the key is held
	if (bPlayer && TargetActorClass)
	{
		UAbilityTask_WaitTargetData* Task = UAbilityTask_WaitTargetData::WaitTargetData(this, NAME_None, EGameplayTargetingConfirmation::UserConfirmed, TargetActorClass);
		Task->ValidData.AddDynamic(this, &ThisClass::HandleTargetData);
		Task->Cancelled.AddDynamic(this, &ThisClass::HandleTargetCancelled);

		AGameplayAbilityTargetActor* TargetActor = nullptr;
		if (Task->BeginSpawningActor(this, TargetActorClass, TargetActor) && TargetActor)
		{
			// Trace from the character itself; the old setup used a staff socket on the (empty) CharacterMesh0
			FGameplayAbilityTargetingLocationInfo Start;
			Start.LocationType = EGameplayAbilityTargetingLocationType::ActorTransform;
			Start.SourceActor = ActorInfo->AvatarActor.Get();
			Start.SourceAbility = this;
			TargetActor->StartLocation = Start;
			if (AGameplayAbilityTargetActor_Trace* TraceActor = Cast<AGameplayAbilityTargetActor_Trace>(TargetActor))
			{
				TraceActor->MaxRange = MaxTargetRange;
			}
			Task->FinishSpawningActor(this, TargetActor);
		}
		Task->ReadyForActivation();
		return;
	}

	FVector Location;
	const bool bFound = bPlayer ? FindCrosshairLocation(Location) : FindAILocation(Location);
	if (!bFound)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	BeginCast(Location);
}

void UBeyondGA_GroundStrike::HandleTargetData(const FGameplayAbilityTargetDataHandle& Data)
{
	FVector Location = FVector::ZeroVector;
	bool bFound = false;
	if (Data.IsValid(0))
	{
		if (const FHitResult* Hit = Data.Get(0)->GetHitResult())
		{
			Location = Hit->bBlockingHit ? Hit->ImpactPoint : Hit->TraceEnd;
			bFound = true;
		}
		else if (Data.Get(0)->HasEndPoint())
		{
			Location = Data.Get(0)->GetEndPoint();
			bFound = true;
		}
	}

	if (!bFound && !FindCrosshairLocation(Location))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	BeginCast(ProjectToGround(Location));
}

void UBeyondGA_GroundStrike::HandleTargetCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

bool UBeyondGA_GroundStrike::FindAILocation(FVector& OutLocation) const
{
	const AActor* Target = GetAIFocusTarget();
	if (!Target)
	{
		return false;
	}
	OutLocation = ProjectToGround(Target->GetActorLocation());
	return true;
}

bool UBeyondGA_GroundStrike::FindCrosshairLocation(FVector& OutLocation) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return false;
	}
	const FVector Origin = Avatar->GetActorLocation();
	const FVector Aim = GetAimRotation(Origin).Vector();
	OutLocation = ProjectToGround(Origin + Aim.GetSafeNormal2D() * FMath::Min(MaxTargetRange, 800.0f));

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondGroundStrikeAim), false, Avatar);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Aim * MaxTargetRange, ECC_Visibility, Params))
	{
		OutLocation = ProjectToGround(Hit.ImpactPoint);
	}
	return true;
}

FVector UBeyondGA_GroundStrike::ProjectToGround(const FVector& Location) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondGroundStrikeGround), false, GetAvatarActorFromActorInfo());
	if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0.0f, 0.0f, 200.0f), Location - FVector(0.0f, 0.0f, 1000.0f), ECC_WorldStatic, Params))
	{
		return Hit.ImpactPoint;
	}
	return Location;
}

void UBeyondGA_GroundStrike::BeginCast(const FVector& Location)
{
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	StrikeLocation = Location;

	// Face the strike
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		const FVector ToStrike = (Location - Avatar->GetActorLocation()).GetSafeNormal2D();
		if (!ToStrike.IsNearlyZero())
		{
			Avatar->SetActorRotation(ToStrike.Rotation());
		}
	}

	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	ASC->GenericGameplayEventCallbacks.FindOrAdd(BeyondTags::Event_Montage_Trigger).AddUObject(this, &ThisClass::HandleCastEvent);
	ASC->GenericGameplayEventCallbacks.FindOrAdd(BeyondTags::Event_ShootProjectile).AddUObject(this, &ThisClass::HandleCastEvent);

	const float Duration = PlayMontageOnAvatar(CastMontage);
	UE_LOG(LogBeyond, Verbose, TEXT("%s: strike cast at %s (montage %.2f s)"), *GetNameSafe(GetAvatarActorFromActorInfo()), *Location.ToCompactString(), Duration);
	if (Duration > 0.0f)
	{
		if (UAnimInstance* AnimInstance = GetAnimatedMesh()->GetAnimInstance())
		{
			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &ThisClass::HandleMontageEnded, ActivationSerial);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, CastMontage);
		}
		GetWorld()->GetTimerManager().SetTimer(StrikeTimer, this, &ThisClass::Strike, FMath::Clamp(FallbackStrikeDelay, 0.01f, Duration * 0.9f), false);
	}
	else
	{
		bMontageDone = true;
		Strike();
	}
}

void UBeyondGA_GroundStrike::HandleCastEvent(const FGameplayEventData* Payload)
{
	Strike();
}

void UBeyondGA_GroundStrike::Strike()
{
	if (bStruck || !IsActive())
	{
		return;
	}
	bStruck = true;
	GetWorld()->GetTimerManager().ClearTimer(StrikeTimer);
	UnbindCastEvents();

	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	if (StrikeCueTag.IsValid() && ASC)
	{
		FHitResult Hit;
		Hit.Location = Hit.ImpactPoint = StrikeLocation;
		Hit.ImpactNormal = Hit.Normal = FVector::UpVector;
		Hit.bBlockingHit = true;

		FGameplayEffectContextHandle Context = MakeEffectContext(CurrentSpecHandle, CurrentActorInfo);
		Context.AddHitResult(Hit);

		FGameplayCueParameters Params(Context);
		Params.Location = StrikeLocation;
		Params.Normal = FVector::UpVector;
		Params.RawMagnitude = Radius;
		ASC->ExecuteGameplayCue(StrikeCueTag, Params);
	}
	BeyondFX::SpawnAtLocation(this, StrikeFX, StrikeLocation);

	if (ImpactDelay > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(ImpactTimer, this, &ThisClass::ApplyImpact, ImpactDelay, false);
	}
	else
	{
		ApplyImpact();
	}
}

void UBeyondGA_GroundStrike::ApplyImpact()
{
	if (bImpactDone || !IsActive())
	{
		return;
	}
	bImpactDone = true;

	// A little height so enemies standing on uneven ground still count
	const TArray<AActor*> Hostiles = FindHostilesInRadius(StrikeLocation + FVector(0.0f, 0.0f, 60.0f), Radius);
	UE_LOG(LogBeyond, Verbose, TEXT("%s: strike landed at %s, %d hostile(s) hit"), *GetNameSafe(GetAvatarActorFromActorInfo()), *StrikeLocation.ToCompactString(), Hostiles.Num());
	for (AActor* Hostile : Hostiles)
	{
		ApplyDamageToTarget(Hostile, Damage, DamageType, HitResponse);
	}
	OnStrikeLanded(StrikeLocation, Hostiles);
	TryEnd();
}

void UBeyondGA_GroundStrike::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 Serial)
{
	if (!IsCurrentActivation(Serial))
	{
		return;
	}
	bMontageDone = true;
	// Interrupted before the strike (hit reaction, death): the cast fizzles
	if (!bStruck)
	{
		if (bInterrupted)
		{
			UE_LOG(LogBeyond, Verbose, TEXT("%s: strike cast interrupted before it landed"), *GetNameSafe(GetAvatarActorFromActorInfo()));
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}
		Strike();
	}
	TryEnd();
}

void UBeyondGA_GroundStrike::TryEnd()
{
	if (IsActive() && bImpactDone && bMontageDone)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UBeyondGA_GroundStrike::UnbindCastEvents()
{
	if (UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		for (const FGameplayTag& Tag : { FGameplayTag(BeyondTags::Event_Montage_Trigger), FGameplayTag(BeyondTags::Event_ShootProjectile) })
		{
			if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(Tag))
			{
				Delegate->RemoveAll(this);
			}
		}
	}
}

void UBeyondGA_GroundStrike::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StrikeTimer);
		World->GetTimerManager().ClearTimer(ImpactTimer);
	}
	UnbindCastEvents();

	// Cancelled mid-cast: don't leave the cast animation running
	if (bWasCancelled && CastMontage && !bMontageDone)
	{
		if (const USkeletalMeshComponent* Mesh = GetAnimatedMesh(); Mesh && Mesh->GetAnimInstance())
		{
			Mesh->GetAnimInstance()->Montage_Stop(0.2f, CastMontage);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
