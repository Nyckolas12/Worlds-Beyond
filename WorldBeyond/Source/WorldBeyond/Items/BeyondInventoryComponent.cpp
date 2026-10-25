// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/BeyondInventoryComponent.h"
#include "Characters/BeyondCharacterBase.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Items/BeyondEquipmentComponent.h"
#include "Items/BeyondItemLibrary.h"
#include "Player/BeyondPartyComponent.h"
#include "WorldBeyond.h"

#define LOCTEXT_NAMESPACE "BeyondInventory"

namespace
{
	// Beyond.GiveCurrency <Gold|Crystals|EchoShards|ReverieFragments|All> <amount>
	FAutoConsoleCommandWithWorldAndArgs GiveCurrencyCommand(
		TEXT("Beyond.GiveCurrency"),
		TEXT("Adds to the party's wallet: Beyond.GiveCurrency <Gold|Crystals|EchoShards|ReverieFragments|All> <amount>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			UBeyondInventoryComponent* Inventory = PC ? PC->FindComponentByClass<UBeyondInventoryComponent>() : nullptr;
			if (!Inventory || Args.Num() < 2)
			{
				UE_LOG(LogBeyond, Warning, TEXT("Beyond.GiveCurrency <Gold|Crystals|EchoShards|ReverieFragments|All> <amount> (needs a game running)"));
				return;
			}
			const int32 Amount = FCString::Atoi(*Args[1]);
			const UEnum* Enum = StaticEnum<EBeyondCurrency>();
			for (const EBeyondCurrency Currency : FBeyondCurrencyBundle::All)
			{
				if (Args[0].Equals(TEXT("All"), ESearchCase::IgnoreCase) || Args[0].Equals(Enum->GetNameStringByValue(static_cast<int64>(Currency)), ESearchCase::IgnoreCase))
				{
					Inventory->AddCurrency(Currency, Amount);
				}
			}
			UE_LOG(LogBeyond, Log, TEXT("Wallet: %s"), *UBeyondItemLibrary::FormatCurrencies(Inventory->GetWallet()).ToString());
		}));
}

UBeyondInventoryComponent::UBeyondInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UBeyondInventoryComponent::AddItem(const FBeyondItemInstance& Item)
{
	if (!Item.IsValid() || IsFull())
	{
		return false;
	}
	Items.Add(Item);
	UE_LOG(LogBeyond, Log, TEXT("Inventory: + %s (%d / %d)"), *Item.Definition.GetAssetName(), Items.Num(), Capacity);
	OnItemAdded.Broadcast(Item);
	OnInventoryChanged.Broadcast();
	return true;
}

void UBeyondInventoryComponent::ReturnItem(const FBeyondItemInstance& Item)
{
	if (Item.IsValid())
	{
		Items.Add(Item);
		OnInventoryChanged.Broadcast();
	}
}

