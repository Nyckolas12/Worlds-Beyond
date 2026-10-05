// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondBondMeterWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * The party's Bond meter (charges the duo super move). Builds its own widget tree in C++, so it works
 * without a Widget Blueprint; make a Blueprint child (or a new widget with a SetBond function) to restyle it.
 * Colour runs from Angel's blue through purple to Ji-Woong's gold as it fills.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondBondMeterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Bond")
	void SetBond(float Bond, float MaxBond);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FLinearColor EmptyColor = FLinearColor(0.10f, 0.35f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FLinearColor HalfColor = FLinearColor(0.55f, 0.15f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FLinearColor FullColor = FLinearColor(1.0f, 0.78f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FText ChargingText = NSLOCTEXT("Beyond", "BondCharging", "BOND");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FText ReadyText = NSLOCTEXT("Beyond", "BondReady", "HEAVEN'S JUDGMENT READY  [G]");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FVector2D Size = FVector2D(380.0f, 22.0f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// Called after SetBond, e.g. to play a "ready" animation in a Blueprint child
	UFUNCTION(BlueprintImplementableEvent, Category = "Bond")
	void OnBondUpdated(float FillPercent, bool bIsFull);

private:
	void ApplyToWidgets();

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;

	float Percent = 0.0f;
	bool bFull = false;
};
