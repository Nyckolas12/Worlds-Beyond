// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondRegionBannerWidget.generated.h"

/**
 * The open world's place names (Plan 5), drawn in C++: the region / village banner (FF7 Rebirth style: a small line
 * over the name, thin rules either side, the level band and "Discovered + EXP" the first time) and short toasts on the
 * left ("Waystone attuned", "Discovered: Hollowroot Cave"). State is set by events and faded from timestamps while it
 * paints, so it works without ticking (-nullrhi tests read it).
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondRegionBannerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Banner")
	void ShowRegion(const FText& Title, const FText& Subtitle, const FText& Detail, const FText& Reward, FLinearColor Accent);

	UFUNCTION(BlueprintCallable, Category = "Banner")
	void ShowToast(const FText& Title, const FText& Detail, FLinearColor Colour);

	UFUNCTION(BlueprintPure, Category = "Banner")
	bool IsBannerShowing() const;

	UFUNCTION(BlueprintPure, Category = "Banner")
	FText GetBannerTitle() const { return BannerTitle; }

	UFUNCTION(BlueprintPure, Category = "Banner")
	FText GetBannerReward() const { return BannerReward; }

	// Toasts still on screen, newest first
	UFUNCTION(BlueprintPure, Category = "Banner")
	TArray<FText> GetToastTitles() const;

	// Seconds the banner stays (after fading in)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banner", meta = (ClampMin = "0.5"))
	float HoldTime = 3.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banner", meta = (ClampMin = "0.5"))
	float ToastDuration = 4.0f;

	// Banner centre: fraction of the screen width, fraction of the height
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banner|Style")
	FVector2D BannerPosition = FVector2D(0.5f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banner|Style")
	FLinearColor TextColor = FLinearColor(0.97f, 0.96f, 0.92f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banner|Style")
	FLinearColor GoldColor = FLinearColor(1.0f, 0.78f, 0.3f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banner|Style")
	FLinearColor PanelColor = FLinearColor(0.02f, 0.03f, 0.06f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banner|Style")
	FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);

protected:
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	static constexpr float FadeIn = 0.6f;
	static constexpr float FadeOut = 0.9f;

	double GetNow() const;
	float GetBannerDuration() const { return FadeIn + HoldTime + FadeOut; }

	FText BannerTitle;
	FText BannerSubtitle;
	FText BannerDetail;
	FText BannerReward;
	FLinearColor BannerAccent = FLinearColor::White;
	double BannerStart = -1000.0;

	struct FToast
	{
		FText Title;
		FText Detail;
		FLinearColor Colour = FLinearColor::White;
		double Start = 0.0;
	};
	// Newest first
	TArray<FToast> Toasts;
};
