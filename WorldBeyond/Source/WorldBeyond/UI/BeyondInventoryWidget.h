// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/BeyondItemTypes.h"
#include "Styling/SlateBrush.h"
#include "BeyondInventoryWidget.generated.h"

class ABeyondCharacterBase;
class UBeyondEquipmentComponent;
class UBeyondInventoryComponent;
class USoundBase;
class UTexture2D;

/**
 * The equipment & inventory screen (I), drawn in C++ in the same style as the skill tree (SkillTreeSystem pack art).
 * Left: the shown demigod's paper doll (Helm, Chest, Gauntlets, Boots, Weapon), their stats with the equipment's share
 * and the armor sets they wear (2 / 4 pieces). Right: the party's bag, sorted by slot then tier.
 * Mouse: hover for details (stats compared with what is worn), left-click a bag item to equip it on the shown demigod,
 * left-click a worn item to take it off. Keys: Q / E switch demigod, X twice discards the hovered bag item, I / Esc close.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBeyondInventoryWidget(const FObjectInitializer& ObjectInitializer);

	// Rebuilds the tabs from the party (call when opening)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshTabs();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SelectTab(int32 Index);

	// The tab of a demigod; 0 if not found
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 FindTabFor(const AActor* Character) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetTabCount() const { return Tabs.Num(); }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetActiveTab() const { return ActiveTab; }

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

	// The last feedback line ("Ji-Woong equips Gilded Saber")
	UFUNCTION(BlueprintPure, Category = "Inventory")
	FText GetMessage() const { return Message; }

	// The bag in display order: slot (Helm ... Weapon), then best tier first, then item level
	TArray<FBeyondItemInstance> GetSortedBagItems() const;

	// Advance message / flash timers (NativeTick calls it; tests call it directly)
	void AdvanceAnimation(float DeltaSeconds);

	//~ Art (SkillTreeSystem pack); anything missing falls back to plain shapes

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Art")
	TSoftObjectPtr<UTexture2D> BackgroundTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Art")
	TSoftObjectPtr<UTexture2D> SlotTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Art")
	TSoftObjectPtr<UTexture2D> GlowTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Art")
	TSoftObjectPtr<USoundBase> EquipSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Art")
	TSoftObjectPtr<USoundBase> DeniedSound;

	//~ Layout and colours

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Style", meta = (ClampMin = "40"))
	float SlotSize = 84.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Style", meta = (ClampMin = "32"))
	float CellSize = 64.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Style", meta = (ClampMin = "1"))
	int32 GridColumns = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Style")
	FLinearColor AccentColor = FLinearColor(0.10f, 0.45f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Style")
	FLinearColor GoldColor = FLinearColor(1.0f, 0.72f, 0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Style")
	FLinearColor PanelColor = FLinearColor(0.02f, 0.03f, 0.07f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Style")
	FLinearColor TextColor = FLinearColor(0.95f, 0.95f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Style")
	FLinearColor BetterColor = FLinearColor(0.35f, 0.95f, 0.4f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Style")
	FLinearColor WarningColor = FLinearColor(1.0f, 0.42f, 0.35f, 1.0f);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	struct FInventoryTab
	{
		FText Title;
		TWeakObjectPtr<ABeyondCharacterBase> Character;
	};

	// What the mouse is over
	struct FInventoryHit
	{
		int32 Tab = INDEX_NONE;
		bool bSlot = false;
		EBeyondItemSlot ItemSlot = EBeyondItemSlot::Helm;
		FGuid BagItem;
	};

	void LoadArt();
	UBeyondInventoryComponent* GetInventory() const;
	UBeyondEquipmentComponent* GetShownEquipment() const;
	const FSlateBrush* GetTextureBrush(UTexture2D* Texture) const;

	FVector2f GetTabCenter(int32 Index, const FVector2f& LocalSize) const;
	FVector2f GetDollCenter(const FVector2f& LocalSize) const;
	FVector2f GetSlotCenter(EBeyondItemSlot ItemSlot, const FVector2f& LocalSize) const;
	FVector2f GetGridTopLeft(const FVector2f& LocalSize) const;
	FVector2f GetCellTopLeft(int32 Index, const FVector2f& LocalSize) const;
	FInventoryHit HitTest(const FVector2f& Local, const FVector2f& LocalSize) const;

	// Hover details: name, tier, stats against what is worn, set bonuses, what a click does
	TArray<TPair<FString, FLinearColor>> DescribeItem(const FBeyondItemInstance& Item, bool bWorn) const;

	void ShowMessage(const FText& Text, bool bWarning = false);
	void PlayUISound(const TSoftObjectPtr<USoundBase>& Sound) const;
	void CycleTab(int32 Direction);
	void HandleDiscardKey();
	void Close();

	TArray<FInventoryTab> Tabs;
	int32 ActiveTab = 0;

	FInventoryHit Hovered;
	FVector2f HoverPosition = FVector2f::ZeroVector;

	FText Message;
	bool bMessageWarning = false;
	float MessageAge = -1.0f;
	FGuid DiscardArmedItem;
	float DiscardArmedAge = -1.0f;
	// A slot or bag item that just changed lights up briefly
	bool bFlashSlot = false;
	EBeyondItemSlot FlashSlot = EBeyondItemSlot::Helm;
	float FlashAge = -1.0f;
	float AnimTime = 0.0f;

	// Size the last mouse event saw (hit tests use the same layout as painting)
	FVector2f LastLocalSize = FVector2f(1920.0f, 1080.0f);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> LoadedTextures;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Background;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> SlotFrame;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Glow;

	FSlateBrush PillBrush;
	FSlateBrush PanelBrush;
	// Stable addresses: paint keeps brush pointers while it adds more
	mutable TMap<const UTexture2D*, TUniquePtr<FSlateBrush>> TextureBrushes;
};
