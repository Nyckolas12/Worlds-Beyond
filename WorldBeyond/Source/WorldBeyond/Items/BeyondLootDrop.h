// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/BeyondItemTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BeyondLootDrop.generated.h"

class UFXSystemComponent;
class UPointLightComponent;

/** An item lying on the ground: a glow coloured by tier; the player picks it up with F (ABeyondPlayerController) */
UCLASS()
class WORLDBEYOND_API ABeyondLootDrop : public AActor
{
	GENERATED_BODY()

public:
	ABeyondLootDrop();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UPointLightComponent> Light;

	// Call before or right after spawning
	void SetItem(const FBeyondItemInstance& InItem);

	const FBeyondItemInstance& GetItem() const { return Item; }

	// "Venomweave Helm (Rare)"
	UFUNCTION(BlueprintPure, Category = "Loot")
	FText GetLabel() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RefreshLook();

	UPROPERTY(VisibleInstanceOnly, Category = "Loot")
	FBeyondItemInstance Item;

	TWeakObjectPtr<UFXSystemComponent> Glow;
	float Age = 0.0f;
	float BaseIntensity = 30.0f;
};

/** Keeps track of the loot on the ground: spawns drops, finds the nearest one for F and the HUD prompt */
UCLASS()
class WORLDBEYOND_API UBeyondLootSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBeyondLootSubsystem* Get(const UObject* WorldContext);

	// Spawns one drop per item in a small ring around Location, on the ground
	TArray<ABeyondLootDrop*> SpawnDrops(const TArray<FBeyondItemInstance>& Items, const FVector& Location);

	// Closest drop within MaxDistance of Location (null if none)
	ABeyondLootDrop* FindNearestDrop(const FVector& Location, float MaxDistance) const;

	int32 GetDropCount() const;

	void RegisterDrop(ABeyondLootDrop* Drop);
	void UnregisterDrop(ABeyondLootDrop* Drop);

private:
	TArray<TWeakObjectPtr<ABeyondLootDrop>> Drops;
};
