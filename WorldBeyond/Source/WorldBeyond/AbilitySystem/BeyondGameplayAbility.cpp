// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/BeyondGameplayAbility.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Characters/BeyondCharacterBase.h"

UBeyondGameplayAbility::UBeyondGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

ABeyondCharacterBase* UBeyondGameplayAbility::GetBeyondCharacter() const
{
	return Cast<ABeyondCharacterBase>(GetAvatarActorFromActorInfo());
}

AActor* UBeyondGameplayAbility::GetAIFocusTarget() const
{
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AAIController* AIController = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	return AIController ? AIController->GetFocusActor() : nullptr;
}

FRotator UBeyondGameplayAbility::GetAimRotation(FVector Origin) const
{
	if (const AActor* Target = GetAIFocusTarget())
	{
		// Aim at the middle of the target's collision
		FVector BoundsOrigin, BoundsExtent;
		Target->GetActorBounds(true, BoundsOrigin, BoundsExtent);
		return (BoundsOrigin - Origin).Rotation();
	}

	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		// Trace from the camera so projectiles land on the crosshair, not parallel to it
		FVector CameraLocation;
		FRotator CameraRotation;
		PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

		const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * 10000.0f;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondAim), false, Pawn);
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, TraceEnd, ECC_Visibility, Params);
		return ((bHit ? Hit.ImpactPoint : TraceEnd) - Origin).Rotation();
	}

	return Pawn ? Pawn->GetActorRotation() : FRotator::ZeroRotator;
}

bool UBeyondGameplayAbility::ApplyDamageToTarget(AActor* Target, float Amount, FGameplayTag DamageType, FGameplayTag HitResponse, bool bUnblockable)
{
	return UBeyondCombatLibrary::ApplyDamage(GetAvatarActorFromActorInfo(), Target, Amount, DamageType, HitResponse, bUnblockable, GetAvatarActorFromActorInfo());
}

void UBeyondGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	if (bActivateOnGranted && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid() && !Spec.IsActive())
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
	}
}
