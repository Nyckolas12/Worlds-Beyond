// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondBondMeterWidget.h"
#include "WorldBeyond.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	UTextBlock* MakeLabel(UWidgetTree* Tree, FName Name, int32 FontSize)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		return Text;
	}

	// Medallion glyphs (M_UI_DuoMedallion "Mode")
	constexpr float BondGlyphTexture = 0.0f;
	constexpr float BondGlyphSun = 1.0f;
	constexpr float BondGlyphBolt = 2.0f;

	// 0 -> 1 with a little overshoot (the duo medallion popping in)
	float BondEaseOutBack(float X)
	{
		const float C1 = 1.70158f;
		const float C3 = C1 + 1.0f;
		const float Y = X - 1.0f;
		return 1.0f + C3 * Y * Y * Y + C1 * Y * Y;
	}

	void SetShown(UWidget* Widget, bool bShown)
	{
		const ESlateVisibility Wanted = bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (Widget && Widget->GetVisibility() != Wanted)
		{
			Widget->SetVisibility(Wanted);
		}
	}
}

TSharedRef<SWidget> UBeyondBondMeterWidget::RebuildWidget()
{
	// Native class without a designer tree: build the arc (or the simple row) once
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		bArcStyle = BuildArcLayout();
		if (!bArcStyle)
		{
			BuildSimpleLayout();
		}
	}

	TSharedRef<SWidget> Result = Super::RebuildWidget();
	ApplyToWidgets();
	return Result;
}

void UBeyondBondMeterWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Never eats clicks
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBeyondBondMeterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AdvanceAnimation(InDeltaTime);
}

