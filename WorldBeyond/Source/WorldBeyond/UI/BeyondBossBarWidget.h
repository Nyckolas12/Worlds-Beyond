// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondBossBarWidget.generated.h"

class ABeyondCharacterBase;

/**
 * The boss bar (Plan 3B), drawn in C++ at the top of the screen: name and title, health with a trailing chip of the
 * last hits, notches where the next phases start, a shimmer while the boss can't be hurt (phase change), and the
 * twist's announcement under it. Up to two bars stack (a boss and its mini-boss partner). ABeyondPlayerController
 * shows it for mini-bosses and bosses near the party.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondBossBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// The bosses to show, nearest first (two at most)
	void SetBosses(const TArray<ABeyondCharacterBase*>& Bosses);

	UFUNCTION(BlueprintPure, Category = "Boss Bar")
	TArray<ABeyondCharacterBase*> GetBosses() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss Bar|Style")
	float BarWidth = 760.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss Bar|Style")
	float BarHeight = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss Bar|Style")
	float TopMargin = 54.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss Bar|Style")
	FLinearColor HealthColor = FLinearColor(0.8f, 0.1f, 0.06f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss Bar|Style")
	FLinearColor MiniBossColor = FLinearColor(0.85f, 0.45f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss Bar|Style")
	FLinearColor TitleColor = FLinearColor(0.95f, 0.82f, 0.55f, 1.0f);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	struct FEntry
	{
		TWeakObjectPtr<ABeyondCharacterBase> Boss;
		FString Name;
		FString Title;
		float Health = 1.0f;
		float Chip = 1.0f;
		TArray<float> Notches;
		int32 Phase = 0;
		int32 PhaseCount = 0;
		bool bInvulnerable = false;
		bool bMiniBoss = false;
		FString Announcement;
		float AnnouncementAlpha = 0.0f;
	};

	TArray<FEntry> Entries;
	float Time = 0.0f;
};
