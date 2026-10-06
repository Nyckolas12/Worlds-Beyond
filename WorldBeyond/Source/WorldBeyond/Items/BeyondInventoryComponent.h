// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/BeyondItemTypes.h"
#include "BeyondInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBeyondInventoryChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBeyondItemAddedSignature, const FBeyondItemInstance&, Item);

/** The party's shared bag (on the player controller). Equipped items live on each demigod's UBeyondEquipmentComponent. */
UCLASS(ClassGroup = (Beyond), meta = (BlueprintSpawnableComponent))
class WORLDBEYOND_API UBeyondInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBeyondInventoryComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Capacity = 60;

	// Anything added, removed or restored
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FBeyondInventoryChangedSignature OnInventoryChanged;

	// A new item came in (a pickup; the HUD shows it)
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FBeyondItemAddedSignature OnItemAdded;

	// False if the bag is full or the item is invalid
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddItem(const FBeyondItemInstance& Item);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(FGuid ItemId, FBeyondItemInstance& OutItem);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool FindItem(FGuid ItemId, FBeyondItemInstance& OutItem) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsFull() const { return Items.Num() >= Capacity; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount() const { return Items.Num(); }

	const TArray<FBeyondItemInstance>& GetItems() const { return Items; }

	// Replaces the bag (loading a save); no Item Added events
	void RestoreItems(const TArray<FBeyondItemInstance>& SavedItems);

	// Puts an item back without the "new item" event (unequipping); ignores the capacity so nothing is ever lost
	void ReturnItem(const FBeyondItemInstance& Item);

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	TArray<FBeyondItemInstance> Items;
};
