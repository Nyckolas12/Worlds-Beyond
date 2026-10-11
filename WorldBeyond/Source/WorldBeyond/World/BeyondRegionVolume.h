// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BeyondRegionVolume.generated.h"

class UBeyondRegionDefinition;
class UBillboardComponent;
class USplineComponent;

/**
 * Where a region, village or area is (Plan 5): a closed spline drawn on the ground (only X / Y count; heights are
 * ignored), or a circle of Radius when the spline has fewer than three points. The world subsystem checks the party
 * leader against every volume four times a second; where volumes overlap the highest Priority wins (villages 10, areas
 * 5, regions 0). Always loaded in a World Partition map (it is logic, not art).
 */
UCLASS()
class WORLDBEYOND_API ABeyondRegionVolume : public AActor
{
	GENERATED_BODY()

public:
	ABeyondRegionVolume();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	TObjectPtr<UBeyondRegionDefinition> Region;

	// Wins over lower priorities where volumes overlap
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	int32 Priority = 0;

	// Used when the spline has fewer than three points
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region", meta = (ClampMin = "100"))
	float Radius = 3000.0f;

	// The outline (closed loop, linear points)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Region")
	TObjectPtr<USplineComponent> Outline;

	// Replaces the outline with these world points (scripts: migrate_pass16 / the world demo)
	UFUNCTION(BlueprintCallable, Category = "Region")
	void SetOutline(const TArray<FVector>& WorldPoints);

	// Location (X / Y) is inside the outline
	UFUNCTION(BlueprintPure, Category = "Region")
	bool ContainsLocation(const FVector& Location) const;

	// The middle of the outline (map labels)
	UFUNCTION(BlueprintPure, Category = "Region")
	FVector GetCentre() const;

	// The outline in world space (X / Y), rebuilt from the spline
	const TArray<FVector2D>& GetPolygon() const { return Polygon; }

	void RebuildPolygon();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif

	TArray<FVector2D> Polygon;
	FBox2D Bounds = FBox2D(ForceInit);
};
