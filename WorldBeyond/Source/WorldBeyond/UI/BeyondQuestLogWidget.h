// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/BeyondMenuWidget.h"
#include "BeyondQuestLogWidget.generated.h"

class UBeyondQuestComponent;
struct FBeyondQuest;

/**
 * The character menu's Quests page (J): the main story's chapters and the side quests the party has found on the left,
 * the selected quest's picture, summary, objectives with their progress and rewards on the right (Plan 6, from
 * UBeyondQuestComponent). The header tabs filter it (All, Main, Side). Keys: T tracks the selected quest on the HUD,
 * Up / Down and the wheel move through the list.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondQuestLogWidget : public UBeyondMenuWidget
{
	GENERATED_BODY()

public:
	virtual EBeyondMenuPage GetPage() const override { return EBeyondMenuPage::Quests; }

	// 0 all, 1 main story, 2 side quests
	UFUNCTION(BlueprintPure, Category = "Quests")
	int32 GetFilter() const { return PageHeaderTab; }

	// The list as shown: main chapters (sealed ones too), then the side quests that have started
	TArray<FName> GetListedQuests() const;

	// The selected quest (the first listed if the selection isn't listed)
	UFUNCTION(BlueprintPure, Category = "Quests")
	FName GetSelectedQuest() const;

	UFUNCTION(BlueprintCallable, Category = "Quests")
	void SelectQuest(FName QuestId);

	// T: follows the selected quest on the HUD (again: stops); false and a message if it isn't active
	UFUNCTION(BlueprintCallable, Category = "Quests")
	bool TrackSelectedQuest();

protected:
	virtual void BuildHeaderTabs(TArray<FBeyondMenuTab>& OutTabs) const override;
	virtual FText GetTitle() const override;
	virtual bool UsesCharacterTabs() const override { return false; }
	virtual void OnHeaderTabSelected(int32 Index) override;
	virtual void PaintPage(BeyondUI::FPainter& Paint) const override;
	virtual void GetPageKeyHints(TArray<FBeyondKeyHint>& OutHints) const override;
	virtual bool GetCounter(FBeyondMenuCounter& OutCounter) const override;
	virtual bool HandlePageClick(const FVector2f& Design, const FKey& Button) override;
	virtual bool HandlePageKey(const FKey& Key) override;
	virtual void HandlePageHover(const FVector2f& Design) override;
	virtual bool HandlePageWheel(const FVector2f& Design, float Delta) override;

private:
	UBeyondQuestComponent* GetQuests() const;
	// Index into the listed quests under the cursor (INDEX_NONE: none)
	int32 HitTestEntry(const FVector2f& Design) const;
	void ScrollToSelection();
	void PaintList(const BeyondUI::FPainter& Paint, const TArray<FName>& Listed) const;
	void PaintDetail(const BeyondUI::FPainter& Paint, const FBeyondQuest& Quest) const;

	FName SelectedQuest;
	int32 HoveredEntry = INDEX_NONE;
	int32 ScrollIndex = 0;
};
