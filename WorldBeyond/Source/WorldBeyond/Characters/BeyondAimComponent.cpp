// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/BeyondAimComponent.h"
#include "AbilitySystem/Abilities/BeyondGA_EquipWeapon.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BeyondGameplayTags.h"
#include "Camera/CameraComponent.h"
#include "Characters/BeyondCharacterBase.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Game/BeyondCombatSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// How often the crosshair looks for an enemy
	constexpr float AimTargetScanInterval = 0.05f;

	const FBeyondAimSettings DisabledAimSettings;
}

UBeyondAimComponent::UBeyondAimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After movement, so turning to the aim wins over orient-to-movement this frame
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UBeyondAimComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* Owner = GetOwner())
	{
		SpringArm = Owner->FindComponentByClass<USpringArmComponent>();
		Camera = Owner->FindComponentByClass<UCameraComponent>();
	}
	BindAnimInstance();
}

void UBeyondAimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAim();
	SetAimTarget(nullptr);
	RestoreCamera();
	if (UAnimInstance* AnimInstance = BoundAnimInstance.Get())
	{
		AnimInstance->OnMontageStarted.RemoveDynamic(this, &ThisClass::HandleMontageStarted);
	}
	BoundAnimInstance.Reset();
	Super::EndPlay(EndPlayReason);
}

const FBeyondAimSettings& UBeyondAimComponent::GetSettings() const
{
	const ABeyondCharacterBase* Character = GetCharacter();
	return Character ? Character->GetAimSettings() : DisabledAimSettings;
}

ABeyondCharacterBase* UBeyondAimComponent::GetCharacter() const
{
	return Cast<ABeyondCharacterBase>(GetOwner());
}

bool UBeyondAimComponent::IsPlayerControlled() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsPlayerControlled() && Pawn->IsLocallyControlled();
}

UBeyondGA_EquipWeapon* UBeyondAimComponent::FindEquipAbility() const
{
	if (UBeyondGA_EquipWeapon* Cached = CachedEquip.Get())
	{
		return Cached;
	}
	const ABeyondCharacterBase* Character = GetCharacter();
	const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return nullptr;
	}
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (UBeyondGA_EquipWeapon* Equip = Cast<UBeyondGA_EquipWeapon>(Spec.GetPrimaryInstance()))
		{
			CachedEquip = Equip;
			return Equip;
		}
	}
	return nullptr;
}

bool UBeyondAimComponent::IsCombatReady() const
{
	const FBeyondAimSettings& Settings = GetSettings();
	if (!Settings.bEnabled || !IsPlayerControlled())
	{
		return false;
	}
	if (bAiming || Settings.bAlwaysReady)
	{
		return true;
	}
	// Without an equip ability there is no "put away" state: always ready
	const UBeyondGA_EquipWeapon* Equip = FindEquipAbility();
	return !Equip || Equip->IsWeaponDrawn();
}

bool UBeyondAimComponent::ShouldShowCrosshair() const
{
	const AActor* Owner = GetOwner();
	return IsCombatReady() && !UBeyondCombatLibrary::IsActorDead(Owner) && !UBeyondCombatLibrary::IsInCutscene(Owner);
}

void UBeyondAimComponent::StartAim()
{
	ABeyondCharacterBase* Character = GetCharacter();
	const FBeyondAimSettings& Settings = GetSettings();
	if (bAiming || !Character || !Settings.bEnabled || !IsPlayerControlled() || UBeyondCombatLibrary::IsActorDead(Character))
	{
		return;
	}

	// The right mouse button also cancels a ground reticle (E): leave it to that while one is open
	if (const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent(); ASC && ASC->GenericLocalCancelCallbacks.IsBound())
	{
		return;
	}

	bAiming = true;

	// Turn with the camera (smoothly, at the movement component's rotation rate)
	bSavedUseControllerRotationYaw = Character->bUseControllerRotationYaw;
	Character->bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		bSavedOrientRotationToMovement = Movement->bOrientRotationToMovement;
		bSavedUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
		Movement->bOrientRotationToMovement = false;
		Movement->bUseControllerDesiredRotation = true;

		SavedWalkSpeed = Movement->MaxWalkSpeed;
		AimWalkSpeed = SavedWalkSpeed * Settings.AimWalkSpeedScale;
		Movement->MaxWalkSpeed = AimWalkSpeed;
	}

	if (Settings.bDrawWeaponToAim)
	{
		if (UBeyondGA_EquipWeapon* Equip = FindEquipAbility(); Equip && !Equip->IsWeaponDrawn())
		{
			Equip->RequestWeaponAction(BeyondTags::Weapon_Action_Draw);
		}
	}
}

