// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondInventoryWidget.h"
#include "Characters/BeyondCharacterBase.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "Items/BeyondEquipmentComponent.h"
#include "Items/BeyondInventoryComponent.h"
#include "Items/BeyondItemLibrary.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Progression/BeyondSkillTree.h"
#include "Sound/SoundBase.h"
#include "UI/BeyondCharacterProfile.h"
#include "UI/BeyondMenuPanels.h"

#define LOCTEXT_NAMESPACE "BeyondInventory"

namespace BeyondArmorLocal
{
	constexpr double DoubleClickSeconds = 0.35;

	// The paper doll: slots on a rune ring around the silhouette
	const FVector2f RingCentre(640.0f, 404.0f);
	constexpr float RingRadius = 222.0f;
	constexpr float SlotSize = 92.0f;

	// Panels
	const FVector2f StatsTopLeft(958.0f, 124.0f);
	const FVector2f StatsSize(384.0f, 474.0f);
	const FVector2f BagTopLeft(1360.0f, 118.0f);
	const FVector2f BagSize(530.0f, 480.0f);
	const FVector2f SetsTopLeft(300.0f, 712.0f);
	const FVector2f SetsSize(636.0f, 276.0f);
	const FVector2f CardTopLeft(958.0f, 612.0f);
	const FVector2f CardSize(932.0f, 376.0f);

	// The bag grid
	constexpr int32 BagColumns = 5;
	constexpr int32 BagVisibleRows = 4;
	constexpr float CellSize = 80.0f;
	constexpr float CellGap = 8.0f;
	constexpr float GridTop = 244.0f;
	constexpr float FilterTop = 184.0f;
	constexpr float FilterSize = 50.0f;

	struct FSlotSpot
	{
		const TCHAR* Label;
		float AngleDegrees;
		EBeyondItemSlot Slot;
		EBeyondGlyph Glyph;
	};

	const TArray<FSlotSpot>& GetSlotSpots()
	{
		static const TArray<FSlotSpot> Spots = {
			{ TEXT("Helmet"), -90.0f, EBeyondItemSlot::Helm, EBeyondGlyph::Helm },
			{ TEXT("Chest"), -32.0f, EBeyondItemSlot::Chest, EBeyondGlyph::Chest },
			{ TEXT("Boots"), 16.0f, EBeyondItemSlot::Boots, EBeyondGlyph::Boots },
			{ TEXT("Charm"), 62.0f, EBeyondItemSlot::Charm, EBeyondGlyph::Amulet },
			{ TEXT("Accessory"), 118.0f, EBeyondItemSlot::Accessory, EBeyondGlyph::Ring },
			{ TEXT("Gauntlets"), 164.0f, EBeyondItemSlot::Gauntlets, EBeyondGlyph::Gauntlets },
			{ TEXT("Weapon"), 212.0f, EBeyondItemSlot::Weapon, EBeyondGlyph::Sword },
		};
		return Spots;
	}

	FVector2f GetSpotCentre(const FSlotSpot& Spot)
	{
		const float Angle = FMath::DegreesToRadians(Spot.AngleDegrees);
		return RingCentre + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * (RingRadius + 8.0f);
	}

	bool IsStaffTag(const FGameplayTag& Tag)
	{
		return Tag.ToString().Contains(TEXT("Staff"));
	}

	EBeyondGlyph GetItemGlyph(const UBeyondItemDefinition* Definition)
	{
		return Definition ? BeyondUI::SlotGlyph(Definition->Slot, IsStaffTag(Definition->WeaponTag)) : EBeyondGlyph::None;
	}

	EBeyondGlyph GetFilterGlyph(EBeyondBagFilter Filter)
	{
		switch (Filter)
		{
		case EBeyondBagFilter::Helm: return EBeyondGlyph::Helm;
		case EBeyondBagFilter::Chest: return EBeyondGlyph::Chest;
		case EBeyondBagFilter::Gauntlets: return EBeyondGlyph::Gauntlets;
		case EBeyondBagFilter::Boots: return EBeyondGlyph::Boots;
		case EBeyondBagFilter::Weapon: return EBeyondGlyph::Sword;
		case EBeyondBagFilter::Ring: return EBeyondGlyph::Ring;
		case EBeyondBagFilter::Amulet: return EBeyondGlyph::Amulet;
		default: return EBeyondGlyph::Grid;
		}
	}

	bool PassesFilter(const FBeyondItemInstance& Item, EBeyondBagFilter Filter)
	{
		const UBeyondItemDefinition* Definition = Item.GetDefinition();
		if (Filter == EBeyondBagFilter::All)
		{
			return true;
		}
		if (!Definition)
		{
			return false;
		}
		switch (Filter)
		{
		case EBeyondBagFilter::Helm: return Definition->Slot == EBeyondItemSlot::Helm;
		case EBeyondBagFilter::Chest: return Definition->Slot == EBeyondItemSlot::Chest;
		case EBeyondBagFilter::Gauntlets: return Definition->Slot == EBeyondItemSlot::Gauntlets;
		case EBeyondBagFilter::Boots: return Definition->Slot == EBeyondItemSlot::Boots;
		case EBeyondBagFilter::Weapon: return Definition->Slot == EBeyondItemSlot::Weapon;
		case EBeyondBagFilter::Ring: return Definition->Slot == EBeyondItemSlot::Accessory;
		case EBeyondBagFilter::Amulet: return Definition->Slot == EBeyondItemSlot::Charm;
		default: return false;
		}
	}

	// Rough worth of an item's stats, for the "better than worn" mark (health and stamina come in bigger numbers)
	float ItemScore(const FBeyondItemInstance& Item)
	{
		float Score = 0.0f;
		for (const FBeyondItemStat& Stat : UBeyondItemLibrary::GetItemStats(Item))
		{
			const float Weight = Stat.Stat == EBeyondSkillStat::MaxHealth ? 0.3f : Stat.Stat == EBeyondSkillStat::MaxStamina ? 0.5f : 1.0f;
			Score += Stat.Value * Weight;
		}
		return Score;
	}
}

UBeyondInventoryWidget::UBeyondInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EquipSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/axe-block.axe-block")));
}

void UBeyondInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshTabs();
}

void UBeyondInventoryWidget::RefreshTabs()
{
	const ABeyondCharacterBase* Previous = GetShownCharacter();
	Tabs.Reset();
	// Same order as the header: Conduit, Striker, anyone else
	for (ABeyondCharacterBase* Member : GetPartyInDuoOrder())
	{
		if (Member->GetEquipmentComponent())
		{
			FInventoryTab& Tab = Tabs.AddDefaulted_GetRef();
			Tab.Title = Member->GetCharacterDisplayName();
			Tab.Character = Member;
		}
	}
	ActiveTab = FMath::Max(FindTabFor(Previous), 0);
}

void UBeyondInventoryWidget::SelectTab(int32 Index)
{
	if (Tabs.IsValidIndex(Index) && (Index != ActiveTab || bPartyView))
	{
		ActiveTab = Index;
		bPartyView = false;
		Hovered = FInventoryHit();
		bSlotSelected = false;
		FlashAge = -1.0f;
	}
}

void UBeyondInventoryWidget::OnHeaderTabSelected(int32 Index)
{
	if (Tabs.IsValidIndex(Index))
	{
		SelectTab(Index);
	}
	else if (IsDuoHeaderTab(Index))
	{
		bPartyView = true;
		bSlotSelected = false;
		Hovered = FInventoryHit();
	}
}

int32 UBeyondInventoryWidget::FindTabFor(const AActor* Character) const
{
	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		if (Character && Tabs[Index].Character.Get() == Character)
		{
			return Index;
		}
	}
	return 0;
}

ABeyondCharacterBase* UBeyondInventoryWidget::GetShownCharacter() const
{
	return Tabs.IsValidIndex(ActiveTab) ? Tabs[ActiveTab].Character.Get() : nullptr;
}

UBeyondEquipmentComponent* UBeyondInventoryWidget::GetShownEquipment() const
{
	const ABeyondCharacterBase* Character = GetShownCharacter();
	return Character ? Character->GetEquipmentComponent() : nullptr;
}

