// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondInteractPromptWidget.generated.h"

/**
 * "[F] Talk - Elder Maren" / "[F] Pick up": what F would do now, drawn in C++ (no Widget Blueprint), low in the
 * middle of the screen. ABeyondPlayerController sets it on a timer (widgets don't tick under -nullrhi).
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondInteractPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Empty Action hides the prompt
	UFUNCTION(BlueprintCallable, Category = "Prompt")
	void SetPrompt(const FText& InAction, const FText& InTarget);

	UFUNCTION(BlueprintPure, Category = "Prompt")
	FText GetAction() const { return Action; }

	UFUNCTION(BlueprintPure, Category = "Prompt")
	FText GetTarget() const { return Target; }

	UFUNCTION(BlueprintPure, Category = "Prompt")
	bool HasPrompt() const { return !Action.IsEmpty(); }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prompt")
	FText KeyLabel = NSLOCTEXT("BeyondPrompt", "Key", "F");

	// Height of the prompt's centre as a share of the screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prompt|Style", meta = (ClampMin = "0", ClampMax = "1"))
	float ScreenHeight = 0.68f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prompt|Style")
	int32 FontSize = 15;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prompt|Style")
	FLinearColor TextColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.95f);

	// Ji-Woong's gold on the key cap
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prompt|Style")
	FLinearColor KeyColor = FLinearColor(1.0f, 0.78f, 0.3f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prompt|Style")
	FLinearColor BackColor = FLinearColor(0.02f, 0.02f, 0.05f, 0.6f);

protected:
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	FText Action;
	FText Target;
};