void UBeyondAimComponent::StopAim()
{
	if (!bAiming)
	{
		return;
	}
	bAiming = false;

	ABeyondCharacterBase* Character = GetCharacter();
	if (!Character)
	{
		return;
	}
	Character->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = bSavedOrientRotationToMovement;
		Movement->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		// Something else (the equip ability's armed speed) may have changed it meanwhile: then leave it
		if (FMath::IsNearlyEqual(Movement->MaxWalkSpeed, AimWalkSpeed))
		{
			Movement->MaxWalkSpeed = SavedWalkSpeed;
		}
	}
}

void UBeyondAimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const bool bPlayer = IsPlayerControlled();
	if (bAiming && (!bPlayer || UBeyondCombatLibrary::IsActorDead(GetOwner())))
	{
		// Swapped to the other demigod (or died) while holding the aim input
		StopAim();
	}

	BindAnimInstance();
	UpdateCamera(DeltaTime);
	UpdateFacing(DeltaTime);

	TargetScanAccumulator += DeltaTime;
	if (TargetScanAccumulator >= AimTargetScanInterval)
	{
		TargetScanAccumulator = 0.0f;
		UpdateAimTarget();
	}
}

void UBeyondAimComponent::UpdateCamera(float DeltaTime)
{
	USpringArmComponent* Arm = SpringArm.Get();
	if (!Arm)
	{
		return;
	}
	const FBeyondAimSettings& Settings = GetSettings();
	const float Speed = Settings.CameraInterpSpeed;

	// The arm's own values, without what we added
	const float BaseArmLength = Arm->TargetArmLength - AppliedArmLength;

	FVector WantedOffset = FVector::ZeroVector;
	float WantedArmLength = 0.0f;
	float WantedFieldOfView = 0.0f;
	if (bAiming)
	{
		WantedOffset = Settings.AimCameraOffset;
		WantedArmLength = BaseArmLength * (Settings.AimArmLengthScale - 1.0f);
		WantedFieldOfView = Settings.AimFieldOfViewChange;
	}
	else if (IsCombatReady())
	{
		WantedOffset = Settings.ReadyCameraOffset;
	}

	const FVector NewOffset = FMath::VInterpTo(AppliedOffset, WantedOffset, DeltaTime, Speed);
	Arm->SocketOffset += NewOffset - AppliedOffset;
	AppliedOffset = NewOffset;

	const float NewArmLength = FMath::FInterpTo(AppliedArmLength, WantedArmLength, DeltaTime, Speed);
	Arm->TargetArmLength += NewArmLength - AppliedArmLength;
	AppliedArmLength = NewArmLength;

	if (UCameraComponent* Cam = Camera.Get())
	{
		const float NewFieldOfView = FMath::FInterpTo(AppliedFieldOfView, WantedFieldOfView, DeltaTime, Speed);
		if (!FMath::IsNearlyEqual(NewFieldOfView, AppliedFieldOfView))
		{
			Cam->SetFieldOfView(Cam->FieldOfView + NewFieldOfView - AppliedFieldOfView);
			AppliedFieldOfView = NewFieldOfView;
		}
	}
}

void UBeyondAimComponent::RestoreCamera()
{
	if (USpringArmComponent* Arm = SpringArm.Get())
	{
		Arm->SocketOffset -= AppliedOffset;
		Arm->TargetArmLength -= AppliedArmLength;
	}
	if (UCameraComponent* Cam = Camera.Get(); Cam && AppliedFieldOfView != 0.0f)
	{
		Cam->SetFieldOfView(Cam->FieldOfView - AppliedFieldOfView);
	}
	AppliedOffset = FVector::ZeroVector;
	AppliedArmLength = 0.0f;
	AppliedFieldOfView = 0.0f;
}