UBeyondInventoryComponent* UBeyondInventoryWidget::GetInventory() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? PC->FindComponentByClass<UBeyondInventoryComponent>() : nullptr;
}

TArray<FBeyondItemInstance> UBeyondInventoryWidget::GetSortedBagItems() const
{
	TArray<FBeyondItemInstance> Items;
	if (const UBeyondInventoryComponent* Inventory = GetInventory())
	{
		Items = Inventory->GetItems();
	}
	Items.StableSort([](const FBeyondItemInstance& A, const FBeyondItemInstance& B)
	{
		const UBeyondItemDefinition* DefinitionA = A.GetDefinition();
		const UBeyondItemDefinition* DefinitionB = B.GetDefinition();
		const int32 SlotA = DefinitionA ? static_cast<int32>(DefinitionA->Slot) : 99;
		const int32 SlotB = DefinitionB ? static_cast<int32>(DefinitionB->Slot) : 99;
		if (SlotA != SlotB)
		{
			return SlotA < SlotB;
		}
		if (A.Tier != B.Tier)
		{
			return A.Tier > B.Tier;
		}
		return A.ItemLevel > B.ItemLevel;
	});
	return Items;
}

TArray<FBeyondItemInstance> UBeyondInventoryWidget::GetFilteredBagItems() const
{
	TArray<FBeyondItemInstance> Items = GetSortedBagItems();
	Items.RemoveAll([this](const FBeyondItemInstance& Item) { return !BeyondArmorLocal::PassesFilter(Item, BagFilter); });
	return Items;
}

void UBeyondInventoryWidget::SetBagFilter(EBeyondBagFilter Filter)
{
	if (Filter != BagFilter)
	{
		BagFilter = Filter;
		BagScrollRow = 0;
		PlaySelectSound();
	}
}

int32 UBeyondInventoryWidget::GetBagRows() const
{
	return FMath::DivideAndRoundUp(FMath::Max(GetFilteredBagItems().Num(), 1), BeyondArmorLocal::BagColumns);
}

void UBeyondInventoryWidget::ClampScroll()
{
	BagScrollRow = FMath::Clamp(BagScrollRow, 0, FMath::Max(GetBagRows() - BeyondArmorLocal::BagVisibleRows, 0));
}

void UBeyondInventoryWidget::SelectBagItem(FGuid ItemId)
{
	SelectedBagItem = ItemId;
	bSlotSelected = false;
}

void UBeyondInventoryWidget::SelectSlot(EBeyondItemSlot ItemSlot)
{
	SelectedSlot = ItemSlot;
	bSlotSelected = true;
	SelectedBagItem.Invalidate();
}

bool UBeyondInventoryWidget::GetDetailItem(FBeyondItemInstance& OutItem, bool& bOutWorn) const
{
	const UBeyondInventoryComponent* Inventory = GetInventory();
	auto FindWorn = [this, &OutItem](EBeyondItemSlot ItemSlot, int32 Member)
	{
		const ABeyondCharacterBase* Character = Member >= 0 ? (Tabs.IsValidIndex(Member) ? Tabs[Member].Character.Get() : nullptr) : GetShownCharacter();
		const UBeyondEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
		return Equipment && Equipment->GetEquipped(ItemSlot, OutItem);
	};
	if (Hovered.bSlot && FindWorn(Hovered.ItemSlot, Hovered.Member))
	{
		bOutWorn = true;
		return true;
	}
	if (Hovered.BagItem.IsValid() && Inventory && Inventory->FindItem(Hovered.BagItem, OutItem))
	{
		bOutWorn = false;
		return true;
	}
	if (bSlotSelected && FindWorn(SelectedSlot, INDEX_NONE))
	{
		bOutWorn = true;
		return true;
	}
	if (SelectedBagItem.IsValid() && Inventory && Inventory->FindItem(SelectedBagItem, OutItem))
	{
		bOutWorn = false;
		return true;
	}
	return false;
}

//~ Actions

bool UBeyondInventoryWidget::EquipBagItem(FGuid ItemId)
{
	UBeyondEquipmentComponent* Equipment = GetShownEquipment();
	UBeyondInventoryComponent* Inventory = GetInventory();
	FBeyondItemInstance Item;
	if (!Equipment || !Inventory || !Inventory->FindItem(ItemId, Item))
	{
		return false;
	}

	FText Reason;
	if (!Equipment->CanEquip(Item, Reason))
	{
		ShowMessage(Reason, true);
		PlayDeniedSound();
		return false;
	}
	if (!Equipment->EquipFromInventory(ItemId))
	{
		return false;
	}
	ShowEquipResult(Item);
	return true;
}

void UBeyondInventoryWidget::ShowEquipResult(const FBeyondItemInstance& Item)
{
	const UBeyondEquipmentComponent* Equipment = GetShownEquipment();
	const UBeyondItemDefinition* Definition = Item.GetDefinition();
	const ABeyondCharacterBase* Character = GetShownCharacter();
	FText Text = FText::Format(LOCTEXT("Equipped", "{0} equips {1}"), Character ? Character->GetCharacterDisplayName() : FText::GetEmpty(),
		UBeyondItemLibrary::GetItemName(Item));
	if (Definition && Definition->Set && Equipment)
	{
		const int32 Pieces = Equipment->GetSetPieceCount(Definition->Set);
		if (Pieces == UBeyondArmorSet::FullSetPieces)
		{
			Text = FText::Format(LOCTEXT("SetComplete", "{0} set complete: {1}"), Definition->Set->SetName, Definition->Set->FullSetDescription);
		}
		else if (Pieces == UBeyondArmorSet::PartialSetPieces)
		{
			Text = FText::Format(LOCTEXT("SetTwo", "{0}: 2-piece bonus active"), Definition->Set->SetName);
		}
	}
	ShowMessage(Text);
	PlayUISound(EquipSound, 0.7f);
	if (Definition)
	{
		bFlashSlot = true;
		FlashSlot = Definition->Slot;
		FlashAge = 0.0f;
		SelectSlot(Definition->Slot);
	}
}

bool UBeyondInventoryWidget::UnequipSlot(EBeyondItemSlot ItemSlot)
{
	UBeyondEquipmentComponent* Equipment = GetShownEquipment();
	FBeyondItemInstance Item;
	if (!Equipment || !Equipment->GetEquipped(ItemSlot, Item) || !Equipment->Unequip(ItemSlot))
	{
		return false;
	}
	ShowMessage(FText::Format(LOCTEXT("Unequipped", "{0} is back in the bag"), UBeyondItemLibrary::GetItemName(Item)));
	PlayUISound(EquipSound, 0.7f);
	SelectBagItem(Item.Id);
	return true;
}

bool UBeyondInventoryWidget::DiscardBagItem(FGuid ItemId)
{
	UBeyondInventoryComponent* Inventory = GetInventory();
	FBeyondItemInstance Item;
	if (!Inventory || !Inventory->FindItem(ItemId, Item))
	{
		return false;
	}
	FBeyondCurrencyBundle Yield;
	if (!Inventory->SalvageItem(ItemId, Yield))
	{
		ShowMessage(LOCTEXT("LockedSalvage", "Locked items can't be salvaged (L unlocks)"), true);
		return false;
	}
	if (Hovered.BagItem == ItemId)
	{
		Hovered = FInventoryHit();
	}
	if (SelectedBagItem == ItemId)
	{
		SelectedBagItem.Invalidate();
	}
	ShowMessage(FText::Format(LOCTEXT("Salvaged", "{0} salvaged: +{1}"), UBeyondItemLibrary::GetItemName(Item), UBeyondItemLibrary::FormatCurrencies(Yield)));
	ClampScroll();
	return true;
}

