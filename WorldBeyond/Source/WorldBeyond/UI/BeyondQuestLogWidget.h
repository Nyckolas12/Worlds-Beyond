// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/BeyondMenuWidget.h"
#include "BeyondQuestLogWidget.generated.h"

/**
 * The character menu's Quests page (J): the main story's chapters and side quests on the left, the selected quest's
 * picture, summary, objectives and rewards on the right. The header tabs filter it (All, Main, Side). Until the quest
 * system is in, it lists the main chapters as sealed entries.
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

protected:
	virtual void BuildHeaderTabs(TArray<FBeyondMenuTab>& OutTabs) const override;
	virtual FText GetTitle() const override;
	virtual bool UsesCharacterTabs() const override { return false; }
	virtual void PaintPage(BeyondUI::FPainter& Paint) const override;
	virtual void GetPageKeyHints(TArray<FBeyondKeyHint>& OutHints) const override;
	virtual bool HandlePageClick(const FVector2f& Design, const FKey& Button) override;
	virtual bool HandlePageKey(const FKey& Key) override;
	virtual void HandlePageHover(const FVector2f& Design) override;

private:
	int32 HitTestEntry(const FVector2f& Design) const;
	int32 SelectedEntry = 0;
	int32 HoveredEntry = INDEX_NONE;
};
