// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "UI/BeyondUIGlyphs.h"
#include "UI/BeyondUIStyle.h"
#include "UI/BeyondUITypes.h"
#include "BeyondMenuWidget.generated.h"

class ABeyondCharacterBase;
class ABeyondPlayerController;
class UBeyondCharacterProfile;
class USoundBase;
class UTexture2D;

/** A tab in the menu header: a demigod, the duo, or a filter */
struct FBeyondMenuTab
{
	FText Title;
	TWeakObjectPtr<ABeyondCharacterBase> Character;
	bool bDuo = false;
	FLinearColor Accent = FLinearColor(0.06f, 0.32f, 1.0f);
};

/** A key and what it does, in the bar along the bottom (clicking it presses the key) */
struct FBeyondKeyHint
{
	FString Cap;
	FText Label;
	FKey Key;
};

/** The figures at the top right (skill points, resources) */
struct FBeyondMenuCounter
{
	struct FValue
	{
		UTexture2D* Icon = nullptr;
		EBeyondGlyph Glyph = EBeyondGlyph::None;
		FLinearColor Tint = FLinearColor::White;
		FString Text;
	};
	FText Label;
	TArray<FValue> Values;
};

namespace BeyondMenuLayout
{
	// The page in design units (BeyondUI::FDesignSpace): header, sidebar, content and the key bar
	constexpr float HeaderTabTop = 30.0f;
	constexpr float HeaderTabWidth = 236.0f;
	constexpr float HeaderTabHeight = 50.0f;
	constexpr float HeaderTabGap = 20.0f;
	constexpr float SidebarLeft = 36.0f;
	constexpr float SidebarWidth = 262.0f;
	constexpr float SidebarTop = 150.0f;
	constexpr float SidebarEntryHeight = 62.0f;
	constexpr float SidebarGap = 8.0f;
	constexpr float ContentLeft = 320.0f;
	constexpr float ContentTop = 118.0f;
	constexpr float ContentRight = 1890.0f;
	constexpr float ContentBottom = 990.0f;
	constexpr float KeyBarTop = 1012.0f;
}

/**
 * The character menu's shared frame (Plan 6), drawn in C++ like the screens before it: the backdrop, the CHARACTER
 * title, the ANGEL / JI-WOONG / DUO tabs (Q / E, 1-3), the sidebar (Skill Tree, Armor, Overview, Quests), the counter at
 * the top right, the key bar and the message line. Each page is its own menu (the player controller opens one at a time)
 * and paints its content through PaintPage. Everything is laid out on a 1920 x 1080 page scaled to the screen.
 * Keys every page shares: Esc closes, K / I / C / J / M switch page, Q / E / 1-3 pick the tab.
 */