int32 UBeyondInventoryWidget::SalvageJunk()
{
	UBeyondInventoryComponent* Inventory = GetInventory();
	FBeyondCurrencyBundle Yield;
	const int32 Count = Inventory ? Inventory->SalvageJunk(Yield) : 0;
	if (Count == 0)
	{
		ShowMessage(LOCTEXT("NoJunk", "Select a bag item, or mark junk with X, then hold V to salvage"), true);
		return 0;
	}
	Hovered = FInventoryHit();
	SelectedBagItem.Invalidate();
	ShowMessage(FText::Format(LOCTEXT("SalvagedJunk", "{0} junk {0}|plural(one=item,other=items) salvaged: +{1}"), FText::AsNumber(Count),
		UBeyondItemLibrary::FormatCurrencies(Yield)));
	ClampScroll();
	return Count;
}

bool UBeyondInventoryWidget::UpgradeItem(FGuid ItemId)
{
	UBeyondInventoryComponent* Inventory = GetInventory();
	FText Reason;
	if (!Inventory || !Inventory->UpgradeItem(ItemId, Reason))
	{
		ShowMessage(Reason.IsEmpty() ? LOCTEXT("CantUpgrade", "Can't upgrade that") : Reason, true);
		PlayDeniedSound();
		return false;
	}
	FBeyondItemInstance Item;
	Inventory->FindOwnedItem(ItemId, Item);
	ShowMessage(FText::Format(LOCTEXT("Upgraded", "{0} is now +{1}"), UBeyondItemLibrary::GetItemName(Item), FText::AsNumber(Item.UpgradeLevel)));
	PlayUISound(EquipSound, 0.7f);
	FlashWornSlot(Item);
	return true;
}

bool UBeyondInventoryWidget::ToggleLock(FGuid ItemId)
{
	UBeyondInventoryComponent* Inventory = GetInventory();
	FBeyondItemInstance Item;
	if (!Inventory || !Inventory->FindOwnedItem(ItemId, Item) || !Inventory->SetItemLocked(ItemId, !Item.bLocked))
	{
		return false;
	}
	ShowMessage(FText::Format(Item.bLocked ? LOCTEXT("Unlocked", "{0} unlocked") : LOCTEXT("Locked", "{0} locked: it can't be salvaged"),
		UBeyondItemLibrary::GetItemName(Item)));
	PlaySelectSound();
	return true;
}

bool UBeyondInventoryWidget::ToggleJunk(FGuid ItemId)
{
	UBeyondInventoryComponent* Inventory = GetInventory();
	FBeyondItemInstance Item;
	if (!Inventory || !Inventory->FindItem(ItemId, Item))
	{
		ShowMessage(LOCTEXT("JunkBagOnly", "Only bag items can be marked as junk"), true);
		return false;
	}
	if (!Inventory->SetItemJunk(ItemId, !Item.bJunk))
	{
		ShowMessage(LOCTEXT("JunkLocked", "Locked items can't be junk (L unlocks)"), true);
		PlayDeniedSound();
		return false;
	}
	ShowMessage(FText::Format(Item.bJunk ? LOCTEXT("NotJunk", "{0} is no longer junk") : LOCTEXT("Junk", "{0} marked as junk (hold V to salvage the junk)"),
		UBeyondItemLibrary::GetItemName(Item)));
	PlaySelectSound();
	return true;
}

void UBeyondInventoryWidget::BeginHold(EArmorHold Action)
{
	FBeyondItemInstance Item;
	bool bWorn = false;
	const bool bHasItem = GetDetailItem(Item, bWorn);
	if (Action == EArmorHold::Upgrade)
	{
		FText Reason;
		const UBeyondInventoryComponent* Inventory = GetInventory();
		if (!bHasItem || !Inventory || !Inventory->CanUpgradeItem(Item.Id, Reason))
		{
			ShowMessage(bHasItem ? Reason : LOCTEXT("SelectToUpgrade", "Select a piece of gear, then hold U to upgrade it"), true);
			PlayDeniedSound();
			return;
		}
	}
	else if (bHasItem && bWorn)
	{
		ShowMessage(LOCTEXT("TakeOffFirst", "Take it off first to salvage it"), true);
		return;
	}
	HoldAction = Action;
	HoldTime = 0.0f;
	HoldItem = bHasItem ? Item.Id : FGuid();
}

void UBeyondInventoryWidget::CancelHold()
{
	HoldAction = EArmorHold::None;
	HoldTime = 0.0f;
	HoldItem.Invalidate();
}

void UBeyondInventoryWidget::FinishHold()
{
	const EArmorHold Action = HoldAction;
	const FGuid ItemId = HoldItem;
	CancelHold();
	if (Action == EArmorHold::Upgrade)
	{
		UpgradeItem(ItemId);
	}
	else if (Action == EArmorHold::Salvage)
	{
		if (ItemId.IsValid())
		{
			DiscardBagItem(ItemId);
		}
		else
		{
			SalvageJunk();
		}
	}
}

void UBeyondInventoryWidget::FlashWornSlot(const FBeyondItemInstance& Item)
{
	const UBeyondEquipmentComponent* Equipment = GetShownEquipment();
	FBeyondItemInstance Worn;
	if (const UBeyondItemDefinition* Definition = Item.GetDefinition(); Definition && Equipment && Equipment->FindEquippedItem(Item.Id, Worn))
	{
		bFlashSlot = true;
		FlashSlot = Definition->Slot;
		FlashAge = 0.0f;
	}
}

void UBeyondInventoryWidget::ActivateSelection()
{
	if (bPartyView)
	{
		ShowMessage(LOCTEXT("PickDemigod", "Pick Angel or Ji-Woong (Q / E) to change their gear"), true);
		return;
	}
	FBeyondItemInstance Item;
	bool bWorn = false;
	if (!GetDetailItem(Item, bWorn))
	{
		return;
	}
	if (bWorn)
	{
		if (const UBeyondItemDefinition* Definition = Item.GetDefinition())
		{
			UnequipSlot(Definition->Slot);
		}
	}
	else
	{
		EquipBagItem(Item.Id);
	}
	ClampScroll();
}

void UBeyondInventoryWidget::AdvancePage(float DeltaSeconds)
{
	if (HoldAction != EArmorHold::None)
	{
		HoldTime += DeltaSeconds;
		if (HoldTime >= HoldDuration)
		{
			FinishHold();
		}
	}
	if (FlashAge >= 0.0f)
	{
		FlashAge += DeltaSeconds;
		if (FlashAge > 0.6f)
		{
			FlashAge = -1.0f;
		}
	}
}

//~ Layout and input

FVector2f UBeyondInventoryWidget::GetCellCentre(int32 VisibleIndex) const
{
	using namespace BeyondArmorLocal;
	const float GridWidth = BagColumns * CellSize + (BagColumns - 1) * CellGap;
	const float Left = BagTopLeft.X + (BagSize.X - GridWidth) * 0.5f;
	return FVector2f(Left + (VisibleIndex % BagColumns) * (CellSize + CellGap) + CellSize * 0.5f,
		GridTop + (VisibleIndex / BagColumns) * (CellSize + CellGap) + CellSize * 0.5f);
}

