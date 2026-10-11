// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystem/BeyondFX.h"
#include "BeyondHazardVolume.generated.h"

class UBoxComponent;

UENUM(BlueprintType)
enum class EBeyondHazardKind : uint8
{
	// Lava, embers: a burn that keeps being refreshed while a demigod stands in it
	Burn,
	// Water too deep to wade (there's no swimming): fade out and back to the last safe ground, a little hurt
	DeepWater
};

/**
 * A box that hurts the party (Plan 5): lava channels in the Cinderlands, the deep middle of a lake. Checked four times a
 * second against the demigods (no overlap events, so a teleport into it counts too). Streams with the art around it.
 */
UCLASS()
class WORLDBEYOND_API ABeyondHazardVolume : public AActor
{
	GENERATED_BODY()

public:
	ABeyondHazardVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard")
	EBeyondHazardKind Kind = EBeyondHazardKind::Burn;

	// Damage every second while standing in it
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0", EditCondition = "Kind == EBeyondHazardKind::Burn"))
	float BurnDamagePerSecond = 12.0f;

	// A burn (the same damage per second) lasts this long after stepping out
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.5", EditCondition = "Kind == EBeyondHazardKind::Burn"))
	float BurnLinger = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard", meta = (EditCondition = "Kind == EBeyondHazardKind::Burn"))
	FBeyondFX BurnFX;

	// Share of max health lost when deep water sends a demigod back
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "Kind == EBeyondHazardKind::DeepWater"))
	float DeepWaterDamageFraction = 0.1f;

	// Location is inside the box
	UFUNCTION(BlueprintPure, Category = "Hazard")
	bool ContainsLocation(const FVector& Location) const;

	// Deep water and lava: the world subsystem doesn't count a spot inside as safe ground
	static bool IsInsideAnyHazard(const UWorld* World, const FVector& Location);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Check();

	FTimerHandle CheckTimer;
	// Deep water: who was already sent back (once per dip)
	TSet<TWeakObjectPtr<AActor>> Returning;
	// Lava: who stands in it, and when they were last burned
	TMap<TWeakObjectPtr<AActor>, double> Burning;
};
