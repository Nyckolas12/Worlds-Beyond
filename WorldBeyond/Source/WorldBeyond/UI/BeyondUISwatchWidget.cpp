// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondUISwatchWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UI/BeyondUIArt.h"
#include "UI/BeyondUIStyle.h"

namespace BeyondSwatchLocal
{
	TWeakObjectPtr<UBeyondUISwatchWidget> GShownSwatch;

	FAutoConsoleCommandWithWorld GSwatchCommand(
		TEXT("Beyond.UI.Swatch"),
		TEXT("Shows / hides every piece of the shared UI look (fonts, panels, tabs, item frames, bars, glyphs)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { UBeyondUISwatchWidget::Toggle(World); }));
}

void UBeyondUISwatchWidget::Toggle(UWorld* World)
{
	if (UBeyondUISwatchWidget* Shown = BeyondSwatchLocal::GShownSwatch.Get(); Shown && Shown->IsInViewport())
	{
		Shown->RemoveFromParent();
		BeyondSwatchLocal::GShownSwatch.Reset();
		return;
	}
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	if (UBeyondUISwatchWidget* Swatch = CreateWidget<UBeyondUISwatchWidget>(PC, UBeyondUISwatchWidget::StaticClass()))
	{
		Swatch->AddToViewport(200);
		BeyondSwatchLocal::GShownSwatch = Swatch;
	}
}

void UBeyondUISwatchWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	BeyondUI::Preload();
}

void UBeyondUISwatchWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AnimTime += InDeltaTime;
}

