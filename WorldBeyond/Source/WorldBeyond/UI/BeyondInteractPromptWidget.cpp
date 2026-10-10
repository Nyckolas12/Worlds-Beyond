// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondInteractPromptWidget.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UBeyondInteractPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Never eats clicks
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBeyondInteractPromptWidget::SetPrompt(const FText& InAction, const FText& InTarget)
{
	if (!InAction.EqualTo(Action) || !InTarget.EqualTo(Target))
	{
		Action = InAction;
		Target = InTarget;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

int32 UBeyondInteractPromptWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	FSlateRenderer* Renderer = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetRenderer() : nullptr;
	if (Action.IsEmpty() || !Renderer)
	{
		return BaseLayer;
	}

	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FontSize);
	const FSlateFontInfo KeyFont = FCoreStyle::GetDefaultFontStyle("Bold", FontSize + 1);
	const FText Label = Target.IsEmpty() ? Action : FText::Format(NSLOCTEXT("BeyondPrompt", "ActionTarget", "{0}  -  {1}"), Action, Target);
	const TSharedRef<FSlateFontMeasure> Measure = Renderer->GetFontMeasureService();
	const FVector2f LabelSize = FVector2f(Measure->Measure(Label, Font));
	const FVector2f KeySize = FVector2f(Measure->Measure(KeyLabel, KeyFont));

	const float Pad = 10.0f;
	const float CapSize = FMath::Max(KeySize.X, KeySize.Y) + 10.0f;
	const FVector2f BoxSize(CapSize + Pad * 3.0f + LabelSize.X, FMath::Max(CapSize, LabelSize.Y) + Pad * 1.4f);
	const FVector2f Screen = FVector2f(AllottedGeometry.GetLocalSize());
	const FVector2f BoxPos(Screen.X * 0.5f - BoxSize.X * 0.5f, Screen.Y * ScreenHeight - BoxSize.Y * 0.5f);
	const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));

	// Backing
	FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 1, AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(BoxPos)),
		White, ESlateDrawEffect::None, BackColor);

	// Key cap: outlined square with the key in it
	const FVector2f CapPos(BoxPos.X + Pad, BoxPos.Y + (BoxSize.Y - CapSize) * 0.5f);
	TArray<FVector2f> Outline = { CapPos, CapPos + FVector2f(CapSize, 0.0f), CapPos + FVector2f(CapSize, CapSize), CapPos + FVector2f(0.0f, CapSize), CapPos };
	FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 2, AllottedGeometry.ToPaintGeometry(), MoveTemp(Outline), ESlateDrawEffect::None, KeyColor, true, 2.0f);
	FSlateDrawElement::MakeText(OutDrawElements, BaseLayer + 2,
		AllottedGeometry.ToPaintGeometry(KeySize, FSlateLayoutTransform(CapPos + (FVector2f(CapSize, CapSize) - KeySize) * 0.5f)),
		KeyLabel, KeyFont, ESlateDrawEffect::None, KeyColor);

	// "Talk - Elder Maren"
	const FVector2f LabelPos(CapPos.X + CapSize + Pad, BoxPos.Y + (BoxSize.Y - LabelSize.Y) * 0.5f);
	FSlateDrawElement::MakeText(OutDrawElements, BaseLayer + 2, AllottedGeometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(LabelPos)),
		Label, Font, ESlateDrawEffect::None, TextColor);
	return BaseLayer + 2;
}
