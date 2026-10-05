// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BeyondAimComponent.generated.h"

class ABeyondCharacterBase;
class UAnimInstance;
class UAnimMontage;
class UBeyondGA_EquipWeapon;
class UCameraComponent;
class UInputAction;
class UMaterialInterface;
class UMeshComponent;
class USpringArmComponent;

/** How a caster aims (set on the character: Angel). See UBeyondAimComponent. */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondAimSettings
{
	GENERATED_BODY()

	// Off: no crosshair, the camera is left alone (Ji-Woong, enemies)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim")
	bool bEnabled = false;

	// Hold to aim (IA_Aim on the right mouse button)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim")
	TObjectPtr<const UInputAction> AimAction;

	// Show the crosshair (and the shoulder camera) even with the weapon put away
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim")
	bool bAlwaysReady = false;

	/**
	 * Added to the spring arm's Socket Offset while the weapon is out: moves the camera over the right shoulder so
	 * the screen centre (where spells go) is beside the character instead of on his back.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim|Camera")
	FVector ReadyCameraOffset = FVector(0.0f, 55.0f, 15.0f);

	// Added to the spring arm's Socket Offset while aiming
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim|Camera")
	FVector AimCameraOffset = FVector(0.0f, 70.0f, 20.0f);

	// The spring arm gets this much shorter while aiming (0.6 = 60 % of its length)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim|Camera", meta = (ClampMin = "0.1", ClampMax = "1"))
	float AimArmLengthScale = 0.6f;

	// Added to the camera's field of view while aiming (negative zooms in)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim|Camera", meta = (ClampMin = "-60", ClampMax = "0"))
	float AimFieldOfViewChange = -20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim|Camera", meta = (ClampMin = "0.1"))
	float CameraInterpSpeed = 10.0f;

	// Walk speed multiplier while aiming
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim|Movement", meta = (ClampMin = "0.1", ClampMax = "1"))
	float AimWalkSpeedScale = 0.6f;

	// Aiming takes the weapon out (through the equip ability's draw)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim|Movement")
	bool bDrawWeaponToAim = true;

	// How far the crosshair looks for an enemy
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim|Target", meta = (ClampMin = "100"))
	float MaxAimRange = 5000.0f;

	// Overlay material put on the enemy under the crosshair (M_Beyond_AimHighlight); empty: no highlight
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim|Target")
	TObjectPtr<UMaterialInterface> AimHighlightMaterial;

	// Casting one of these turns the character to where the camera looks (LMB spell, Arcane Bolt, E)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim|Facing")
	TArray<TObjectPtr<UAnimMontage>> FaceAimMontages;

	// Seconds the turn takes
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim|Facing", meta = (ClampMin = "0"))
	float FaceAimTime = 0.12f;
};

/**
 * Third-person aiming for a caster, created at BeginPlay on characters whose Aim Settings are enabled (Angel).
 * While he leads with his weapon out the camera eases over his right shoulder and the player controller shows a
 * crosshair; the enemy under it (the same camera trace spells use) glows and turns the crosshair purple.
 * Holding the aim input zooms in and turns him with the camera. Casting a Face Aim montage turns him to the aim.
 * The camera changes are applied as offsets on top of the spring arm / camera values, so Blueprint tweaks survive.
 */
UCLASS(ClassGroup = (Beyond))
class WORLDBEYOND_API UBeyondAimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBeyondAimComponent();

	void StartAim();
	void StopAim();

	UFUNCTION(BlueprintPure, Category = "Aim")
	bool IsAiming() const { return bAiming; }

	// The player controls this character and has the weapon out (or is aiming)
	UFUNCTION(BlueprintPure, Category = "Aim")
	bool IsCombatReady() const;

	// What the player controller checks before showing the crosshair
	UFUNCTION(BlueprintPure, Category = "Aim")
	bool ShouldShowCrosshair() const;

	// Living hostile under the crosshair, if any
	UFUNCTION(BlueprintPure, Category = "Aim")
	AActor* GetAimTarget() const { return AimTarget.Get(); }

	// Where a spell cast now would go (what the camera trace hits, or its end)
	UFUNCTION(BlueprintPure, Category = "Aim")
	FVector GetAimPoint() const { return AimPoint; }

	// Spring arm offset currently added by aiming (for tests)
	FVector GetAppliedCameraOffset() const { return AppliedOffset; }

	USpringArmComponent* GetSpringArm() const { return SpringArm.Get(); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	const FBeyondAimSettings& GetSettings() const;
	ABeyondCharacterBase* GetCharacter() const;
	bool IsPlayerControlled() const;
	UBeyondGA_EquipWeapon* FindEquipAbility() const;

	void UpdateCamera(float DeltaTime);
	void RestoreCamera();
	void UpdateAimTarget();
	void SetAimTarget(AActor* NewTarget);
	void UpdateFacing(float DeltaTime);
	void BindAnimInstance();

	UFUNCTION()
	void HandleMontageStarted(UAnimMontage* Montage);

	TWeakObjectPtr<USpringArmComponent> SpringArm;
	TWeakObjectPtr<UCameraComponent> Camera;
	mutable TWeakObjectPtr<UBeyondGA_EquipWeapon> CachedEquip;
	TWeakObjectPtr<UAnimInstance> BoundAnimInstance;

	// What this component has added on top of the spring arm / camera's own values
	FVector AppliedOffset = FVector::ZeroVector;
	float AppliedArmLength = 0.0f;
	float AppliedFieldOfView = 0.0f;

	bool bAiming = false;
	bool bSavedUseControllerDesiredRotation = false;
	bool bSavedOrientRotationToMovement = true;
	bool bSavedUseControllerRotationYaw = false;
	float SavedWalkSpeed = 0.0f;
	float AimWalkSpeed = 0.0f;

	TWeakObjectPtr<AActor> AimTarget;
	TWeakObjectPtr<UMeshComponent> HighlightedMesh;
	TWeakObjectPtr<UMaterialInterface> HighlightPreviousOverlay;
	FVector AimPoint = FVector::ZeroVector;
	float TargetScanAccumulator = 0.0f;

	bool bFacingAim = false;
	float FaceAimEndTime = 0.0f;
};