bool UBeyondBondMeterWidget::BuildArcLayout()
{
	UMaterialInterface* Arc = ArcMaterial.LoadSynchronous();
	UMaterialInterface* Flames = FlameMaterial.LoadSynchronous();
	UMaterialInterface* Medallion = MedallionMaterial.LoadSynchronous();
	if (!Arc || !Flames || !Medallion)
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogBeyond, Warning, TEXT("Bond meter: the duo meter materials are missing (run Scripts/Migration/migrate_pass6.py); showing the simple bar"));
		}
		return false;
	}

	// Arc geometry in arc-image pixels; the same numbers go to M_UI_BondArc so the medallions sit on its ends
	const float Width = static_cast<float>(ArcSize.X);
	const float Height = static_cast<float>(ArcSize.Y);
	const float Radius = FMath::Max(ArcRadius, Width * 0.5f);
	ArcHalfAngle = FMath::Asin(FMath::Clamp((Width * 0.5f - ArcEndInset) / Radius, 0.05f, 0.95f));
	ArcShape = FVector(Width * 0.5f, ArcApexY + Radius, Radius);
	const FVector2D EndOffset(Radius * FMath::Sin(ArcHalfAngle), -Radius * FMath::Cos(ArcHalfAngle));

	// Room for the end medallions at the sides and the duo medallion above the middle
	const float SideRoom = FMath::Max(0.0f, EmblemSize * 0.5f - ArcEndInset);
	const float ArcTop = FMath::Max(0.0f, DuoIconLift + DuoIconSize * 0.5f - ArcApexY + 8.0f);
	const FVector2D ArcOrigin(SideRoom, ArcTop);
	const FVector2D ArcCentre = ArcOrigin + FVector2D(ArcShape.X, ArcShape.Y);
	const FVector2D LeftEnd = ArcCentre + FVector2D(-EndOffset.X, EndOffset.Y);
	const FVector2D RightEnd = ArcCentre + EndOffset;
	const FVector2D Apex(ArcOrigin.X + Width * 0.5f, ArcOrigin.Y + ArcApexY);
	const FVector2D DuoCentre(Apex.X, Apex.Y - DuoIconLift);

	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DuoMeterBox"));
	Box->SetWidthOverride(Width + SideRoom * 2.0f);
	Box->SetHeightOverride(FMath::Max(ArcTop + Height, static_cast<float>(LeftEnd.Y) + EmblemSize * 0.5f));
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DuoMeterCanvas"));
	Box->AddChild(Canvas);

	// Back to front: flames behind the duo medallion, the arc, its end medallions, the duo medallion, the label
	FlameImage = AddMaterialImage(Canvas, TEXT("DuoFlames"), Flames, DuoCentre, FlameSize, FlameSize, FlameMID);
	AddMaterialImage(Canvas, TEXT("BondArc"), Arc, ArcOrigin + FVector2D(Width, Height) * 0.5f, Width, Height, ArcMID);
	AddMaterialImage(Canvas, TEXT("AngelMedallion"), Medallion, LeftEnd, EmblemSize, EmblemSize, AngelMID);
	AddMaterialImage(Canvas, TEXT("JiWoongMedallion"), Medallion, RightEnd, EmblemSize, EmblemSize, JiWoongMID);
	DuoImage = AddMaterialImage(Canvas, TEXT("DuoMedallion"), Medallion, DuoCentre, DuoIconSize, DuoIconSize, DuoMID);

	// "READY [G]" between the duo medallion and the arc
	UHorizontalBox* LabelRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DuoReadyRow"));
	UTextBlock* ReadyWord = MakeLabel(WidgetTree, TEXT("DuoReady"), 18);
	FSlateFontInfo ReadyFont = ReadyWord->GetFont();
	ReadyFont.TypefaceFontName = TEXT("Bold");
	ReadyFont.OutlineSettings.OutlineSize = 2;
	ReadyFont.OutlineSettings.OutlineColor = FLinearColor(0.02f, 0.01f, 0.04f, 0.9f);
	ReadyWord->SetFont(ReadyFont);
	ReadyWord->SetText(ArcReadyText);
	ReadyWord->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.93f, 0.72f)));
	if (UHorizontalBoxSlot* WordSlot = LabelRow->AddChildToHorizontalBox(ReadyWord))
	{
		WordSlot->SetVerticalAlignment(VAlign_Center);
		WordSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	}
	UBorder* KeyCap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DuoKeyCap"));
	KeyCap->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.03f, 0.03f, 0.07f, 0.85f), 4.0f, FLinearColor(1.0f, 0.75f, 0.25f, 1.0f), 1.5f));
	KeyCap->SetPadding(FMargin(7.0f, 0.0f));
	UTextBlock* KeyWord = MakeLabel(WidgetTree, TEXT("DuoKey"), 15);
	FSlateFontInfo KeyFont = KeyWord->GetFont();
	KeyFont.TypefaceFontName = TEXT("Bold");
	KeyWord->SetFont(KeyFont);
	KeyWord->SetText(KeyText);
	KeyWord->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.45f)));
	KeyCap->SetContent(KeyWord);
	if (UHorizontalBoxSlot* CapSlot = LabelRow->AddChildToHorizontalBox(KeyCap))
	{
		CapSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UCanvasPanelSlot* RowSlot = Canvas->AddChildToCanvas(LabelRow))
	{
		RowSlot->SetAutoSize(true);
		RowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		RowSlot->SetPosition(FVector2D(Apex.X, (DuoCentre.Y + DuoIconSize * 0.5f + Apex.Y - BandThickness * 0.5f) * 0.5f));
	}
	ReadyLabel = LabelRow;

	WidgetTree->RootWidget = Box;
	return true;
}

UImage* UBeyondBondMeterWidget::AddMaterialImage(UCanvasPanel* Canvas, FName Name, UMaterialInterface* Material, const FVector2D& Centre,
	float Width, float Height, TObjectPtr<UMaterialInstanceDynamic>& OutMaterial)
{
	UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	OutMaterial = UMaterialInstanceDynamic::Create(Material, this);
	Image->SetBrushFromMaterial(OutMaterial);
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	Image->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	if (UCanvasPanelSlot* ImageSlot = Canvas->AddChildToCanvas(Image))
	{
		ImageSlot->SetPosition(Centre - FVector2D(Width, Height) * 0.5f);
		ImageSlot->SetSize(FVector2D(Width, Height));
	}
	return Image;
}