int32 UBeyondUISwatchWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace BeyondUI;
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FDesignSpace Space = FDesignSpace::Fit(FVector2f(AllottedGeometry.GetLocalSize()));
	FPainter Paint(Space.MakeGeometry(AllottedGeometry), OutDrawElements, BaseLayer);
	Paint.AnimTime = AnimTime;
	const FPalette& P = Palette();

	UBeyondUISubsystem* UI = UBeyondUISubsystem::Get();
	const UBeyondUIArtSet* Art = UI ? UI->GetArtSet() : nullptr;
	Paint.Backdrop(Space, Art && UI ? UI->LoadTexture(Art->MenuBackdrop) : nullptr, 0);

	// Header: emblem, title, tabs
	Paint.Sparkle(FVector2f(64.0f, 50.0f), 22.0f, P.GoldBright, 10);
	Paint.Text(TEXT("Character"), Font(EBeyondFont::Display, 26.0f), FVector2f(102.0f, 32.0f), P.GoldBright, 0.0f, 10);
	const TCHAR* Tabs[] = { TEXT("Angel"), TEXT("Ji-Woong"), TEXT("Duo") };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Paint.SkewTab(FVector2f(620.0f + Index * 256.0f, 32.0f), FVector2f(236.0f, 50.0f), FString(Tabs[Index]).ToUpper(), Index == 0, Index == 1, P.Blue, 10);
	}
	Paint.KeyCap(TEXT("Q"), FVector2f(568.0f, 43.0f), 10);
	Paint.KeyCap(TEXT("E"), FVector2f(1414.0f, 43.0f), 10);

	// Fonts
	Paint.Panel(FVector2f(40.0f, 120.0f), FVector2f(560.0f, 420.0f), 20);
	float Y = Paint.SectionHeader(TEXT("Typography"), FVector2f(64.0f, 140.0f), 510.0f, 22);
	Paint.Text(TEXT("Stormcaller"), Font(EBeyondFont::Display, 34.0f), FVector2f(64.0f, Y), P.GoldBright, 0.0f, 22);
	Y += 54.0f;
	Paint.Text(TEXT("Combat Stats  ·  Heading"), Font(EBeyondFont::Heading, 18.0f), FVector2f(64.0f, Y), P.Text, 0.0f, 22);
	Y += 34.0f;
	Paint.Text(TEXT("1713  +137   12,480 / 23,000"), Font(EBeyondFont::Number, 20.0f), FVector2f(64.0f, Y), P.Text, 0.0f, 22);
	Y += 36.0f;
	Y += Paint.Paragraph(TEXT("Unleashes a spinning storm around you, dealing wind damage to nearby enemies and slightly lifting them into the air."),
		Font(EBeyondFont::Body, 17.0f), FVector2f(64.0f, Y), 510.0f, 24.0f, P.Text, 22);
	Paint.Text(TEXT("A helm bathed in the light of a fallen sun."), Font(EBeyondFont::BodyItalic, 17.0f), FVector2f(64.0f, Y + 6.0f), P.TextDim, 0.0f, 22);
	Paint.Text(TEXT("+10% Defense"), Font(EBeyondFont::BodyBold, 17.0f), FVector2f(64.0f, Y + 34.0f), P.Plus, 0.0f, 22);

	// Item frames per tier
	Paint.Panel(FVector2f(620.0f, 120.0f), FVector2f(620.0f, 260.0f), 20);
	Paint.SectionHeader(TEXT("Item Frames"), FVector2f(644.0f, 140.0f), 570.0f, 22);
	const EBeyondItemTier Tiers[] = { EBeyondItemTier::Common, EBeyondItemTier::Uncommon, EBeyondItemTier::Rare, EBeyondItemTier::Epic, EBeyondItemTier::Legendary };
	const EBeyondGlyph Glyphs[] = { EBeyondGlyph::Helm, EBeyondGlyph::Chest, EBeyondGlyph::Gauntlets, EBeyondGlyph::Boots, EBeyondGlyph::Sword };
	for (int32 Index = 0; Index < 5; ++Index)
	{
		FItemFrameStyle Style;
		Style.Tier = Tiers[Index];
		Style.Glyph = Glyphs[Index];
		Style.UpgradeLevel = 3 * Index + 1;
		Style.bShowUpgrade = true;
		Style.bLocked = Index == 4;
		Style.bJunk = Index == 0;
		Style.bSelected = Index == 3;
		Paint.ItemFrame(FVector2f(690.0f + Index * 112.0f, 250.0f), 88.0f, Style, 24);
	}
	FItemFrameStyle Empty;
	Empty.bEmpty = true;
	Empty.Glyph = EBeyondGlyph::Ring;
	Paint.ItemFrame(FVector2f(690.0f, 340.0f), 56.0f, Empty, 24);

	// Bars
	Paint.Panel(FVector2f(620.0f, 400.0f), FVector2f(620.0f, 140.0f), 20);
	Paint.Bar(FVector2f(660.0f, 440.0f), FVector2f(520.0f, 8.0f), 0.54f, P.GoldBright, 24);
	Paint.Bar(FVector2f(660.0f, 470.0f), FVector2f(520.0f, 10.0f), 1.0f, P.Health, 24, false);
	Paint.Bar(FVector2f(660.0f, 500.0f), FVector2f(520.0f, 10.0f), 0.8f, P.Stamina, 24, false);

	// Rune circle
	Paint.RuneCircle(FVector2f(1560.0f, 330.0f), 200.0f, P.Gold, 20);

	// Glyphs
	Paint.Panel(FVector2f(40.0f, 560.0f), FVector2f(1200.0f, 300.0f), 20);
	Paint.SectionHeader(TEXT("Glyphs"), FVector2f(64.0f, 580.0f), 1150.0f, 22);
	for (int32 Index = 1; Index <= static_cast<int32>(EBeyondGlyph::Flame); ++Index)
	{
		const int32 Cell = Index - 1;
		Paint.Glyph(static_cast<EBeyondGlyph>(Index), FVector2f(90.0f + (Cell % 20) * 56.0f, 660.0f + (Cell / 20) * 64.0f), 36.0f, P.Text, 24);
	}

	// Key bar
	float X = 60.0f;
	const TPair<const TCHAR*, const TCHAR*> Hints[] = { { TEXT("F"), TEXT("Unequip") }, { TEXT("R"), TEXT("Compare") }, { TEXT("X"), TEXT("Mark as Junk") },
		{ TEXT("T"), TEXT("Details") }, { TEXT("Esc"), TEXT("Back") } };
	Paint.Separator(FVector2f(40.0f, 1004.0f), 1840.0f, 20);
	for (const TPair<const TCHAR*, const TCHAR*>& Hint : Hints)
	{
		X += Paint.KeyHint(Hint.Key, Hint.Value, FVector2f(X, 1024.0f), 24) + 40.0f;
	}
	return Paint.GetMaxLayer();
}