UBeyondInventoryWidget::FInventoryHit UBeyondInventoryWidget::HitTest(const FVector2f& Design) const
{
	using namespace BeyondArmorLocal;
	FInventoryHit Hit;
	auto InBox = [&Design](const FVector2f& Centre, float Size) { return FMath::Abs(Design.X - Centre.X) <= Size * 0.5f && FMath::Abs(Design.Y - Centre.Y) <= Size * 0.5f; };

	// Filters
	for (int32 Index = 0; Index <= static_cast<int32>(EBeyondBagFilter::Amulet); ++Index)
	{
		const float GridWidth = BagColumns * CellSize + (BagColumns - 1) * CellGap;
		const float Left = BagTopLeft.X + (BagSize.X - GridWidth) * 0.5f;
		const float Step = (GridWidth - FilterSize) / static_cast<float>(EBeyondBagFilter::Amulet);
		if (InBox(FVector2f(Left + FilterSize * 0.5f + Index * Step, FilterTop + FilterSize * 0.5f), FilterSize))
		{
			Hit.Filter = Index;
			return Hit;
		}
	}

	// Worn gear: the paper doll, or both loadouts in the party view
	if (bPartyView)
	{
		for (int32 Member = 0; Member < Tabs.Num() && Member < 2; ++Member)
		{
			const TArray<FSlotSpot>& Spots = GetSlotSpots();
			for (int32 Index = 0; Index < Spots.Num(); ++Index)
			{
				const FVector2f Centre(340.0f + Member * 316.0f + (Index % 4) * 76.0f + 32.0f, 214.0f + (Index / 4) * 96.0f);
				if (InBox(Centre, 64.0f))
				{
					Hit.bSlot = true;
					Hit.ItemSlot = Spots[Index].Slot;
					Hit.Member = Member;
					return Hit;
				}
			}
		}
	}
	else
	{
		for (const FSlotSpot& Spot : GetSlotSpots())
		{
			if (InBox(GetSpotCentre(Spot), SlotSize))
			{
				Hit.bSlot = true;
				Hit.ItemSlot = Spot.Slot;
				return Hit;
			}
		}
	}

	// The bag
	const TArray<FBeyondItemInstance> Items = GetFilteredBagItems();
	for (int32 Visible = 0; Visible < BagColumns * BagVisibleRows; ++Visible)
	{
		const int32 Index = BagScrollRow * BagColumns + Visible;
		if (Items.IsValidIndex(Index) && InBox(GetCellCentre(Visible), CellSize))
		{
			Hit.BagItem = Items[Index].Id;
			return Hit;
		}
	}
	return Hit;
}

void UBeyondInventoryWidget::HandlePageHover(const FVector2f& Design)
{
	Hovered = HitTest(Design);
}

bool UBeyondInventoryWidget::HandlePageClick(const FVector2f& Design, const FKey& Button)
{
	if (Button != EKeys::LeftMouseButton)
	{
		return false;
	}
	const FInventoryHit Hit = HitTest(Design);
	const double Now = FPlatformTime::Seconds();
	if (Hit.Filter != INDEX_NONE)
	{
		SetBagFilter(static_cast<EBeyondBagFilter>(Hit.Filter));
		return true;
	}
	if (Hit.bSlot)
	{
		const bool bDouble = bLastClickSlot && Now - LastClickTime < BeyondArmorLocal::DoubleClickSeconds && SelectedSlot == Hit.ItemSlot && Hit.Member == INDEX_NONE;
		if (Hit.Member == INDEX_NONE)
		{
			SelectSlot(Hit.ItemSlot);
		}
		LastClickTime = Now;
		bLastClickSlot = true;
		if (bDouble)
		{
			UnequipSlot(Hit.ItemSlot);
		}
		else
		{
			PlaySelectSound();
		}
	}
	else if (Hit.BagItem.IsValid())
	{
		const bool bDouble = !bLastClickSlot && LastClickItem == Hit.BagItem && Now - LastClickTime < BeyondArmorLocal::DoubleClickSeconds;
		SelectBagItem(Hit.BagItem);
		LastClickTime = Now;
		LastClickItem = Hit.BagItem;
		bLastClickSlot = false;
		if (bDouble && !bPartyView)
		{
			EquipBagItem(Hit.BagItem);
			ClampScroll();
		}
		else
		{
			PlaySelectSound();
		}
	}
	// The layout shifts after a change; hover what is under the mouse now
	Hovered = HitTest(Design);
	return true;
}

bool UBeyondInventoryWidget::HandlePageWheel(const FVector2f& Design, float Delta)
{
	BagScrollRow -= FMath::Sign(Delta);
	ClampScroll();
	Hovered = HitTest(Design);
	return true;
}

bool UBeyondInventoryWidget::HandlePageKey(const FKey& Key)
{
	if (Key == EKeys::F || Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		ActivateSelection();
		return true;
	}
	if (Key == EKeys::R)
	{
		bCompare = !bCompare;
		ShowMessage(bCompare ? LOCTEXT("CompareOn", "Comparing with what is worn") : LOCTEXT("CompareOff", "Comparison off"));
		return true;
	}
	if (Key == EKeys::T)
	{
		bDetails = !bDetails;
		return true;
	}
	if (Key == EKeys::U || Key == EKeys::V)
	{
		BeginHold(Key == EKeys::U ? EArmorHold::Upgrade : EArmorHold::Salvage);
		return true;
	}
	if (Key == EKeys::L || Key == EKeys::X)
	{
		FBeyondItemInstance Item;
		bool bWorn = false;
		if (!GetDetailItem(Item, bWorn))
		{
			ShowMessage(Key == EKeys::L ? LOCTEXT("SelectToLock", "Select a piece of gear to lock it") : LOCTEXT("SelectToJunk", "Select a bag item to mark it as junk"), true);
		}
		else if (Key == EKeys::L)
		{
			ToggleLock(Item.Id);
		}
		else
		{
			ToggleJunk(Item.Id);
		}
		return true;
	}
	if (Key == EKeys::Up || Key == EKeys::Down)
	{
		BagScrollRow += Key == EKeys::Down ? 1 : -1;
		ClampScroll();
		return true;
	}
	return false;
}

bool UBeyondInventoryWidget::HandlePageKeyUp(const FKey& Key)
{
	if ((Key == EKeys::U && HoldAction == EArmorHold::Upgrade) || (Key == EKeys::V && HoldAction == EArmorHold::Salvage))
	{
		CancelHold();
		return true;
	}
	return false;
}

void UBeyondInventoryWidget::GetPageKeyHints(TArray<FBeyondKeyHint>& OutHints) const
{
	FBeyondItemInstance Item;
	bool bWorn = false;
	const bool bHasItem = GetDetailItem(Item, bWorn);
	OutHints.Add({ TEXT("LMB"), LOCTEXT("Select", "Select"), EKeys::LeftMouseButton });
	OutHints.Add({ TEXT("F"), bHasItem && bWorn ? LOCTEXT("Unequip", "Unequip") : LOCTEXT("Equip", "Equip"), EKeys::F });
	OutHints.Add({ TEXT("U"), LOCTEXT("UpgradeHold", "Upgrade (Hold)"), EKeys::U });
	OutHints.Add({ TEXT("L"), bHasItem && Item.bLocked ? LOCTEXT("Unlock", "Unlock") : LOCTEXT("Lock", "Lock"), EKeys::L });
	OutHints.Add({ TEXT("X"), LOCTEXT("JunkHint", "Junk"), EKeys::X });
	OutHints.Add({ TEXT("V"), LOCTEXT("SalvageHold", "Salvage (Hold)"), EKeys::V });
	OutHints.Add({ TEXT("R"), LOCTEXT("Compare", "Compare"), EKeys::R });
	OutHints.Add({ TEXT("T"), LOCTEXT("Details", "Details"), EKeys::T });
}

bool UBeyondInventoryWidget::GetCounter(FBeyondMenuCounter& OutCounter) const
{
	// What upgrades are paid with
	OutCounter.Label = LOCTEXT("Wallet", "Wallet");
	AddCurrencyToCounter(OutCounter, EBeyondCurrency::Gold);
	AddCurrencyToCounter(OutCounter, EBeyondCurrency::Crystals);
	AddCurrencyToCounter(OutCounter, EBeyondCurrency::EchoShards);
	return true;
}

//~ Painting

void UBeyondInventoryWidget::PaintItemTile(const BeyondUI::FPainter& Paint, const FBeyondItemInstance* Item, const FVector2f& Centre, float Size,
	EBeyondGlyph EmptyGlyph, bool bSelected, bool bHovered, int32 Layer) const
{
	BeyondUI::FItemFrameStyle Style;
	const UBeyondItemDefinition* Definition = Item ? Item->GetDefinition() : nullptr;
	Style.bEmpty = Definition == nullptr;
	Style.bSelected = bSelected;
	Style.bHovered = bHovered;
	Style.bShowUpgrade = true;
	if (Definition)
	{
		Style.Tier = Item->Tier;
		Style.Icon = Definition->Icon;
		Style.Glyph = BeyondArmorLocal::GetItemGlyph(Definition);
		Style.UpgradeLevel = Item->UpgradeLevel;
		Style.bLocked = Item->bLocked;
		Style.bJunk = Item->bJunk;
	}
	else
	{
		Style.Glyph = EmptyGlyph;
	}
	Paint.ItemFrame(Centre, Size, Style, Layer);
}