void UBeyondAimComponent::UpdateAimTarget()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!ShouldShowCrosshair())
	{
		SetAimTarget(nullptr);
		return;
	}

	FHitResult Hit;
	if (!UBeyondCombatLibrary::TraceAlongView(Pawn, GetSettings().MaxAimRange, Hit))
	{
		SetAimTarget(nullptr);
		return;
	}
	AimPoint = Hit.bBlockingHit ? Hit.ImpactPoint : Hit.TraceEnd;

	AActor* HitActor = Hit.GetActor();
	const bool bHostile = HitActor && UBeyondCombatLibrary::AreHostile(Pawn, HitActor) && !UBeyondCombatLibrary::IsActorDead(HitActor);
	SetAimTarget(bHostile ? HitActor : nullptr);
}

void UBeyondAimComponent::SetAimTarget(AActor* NewTarget)
{
	if (AimTarget.Get() == NewTarget)
	{
		return;
	}

	// Give the previous target its own overlay back
	if (UMeshComponent* Mesh = HighlightedMesh.Get())
	{
		Mesh->SetOverlayMaterial(HighlightPreviousOverlay.Get());
	}
	HighlightedMesh.Reset();
	HighlightPreviousOverlay.Reset();

	AimTarget = NewTarget;

	UMaterialInterface* Highlight = GetSettings().AimHighlightMaterial;
	if (!NewTarget || !Highlight)
	{
		return;
	}
	UMeshComponent* Mesh = nullptr;
	if (const ABeyondCharacterBase* Character = Cast<ABeyondCharacterBase>(NewTarget))
	{
		Mesh = Character->GetCombatMesh();
	}
	if (!Mesh)
	{
		Mesh = NewTarget->FindComponentByClass<USkeletalMeshComponent>();
	}
	if (Mesh)
	{
		HighlightedMesh = Mesh;
		HighlightPreviousOverlay = Mesh->GetOverlayMaterial();
		Mesh->SetOverlayMaterial(Highlight);
	}
}

void UBeyondAimComponent::BindAnimInstance()
{
	const ABeyondCharacterBase* Character = GetCharacter();
	const USkeletalMeshComponent* Mesh = Character ? Character->GetCombatMesh() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (AnimInstance == BoundAnimInstance.Get())
	{
		return;
	}
	if (UAnimInstance* Previous = BoundAnimInstance.Get())
	{
		Previous->OnMontageStarted.RemoveDynamic(this, &ThisClass::HandleMontageStarted);
	}
	BoundAnimInstance = AnimInstance;
	if (AnimInstance)
	{
		AnimInstance->OnMontageStarted.AddUniqueDynamic(this, &ThisClass::HandleMontageStarted);
	}
}

void UBeyondAimComponent::HandleMontageStarted(UAnimMontage* Montage)
{
	const FBeyondAimSettings& Settings = GetSettings();
	if (!Settings.bEnabled || !IsPlayerControlled() || !Montage)
	{
		return;
	}
	if (!Settings.FaceAimMontages.Contains(UBeyondCombatSubsystem::GetMontageSource(Montage)))
	{
		return;
	}
	bFacingAim = true;
	FaceAimEndTime = GetWorld()->GetTimeSeconds() + Settings.FaceAimTime;
}

void UBeyondAimComponent::UpdateFacing(float DeltaTime)
{
	if (!bFacingAim)
	{
		return;
	}
	AActor* Owner = GetOwner();
	const APawn* Pawn = Cast<APawn>(Owner);
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller || !IsPlayerControlled())
	{
		bFacingAim = false;
		return;
	}

	// Cover the rest of the angle in the time left, so the turn ends on time even if the camera moves meanwhile
	const float Remaining = FMath::Max(FaceAimEndTime - GetWorld()->GetTimeSeconds(), 0.0f);
	const float Alpha = Remaining <= 0.0f ? 1.0f : FMath::Clamp(DeltaTime / (DeltaTime + Remaining), 0.0f, 1.0f);
	const float CurrentYaw = Owner->GetActorRotation().Yaw;
	const float TargetYaw = Controller->GetControlRotation().Yaw;
	Owner->SetActorRotation(FRotator(0.0f, CurrentYaw + FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw) * Alpha, 0.0f));

	if (Remaining <= 0.0f)
	{
		bFacingAim = false;
	}
}
