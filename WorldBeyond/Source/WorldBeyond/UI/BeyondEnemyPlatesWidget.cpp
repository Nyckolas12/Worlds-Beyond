// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondEnemyPlatesWidget.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AI/BeyondEnemyController.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/BeyondAffixDefinition.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemySettings.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr float PlateChipSpeed = 0.6f;

	FVector2f MeasurePlateText(const FString& Text, const FSlateFontInfo& Font)
	{
		FSlateRenderer* Renderer = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetRenderer() : nullptr;
		if (!Renderer)
		{
			return FVector2f(Text.Len() * Font.Size * 0.6f, Font.Size * 1.2f);
		}
		return FVector2f(Renderer->GetFontMeasureService()->Measure(Text, Font));
	}
}

void UBeyondEnemyPlatesWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

TArray<ABeyondEnemyCharacter*> UBeyondEnemyPlatesWidget::GetShownEnemies() const
{
	TArray<ABeyondEnemyCharacter*> Result;
	for (const FPlate& Plate : Plates)
	{
		if (ABeyondEnemyCharacter* Enemy = Plate.Enemy.Get())
		{
			Result.Add(Enemy);
		}
	}
	return Result;
}

void UBeyondEnemyPlatesWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	APlayerController* PC = GetOwningPlayer();
	const UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(this);
	if (!PC || !Enemies || !PC->PlayerCameraManager)
	{
		Plates.Reset();
		return;
	}

	const UBeyondEnemySettings* Settings = GetDefault<UBeyondEnemySettings>();
	const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
	const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);

	TArray<FPlate> Updated;
	for (ABeyondEnemyCharacter* Enemy : Enemies->GetLiveEnemies())
	{
		// The boss bar covers mini-bosses and bosses
		if (Enemy->Rank == EBeyondEnemyRank::MiniBoss || Enemy->Rank == EBeyondEnemyRank::Boss)
		{
			continue;
		}
		const ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(Enemy->GetController());
		const bool bFighting = AI && AI->GetAIState() == EBeyondEnemyAIState::Combat;
		if (!bFighting && Enemy->GetTimeSinceDamaged() > Settings->PlateShowAfterHit)
		{
			continue;
		}

		const float Distance = FVector::Dist(Camera, Enemy->GetActorLocation());
		if (Distance > Settings->PlateRange)
		{
			continue;
		}

		const FVector Head = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 35.0f);
		FVector2D Screen;
		if (!UGameplayStatics::ProjectWorldToScreen(PC, Head, Screen, true))
		{
			continue;
		}

		FPlate Plate;
		Plate.Enemy = Enemy;
		Plate.Position = FVector2f(Screen / ViewportScale);
		Plate.Health = FMath::Clamp(UBeyondCombatLibrary::GetActorHealthPercent(Enemy), 0.0f, 1.0f);
		Plate.Chip = Plate.Health;
		if (const FPlate* Previous = Plates.FindByPredicate([Enemy](const FPlate& Old) { return Old.Enemy.Get() == Enemy; }))
		{
			Plate.Chip = FMath::Max(Plate.Health, Previous->Chip - PlateChipSpeed * InDeltaTime);
		}
		Plate.Level = Enemy->GetCharacterLevel();
		Plate.bElite = Enemy->IsElite();
		Plate.Label = Plate.bElite ? Enemy->GetEnemyName().ToString() : FString();
		Plate.LabelColor = EliteColor;
		for (const UBeyondAffixDefinition* Affix : Enemy->Affixes)
		{
			if (Affix && Affix->Tint.A > 0.0f)
			{
				Plate.LabelColor = FLinearColor(Affix->Tint.R, Affix->Tint.G, Affix->Tint.B, 1.0f);
				break;
			}
		}
		// Fade the far ones a little
		Plate.Opacity = FMath::GetMappedRangeValueClamped(FVector2f(Settings->PlateRange * 0.7f, Settings->PlateRange), FVector2f(1.0f, 0.35f), Distance);
		Updated.Add(Plate);
	}
	Plates = MoveTemp(Updated);
}

int32 UBeyondEnemyPlatesWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 BackLayer = BaseLayer + 1;
	const int32 FillLayer = BaseLayer + 2;
	const int32 TextLayer = BaseLayer + 3;

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
	const FSlateFontInfo LevelFont = FCoreStyle::GetDefaultFontStyle("Bold", 8);
	const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);

	auto DrawBox = [&](const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Tint, int32 Layer)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft)),
			WhiteBrush, ESlateDrawEffect::None, Tint);
	};
	auto DrawPlateText = [&](const FString& Text, const FSlateFontInfo& Font, const FVector2f& TopLeft, const FLinearColor& Tint)
	{
		const FVector2f Size = MeasurePlateText(Text, Font);
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft + FVector2f(1.0f, 1.0f))),
			Text, Font, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, Tint.A * 0.8f));
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer + 1, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft)),
			Text, Font, ESlateDrawEffect::None, Tint);
		return Size;
	};

	for (const FPlate& Plate : Plates)
	{
		const FVector2f Size(BarSize.X * (Plate.bElite ? 1.25f : 1.0f), BarSize.Y);
		const FVector2f TopLeft = Plate.Position - FVector2f(Size.X * 0.5f, Size.Y);
		auto Faded = [&Plate](FLinearColor Color) { Color.A *= Plate.Opacity; return Color; };

		// Frame (gold for elites), back, chip, health
		const FVector2f Border(1.5f, 1.5f);
		DrawBox(TopLeft - Border, Size + Border * 2.0f, Faded(Plate.bElite ? EliteColor * FLinearColor(1.0f, 1.0f, 1.0f, 0.9f) : BackColor), BackLayer);
		DrawBox(TopLeft, Size, Faded(BackColor), BackLayer);
		DrawBox(TopLeft, FVector2f(Size.X * Plate.Chip, Size.Y), Faded(FLinearColor(1.0f, 0.85f, 0.6f, 0.9f)), FillLayer);
		DrawBox(TopLeft, FVector2f(Size.X * Plate.Health, Size.Y), Faded(HealthColor), FillLayer + 0);

		// Level badge on the left, elite name above
		const FString LevelText = FString::FromInt(Plate.Level);
		const FVector2f LevelSize = MeasurePlateText(LevelText, LevelFont);
		DrawPlateText(LevelText, LevelFont, FVector2f(TopLeft.X - LevelSize.X - 4.0f, TopLeft.Y + (Size.Y - LevelSize.Y) * 0.5f), Faded(FLinearColor::White));
		if (!Plate.Label.IsEmpty())
		{
			const FVector2f NameSize = MeasurePlateText(Plate.Label, NameFont);
			DrawPlateText(Plate.Label, NameFont, FVector2f(Plate.Position.X - NameSize.X * 0.5f, TopLeft.Y - NameSize.Y - 2.0f), Faded(Plate.LabelColor));
		}
	}
	return TextLayer + 1;
}
