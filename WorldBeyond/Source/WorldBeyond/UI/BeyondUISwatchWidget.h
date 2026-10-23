// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondUISwatchWidget.generated.h"

/**
 * Every piece of the shared UI look on one screen (Plan 6): fonts, panels, tabs, key caps, item frames per tier, bars,
 * the rune circle and the glyphs. Toggle it with the cheat Beyond.UI.Swatch to check the style in game.
 */
UCLASS()
class WORLDBEYOND_API UBeyondUISwatchWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Adds the swatch to the first local player's screen, or removes it when it shows
	static void Toggle(UWorld* World);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	float AnimTime = 0.0f;
};
