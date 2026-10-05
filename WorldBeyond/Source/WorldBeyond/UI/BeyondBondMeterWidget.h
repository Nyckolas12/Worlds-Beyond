// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondBondMeterWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;

/**
 * The party's Bond meter and the duo super move's slot: [duo icon + key] [meter].
 * Builds its own widget tree in C++, so it works without a Widget Blueprint; make a Blueprint child (or a new widget
 * with SetBond / SetDuoIcon functions) to restyle it. The meter runs from Angel's blue through purple to Ji-Woong's
 * gold; the icon is greyed out until the meter is full.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondBondMeterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Bond")
	void SetBond(float Bond, float MaxBond);

	// The duo ability's Icon (empty: Default Icon)
	UFUNCTION(BlueprintCallable, Category = "Bond")
	void SetDuoIcon(UTexture2D* Icon);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FLinearColor EmptyColor = FLinearColor(0.10f, 0.35f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FLinearColor HalfColor = FLinearColor(0.55f, 0.15f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FLinearColor FullColor = FLinearColor(1.0f, 0.78f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FText ChargingText = NSLOCTEXT("Beyond", "BondCharging", "BOND");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FText ReadyText = NSLOCTEXT("Beyond", "BondReady", "HEAVEN'S JUDGMENT READY");

	// Key shown on the duo icon
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FText KeyText = NSLOCTEXT("Beyond", "BondKey", "G");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FVector2D Size = FVector2D(380.0f, 22.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	float IconSize = 56.0f;

	// Shown when the duo ability has no Icon yet
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	TSoftObjectPtr<UTexture2D> DefaultIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/Blueprints/Widgets/Images/frameBackground.frameBackground")));

	// Icon tint while the meter is charging
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FLinearColor ChargingIconTint = FLinearColor(0.35f, 0.35f, 0.35f, 0.65f);

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

	UPROPERTY(Transient)
	TObjectPtr<UImage> IconImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> KeyLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DuoIcon;

	float Percent = 0.0f;
	bool bFull = false;
};
