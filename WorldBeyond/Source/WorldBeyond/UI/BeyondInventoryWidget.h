// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/BeyondItemTypes.h"
#include "UI/BeyondMenuWidget.h"
#include "BeyondInventoryWidget.generated.h"

class ABeyondCharacterBase;
class UBeyondEquipmentComponent;
class UBeyondInventoryComponent;
class USoundBase;
class UTexture2D;

/** What the bag shows */
UENUM(BlueprintType)
enum class EBeyondBagFilter : uint8
{
	All,
	Helm,
	Chest,
	Gauntlets,
	Boots,
	Weapon,
	Ring,
	Amulet
};

/**
 * The character menu's Armor page (I). Angel / Ji-Woong: the demigod's gear on a rune ring around their silhouette
 * (Helmet, Weapon, Chest, Gauntlets, Boots, Accessory, Charm), level and combat stats with the gear's share, the party's
 * bag (filters, 5 wide, scrolls), the armor sets worn and the selected item's card. Duo: both demigods' loadouts side by
 * side over the same bag.
 * Mouse: click to select, double-click to equip / take off; wheel scrolls the bag. Keys: F equip / take off, R compare
 * with what is worn, T details, X twice discards; the frame's keys switch tab and page.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondInventoryWidget : public UBeyondMenuWidget
{
	GENERATED_BODY()

public:
	UBeyondInventoryWidget(const FObjectInitializer& ObjectInitializer);

	virtual EBeyondMenuPage GetPage() const override { return EBeyondMenuPage::Armor; }

	// Rebuilds the tabs from the party (call when opening)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshTabs();

	// A demigod's tab (0, 1); the duo's party view is the header's third tab
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SelectTab(int32 Index);

	// The tab of a demigod; 0 if not found
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 FindTabFor(const AActor* Character) const;

	// Demigods with equipment (the duo isn't one)
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetTabCount() const { return Tabs.Num(); }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetActiveTab() const { return ActiveTab; }

	// The Duo tab: both loadouts, no paper doll
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsPartyView() const { return bPartyView; }

	// The demigod whose equipment is shown
	UFUNCTION(BlueprintPure, Category = "Inventory")
	ABeyondCharacterBase* GetShownCharacter() const;

	// Equips a bag item on the shown demigod (what was worn goes to the bag); false and a message if it can't
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool EquipBagItem(FGuid ItemId);

	// Takes the shown demigod's item in ItemSlot off into the bag
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool UnequipSlot(EBeyondItemSlot ItemSlot);

	// Throws a bag item away for good (the X key asks twice first)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool DiscardBagItem(FGuid ItemId);

	// The bag in display order: slot (Helm ... Weapon), then best tier first, then item level
	TArray<FBeyondItemInstance> GetSortedBagItems() const;

	// The sorted bag through the current filter
	TArray<FBeyondItemInstance> GetFilteredBagItems() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetBagFilter(EBeyondBagFilter Filter);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	EBeyondBagFilter GetBagFilter() const { return BagFilter; }

	// Selects a bag item / a worn slot for the card
	void SelectBagItem(FGuid ItemId);
	void SelectSlot(EBeyondItemSlot ItemSlot);

	// The item the card shows (the one under the mouse, else the selected one); false if none
	bool GetDetailItem(FBeyondItemInstance& OutItem, bool& bOutWorn) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Art")
	TSoftObjectPtr<USoundBase> EquipSound;

protected:
	virtual void NativeConstruct() override;
	virtual void PaintPage(BeyondUI::FPainter& Paint) const override;
	virtual int32 GetPageTab() const override { return bPartyView ? Tabs.Num() : ActiveTab; }
	virtual void OnHeaderTabSelected(int32 Index) override;
	virtual void GetPageKeyHints(TArray<FBeyondKeyHint>& OutHints) const override;
	virtual bool GetCounter(FBeyondMenuCounter& OutCounter) const override;
	virtual bool HandlePageKey(const FKey& Key) override;
	virtual bool HandlePageClick(const FVector2f& Design, const FKey& Button) override;
	virtual void HandlePageHover(const FVector2f& Design) override;
	virtual bool HandlePageWheel(const FVector2f& Design, float Delta) override;
	virtual void AdvancePage(float DeltaSeconds) override;

private:
	struct FInventoryTab
	{
		FText Title;
		TWeakObjectPtr<ABeyondCharacterBase> Character;
	};

	// What the mouse is over
	struct FInventoryHit
	{
		bool bSlot = false;
		EBeyondItemSlot ItemSlot = EBeyondItemSlot::Helm;
		FGuid BagItem;
		int32 Filter = INDEX_NONE;
		// Party view: whose slot
		int32 Member = INDEX_NONE;
	};

	UBeyondInventoryComponent* GetInventory() const;
	UBeyondEquipmentComponent* GetShownEquipment() const;
	FInventoryHit HitTest(const FVector2f& Design) const;
	FVector2f GetCellCentre(int32 VisibleIndex) const;
	int32 GetBagRows() const;
	void ClampScroll();
	void ActivateSelection();
	void HandleDiscardKey();
	void ShowEquipResult(const FBeyondItemInstance& Item);

	void PaintPaperDoll(const BeyondUI::FPainter& Paint) const;
	void PaintPartyView(const BeyondUI::FPainter& Paint) const;
	void PaintStats(const BeyondUI::FPainter& Paint) const;
	void PaintBag(const BeyondUI::FPainter& Paint) const;
	void PaintSets(const BeyondUI::FPainter& Paint) const;
	void PaintCard(const BeyondUI::FPainter& Paint) const;
	void PaintItemTile(const BeyondUI::FPainter& Paint, const FBeyondItemInstance* Item, const FVector2f& Centre, float Size, EBeyondGlyph EmptyGlyph,
		bool bSelected, bool bHovered, bool bShowUpgrade, int32 Layer) const;

	TArray<FInventoryTab> Tabs;
	int32 ActiveTab = 0;
	bool bPartyView = false;
	EBeyondBagFilter BagFilter = EBeyondBagFilter::All;
	int32 BagScrollRow = 0;
	bool bCompare = true;
	bool bDetails = false;

	FInventoryHit Hovered;
	FGuid SelectedBagItem;
	bool bSlotSelected = false;
	EBeyondItemSlot SelectedSlot = EBeyondItemSlot::Helm;

	FGuid DiscardArmedItem;
	float DiscardArmedAge = -1.0f;
	// A slot that just changed lights up briefly
	bool bFlashSlot = false;
	EBeyondItemSlot FlashSlot = EBeyondItemSlot::Helm;
	float FlashAge = -1.0f;
	// Double-click
	double LastClickTime = -10.0;
	FGuid LastClickItem;
	bool bLastClickSlot = false;
};