void UBeyondInventoryWidget::PaintPage(BeyondUI::FPainter& Paint) const
{
	if (bPartyView)
	{
		PaintPartyView(Paint);
	}
	else
	{
		PaintPaperDoll(Paint);
		PaintStats(Paint);
	}
	PaintBag(Paint);
	PaintSets(Paint);
	PaintCard(Paint);
}

void UBeyondInventoryWidget::PaintPaperDoll(const BeyondUI::FPainter& Paint) const
{
	using namespace BeyondUI;
	using namespace BeyondArmorLocal;
	const FPalette& P = Palette();
	const ABeyondCharacterBase* Character = GetShownCharacter();
	const UBeyondEquipmentComponent* Equipment = GetShownEquipment();
	const UBeyondCharacterProfile* Profile = Character ? Character->Profile.Get() : nullptr;

	// The ring and the demigod's silhouette inside it
	Paint.RuneCircle(RingCentre, RingRadius, WithAlpha(P.Gold, 0.65f), 20);
	Paint.Glow(RingCentre, RingRadius * 0.9f, WithAlpha(GetAccent(), 0.12f), 20);
	if (UTexture2D* Silhouette = Profile ? LoadTexture(Profile->Silhouette) : nullptr)
	{
		Paint.ImageCentred(Silhouette, RingCentre + FVector2f(0.0f, 10.0f), FVector2f(RingRadius * 1.05f, RingRadius * 1.6f), FLinearColor(0.6f, 0.68f, 0.85f, 0.8f), 21);
	}
	else if (Profile)
	{
		Paint.Glyph(ElementGlyph(Profile->PrimaryElement), RingCentre, RingRadius * 0.9f, WithAlpha(GetAccent(), 0.14f), 21, 2.5f);
	}

	// Slots on the ring, joined to it by a short line
	const bool bStaff = Character && IsStaffTag(Character->GetDefaultWeaponTag());
	for (const FSlotSpot& Spot : GetSlotSpots())
	{
		const FVector2f Centre = GetSpotCentre(Spot);
		const FVector2f Direction = (Centre - RingCentre).GetSafeNormal();
		Paint.Line(RingCentre + Direction * (RingRadius * 0.74f), Centre - Direction * (SlotSize * 0.55f), WithAlpha(P.Gold, 0.3f), 1.0f, 22);

		FBeyondItemInstance Worn;
		const bool bHasItem = Equipment && Equipment->GetEquipped(Spot.Slot, Worn);
		const bool bHover = Hovered.bSlot && Hovered.Member == INDEX_NONE && Hovered.ItemSlot == Spot.Slot;
		const bool bSelected = bSlotSelected && SelectedSlot == Spot.Slot;
		const EBeyondGlyph EmptyGlyph = Spot.Slot == EBeyondItemSlot::Weapon ? (bStaff ? EBeyondGlyph::Staff : EBeyondGlyph::Sword) : Spot.Glyph;
		PaintItemTile(Paint, bHasItem ? &Worn : nullptr, Centre, SlotSize, EmptyGlyph, bSelected, bHover, 24);
		if (bFlashSlot && FlashAge >= 0.0f && FlashSlot == Spot.Slot)
		{
			const float Alpha = FlashAge / 0.6f;
			const float Grow = SlotSize + 10.0f + 40.0f * Alpha;
			Paint.RectOutline(Centre - FVector2f(Grow, Grow) * 0.5f, FVector2f(Grow, Grow), WithAlpha(P.GoldBright, 1.0f - Alpha), 2.0f, 30);
		}

		// Label and the item's level under the slot
		const float LabelY = Centre.Y + SlotSize * 0.5f + 6.0f;
		Paint.Text(FString(Spot.Label).ToUpper(), Font(EBeyondFont::Heading, 15.0f), FVector2f(Centre.X, LabelY), P.Text, 0.5f, 26);
		if (bHasItem)
		{
			Paint.Text(Worn.UpgradeLevel > 0 ? FString::Printf(TEXT("Lv %d  +%d"), Worn.ItemLevel, Worn.UpgradeLevel) : FString::Printf(TEXT("Lv %d"), Worn.ItemLevel),
				Font(EBeyondFont::Number, 15.0f), FVector2f(Centre.X, LabelY + 20.0f), P.GoldBright, 0.5f, 26);
		}
	}
}

void UBeyondInventoryWidget::PaintPartyView(const BeyondUI::FPainter& Paint) const
{
	using namespace BeyondUI;
	using namespace BeyondArmorLocal;
	const FPalette& P = Palette();
	for (int32 Member = 0; Member < Tabs.Num() && Member < 2; ++Member)
	{
		const ABeyondCharacterBase* Character = Tabs[Member].Character.Get();
		const UBeyondEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
		const FVector2f TopLeft(316.0f + Member * 316.0f, 124.0f);
		const FVector2f Size(300.0f, 572.0f);
		Paint.Panel(TopLeft, Size, 20);
		Paint.Text(Tabs[Member].Title.ToString(), Font(EBeyondFont::Display, 24.0f), TopLeft + FVector2f(22.0f, 16.0f), P.GoldBright, 0.0f, 22);
		if (Character)
		{
			Paint.Text(FString::Printf(TEXT("Lv. %d"), Character->GetCharacterLevel()), Font(EBeyondFont::Number, 18.0f), TopLeft + FVector2f(Size.X - 22.0f, 22.0f),
				P.Text, 1.0f, 22);
		}
		const TArray<FSlotSpot>& Spots = GetSlotSpots();
		for (int32 Index = 0; Index < Spots.Num(); ++Index)
		{
			const FVector2f Centre(TopLeft.X + 24.0f + (Index % 4) * 76.0f + 32.0f, 214.0f + (Index / 4) * 96.0f);
			FBeyondItemInstance Worn;
			const bool bHasItem = Equipment && Equipment->GetEquipped(Spots[Index].Slot, Worn);
			const bool bHover = Hovered.bSlot && Hovered.Member == Member && Hovered.ItemSlot == Spots[Index].Slot;
			PaintItemTile(Paint, bHasItem ? &Worn : nullptr, Centre, 64.0f, Spots[Index].Glyph, false, bHover, 24);
		}
		const float StatsTop = Paint.SectionHeader(LOCTEXT("PartyStats", "Combat Stats").ToString(), TopLeft + FVector2f(22.0f, 338.0f), Size.X - 44.0f, 24, 15.0f);
		BeyondMenuPanels::PaintStats(Paint, BeyondMenuPanels::GetCombatStats(Character), FVector2f(TopLeft.X + 18.0f, StatsTop), Size.X - 40.0f, 27.0f, 24, false, 17.0f);
	}
}

void UBeyondInventoryWidget::PaintStats(const BeyondUI::FPainter& Paint) const
{
	using namespace BeyondArmorLocal;
	const ABeyondCharacterBase* Character = GetShownCharacter();
	if (!Character)
	{
		return;
	}
	Paint.Panel(StatsTopLeft, StatsSize, 20);
	const FVector2f Left = StatsTopLeft + FVector2f(24.0f, 18.0f);
	const float Inner = StatsSize.X - 48.0f;
	const float LevelHeight = BeyondMenuPanels::PaintLevel(Paint, BeyondMenuPanels::GetLevelInfo(Character), Left, Inner, 22);
	const float StatsTop = Paint.SectionHeader(LOCTEXT("CombatStats", "Combat Stats").ToString(), Left + FVector2f(0.0f, LevelHeight + 10.0f), Inner, 22);
	BeyondMenuPanels::PaintStats(Paint, BeyondMenuPanels::GetCombatStats(Character), FVector2f(Left.X, StatsTop), Inner, 33.0f, 22, true, 19.0f);
}

