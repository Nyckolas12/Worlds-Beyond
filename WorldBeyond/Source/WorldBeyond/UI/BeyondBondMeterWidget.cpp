// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondBondMeterWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

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
}

TSharedRef<SWidget> UBeyondBondMeterWidget::RebuildWidget()
{
	// Native class without a designer tree: build [icon] [meter] once
	if (WidgetTree && !WidgetTree->RootWidget)
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

	TSharedRef<SWidget> Result = Super::RebuildWidget();
	ApplyToWidgets();
	return Result;
}

void UBeyondBondMeterWidget::SetBond(float Bond, float MaxBond)
{
	Percent = MaxBond > 0.0f ? FMath::Clamp(Bond / MaxBond, 0.0f, 1.0f) : 0.0f;
	bFull = Percent >= 1.0f;
	ApplyToWidgets();
	OnBondUpdated(Percent, bFull);
}

void UBeyondBondMeterWidget::SetDuoIcon(UTexture2D* Icon)
{
	DuoIcon = Icon;
	ApplyToWidgets();
}

void UBeyondBondMeterWidget::ApplyToWidgets()
{
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