UCLASS(Abstract)
class WORLDBEYOND_API UBeyondMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBeyondMenuWidget(const FObjectInitializer& ObjectInitializer);

	// Which page this is
	virtual EBeyondMenuPage GetPage() const { return EBeyondMenuPage::Overview; }

	// The header tabs as last built (demigods in duo order, then the duo; filters on the quest log)
	UFUNCTION(BlueprintPure, Category = "Menu")
	int32 GetHeaderTabCount() const { return HeaderTabs.Num(); }

	UFUNCTION(BlueprintPure, Category = "Menu")
	int32 GetActiveHeaderTab() const;

	UFUNCTION(BlueprintPure, Category = "Menu")
	FText GetHeaderTabTitle(int32 Index) const;

	// Picks a header tab the way clicking it does
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void SelectHeaderTab(int32 Index);

	// The header tab showing this demigod (the duo tab for null); INDEX_NONE if none
	int32 FindHeaderTabFor(const AActor* Character) const;

	// The character tab to carry to another page (the last one picked when this page has filters instead)
	int32 GetNavigationTab() const;

	bool HasCharacterTabs() const { return UsesCharacterTabs(); }

	// The last feedback line
	UFUNCTION(BlueprintPure, Category = "Menu")
	FText GetMessage() const { return Message; }

	// The key bar's keys on this page right now
	TArray<FBeyondKeyHint> GetKeyHints() const;

	// Called by the player controller each time the menu is shown: rebuilds the tabs and replays the fade-in
	virtual void NotifyOpened();

	// Advance message / fade / page timers (NativeTick calls it; tests call it directly)
	virtual void AdvanceAnimation(float DeltaSeconds);

	// Presses a key as the keyboard does (tests, key bar clicks)
	bool PressKey(const FKey& Key);

	// Design-space hit test helpers (tests): what is under a point of the 1920 x 1080 page
	int32 HitTestHeaderTab(const FVector2f& Design) const;
	int32 HitTestSidebar(const FVector2f& Design) const;

	// The sidebar's pages in order
	static const TArray<EBeyondMenuPage>& GetSidebarPages();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	//~ What pages fill in

	// The page's content (the frame is already drawn); layers 20+ are free
	virtual void PaintPage(BeyondUI::FPainter& Paint) const {}
	// The header tabs (default: the party in duo order, then the duo)
	virtual void BuildHeaderTabs(TArray<FBeyondMenuTab>& OutTabs) const;
	// The page's tab right now / pick one (called with a valid index)
	virtual int32 GetPageTab() const { return PageHeaderTab; }
	virtual void OnHeaderTabSelected(int32 Index) { PageHeaderTab = Index; }
	virtual void GetPageKeyHints(TArray<FBeyondKeyHint>& OutHints) const {}
	virtual bool GetCounter(FBeyondMenuCounter& OutCounter) const { return false; }
	// The title beside the emblem ("Character")
	virtual FText GetTitle() const;
	virtual bool ShowsSidebar() const { return true; }
	virtual bool ShowsHeaderTabs() const { return true; }
	// The header tabs are Angel / Ji-Woong / Duo (false: filters or none, e.g. the quest log and the pause menu)
	virtual bool UsesCharacterTabs() const { return true; }
	// Keys and mouse on the page (Design: the cursor on the 1920 x 1080 page); true when handled
	virtual bool HandlePageKey(const FKey& Key) { return false; }
	virtual bool HandlePageKeyUp(const FKey& Key) { return false; }
	virtual bool HandlePageClick(const FVector2f& Design, const FKey& Button) { return false; }
	virtual bool HandlePageMouseUp(const FVector2f& Design, const FKey& Button) { return false; }
	virtual void HandlePageHover(const FVector2f& Design) {}
	virtual bool HandlePageWheel(const FVector2f& Design, float Delta) { return false; }
	virtual void AdvancePage(float DeltaSeconds) {}
	// Closes this menu through the player controller
	virtual void CloseMenu();

	//~ Helpers for pages

	ABeyondPlayerController* GetBeyondPlayerController() const;
	// The party, Conduit (Angel) first, then the Striker (Ji-Woong), then anyone else
	TArray<ABeyondCharacterBase*> GetPartyInDuoOrder() const;
	// The demigod of a header tab (null for the duo or a filter)
	ABeyondCharacterBase* GetHeaderCharacter(int32 Index) const;
	bool IsDuoHeaderTab(int32 Index) const;
	const UBeyondCharacterProfile* GetProfileFor(int32 HeaderTab) const;
	FLinearColor GetAccent() const;
	void ShowMessage(const FText& Text, bool bWarning = false);
	void PlayUISound(const TSoftObjectPtr<USoundBase>& Sound, float Volume = 0.6f) const;
	void PlaySelectSound() const;
	void PlayDeniedSound() const;
	UTexture2D* LoadTexture(const TSoftObjectPtr<UTexture2D>& Texture) const;
	// The last design-space size and cursor
	BeyondUI::FDesignSpace GetDesignSpace() const { return BeyondUI::FDesignSpace::Fit(LastLocalSize); }
	FVector2f GetCursor() const { return CursorDesign; }
	bool IsCursorIn(const FVector2f& TopLeft, const FVector2f& Size) const;

	TArray<FBeyondMenuTab> HeaderTabs;
	int32 PageHeaderTab = 0;
	float AnimTime = 0.0f;
	float OpenAge = 10.0f;

	FText Message;
	bool bMessageWarning = false;
	float MessageAge = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu|Sounds")
	TSoftObjectPtr<USoundBase> DeniedSound;

	// Size the last mouse event or tick saw (hit tests use the same layout as painting)
	FVector2f LastLocalSize = FVector2f(1920.0f, 1080.0f);
	FVector2f CursorDesign = FVector2f(-1000.0f, -1000.0f);

private:
	void PaintFrame(BeyondUI::FPainter& Paint, const BeyondUI::FDesignSpace& Space) const;
	void RebuildHeaderTabs();
	void CycleHeaderTab(int32 Direction);
	bool HandleFrameKey(const FKey& Key);
	FVector2f GetHeaderTabTopLeft(int32 Index) const;
	FVector2f ToDesign(const FGeometry& Geometry, const FVector2D& ScreenPosition);

	int32 HoveredHeaderTab = INDEX_NONE;
	int32 HoveredSidebar = INDEX_NONE;
	int32 HoveredKeyHint = INDEX_NONE;
	// Key bar hit boxes from the last paint
	mutable TArray<TPair<FBox2f, FKey>> KeyHintBoxes;
};
