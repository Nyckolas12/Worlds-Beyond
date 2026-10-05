// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondCrosshairWidget.generated.h"

/**
 * Screen-centre crosshair for casters (Angel): a dot, four ticks and a thin ring, drawn in C++ so it needs no
 * Widget Blueprint. Turns purple and tightens when an enemy is under it (that's who the spell will hit), opens a
 * little while running and closes while aiming. Shown by ABeyondPlayerController while the leader is combat ready.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Speed01: 0 standing .. 1 running
	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void SetAimState(bool bOverHostile, bool bAiming, float Speed01);

	UFUNCTION(BlueprintPure, Category = "Crosshair")
	bool IsOverHostile() const { return bHostile; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Style")
	FLinearColor IdleColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Style")
	FLinearColor HostileColor = FLinearColor(0.85f, 0.3f, 1.0f, 1.0f);

	// Dark outline under every shape so it reads on bright sky and lava alike
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Style")
	FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Style", meta = (ClampMin = "1"))
	float DotSize = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Style", meta = (ClampMin = "1"))
	float TickLength = 7.0f;

	// Distance from the centre to the inner end of each tick
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Style", meta = (ClampMin = "0"))
	float Gap = 8.0f;

	// Extra gap at full running speed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Style", meta = (ClampMin = "0"))
	float MovingGap = 6.0f;

	// Gap multiplier while aiming and over an enemy
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Style", meta = (ClampMin = "0.1", ClampMax = "1"))
	float FocusedGapScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Style", meta = (ClampMin = "0"))
	float RingRadius = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Style", meta = (ClampMin = "0.5"))
	float Thickness = 2.0f;

	// How fast gap and colour follow the state
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Style", meta = (ClampMin = "0.1"))
	float BlendSpeed = 14.0f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	bool bHostile = false;
	bool bAimingNow = false;
	float Speed = 0.0f;

	// Smoothed: 0 idle .. 1 over an enemy, and the current tick gap
	float HostileBlend = 0.0f;
	float CurrentGap = 8.0f;
};
