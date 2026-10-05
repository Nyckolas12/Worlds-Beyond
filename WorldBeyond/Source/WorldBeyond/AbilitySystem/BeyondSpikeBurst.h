// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystem/BeyondFX.h"
#include "BeyondSpikeBurst.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Crystal spikes bursting out of the ground in a wave from the centre, then sinking back (Angel's E).
 * Purely visual: the ability that spawns it deals the damage. Builds its spikes at BeginPlay from Spike Mesh
 * (any mesh; its bounds give the base and the height), each with its own blue-to-purple colour and a glow that
 * flashes as it breaks the surface. A point light flashes with the burst; Erupt FX / sound are optional extras.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API ABeyondSpikeBurst : public AActor
{
	GENERATED_BODY()

public:
	ABeyondSpikeBurst();

	// Spikes fill this radius (the ability sets its damage radius before the burst starts)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spikes", meta = (ExposeOnSpawn = "true", ClampMin = "10"))
	float Radius = 256.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Look")
	TObjectPtr<UStaticMesh> SpikeMesh;

	// Needs a vector parameter (Color Parameter); a scalar Glow Parameter is used when it has one
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Look")
	TObjectPtr<UMaterialInterface> SpikeMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Look")
	FName ColorParameter = TEXT("Color");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Look")
	FName GlowParameter = TEXT("Glow");

	// Each spike picks a colour between these two
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Look")
	FLinearColor ColorA = FLinearColor(0.10f, 0.35f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Look")
	FLinearColor ColorB = FLinearColor(0.60f, 0.15f, 1.0f);

	// Glow while standing, and the flash as a spike breaks the surface
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Look", meta = (ClampMin = "0"))
	float Glow = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Look", meta = (ClampMin = "0"))
	float RiseGlow = 25.0f;

	// Rings of spikes around the central one
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Layout", meta = (ClampMin = "0", ClampMax = "8"))
	int32 Rings = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Layout", meta = (ClampMin = "1"))
	int32 SpikesInFirstRing = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Layout", meta = (ClampMin = "0"))
	int32 SpikesAddedPerRing = 3;

	// Height of the central spike; spikes get shorter towards the edge
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Layout", meta = (ClampMin = "10"))
	float CenterHeight = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Layout", meta = (ClampMin = "10"))
	float EdgeHeight = 110.0f;

	// Base width (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Layout", meta = (ClampMin = "1"))
	float SpikeWidth = 45.0f;

	// Random size variation (0.2 = +-20 %)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Layout", meta = (ClampMin = "0", ClampMax = "0.9"))
	float SizeJitter = 0.2f;

	// Outward lean of the ring spikes (degrees)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Layout", meta = (ClampMin = "0", ClampMax = "80"))
	float MinTilt = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Layout", meta = (ClampMin = "0", ClampMax = "80"))
	float MaxTilt = 35.0f;

	// Seconds the wave takes from the centre to the edge
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Timing", meta = (ClampMin = "0"))
	float WaveTime = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Timing", meta = (ClampMin = "0.01"))
	float RiseTime = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Timing", meta = (ClampMin = "0"))
	float HoldTime = 0.7f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Timing", meta = (ClampMin = "0.01"))
	float SinkTime = 0.35f;

	// How far a spike shoots past its height before settling (fraction)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Timing", meta = (ClampMin = "0", ClampMax = "0.5"))
	float Overshoot = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Light")
	FLinearColor LightColor = FLinearColor(0.45f, 0.3f, 1.0f);

	// Peak brightness (candelas); 0: no light
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|Light", meta = (ClampMin = "0"))
	float LightIntensity = 400.0f;

	// Extra effect / sound / camera shake at the centre when the burst starts
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spikes|FX")
	FBeyondFX EruptFX;

	UFUNCTION(BlueprintPure, Category = "Spikes")
	int32 GetSpikeCount() const { return Spikes.Num(); }

	// The colour a spike was given (for tests and Blueprint extras)
	UFUNCTION(BlueprintPure, Category = "Spikes")
	FLinearColor GetSpikeColor(int32 Index) const;

	// Seconds from spawn until the last spike is back underground
	UFUNCTION(BlueprintPure, Category = "Spikes")
	float GetTotalDuration() const { return WaveTime + RiseTime + HoldTime + SinkTime; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	// Blueprint extras when the burst starts (spikes are already built)
	UFUNCTION(BlueprintImplementableEvent, Category = "Spikes")
	void OnErupt();

private:
	struct FSpike
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		FVector Base = FVector::ZeroVector;
		FVector Axis = FVector::UpVector;
		FQuat Rotation = FQuat::Identity;
		FVector Scale = FVector::OneVector;
		float Height = 100.0f;
		float Delay = 0.0f;
		float LastGlow = -1.0f;
		FLinearColor Color = FLinearColor::White;
	};

	void BuildSpikes();
	void AddSpike(const FVector& LocalOffset, float Height, float Tilt, float Delay);
	void UpdateSpike(FSpike& Spike) const;
	FVector FindGround(const FVector& Location) const;

	UPROPERTY(VisibleAnywhere, Category = "Spikes")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Spikes")
	TObjectPtr<UPointLightComponent> FlashLight;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SpikeComponents;

	TArray<FSpike> Spikes;
	float Age = 0.0f;

	// Spike Mesh bounds: bottom (local Z), height and half-width
	float MeshBottom = 0.0f;
	float MeshHeight = 100.0f;
	float MeshHalfWidth = 50.0f;
};