void UBeyondBondMeterWidget::BuildSimpleLayout()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BondRow"));

	// Duo slot: icon with its key in the corner
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DuoIconBox"));
	IconBox->SetWidthOverride(IconSize);
	IconBox->SetHeightOverride(IconSize);
	UOverlay* IconOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("DuoIconOverlay"));
	IconBox->AddChild(IconOverlay);

	IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DuoIcon"));
	if (UOverlaySlot* ImageSlot = IconOverlay->AddChildToOverlay(IconImage))
	{
		ImageSlot->SetHorizontalAlignment(HAlign_Fill);
		ImageSlot->SetVerticalAlignment(VAlign_Fill);
	}
	KeyLabel = MakeLabel(WidgetTree, TEXT("DuoKey"), 12);
	if (UOverlaySlot* KeySlot = IconOverlay->AddChildToOverlay(KeyLabel))
	{
		KeySlot->SetHorizontalAlignment(HAlign_Right);
		KeySlot->SetVerticalAlignment(VAlign_Bottom);
	}
	if (UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconBox))
	{
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	}

	// Meter with its label
	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BondBox"));
	BarBox->SetWidthOverride(Size.X);
	BarBox->SetHeightOverride(Size.Y);
	UOverlay* BarOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BondOverlay"));
	BarBox->AddChild(BarOverlay);

	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("BondBar"));
	if (UOverlaySlot* BarSlot = BarOverlay->AddChildToOverlay(Bar))
	{
		BarSlot->SetHorizontalAlignment(HAlign_Fill);
		BarSlot->SetVerticalAlignment(VAlign_Fill);
	}
	Label = MakeLabel(WidgetTree, TEXT("BondLabel"), 11);
	if (UOverlaySlot* LabelSlot = BarOverlay->AddChildToOverlay(Label))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* MeterSlot = Row->AddChildToHorizontalBox(BarBox))
	{
		MeterSlot->SetVerticalAlignment(VAlign_Center);
	}

	WidgetTree->RootWidget = Row;
}

void UBeyondBondMeterWidget::SetBond(float Bond, float MaxBond)
{
	const float NewPercent = MaxBond > 0.0f ? FMath::Clamp(Bond / MaxBond, 0.0f, 1.0f) : 0.0f;
	if (NewPercent > Percent + 0.001f)
	{
		// Every gain flashes the front of the fill
		Surge = 1.0f;
	}
	Percent = NewPercent;
	bFull = Percent >= 1.0f;
	if (!bArcStyle)
	{
		ApplyToWidgets();
	}
	OnBondUpdated(Percent, bFull);
}

void UBeyondBondMeterWidget::SetDuoIcon(UTexture2D* Icon)
{
	DuoIcon = Icon;
	ApplyToWidgets();
}

void UBeyondBondMeterWidget::ApplyDuoGlyph()
{
	if (!DuoMID)
	{
		return;
	}
	// The duo ability's own icon when it has one, else a lightning bolt
	if (DuoIcon)
	{
		DuoMID->SetTextureParameterValue(TEXT("Glyph"), DuoIcon);
		DuoMID->SetScalarParameterValue(TEXT("Mode"), BondGlyphTexture);
	}
	else
	{
		DuoMID->SetScalarParameterValue(TEXT("Mode"), BondGlyphBolt);
	}
}

