// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGA_Projectile.generated.h"

class UAnimMontage;

/**
 * Plays a cast montage and launches a projectile at the crosshair (player) or the focus target (AI).
 * Works with the existing BP_Projectile_GABase family: it fills their TargetLocation, Speed and
 * EfectSpecHandle variables before they spawn. The projectile fires on Event.ShootProjectile /
 * Event.Montage.Trigger if the montage sends one, otherwise after FallbackFireDelay.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGA_Projectile : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_Projectile();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TSubclassOf<AActor> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UAnimMontage> CastMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0"))
	float Damage = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (Categories = "Event.Hit"))
	FGameplayTag HitResponse;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "1"))
	float ProjectileSpeed = 2500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "1"))
	float MaxRange = 5000.0f;

	// Where the projectile starts: this socket on the character mesh (falls back to the equipped weapon's SpawnPoint, then the chest)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	FName SpawnSocket = TEXT("hand_r");

	// Seconds into the montage to fire when it sends no gameplay event
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0"))
	float FallbackFireDelay = 0.25f;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// Override in Blueprint to change where the projectile spawns
	UFUNCTION(BlueprintNativeEvent, Category = "Projectile")
	FVector GetProjectileSpawnLocation() const;

private:
	void HandleFireEvent(const FGameplayEventData* Payload);
	void Fire();
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 Serial);

	bool bFired = false;
	bool bMontageDone = false;
	FTimerHandle FireTimer;
};
