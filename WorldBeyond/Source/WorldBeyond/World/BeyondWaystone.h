// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystem/BeyondFX.h"
#include "BeyondWaystone.generated.h"

class UArrowComponent;
class UFXSystemComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * A fast-travel stone (Plan 5). Walking within Discovery Radius puts it on the world map; F next to it attunes it
 * (it can be travelled to from the map from then on, and it becomes the respawn point); F again rests: the party is
 * healed, saved, the respawn point moves here and camps with the On Rest rule fill up again. The party arrives at the
 * Arrival Point. Always loaded in a World Partition map, so the map and fast travel know every stone.
 */
UCLASS()
class WORLDBEYOND_API ABeyondWaystone : public AActor
{
	GENERATED_BODY()

public:
	ABeyondWaystone();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waystone")
	FName WaystoneId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waystone")
	FText DisplayName;

	// Attuned from the start (the starting village's stone)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waystone")
	bool bStartAttuned = false;

	// The party sees it (it goes on the map) this close
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waystone", meta = (ClampMin = "0"))
	float DiscoveryRadius = 8000.0f;

	// F works this close (from the stone's centre to the leader)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waystone", meta = (ClampMin = "50"))
	float InteractRange = 350.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waystone")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// Glows once attuned
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waystone")
	TObjectPtr<UPointLightComponent> Glow;

	// Where (and facing which way) the party lands after fast travel
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waystone")
	TObjectPtr<UArrowComponent> ArrivalPoint;

	// Looping on the stone while it is attuned (Max Lifetime 0)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waystone")
	FBeyondFX AttunedFX;

	// Played when it is attuned and when the party rests
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waystone")
	FBeyondFX ActivateFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waystone")
	FLinearColor GlowColour = FLinearColor(0.35f, 0.55f, 1.0f);

	UFUNCTION(BlueprintPure, Category = "Waystone")
	FTransform GetArrivalTransform() const;

	UFUNCTION(BlueprintPure, Category = "Waystone")
	FText GetDisplayNameOrId() const;

	// The look follows the party's progress (called by the world subsystem)
	void RefreshAttunedLook(bool bAttuned);

	void PlayActivateFX();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<UFXSystemComponent> AttunedEffect;
	bool bShownAttuned = false;
};
