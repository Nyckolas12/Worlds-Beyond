// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/BeyondCurrency.h"
#include "Items/BeyondItemTypes.h"
#include "BeyondInventoryComponent.generated.h"

class UBeyondEquipmentComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBeyondInventoryChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBeyondItemAddedSignature, const FBeyondItemInstance&, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBeyondCurrencyChangedSignature, EBeyondCurrency, Currency, int32, NewAmount, int32, Delta);

/**
 * The party's shared bag and wallet (on the player controller). Equipped items live on each demigod's
 * UBeyondEquipmentComponent; upgrading and locking work on an item wherever it is (bag or worn, Plan 6), junk and
 * salvage on bag items only.
 */
UCLASS(ClassGroup = (Beyond), meta = (BlueprintSpawnableComponent))
class WORLDBEYOND_API UBeyondInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBeyondInventoryComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Capacity = 60;

	// Anything added, removed, changed (upgrade, lock, junk) or restored
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FBeyondInventoryChangedSignature OnInventoryChanged;

	// A new item came in (a pickup; the HUD shows it)
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FBeyondItemAddedSignature OnItemAdded;

	// Money in or out (Delta < 0: spent); not for restoring a save
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Wallet")
	FBeyondCurrencyChangedSignature OnCurrencyChanged;

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

	//~ Wallet (Plan 6)

	UFUNCTION(BlueprintPure, Category = "Inventory|Wallet")
	int32 GetCurrency(EBeyondCurrency Currency) const { return Wallet.Get(Currency); }

	const FBeyondCurrencyBundle& GetWallet() const { return Wallet; }

	// Amounts below 1 are ignored
	UFUNCTION(BlueprintCallable, Category = "Inventory|Wallet")
	void AddCurrency(EBeyondCurrency Currency, int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Wallet")
	void AddCurrencies(const FBeyondCurrencyBundle& Bundle);

	UFUNCTION(BlueprintPure, Category = "Inventory|Wallet")
	bool CanAfford(const FBeyondCurrencyBundle& Price) const;

	// Takes the whole price, or nothing when the wallet is short
	UFUNCTION(BlueprintCallable, Category = "Inventory|Wallet")
	bool Spend(const FBeyondCurrencyBundle& Price);

	// Loading a save; no events but Inventory Changed
	void RestoreWallet(const FBeyondCurrencyBundle& SavedWallet);

	//~ Items the party owns: the bag and what the demigods wear

	// Finds an item in the bag or on a party member; OutWearer is the member's equipment (null: in the bag)
	bool FindOwnedItem(FGuid ItemId, FBeyondItemInstance& OutItem, UBeyondEquipmentComponent** OutWearer = nullptr) const;

	// Whether the item can go one upgrade higher now; OutReason says why not ("Already +15", "Not enough Gold")
	UFUNCTION(BlueprintPure, Category = "Inventory|Upgrades")
	bool CanUpgradeItem(FGuid ItemId, FText& OutReason) const;

	// Pays the upgrade cost and raises the item one level, in the bag or worn (worn stats change at once)
	UFUNCTION(BlueprintCallable, Category = "Inventory|Upgrades")
	bool UpgradeItem(FGuid ItemId, FText& OutReason);

	// Locking clears the junk mark
	UFUNCTION(BlueprintCallable, Category = "Inventory|Upgrades")
	bool SetItemLocked(FGuid ItemId, bool bLocked);

	// Bag items that aren't locked only
	UFUNCTION(BlueprintCallable, Category = "Inventory|Salvage")
	bool SetItemJunk(FGuid ItemId, bool bJunk);

	// Breaks a bag item down into currency (UBeyondItemLibrary::GetSalvageYield); never a locked one
	UFUNCTION(BlueprintCallable, Category = "Inventory|Salvage")
	bool SalvageItem(FGuid ItemId, FBeyondCurrencyBundle& OutYield);

	// Salvages every bag item marked as junk; returns how many
	UFUNCTION(BlueprintCallable, Category = "Inventory|Salvage")
	int32 SalvageJunk(FBeyondCurrencyBundle& OutYield);

	UFUNCTION(BlueprintPure, Category = "Inventory|Salvage")
	int32 GetJunkCount() const;

private:
	FBeyondItemInstance* FindBagItem(FGuid ItemId);
	// Writes a changed copy back where the item is (bag or worn)
	bool WriteBack(const FBeyondItemInstance& Item, UBeyondEquipmentComponent* Wearer);
	void BroadcastCurrency(EBeyondCurrency Currency, int32 Delta);

	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	TArray<FBeyondItemInstance> Items;

	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	FBeyondCurrencyBundle Wallet;
};