void UBeyondBondMeterWidget::ApplyToWidgets()
{
	if (bArcStyle)
	{
		if (ArcMID)
		{
			ArcMID->SetVectorParameterValue(TEXT("Size"), FLinearColor(static_cast<float>(ArcSize.X), static_cast<float>(ArcSize.Y), 0.0f));
			ArcMID->SetVectorParameterValue(TEXT("Arc"), FLinearColor(static_cast<float>(ArcShape.X), static_cast<float>(ArcShape.Y), static_cast<float>(ArcShape.Z)));
			ArcMID->SetScalarParameterValue(TEXT("HalfAngle"), ArcHalfAngle);
			ArcMID->SetVectorParameterValue(TEXT("Band"), FLinearColor(BandThickness, FrameWidth, 0.0f));
			ArcMID->SetVectorParameterValue(TEXT("Stops"), FLinearColor(PurpleStop, FMath::Max(GoldStart, PurpleStop + 0.01f), 0.0f));
			ArcMID->SetScalarParameterValue(TEXT("FlowSpeed"), FlowSpeed);
		}
		if (FlameMID)
		{
			// Flames start at the duo medallion's rim
			FlameMID->SetScalarParameterValue(TEXT("RingRadius"), DuoIconSize * 0.44f / FMath::Max(FlameSize * 0.5f, 1.0f));
		}
		for (UMaterialInstanceDynamic* Material : { ArcMID.Get(), FlameMID.Get() })
		{
			if (Material)
			{
				Material->SetVectorParameterValue(TEXT("ColorBlue"), ColorBlue);
				Material->SetVectorParameterValue(TEXT("ColorPurple"), ColorPurple);
				Material->SetVectorParameterValue(TEXT("ColorGold"), ColorGold);
			}
		}
		if (AngelMID)
		{
			if (UTexture2D* Glyph = AngelGlyph.LoadSynchronous())
			{
				AngelMID->SetTextureParameterValue(TEXT("Glyph"), Glyph);
			}
			if (UTexture2D* Rune = RuneTexture.LoadSynchronous())
			{
				AngelMID->SetTextureParameterValue(TEXT("Rune"), Rune);
				AngelMID->SetScalarParameterValue(TEXT("RuneStrength"), 0.45f);
			}
			AngelMID->SetScalarParameterValue(TEXT("Mode"), BondGlyphTexture);
			AngelMID->SetScalarParameterValue(TEXT("GlyphScale"), 0.55f);
			AngelMID->SetVectorParameterValue(TEXT("Tint"), ColorBlue);
		}
		if (JiWoongMID)
		{
			JiWoongMID->SetScalarParameterValue(TEXT("Mode"), BondGlyphSun);
			JiWoongMID->SetScalarParameterValue(TEXT("GlyphScale"), 0.62f);
			JiWoongMID->SetVectorParameterValue(TEXT("Tint"), JiWoongTint);
		}
		if (DuoMID)
		{
			DuoMID->SetScalarParameterValue(TEXT("GlyphScale"), 0.62f);
			DuoMID->SetVectorParameterValue(TEXT("Tint"), DuoTint);
		}
		ApplyDuoGlyph();
		AdvanceAnimation(0.0f);
		return;
	}

	const FLinearColor Fill = bFull ? FullColor
		: (Percent < 0.5f ? FLinearColor::LerpUsingHSV(EmptyColor, HalfColor, Percent * 2.0f)
		                  : FLinearColor::LerpUsingHSV(HalfColor, FullColor, (Percent - 0.5f) * 2.0f));

	if (Bar)
	{
		Bar->SetPercent(Percent);
		Bar->SetFillColorAndOpacity(Fill);
	}
	if (Label)
	{
		Label->SetText(bFull ? ReadyText : FText::Format(NSLOCTEXT("Beyond", "BondPercent", "{0}  {1}%"), ChargingText, FText::AsNumber(FMath::FloorToInt(Percent * 100.0f))));
		Label->SetColorAndOpacity(FSlateColor(bFull ? FLinearColor(1.0f, 0.95f, 0.8f) : FLinearColor::White));
	}
	if (IconImage)
	{
		UTexture2D* Icon = DuoIcon ? DuoIcon.Get() : DefaultIcon.LoadSynchronous();
		if (Icon)
		{
			IconImage->SetBrushFromTexture(Icon);
		}
		IconImage->SetColorAndOpacity(bFull ? FLinearColor::White : ChargingIconTint);
	}
	if (KeyLabel)
	{
		KeyLabel->SetText(KeyText);
		KeyLabel->SetColorAndOpacity(FSlateColor(bFull ? FullColor : FLinearColor(0.7f, 0.7f, 0.7f)));
	}
}

