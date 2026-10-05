// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "BeyondWeapon.generated.h"

class UMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondWeaponHitSignature, AActor*, HitActor, const FHitResult&, Hit);

/**
 * Base class for held weapons. Handles melee hit detection: between HitScanStart and HitScanEnd
 * it sweeps along the blade every frame and applies damage once per target.
 * The blade runs between the TraceStart/TraceEnd sockets of the weapon mesh, or along the mesh's
 * longest side when those sockets don't exist.
 */
UCLASS()
class WORLDBEYOND_API ABeyondWeapon : public AActor
{
	GENERATED_BODY()

public:
	ABeyondWeapon();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Hit Scan")
	FName TraceStartSocket = TEXT("TraceStart");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Hit Scan")
	FName TraceEndSocket = TEXT("TraceEnd");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Hit Scan", meta = (ClampMin = "1"))
	float TraceRadius = 15.0f;

	// Damage used when HitScanStart gets no effect spec (e.g. from the melee combo ability)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage", meta = (ClampMin = "0"))
	float BaseDamage = 20.0f;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FBeyondWeaponHitSignature OnWeaponHit;

	// Resting in its holster (on the hip); FindEquippedWeapon skips it
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon")
	bool bHolstered = false;

	/** Start sweeping. Each hostile target hit gets EffectSpecHandle applied (or BaseDamage if the spec is empty). */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Hit Scan")
	void HitScanStart(FGameplayEffectSpecHandle EffectSpecHandle);

	UFUNCTION(BlueprintCallable, Category = "Weapon|Hit Scan")
	void HitScanEnd();

	// Start sweeping with a plain damage value and hit reaction (used by UBeyondGA_MeleeCombo)
	void BeginMeleeScan(float Damage, const FGameplayTag& HitResponse);

	UFUNCTION(BlueprintPure, Category = "Weapon|Hit Scan")
	bool IsScanning() const { return bScanning; }

	// The character holding this weapon (owner, instigator or attach parent)
	UFUNCTION(BlueprintPure, Category = "Weapon")
	AActor* GetWielder() const;

	// First ABeyondWeapon in Character's hands (attached and not holstered), if any
	static ABeyondWeapon* FindEquippedWeapon(const AActor* Character);

	virtual void Tick(float DeltaSeconds) override;

private:
	bool GetBladeSegment(FVector& OutStart, FVector& OutEnd) const;
	void SweepSegment(const FVector& From, const FVector& To);
	void HandleHit(AActor* HitActor, const FHitResult& Hit);

	bool bScanning = false;
	bool bHasPreviousSegment = false;
	FVector PreviousStart = FVector::ZeroVector;
	FVector PreviousEnd = FVector::ZeroVector;

	FGameplayEffectSpecHandle ActiveSpec;
	float ActiveDamage = 0.0f;
	FGameplayTag ActiveHitResponse;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> ActorsHitThisScan;
};
