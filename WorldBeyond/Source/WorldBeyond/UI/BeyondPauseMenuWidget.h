// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/BeyondMenuWidget.h"
#include "BeyondPauseMenuWidget.generated.h"

/** What a pause menu entry does */
UENUM(BlueprintType)
enum class EBeyondPauseEntry : uint8
{
	Resume,
	Character,
	Quests,
	WorldMap,
	Settings,
	ReturnToTitle,
	Quit
};

/**
 * The pause menu (P, gamepad Start; Plan 6): a centred panel over the dimmed game with Resume, the character menu,
 * quests, the map, settings (to come), return to title and quit (those two ask twice). Up / Down and F / Enter, or the mouse.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondPauseMenuWidget : public UBeyondMenuWidget
{
	GENERATED_BODY()

public:
	virtual EBeyondMenuPage GetPage() const override { return EBeyondMenuPage::Pause; }

	UFUNCTION(BlueprintPure, Category = "Pause")
	int32 GetEntryCount() const;

	UFUNCTION(BlueprintPure, Category = "Pause")
	EBeyondPauseEntry GetSelectedEntry() const;

	// Runs an entry as pressing it does (Return to Title and Quit need a second press within a few seconds)
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void ActivateEntry(EBeyondPauseEntry Entry);

	// The level Return to Title opens
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pause")
	TSoftObjectPtr<UWorld> TitleLevel;

	UBeyondPauseMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual FText GetTitle() const override;
	virtual bool ShowsSidebar() const override { return false; }
	virtual bool ShowsHeaderTabs() const override { return false; }
	virtual bool UsesCharacterTabs() const override { return false; }
	virtual void BuildHeaderTabs(TArray<FBeyondMenuTab>& OutTabs) const override {}
	virtual void PaintPage(BeyondUI::FPainter& Paint) const override;
	virtual void GetPageKeyHints(TArray<FBeyondKeyHint>& OutHints) const override;
	virtual bool HandlePageKey(const FKey& Key) override;
	virtual bool HandlePageClick(const FVector2f& Design, const FKey& Button) override;
	virtual void HandlePageHover(const FVector2f& Design) override;
	virtual void AdvancePage(float DeltaSeconds) override;

private:
	int32 HitTestEntry(const FVector2f& Design) const;
	int32 Selected = 0;
	EBeyondPauseEntry ArmedEntry = EBeyondPauseEntry::Resume;
	float ArmedAge = -1.0f;
};