void UBeyondBondMeterWidget::AdvanceAnimation(float DeltaTime)
{
	if (!bArcStyle)
	{
		return;
	}

	AnimTime += DeltaTime;
	DisplayedFill = FMath::FInterpTo(DisplayedFill, Percent, DeltaTime, FillEaseSpeed);
	if (FMath::IsNearlyEqual(DisplayedFill, Percent, 0.001f))
	{
		DisplayedFill = Percent;
	}
	Surge = FMath::Max(0.0f, Surge - DeltaTime * 2.5f);
	ReadyBlend = FMath::FInterpConstantTo(ReadyBlend, bFull ? 1.0f : 0.0f, DeltaTime, 1.0f / ReadyBlendTime);
	const float ReadyEase = FMath::InterpEaseInOut(0.0f, 1.0f, ReadyBlend, 2.0f);
	const float Pulse = 0.5f + 0.5f * FMath::Sin(AnimTime * 2.2f);

	if (ArcMID)
	{
		ArcMID->SetScalarParameterValue(TEXT("Fill"), DisplayedFill);
		ArcMID->SetScalarParameterValue(TEXT("Surge"), Surge);
		ArcMID->SetScalarParameterValue(TEXT("Ready"), ReadyEase);
	}

	// Angel's medallion lights up as the blue / purple part fills, Ji-Woong's as the gold part does
	const float AngelLit = FMath::Clamp(DisplayedFill / FMath::Max(GoldStart, 0.05f), 0.0f, 1.0f);
	const float JiWoongLit = FMath::Clamp((DisplayedFill - GoldStart) / FMath::Max(1.0f - GoldStart, 0.05f), 0.0f, 1.0f);
	if (AngelMID)
	{
		AngelMID->SetScalarParameterValue(TEXT("Glow"), 0.25f + 0.55f * AngelLit + 0.2f * Pulse * AngelLit);
		AngelMID->SetScalarParameterValue(TEXT("Dim"), 0.4f * (1.0f - AngelLit));
	}
	if (JiWoongMID)
	{
		JiWoongMID->SetScalarParameterValue(TEXT("Glow"), 0.2f + 0.6f * JiWoongLit + 0.2f * Pulse * JiWoongLit);
		JiWoongMID->SetScalarParameterValue(TEXT("Dim"), 0.45f * (1.0f - JiWoongLit));
	}

	// Full: the duo medallion pops in with its flames and the label; spent: they fade away
	if (FlameMID)
	{
		FlameMID->SetScalarParameterValue(TEXT("Intensity"), ReadyEase);
	}
	if (FlameImage)
	{
		SetShown(FlameImage, ReadyEase > 0.001f);
		FlameImage->SetRenderScale(FVector2D(FMath::Lerp(0.8f, 1.0f, ReadyEase)));
	}
	if (DuoImage)
	{
		const float Scale = bShowDuoIconWhileCharging ? FMath::Lerp(0.85f, 1.0f, BondEaseOutBack(ReadyBlend)) : FMath::Lerp(0.3f, 1.0f, BondEaseOutBack(ReadyBlend));
		const float Opacity = bShowDuoIconWhileCharging ? FMath::Lerp(0.6f, 1.0f, ReadyEase) : ReadyEase;
		SetShown(DuoImage, Opacity > 0.001f);
		DuoImage->SetRenderScale(FVector2D(Scale));
		DuoImage->SetRenderOpacity(Opacity);
	}
	if (DuoMID)
	{
		DuoMID->SetScalarParameterValue(TEXT("Glow"), 0.55f + 0.35f * FMath::Sin(AnimTime * 3.0f) * ReadyEase);
		DuoMID->SetScalarParameterValue(TEXT("Dim"), 1.0f - ReadyEase);
	}
	if (UWidget* Ready = ReadyLabel.Get())
	{
		SetShown(Ready, ReadyEase > 0.001f);
		Ready->SetRenderOpacity(ReadyEase);
	}
}