void UBeyondInventoryWidget::PaintBag(const BeyondUI::FPainter& Paint) const
{
	using namespace BeyondUI;
	using namespace BeyondArmorLocal;
	const FPalette& P = Palette();
	const UBeyondInventoryComponent* Inventory = GetInventory();
	const UBeyondEquipmentComponent* Equipment = GetShownEquipment();
	Paint.Panel(BagTopLeft, BagSize, 40);

	const int32 Count = Inventory ? Inventory->GetItemCount() : 0;
	const int32 Capacity = Inventory ? Inventory->Capacity : 0;
	Paint.Text(LOCTEXT("Bag", "Bag").ToString(), Font(EBeyondFont::Display, 26.0f), BagTopLeft + FVector2f(26.0f, 18.0f), P.GoldBright, 0.0f, 42);
	Paint.Text(FString::Printf(TEXT("%d / %d"), Count, Capacity), Font(EBeyondFont::Number, 19.0f), BagTopLeft + FVector2f(BagSize.X - 26.0f, 26.0f),
		Count >= Capacity ? P.Minus : P.Text, 1.0f, 42);

	// Filters
	const float GridWidth = BagColumns * CellSize + (BagColumns - 1) * CellGap;
	const float Left = BagTopLeft.X + (BagSize.X - GridWidth) * 0.5f;
	const float Step = (GridWidth - FilterSize) / static_cast<float>(EBeyondBagFilter::Amulet);
	for (int32 Index = 0; Index <= static_cast<int32>(EBeyondBagFilter::Amulet); ++Index)
	{
		const FVector2f TopLeft(Left + Index * Step, FilterTop);
		const FVector2f Size(FilterSize, FilterSize);
		const bool bActive = static_cast<int32>(BagFilter) == Index;
		const bool bHover = Hovered.Filter == Index;
		if (bActive)
		{
			Paint.GradientV(TopLeft, Size, WithAlpha(P.Blue, 0.75f), WithAlpha(P.BlueDeep, 0.8f), 42);
			Paint.RectOutline(TopLeft, Size, P.BlueBright, 1.2f, 43);
		}
		else
		{
			Paint.GradientV(TopLeft, Size, FromHex(0x141C30, 0.9f), FromHex(0x0A0F1C, 0.9f), 42);
			Paint.RectOutline(TopLeft, Size, WithAlpha(P.Silver, bHover ? 0.7f : 0.3f), 1.0f, 43);
		}
		Paint.Glyph(GetFilterGlyph(static_cast<EBeyondBagFilter>(Index)), TopLeft + Size * 0.5f, 30.0f, bActive ? FLinearColor::White : P.TextDim, 44, 1.5f);
	}

	// The grid (empty cells to the bag's capacity)
	const TArray<FBeyondItemInstance> Items = GetFilteredBagItems();
	const int32 Shown = BagFilter == EBeyondBagFilter::All ? FMath::Max(Capacity, Items.Num()) : FMath::Max(Items.Num(), BagColumns * BagVisibleRows);
	for (int32 Visible = 0; Visible < BagColumns * BagVisibleRows; ++Visible)
	{
		const int32 Index = BagScrollRow * BagColumns + Visible;
		const FVector2f Centre = GetCellCentre(Visible);
		if (!Items.IsValidIndex(Index))
		{
			if (Index < Shown)
			{
				PaintItemTile(Paint, nullptr, Centre, CellSize, EBeyondGlyph::None, false, false, 44);
			}
			continue;
		}
		const FBeyondItemInstance& Item = Items[Index];
		const bool bHover = Hovered.BagItem == Item.Id;
		const bool bSelected = !bSlotSelected && SelectedBagItem == Item.Id;
		PaintItemTile(Paint, &Item, Centre, CellSize, EBeyondGlyph::None, bSelected, bHover, 44);

		// Top right (unless the lock badge is there): green arrow, better than what the shown demigod wears there; red
		// cross, they can't use it. Bottom right: the item level (the frame's +N there once upgraded)
		const UBeyondItemDefinition* Definition = Item.GetDefinition();
		const FVector2f Corner = Centre + FVector2f(CellSize * 0.5f - 12.0f, -CellSize * 0.5f + 12.0f);
		FText Reason;
		if (!Item.bLocked && Equipment && Definition && !Equipment->CanEquip(Item, Reason))
		{
			Paint.Glyph(EBeyondGlyph::Cross, Corner, 14.0f, P.Minus, 50, 2.0f);
		}
		else if (!Item.bLocked && Equipment && Definition)
		{
			FBeyondItemInstance Worn;
			if (!Equipment->GetEquipped(Definition->Slot, Worn) || ItemScore(Item) > ItemScore(Worn) + 0.5f)
			{
				Paint.Glyph(EBeyondGlyph::Up, Corner, 15.0f, P.Plus, 50, 2.0f);
			}
		}
		if (Item.UpgradeLevel <= 0)
		{
			Paint.Text(FString::FromInt(Item.ItemLevel), Font(EBeyondFont::Number, 13.0f), Centre + FVector2f(CellSize * 0.5f - 6.0f, CellSize * 0.5f - 20.0f), P.Text, 1.0f, 50);
		}
		// Being salvaged (V held): a red frame closing in
		if (HoldAction == EArmorHold::Salvage && HoldItem == Item.Id && HoldDuration > 0.0f)
		{
			const float Fill = FMath::Clamp(HoldTime / HoldDuration, 0.0f, 1.0f);
			const float Grow = CellSize + 6.0f + 24.0f * (1.0f - Fill);
			Paint.RectOutline(Centre - FVector2f(Grow, Grow) * 0.5f, FVector2f(Grow, Grow), WithAlpha(P.Minus, 0.4f + 0.6f * Fill), 2.0f, 51);
		}
	}

	// Scroll position
	const int32 Rows = GetBagRows();
	if (Rows > BagVisibleRows)
	{
		const float TrackTop = GridTop;
		const float TrackHeight = BagVisibleRows * (CellSize + CellGap) - CellGap;
		const float X = BagTopLeft.X + BagSize.X - 14.0f;
		Paint.Box(FVector2f(X, TrackTop), FVector2f(2.0f, TrackHeight), WithAlpha(P.Silver, 0.2f), 42);
		const float ThumbHeight = TrackHeight * BagVisibleRows / Rows;
		const float ThumbTop = TrackTop + (TrackHeight - ThumbHeight) * BagScrollRow / FMath::Max(Rows - BagVisibleRows, 1);
		Paint.Box(FVector2f(X - 1.0f, ThumbTop), FVector2f(4.0f, ThumbHeight), WithAlpha(P.GoldBright, 0.8f), 43);
	}
}

