// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondBondMeterWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UBeyondBondMeterWidget::RebuildWidget()
{
	// Native class without a designer tree: build bar + label once
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BondBox"));
		Box->SetWidthOverride(Size.X);
		Box->SetHeightOverride(Size.Y);

		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BondOverlay"));
		Box->AddChild(Overlay);

		Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("BondBar"));
		if (UOverlaySlot* BarSlot = Overlay->AddChildToOverlay(Bar))
		{
			BarSlot->SetHorizontalAlignment(HAlign_Fill);
			BarSlot->SetVerticalAlignment(VAlign_Fill);
		}

		Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BondLabel"));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 11;
		Label->SetFont(Font);
		Label->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Label->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		if (UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(Label))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		WidgetTree->RootWidget = Box;
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
}
