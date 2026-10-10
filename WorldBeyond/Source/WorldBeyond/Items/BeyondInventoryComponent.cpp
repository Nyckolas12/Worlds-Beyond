// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/BeyondInventoryComponent.h"
#include "WorldBeyond.h"

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
