// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondEnemyPlatesWidget.generated.h"

class ABeyondEnemyCharacter;

/**
 * Health plates over Plan 3 enemies (drawn in C++, full screen, never takes input): a bar, the level and, for elites,
 * the affix name in their tint. Shown for enemies in a fight or hit lately, within Plate Range of the camera
 * (Project Settings -> Worlds Beyond Enemies). Mini-bosses and bosses have the boss bar instead.
 * ABeyondPlayerController puts it on the viewport.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondEnemyPlatesWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plates|Style")
	FVector2D BarSize = FVector2D(86.0f, 7.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plates|Style")
	FLinearColor HealthColor = FLinearColor(0.85f, 0.12f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plates|Style")
	FLinearColor EliteColor = FLinearColor(1.0f, 0.62f, 0.15f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plates|Style")
	FLinearColor BackColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);

	// Enemies with a plate on screen right now (tests)
	UFUNCTION(BlueprintPure, Category = "Plates")
	TArray<ABeyondEnemyCharacter*> GetShownEnemies() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	struct FPlate
	{
		TWeakObjectPtr<ABeyondEnemyCharacter> Enemy;
		FVector2f Position = FVector2f::ZeroVector;
		float Health = 1.0f;
		// Trails the health bar down (the chunk a hit took)
		float Chip = 1.0f;
		int32 Level = 1;
		FString Label;
		FLinearColor LabelColor = FLinearColor::White;
		bool bElite = false;
		float Opacity = 1.0f;
	};

	TArray<FPlate> Plates;
};