void UBeyondInventoryWidget::PaintSets(const BeyondUI::FPainter& Paint) const
{
	using namespace BeyondUI;
	using namespace BeyondArmorLocal;
	const FPalette& P = Palette();
	Paint.Panel(SetsTopLeft, SetsSize, 20);
	const float Inner = SetsSize.X - 48.0f;
	float Y = Paint.SectionHeader(LOCTEXT("ArmorSets", "Armor Sets").ToString(), SetsTopLeft + FVector2f(24.0f, 16.0f), Inner, 22, 18.0f);

	// Every set in the item database; the one the card's item belongs to (else the most worn) opens up
	TArray<const UBeyondArmorSet*> Sets;
	if (const UBeyondItemDatabase* Database = UBeyondItemLibrary::GetItemDatabase())
	{
		for (const UBeyondItemDefinition* Item : Database->Items)
		{
			if (Item && Item->Set)
			{
				Sets.AddUnique(Item->Set);
			}
		}
	}
	const UBeyondEquipmentComponent* Equipment = bPartyView ? nullptr : GetShownEquipment();
	const UBeyondArmorSet* Open = nullptr;
	FBeyondItemInstance Detail;
	bool bWorn = false;
	if (GetDetailItem(Detail, bWorn) && Detail.GetDefinition())
	{
		Open = Detail.GetDefinition()->Set;
	}
	if (!Open && Equipment)
	{
		int32 Best = 0;
		for (const UBeyondArmorSet* Set : Sets)
		{
			if (Equipment->GetSetPieceCount(Set) > Best)
			{
				Best = Equipment->GetSetPieceCount(Set);
				Open = Set;
			}
		}
	}

	const float Left = SetsTopLeft.X + 24.0f;
	for (const UBeyondArmorSet* Set : Sets)
	{
		const int32 Pieces = Equipment ? Equipment->GetSetPieceCount(Set) : 0;
		const bool bOpen = Set == Open;
		const float RowHeight = bOpen ? 118.0f : 40.0f;
		if (Y + RowHeight > SetsTopLeft.Y + SetsSize.Y - 8.0f)
		{
			break;
		}
		if (bOpen)
		{
			Paint.GradientH(FVector2f(Left - 6.0f, Y), FVector2f(Inner + 12.0f, RowHeight), WithAlpha(Set->Color, 0.16f), WithAlpha(Set->Color, 0.02f), 22);
			Paint.RectOutline(FVector2f(Left - 6.0f, Y), FVector2f(Inner + 12.0f, RowHeight), WithAlpha(P.Gold, 0.4f), 1.0f, 23);
			Paint.Diamond(FVector2f(Left - 6.0f, Y + 20.0f), 5.0f, 7.0f, P.GoldBright, 24);
		}
		if (UTexture2D* SetEmblem = LoadTexture(Set->Emblem))
		{
			Paint.ImageCentred(SetEmblem, FVector2f(Left + 18.0f, Y + 20.0f), FVector2f(38.0f, 38.0f), FLinearColor::White, 24);
		}
		else
		{
			Paint.Sparkle(FVector2f(Left + 18.0f, Y + 20.0f), 13.0f, Set->Color, 24);
		}
		Paint.Text(Set->SetName.ToString(), Font(EBeyondFont::Body, 21.0f), FVector2f(Left + 46.0f, Y + 6.0f), Pieces > 0 ? P.Text : P.TextDim, 0.0f, 25);
		Paint.Text(FString::Printf(TEXT("%d / %d"), Pieces, UBeyondArmorSet::FullSetPieces), Font(EBeyondFont::Number, 18.0f), FVector2f(Left + Inner, Y + 8.0f),
			Pieces >= UBeyondArmorSet::PartialSetPieces ? P.GoldBright : P.Text, 1.0f, 25);
		if (bOpen)
		{
			TArray<FString> TwoPiece;
			for (const FBeyondItemStat& Stat : Set->TwoPieceStats)
			{
				TwoPiece.Add(UBeyondItemLibrary::FormatStat(Stat).ToString());
			}
			const FSlateFontInfo LineFont = Font(EBeyondFont::Body, 17.0f);
			Paint.Text(FString::Printf(TEXT("2 pieces: %s"), *FString::Join(TwoPiece, TEXT(", "))), LineFont, FVector2f(Left + 46.0f, Y + 38.0f),
				Pieces >= UBeyondArmorSet::PartialSetPieces ? P.Plus : P.TextDim, 0.0f, 25);
			Paint.Paragraph(FString::Printf(TEXT("4 pieces: %s"), *Set->FullSetDescription.ToString()), LineFont, FVector2f(Left + 46.0f, Y + 62.0f), Inner - 50.0f,
				21.0f, Pieces >= UBeyondArmorSet::FullSetPieces ? P.Plus : P.TextDim, 25, 2);
		}
		else
		{
			Paint.Separator(FVector2f(Left, Y + RowHeight - 1.0f), Inner, 22, 0.3f);
		}
		Y += RowHeight + 6.0f;
	}
}

