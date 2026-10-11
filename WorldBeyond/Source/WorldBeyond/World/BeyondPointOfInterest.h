// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BeyondPointOfInterest.generated.h"

class UBillboardComponent;

UENUM(BlueprintType)
enum class EBeyondPoiKind : uint8
{
	Landmark,
	Cave,
	Shrine,
	Ruins,
	Vista,
	Lake,
	Camp,
	// A boss arena site with no boss yet (Plan 5 reserves them for later bosses)
	ArenaSite
};

/**
 * A named place worth finding (Plan 5): a cave, a shrine, ruins, a vista. Walking within Discovery Radius discovers it
 * (a toast, EXP for both demigods, saved) and puts it on the world map. Only logic: the art around it is placed
 * separately. Always loaded in a World Partition map.
 */
UCLASS()
class WORLDBEYOND_API ABeyondPointOfInterest : public AActor
{
	GENERATED_BODY()

public:
	ABeyondPointOfInterest();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place")
	FName PoiId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place")
	EBeyondPoiKind Kind = EBeyondPoiKind::Landmark;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place", meta = (ClampMin = "100"))
	float DiscoveryRadius = 3000.0f;

	// EXP for both demigods when found (0: Project Settings -> Worlds Beyond World)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place", meta = (ClampMin = "0"))
	float DiscoveryExp = 0.0f;

	// Shown on the map (as "?") before it is found
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place")
	bool bVisibleBeforeDiscovery = false;

	UFUNCTION(BlueprintPure, Category = "Place")
	FText GetDisplayNameOrId() const;

	UFUNCTION(BlueprintPure, Category = "Place")
	static FText GetKindName(EBeyondPoiKind InKind);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif
};
