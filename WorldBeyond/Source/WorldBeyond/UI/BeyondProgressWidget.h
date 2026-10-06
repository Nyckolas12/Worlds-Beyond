// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/BeyondItemTypes.h"
#include "Styling/SlateBrush.h"
#include "BeyondProgressWidget.generated.h"

class ABeyondCharacterBase;
class UBeyondInventoryComponent;
class UBeyondPartyComponent;
class USoundBase;

/**
 * The leader's level badge and EXP bar (bottom left) and a "LEVEL UP" banner (top centre), drawn in C++ so no
 * Widget Blueprint is needed. ABeyondPlayerController stretches it over W_PlayerHud's canvas; it reads the leader
 * every frame and listens to the party for level-ups and EXP awards.
 * Loot: the "F  Item (Tier)" prompt when a drop is in reach, a stack of pickup toasts in tier colours (right side)
 * and short notices ("Your bag is full").
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBeyondProgressWidget(const FObjectInitializer& ObjectInitializer);

	// Shows the banner (merges with one that just started for the same level, e.g. both demigods at once)
	UFUNCTION(BlueprintCallable, Category = "Progress")
	void ShowLevelUpBanner(ABeyondCharacterBase* Member, int32 NewLevel);

	// "+25 EXP" over the bar
	UFUNCTION(BlueprintCallable, Category = "Progress")
	void ShowExperienceGain(float Amount);

	// "Venomweave Helm (Rare)" slides in on the right for a few seconds
	UFUNCTION(BlueprintCallable, Category = "Progress|Loot")
	void ShowLootToast(const FBeyondItemInstance& Item);

	// A short centred line (warnings like "Your bag is full")
	UFUNCTION(BlueprintCallable, Category = "Progress|Loot")
	void ShowNotice(const FText& Text);

	UFUNCTION(BlueprintPure, Category = "Progress|Loot")
	int32 GetToastCount() const { return Toasts.Num(); }

	// The pickup prompt being shown (empty when nothing is in reach)
	UFUNCTION(BlueprintPure, Category = "Progress|Loot")
	FString GetPickupPrompt() const { return PromptLabel; }

	UFUNCTION(BlueprintPure, Category = "Progress|Loot")
	FText GetNotice() const { return Notice; }

	// What the badge / bar are heading to (the leader's level and EXP fraction)
	UFUNCTION(BlueprintPure, Category = "Progress")
	int32 GetShownLevel() const { return TargetLevel; }

	UFUNCTION(BlueprintPure, Category = "Progress")
	float GetShownExperienceFraction() const { return TargetFraction; }

	UFUNCTION(BlueprintPure, Category = "Progress")
	bool IsBannerShowing() const { return BannerAge >= 0.0f && BannerAge < GetBannerDuration(); }

	UFUNCTION(BlueprintPure, Category = "Progress")
	FString GetBannerSubtitle() const { return BannerSubtitle; }

	// Advance the bar / banner animation (NativeTick calls it; tests call it directly)
	void AdvanceAnimation(float DeltaSeconds);

	// Where the badge sits, as a fraction of the screen, and the badge centre's offset from there (pixels)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Layout")
	FVector2D PanelAnchor = FVector2D(0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Layout")
	FVector2D BadgeOffset = FVector2D(78.0f, -70.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Layout", meta = (ClampMin = "24"))
	float BadgeSize = 64.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Layout", meta = (ClampMin = "40"))
	float BarWidth = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Layout", meta = (ClampMin = "2"))
	float BarHeight = 9.0f;

	// Banner centre: fraction of the screen width, pixels from the top
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Layout")
	FVector2D BannerPosition = FVector2D(0.5f, 150.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Style")
	FLinearColor GoldColor = FLinearColor(1.0f, 0.72f, 0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Style")
	FLinearColor BlueColor = FLinearColor(0.10f, 0.45f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Style")
	FLinearColor PanelColor = FLinearColor(0.02f, 0.03f, 0.08f, 0.82f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Style")
	FLinearColor TextColor = FLinearColor(0.95f, 0.95f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Style")
	FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.65f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Banner", meta = (ClampMin = "0.5"))
	float BannerHoldTime = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Banner")
	TObjectPtr<USoundBase> LevelUpSound;

	// Seconds a pickup toast stays, and how many show at once
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Loot", meta = (ClampMin = "0.5"))
	float ToastDuration = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Loot", meta = (ClampMin = "1"))
	int32 MaxToasts = 5;

	// Pickup prompt centre: fraction of the screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Loot")
	FVector2D PromptPosition = FVector2D(0.5f, 0.68f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress|Style")
	FLinearColor WarningColor = FLinearColor(1.0f, 0.42f, 0.35f, 1.0f);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UFUNCTION()
	void HandleMemberLevelUp(ABeyondCharacterBase* Member, int32 NewLevel);

	UFUNCTION()
	void HandleExperienceAwarded(ABeyondCharacterBase* Victim, float Experience);

	UFUNCTION()
	void HandleItemAdded(const FBeyondItemInstance& Item);

private:
	static constexpr float BannerFadeIn = 0.25f;
	static constexpr float BannerFadeOut = 0.6f;
	static constexpr float GainDuration = 1.4f;

	float GetBannerDuration() const { return BannerFadeIn + BannerHoldTime + BannerFadeOut; }
	void BindToParty();
	void ReadLeader();
	void ReadPickupTarget();

	TWeakObjectPtr<UBeyondPartyComponent> BoundParty;
	TWeakObjectPtr<UBeyondInventoryComponent> BoundInventory;
	TWeakObjectPtr<ABeyondCharacterBase> ShownLeader;

	// Pill / circle shape (rounded box with half-height corners)
	FSlateBrush PillBrush;

	FString LeaderName;
	int32 TargetLevel = 1;
	float TargetFraction = 0.0f;
	float Experience = 0.0f;
	float ExperienceNeeded = 0.0f;
	int32 SkillPoints = 0;

	// What the bar shows while it catches up (fills to the end and wraps on a level-up)
	int32 DisplayedLevel = 1;
	float DisplayedFraction = 0.0f;

	FString BannerSubtitle;
	TArray<FString> BannerNames;
	int32 BannerLevel = 0;
	float BannerAge = -1.0f;

	float GainAmount = 0.0f;
	float GainAge = -1.0f;
	float AnimTime = 0.0f;

	struct FLootToast
	{
		FString Label;
		FString Detail;
		FLinearColor Color = FLinearColor::White;
		float Age = 0.0f;
	};
	// Newest first
	TArray<FLootToast> Toasts;

	FString PromptLabel;
	FLinearColor PromptColor = FLinearColor::White;
	float PromptAge = 0.0f;

	FText Notice;
	float NoticeAge = -1.0f;
};