void UBeyondInventoryWidget::PaintCard(const BeyondUI::FPainter& Paint) const
{
	using namespace BeyondUI;
	using namespace BeyondArmorLocal;
	const FPalette& P = Palette();
	Paint.Panel(CardTopLeft, CardSize, 40);

	FBeyondItemInstance Item;
	bool bWorn = false;
	const UBeyondItemDefinition* Definition = GetDetailItem(Item, bWorn) ? Item.GetDefinition() : nullptr;
	if (!Definition)
	{
		Paint.Text(LOCTEXT("PickItem", "Select a piece of gear to see it here").ToString(), Font(EBeyondFont::BodyItalic, 20.0f),
			CardTopLeft + CardSize * 0.5f - FVector2f(0.0f, 12.0f), P.TextDim, 0.5f, 42);
		return;
	}
	const FLinearColor Tier = TierColor(Item.Tier);

	// Art on the left
	const FVector2f ArtCentre = CardTopLeft + FVector2f(170.0f, CardSize.Y * 0.5f);
	Paint.Glow(ArtCentre, 170.0f, WithAlpha(Tier, 0.3f), 41);
	Paint.RuneCircle(ArtCentre, 128.0f, WithAlpha(Tier, 0.35f), 41);
	if (Definition->Icon)
	{
		Paint.ImageCentred(Definition->Icon, ArtCentre, FVector2f(270.0f, 270.0f), FLinearColor::White, 44);
	}
	else
	{
		Paint.Glyph(GetItemGlyph(Definition), ArtCentre, 170.0f, FMath::Lerp(Tier, FLinearColor::White, 0.3f), 44, 3.0f);
	}
	Paint.Line(CardTopLeft + FVector2f(340.0f, 24.0f), CardTopLeft + FVector2f(340.0f, CardSize.Y - 24.0f), WithAlpha(P.Gold, 0.25f), 1.0f, 42);

	// Name, tier and slot, level
	const float Left = CardTopLeft.X + 364.0f;
	const float Width = CardSize.X - 364.0f - 26.0f;
	float Y = CardTopLeft.Y + 18.0f;
	const FString Name = UBeyondItemLibrary::GetItemName(Item).ToString();
	Paint.Text(Item.UpgradeLevel > 0 ? FString::Printf(TEXT("%s  +%d"), *Name, Item.UpgradeLevel) : Name, Font(EBeyondFont::Display, 27.0f), FVector2f(Left, Y),
		FMath::Lerp(Tier, FLinearColor::White, 0.15f), 0.0f, 44);
	const FString LevelText = FString::Printf(TEXT("Lv %d"), Item.ItemLevel);
	const FSlateFontInfo LevelFont = Font(EBeyondFont::Number, 22.0f);
	const FVector2f LevelBox(MeasureText(LevelText, LevelFont).X + 24.0f, 38.0f);
	const FVector2f LevelTopLeft(Left + Width - LevelBox.X, Y);
	Paint.Box(LevelTopLeft, LevelBox, FromHex(0x0A0F1C, 0.9f), 43);
	Paint.RectOutline(LevelTopLeft, LevelBox, WithAlpha(P.GoldBright, 0.7f), 1.0f, 44);
	Paint.Text(LevelText, LevelFont, LevelTopLeft + FVector2f(LevelBox.X * 0.5f, 5.0f), P.Text, 0.5f, 45);
	Y += 40.0f;
	FString TierLine = FString::Printf(TEXT("%s %s"), *UBeyondItemLibrary::GetTierName(Item.Tier).ToString(), *UBeyondItemLibrary::GetSlotName(Definition->Slot).ToString());
	TierLine += bWorn ? TEXT("  ·  worn") : TEXT("");
	TierLine += Item.bLocked ? TEXT("  ·  locked") : Item.bJunk ? TEXT("  ·  junk") : TEXT("");
	Paint.Text(TierLine, Font(EBeyondFont::BodyItalic, 19.0f), FVector2f(Left, Y), WithAlpha(Tier, 0.9f), 0.0f, 44);
	Y += 32.0f;
	Paint.Separator(FVector2f(Left, Y), Width, 43, 0.5f);
	Y += 10.0f;

	// Stats, compared with what is worn in that slot
	const UBeyondEquipmentComponent* Equipment = bPartyView ? nullptr : GetShownEquipment();
	FBeyondItemInstance Worn;
	const bool bCompareNow = bCompare && !bWorn && Equipment && Equipment->GetEquipped(Definition->Slot, Worn);
	const FSlateFontInfo StatFont = Font(EBeyondFont::Body, 18.0f);
	const FSlateFontInfo StatValueFont = Font(EBeyondFont::Number, 18.0f);
	for (const FBeyondItemStat& Stat : UBeyondItemLibrary::GetItemStats(Item))
	{
		Paint.Glyph(BeyondMenuPanels::GetStatGlyph(Stat.Stat), FVector2f(Left + 10.0f, Y + 12.0f), 20.0f, P.Text, 44, 1.4f);
		Paint.Text(UBeyondSkillTreeAsset::GetStatName(Stat.Stat).ToString(), StatFont, FVector2f(Left + 34.0f, Y), P.Text, 0.0f, 44);
		Paint.Text(FString::Printf(TEXT("+%d"), FMath::RoundToInt(Stat.Value)), StatValueFont, FVector2f(Left + 300.0f, Y), P.Text, 1.0f, 44);
		if (bCompareNow)
		{
			const float Delta = Stat.Value - UBeyondItemLibrary::GetItemStat(Worn, Stat.Stat);
			if (!FMath::IsNearlyZero(Delta))
			{
				Paint.Text(FString::Printf(TEXT("%s%d"), Delta > 0.0f ? TEXT("+") : TEXT(""), FMath::RoundToInt(Delta)), StatFont, FVector2f(Left + 318.0f, Y),
					Delta > 0.0f ? P.Plus : P.Minus, 0.0f, 44);
			}
		}
		Y += 25.0f;
	}

	// The set this piece belongs to
	if (const UBeyondArmorSet* Set = Definition->Set)
	{
		const int32 Pieces = Equipment ? Equipment->GetSetPieceCount(Set) : 0;
		Y += 6.0f;
		if (UTexture2D* SetEmblem = LoadTexture(Set->Emblem))
		{
			Paint.ImageCentred(SetEmblem, FVector2f(Left + 10.0f, Y + 12.0f), FVector2f(28.0f, 28.0f), FLinearColor::White, 44);
		}
		else
		{
			Paint.Sparkle(FVector2f(Left + 10.0f, Y + 12.0f), 11.0f, Set->Color, 44);
		}
		Paint.Text(FString::Printf(TEXT("%s  (%d / %d)"), *Set->SetName.ToString(), Pieces, UBeyondArmorSet::FullSetPieces), Font(EBeyondFont::BodyBold, 18.0f),
			FVector2f(Left + 32.0f, Y), Set->Color, 0.0f, 44);
		Y += 25.0f;
		if (bDetails)
		{
			TArray<FString> TwoPiece;
			for (const FBeyondItemStat& Stat : Set->TwoPieceStats)
			{
				TwoPiece.Add(UBeyondItemLibrary::FormatStat(Stat).ToString());
			}
			Paint.Text(FString::Printf(TEXT("2 pieces: %s"), *FString::Join(TwoPiece, TEXT(", "))), Font(EBeyondFont::Body, 16.0f), FVector2f(Left + 32.0f, Y),
				Pieces >= UBeyondArmorSet::PartialSetPieces ? P.Plus : P.TextDim, 0.0f, 44);
			Y += 21.0f;
			Y += Paint.Paragraph(FString::Printf(TEXT("4 pieces: %s"), *Set->FullSetDescription.ToString()), Font(EBeyondFont::Body, 16.0f), FVector2f(Left + 32.0f, Y),
				Width - 32.0f, 20.0f, Pieces >= UBeyondArmorSet::FullSetPieces ? P.Plus : P.TextDim, 44, 2);
		}
	}

	// Flavour text, if there is room above the prices
	const float ButtonTop = CardTopLeft.Y + CardSize.Y - 58.0f;
	const float PriceTop = ButtonTop - 54.0f;
	if (!Definition->Description.IsEmpty() && Y < PriceTop - 30.0f)
	{
		Paint.Paragraph(Definition->Description.ToString(), Font(EBeyondFont::BodyItalic, 17.0f), FVector2f(Left, FMath::Max(Y + 6.0f, PriceTop - 48.0f)), Width, 21.0f,
			P.TextDim, 44, 2);
	}

	// What the next upgrade costs, and what salvaging it would give
	const UBeyondInventoryComponent* Inventory = GetInventory();
	const FSlateFontInfo PriceFont = Font(EBeyondFont::Body, 17.0f);
	Paint.Glyph(EBeyondGlyph::Up, FVector2f(Left + 9.0f, PriceTop + 11.0f), 16.0f, P.GoldBright, 44, 1.6f);
	if (Item.UpgradeLevel >= UBeyondItemLibrary::GetMaxUpgradeLevel())
	{
		Paint.Text(FText::Format(LOCTEXT("FullyUpgraded", "Fully upgraded (+{0})"), FText::AsNumber(Item.UpgradeLevel)).ToString(), PriceFont,
			FVector2f(Left + 26.0f, PriceTop), P.GoldBright, 0.0f, 44);
	}
	else
	{
		const FBeyondCurrencyBundle Cost = UBeyondItemLibrary::GetUpgradeCost(Item);
		Paint.Text(FText::Format(LOCTEXT("UpgradeCost", "Upgrade to +{0}: {1}"), FText::AsNumber(Item.UpgradeLevel + 1), UBeyondItemLibrary::FormatCurrencies(Cost)).ToString(),
			PriceFont, FVector2f(Left + 26.0f, PriceTop), Inventory && Inventory->CanAfford(Cost) ? P.Text : P.Minus, 0.0f, 44);
	}
	if (!bWorn)
	{
		const FString SalvageLine = Item.bLocked ? LOCTEXT("LockedNoSalvage", "Locked: it won't be salvaged").ToString()
			: FText::Format(LOCTEXT("SalvageYield", "Salvage: {0}"), UBeyondItemLibrary::FormatCurrencies(UBeyondItemLibrary::GetSalvageYield(Item))).ToString();
		Paint.Glyph(Item.bLocked ? EBeyondGlyph::Lock : EBeyondGlyph::Coin, FVector2f(Left + 9.0f, PriceTop + 33.0f), 16.0f, P.TextDim, 44, 1.4f);
		Paint.Text(SalvageLine, PriceFont, FVector2f(Left + 26.0f, PriceTop + 22.0f), P.TextDim, 0.0f, 44);
	}

	// What F does with it, and the other keys, as buttons
	FText Reason;
	FText Action = bWorn ? LOCTEXT("UnequipButton", "Unequip") : LOCTEXT("EquipButton", "Equip");
	if (!bWorn && Equipment && !Equipment->CanEquip(Item, Reason))
	{
		Action = Reason;
	}
	float X = Left;
	const TPair<FString, FText> Buttons[] = { { TEXT("F"), Action }, { TEXT("U"), LOCTEXT("UpgradeButton", "Upgrade") },
		{ TEXT("L"), Item.bLocked ? LOCTEXT("UnlockButton", "Unlock") : LOCTEXT("LockButton", "Lock") } };
	for (const TPair<FString, FText>& Button : Buttons)
	{
		const FSlateFontInfo ButtonFont = Font(EBeyondFont::Body, 17.0f);
		const float ButtonWidth = 64.0f + MeasureText(Button.Value.ToString(), ButtonFont).X;
		Paint.GradientV(FVector2f(X, ButtonTop), FVector2f(ButtonWidth, 40.0f), FromHex(0x161E33, 0.92f), FromHex(0x0A0F1C, 0.92f), 43);
		// Holding U fills the upgrade button
		if (Button.Key == TEXT("U") && HoldAction == EArmorHold::Upgrade && HoldItem == Item.Id && HoldDuration > 0.0f)
		{
			const float Fill = FMath::Clamp(HoldTime / HoldDuration, 0.0f, 1.0f);
			Paint.GradientH(FVector2f(X, ButtonTop), FVector2f(ButtonWidth * Fill, 40.0f), WithAlpha(P.Gold, 0.55f), WithAlpha(P.GoldBright, 0.75f), 44);
		}
		Paint.RectOutline(FVector2f(X, ButtonTop), FVector2f(ButtonWidth, 40.0f), WithAlpha(P.Silver, 0.5f), 1.0f, 44);
		Paint.KeyHint(Button.Key, Button.Value.ToString(), FVector2f(X + 10.0f, ButtonTop + 6.0f), 45);
		X += ButtonWidth + 12.0f;
	}
}

#undef LOCTEXT_NAMESPACE
