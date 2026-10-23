// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/BeyondMenuPanels.h"
#include "UI/BeyondMenuWidget.h"
#include "BeyondOverviewWidget.generated.h"

class UTexture2D;

/** Everything the Overview page shows, gathered in one place (paint and tests read the same numbers) */
struct FBeyondOverviewModel
{
	struct FPartyCard
	{
		FText Name;
		int32 Level = 1;
		UTexture2D* Portrait = nullptr;
		bool bDuo = false;
	};
	struct FLine
	{
		EBeyondGlyph Glyph = EBeyondGlyph::None;
		UTexture2D* Icon = nullptr;
		FText Title;
		FText Text;
		FString Value;
	};

	TArray<FPartyCard> Party;
	FText Name;
	FText ClassTitle;
	FText Motto;
	EBeyondElement Element = EBeyondElement::None;
	FBeyondLevelInfo Level;
	float Health = 0.0f;
	float MaxHealth = 0.0f;
	float Stamina = 0.0f;
	float MaxStamina = 0.0f;
	TArray<FBeyondStatRow> Stats;
	// The four region elements around the demigod's own (percent bonus against their foes)
	TArray<TPair<EBeyondElement, float>> Affinity;
	TArray<FLine> Passives;

	int32 BondLevel = 0;
	float BondExperience = 0.0f;
	float BondExperienceToNext = 0.0f;
	TArray<FText> BondPerks;
	TArray<FLine> DuoAbilities;
	TArray<FLine> Resources;
	// Weapon, Helm, Chest, Gauntlets, Boots (each may be empty)
	TArray<TPair<EBeyondItemSlot, TOptional<FBeyondItemInstance>>> Loadout;

	FText QuestKind;
	FText QuestTitle;
	FText QuestObjective;
	FText QuestChapter;
	UTexture2D* QuestThumbnail = nullptr;
	int32 ChapterSteps = 0;
	int32 ChapterStepsDone = 0;

	TArray<FLine> Records;
};

/**
 * The character menu's Overview page (C): party cards, the shown demigod's class, level, health and stamina, main stats,
 * elemental affinity and active passives; the Bond, duo abilities and resources on the right; loadout, quest progress
 * and the playtime record along the bottom. The Duo tab shows the pair. Panels for systems still to come say so.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondOverviewWidget : public UBeyondMenuWidget
{
	GENERATED_BODY()

public:
	virtual EBeyondMenuPage GetPage() const override { return EBeyondMenuPage::Overview; }

	// The numbers on the page for the active tab
	FBeyondOverviewModel BuildModel() const;

protected:
	virtual void PaintPage(BeyondUI::FPainter& Paint) const override;
	virtual void GetPageKeyHints(TArray<FBeyondKeyHint>& OutHints) const override;
	virtual bool GetCounter(FBeyondMenuCounter& OutCounter) const override;
	virtual bool HandlePageClick(const FVector2f& Design, const FKey& Button) override;
	virtual void HandlePageHover(const FVector2f& Design) override;

private:
	int32 HitTestPartyCard(const FVector2f& Design) const;
	int32 HoveredCard = INDEX_NONE;
};
