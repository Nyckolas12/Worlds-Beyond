// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/BeyondFX.h"
#include "BeyondAreaStrike.generated.h"

class UDecalComponent;
class UFXSystemComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EBeyondStrikeShape : uint8
{
	Circle,
	// Between Inner Radius and Radius (safe in the middle)
	Ring,
	// A wedge of Cone Angle in front of the facing
	Cone,
	// Length x Width in front of the start point
	Line
};

/** One telegraphed area hit: its shape, the warning time and what it does when it lands */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondStrikeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	EBeyondStrikeShape Shape = EBeyondStrikeShape::Circle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "10"))
	float Radius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0", EditCondition = "Shape == EBeyondStrikeShape::Ring"))
	float InnerRadius = 0.0f;

	// Full angle of the wedge, in degrees
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "5", ClampMax = "360", EditCondition = "Shape == EBeyondStrikeShape::Cone"))
	float ConeAngle = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "10", EditCondition = "Shape == EBeyondStrikeShape::Line"))
	float Length = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "10", EditCondition = "Shape == EBeyondStrikeShape::Line"))
	float Width = 220.0f;

	// Seconds the marker fills before the hit lands (time to step out)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (ClampMin = "0"))
	float WindUp = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (ClampMin = "0"))
	float Damage = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (Categories = "DamageType"))
	FGameplayTag DamageType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (Categories = "Event.Hit"))
	FGameplayTag HitResponse;

	// You dodge a telegraph, you don't block it
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	bool bUnblockable = true;

	// After the hit the area keeps hurting for this long (lava, poison); 0 vanishes
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Linger", meta = (ClampMin = "0"))
	float LingerDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Linger", meta = (ClampMin = "0"))
	float LingerDamagePerSecond = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Linger", meta = (Categories = "DamageType"))
	FGameplayTag LingerDamageType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	FBeyondFX ImpactFX;

	// Looping while the area lingers (Max Lifetime 0; removed with it)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	FBeyondFX LingerFX;

	// Something falling onto the area during the wind-up (meteor, boulder)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	TObjectPtr<UStaticMesh> FallingMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX", meta = (ClampMin = "0.01"))
	float FallingMeshScale = 1.0f;

	// Trail on the falling mesh
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	FBeyondFX FallingFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX", meta = (ClampMin = "0"))
	float FallHeight = 1800.0f;

	// Marker colour; alpha 0 uses the telegraph / hazard colour from Project Settings -> Worlds Beyond Enemies
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	FLinearColor Color = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
};

/**
 * A telegraphed area attack (Plan 3): a ground marker in the strike's shape fills over the wind-up, then everything
 * hostile to the instigator inside it is hit; it can keep burning afterwards. The spot is fixed when it spawns, so
 * stepping out works. Its own actor, so it lands even when the ability that made it ended.
 */
UCLASS(NotBlueprintable)
class WORLDBEYOND_API ABeyondAreaStrike : public AActor
{
	GENERATED_BODY()

public:
	ABeyondAreaStrike();

	/**
	 * Location is the centre (Circle / Ring / Cone) or the start point (Line); Yaw the facing of cones and lines.
	 * The marker shows after StartDelay. Damage is scaled by DamageScale (ability level). Instigator null: hits the
	 * Player team (arena hazards).
	 */
	static ABeyondAreaStrike* SpawnStrike(const UObject* WorldContext, AActor* Instigator, const FBeyondStrikeSettings& Settings, const FVector& Location,
		float Yaw, float StartDelay = 0.0f, float DamageScale = 1.0f);

	// Whether Actor stands inside the shape (with a little tolerance for its capsule)
	UFUNCTION(BlueprintPure, Category = "Beyond|Strike")
	bool IsInside(const AActor* Actor) const;

	UFUNCTION(BlueprintPure, Category = "Beyond|Strike")
	bool HasStruck() const { return bStruck; }

	// 0 when the marker appears, 1 when it lands
	UFUNCTION(BlueprintPure, Category = "Beyond|Strike")
	float GetFill() const;

	const FBeyondStrikeSettings& GetSettings() const { return Settings; }

	AActor* GetStrikeInstigator() const { return StrikeInstigator.Get(); }

	// Removed before it lands when the instigator dies (default); arena hazards keep going
	bool bCancelWithInstigator = true;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Configure(AActor* InInstigator, const FBeyondStrikeSettings& InSettings, float InYaw, float StartDelay, float InDamageScale);
	void Strike();
	void DamageInside(float Amount, const FGameplayTag& DamageTypeTag, const FGameplayTag& Response, bool bUnblockableHit);
	FVector GetShapeCentre() const;
	void UpdateMarker();
	bool IsTarget(const AActor* Actor) const;

	UPROPERTY(VisibleAnywhere, Category = "Strike")
	TObjectPtr<UDecalComponent> Decal;

	UPROPERTY(VisibleAnywhere, Category = "Strike")
	TObjectPtr<UStaticMeshComponent> FallingMeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DecalMaterial;

	FBeyondStrikeSettings Settings;
	TWeakObjectPtr<AActor> StrikeInstigator;
	bool bHadInstigator = false;
	float Yaw = 0.0f;
	float DamageScale = 1.0f;
	// Negative while waiting for StartDelay
	float Elapsed = 0.0f;
	bool bStruck = false;
	float LingerElapsed = 0.0f;
	float NextLingerTick = 0.0f;
	FLinearColor MarkerColor = FLinearColor::Red;
	TWeakObjectPtr<UFXSystemComponent> LingerComponent;
	TWeakObjectPtr<UFXSystemComponent> FallingTrail;
};