bool UBeyondInventoryComponent::RemoveItem(FGuid ItemId, FBeyondItemInstance& OutItem)
{
	const int32 Index = Items.IndexOfByPredicate([&ItemId](const FBeyondItemInstance& Item) { return Item.Id == ItemId; });
	if (Index == INDEX_NONE)
	{
		return false;
	}
	OutItem = Items[Index];
	Items.RemoveAt(Index);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UBeyondInventoryComponent::FindItem(FGuid ItemId, FBeyondItemInstance& OutItem) const
{
	if (const FBeyondItemInstance* Found = Items.FindByPredicate([&ItemId](const FBeyondItemInstance& Item) { return Item.Id == ItemId; }))
	{
		OutItem = *Found;
		return true;
	}
	return false;
}

void UBeyondInventoryComponent::RestoreItems(const TArray<FBeyondItemInstance>& SavedItems)
{
	Items.Reset();
	for (const FBeyondItemInstance& Item : SavedItems)
	{
		// Items whose definition was deleted since the save are dropped
		if (Item.IsValid() && Item.GetDefinition())
		{
			Items.Add(Item);
		}
	}
	OnInventoryChanged.Broadcast();
}

//~ Wallet

void UBeyondInventoryComponent::BroadcastCurrency(EBeyondCurrency Currency, int32 Delta)
{
	if (Delta != 0)
	{
		OnCurrencyChanged.Broadcast(Currency, Wallet.Get(Currency), Delta);
	}
}

void UBeyondInventoryComponent::AddCurrency(EBeyondCurrency Currency, int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	Wallet.Add(Currency, Amount);
	BroadcastCurrency(Currency, Amount);
}

void UBeyondInventoryComponent::AddCurrencies(const FBeyondCurrencyBundle& Bundle)
{
	for (const EBeyondCurrency Currency : FBeyondCurrencyBundle::All)
	{
		AddCurrency(Currency, Bundle.Get(Currency));
	}
}

bool UBeyondInventoryComponent::CanAfford(const FBeyondCurrencyBundle& Price) const
{
	for (const EBeyondCurrency Currency : FBeyondCurrencyBundle::All)
	{
		if (Wallet.Get(Currency) < Price.Get(Currency))
		{
			return false;
		}
	}
	return true;
}

bool UBeyondInventoryComponent::Spend(const FBeyondCurrencyBundle& Price)
{
	if (!CanAfford(Price))
	{
		return false;
	}
	for (const EBeyondCurrency Currency : FBeyondCurrencyBundle::All)
	{
		if (const int32 Amount = Price.Get(Currency); Amount > 0)
		{
			Wallet.Add(Currency, -Amount);
			BroadcastCurrency(Currency, -Amount);
		}
	}
	return true;
}

void UBeyondInventoryComponent::RestoreWallet(const FBeyondCurrencyBundle& SavedWallet)
{
	Wallet = FBeyondCurrencyBundle();
	for (const EBeyondCurrency Currency : FBeyondCurrencyBundle::All)
	{
		Wallet.Add(Currency, FMath::Max(SavedWallet.Get(Currency), 0));
	}
	OnInventoryChanged.Broadcast();
}

//~ Owned items

FBeyondItemInstance* UBeyondInventoryComponent::FindBagItem(FGuid ItemId)
{
	return Items.FindByPredicate([&ItemId](const FBeyondItemInstance& Item) { return Item.Id == ItemId; });
}

bool UBeyondInventoryComponent::FindOwnedItem(FGuid ItemId, FBeyondItemInstance& OutItem, UBeyondEquipmentComponent** OutWearer) const
{
	if (OutWearer)
	{
		*OutWearer = nullptr;
	}
	if (!ItemId.IsValid())
	{
		return false;
	}
	if (FindItem(ItemId, OutItem))
	{
		return true;
	}
	const UBeyondPartyComponent* Party = GetOwner() ? GetOwner()->FindComponentByClass<UBeyondPartyComponent>() : nullptr;
	for (ABeyondCharacterBase* Member : Party ? Party->GetMembers() : TArray<ABeyondCharacterBase*>())
	{
		UBeyondEquipmentComponent* Equipment = IsValid(Member) ? Member->GetEquipmentComponent() : nullptr;
		if (Equipment && Equipment->FindEquippedItem(ItemId, OutItem))
		{
			if (OutWearer)
			{
				*OutWearer = Equipment;
			}
			return true;
		}
	}
	return false;
}

bool UBeyondInventoryComponent::WriteBack(const FBeyondItemInstance& Item, UBeyondEquipmentComponent* Wearer)
{
	if (Wearer)
	{
		return Wearer->ReplaceEquipped(Item);
	}
	if (FBeyondItemInstance* InBag = FindBagItem(Item.Id))
	{
		*InBag = Item;
		OnInventoryChanged.Broadcast();
		return true;
	}
	return false;
}

bool UBeyondInventoryComponent::CanUpgradeItem(FGuid ItemId, FText& OutReason) const
{
	FBeyondItemInstance Item;
	if (!FindOwnedItem(ItemId, Item))
	{
		OutReason = LOCTEXT("NotOwned", "No such item");
		return false;
	}
	if (Item.UpgradeLevel >= UBeyondItemLibrary::GetMaxUpgradeLevel())
	{
		OutReason = FText::Format(LOCTEXT("MaxUpgrade", "Already +{0}"), FText::AsNumber(UBeyondItemLibrary::GetMaxUpgradeLevel()));
		return false;
	}
	const FBeyondCurrencyBundle Cost = UBeyondItemLibrary::GetUpgradeCost(Item);
	for (const EBeyondCurrency Currency : FBeyondCurrencyBundle::All)
	{
		if (Wallet.Get(Currency) < Cost.Get(Currency))
		{
			OutReason = FText::Format(LOCTEXT("NotEnough", "Not enough {0} ({1} needed)"), UBeyondItemLibrary::GetCurrencyName(Currency),
				FText::AsNumber(Cost.Get(Currency)));
			return false;
		}
	}
	OutReason = FText::GetEmpty();
	return true;
}

bool UBeyondInventoryComponent::UpgradeItem(FGuid ItemId, FText& OutReason)
{
	FBeyondItemInstance Item;
	UBeyondEquipmentComponent* Wearer = nullptr;
	if (!CanUpgradeItem(ItemId, OutReason) || !FindOwnedItem(ItemId, Item, &Wearer))
	{
		return false;
	}
	const FBeyondCurrencyBundle Cost = UBeyondItemLibrary::GetUpgradeCost(Item);
	if (!Spend(Cost))
	{
		return false;
	}
	const FBeyondItemInstance Upgraded = UBeyondItemLibrary::WithUpgrade(Item);
	if (!WriteBack(Upgraded, Wearer))
	{
		AddCurrencies(Cost);
		return false;
	}
	UE_LOG(LogBeyond, Log, TEXT("Inventory: %s upgraded to +%d for %s"), *UBeyondItemLibrary::GetItemName(Item).ToString(), Upgraded.UpgradeLevel,
		*UBeyondItemLibrary::FormatCurrencies(Cost).ToString());
	return true;
}

bool UBeyondInventoryComponent::SetItemLocked(FGuid ItemId, bool bLocked)
{
	FBeyondItemInstance Item;
	UBeyondEquipmentComponent* Wearer = nullptr;
	if (!FindOwnedItem(ItemId, Item, &Wearer))
	{
		return false;
	}
	if (Item.bLocked == bLocked && (!bLocked || !Item.bJunk))
	{
		return true;
	}
	Item.bLocked = bLocked;
	Item.bJunk = Item.bJunk && !bLocked;
	return WriteBack(Item, Wearer);
}

bool UBeyondInventoryComponent::SetItemJunk(FGuid ItemId, bool bJunk)
{
	FBeyondItemInstance* Item = FindBagItem(ItemId);
	if (!Item || (bJunk && Item->bLocked))
	{
		return false;
	}
	if (Item->bJunk != bJunk)
	{
		Item->bJunk = bJunk;
		OnInventoryChanged.Broadcast();
	}
	return true;
}

bool UBeyondInventoryComponent::SalvageItem(FGuid ItemId, FBeyondCurrencyBundle& OutYield)
{
	OutYield = FBeyondCurrencyBundle();
	FBeyondItemInstance Item;
	if (!FindItem(ItemId, Item) || Item.bLocked)
	{
		return false;
	}
	OutYield = UBeyondItemLibrary::GetSalvageYield(Item);
	RemoveItem(ItemId, Item);
	AddCurrencies(OutYield);
	UE_LOG(LogBeyond, Log, TEXT("Inventory: salvaged %s for %s"), *UBeyondItemLibrary::GetItemName(Item).ToString(),
		*UBeyondItemLibrary::FormatCurrencies(OutYield).ToString());
	return true;
}

int32 UBeyondInventoryComponent::SalvageJunk(FBeyondCurrencyBundle& OutYield)
{
	OutYield = FBeyondCurrencyBundle();
	TArray<FGuid> Junk;
	for (const FBeyondItemInstance& Item : Items)
	{
		if (Item.bJunk && !Item.bLocked)
		{
			Junk.Add(Item.Id);
		}
	}
	for (const FGuid& ItemId : Junk)
	{
		FBeyondCurrencyBundle Yield;
		SalvageItem(ItemId, Yield);
		OutYield += Yield;
	}
	return Junk.Num();
}

int32 UBeyondInventoryComponent::GetJunkCount() const
{
	return Items.FilterByPredicate([](const FBeyondItemInstance& Item) { return Item.bJunk && !Item.bLocked; }).Num();
}

#undef LOCTEXT_NAMESPACE
