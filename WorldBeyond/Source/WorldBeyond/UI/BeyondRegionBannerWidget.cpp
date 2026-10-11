// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondRegionBannerWidget.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace BeyondRegionBannerLocal
{
	FVector2f MeasureBannerText(const FString& Text, const FSlateFontInfo& Font)
	{
		// Rough size when there is no renderer to ask (-nullrhi automation runs)
		FSlateRenderer* Renderer = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetRenderer() : nullptr;
		if (!Renderer)
		{
			return FVector2f(Text.Len() * Font.Size * 0.6f, Font.Size * 1.2f);
		}
		return FVector2f(Renderer->GetFontMeasureService()->Measure(Text, Font));
	}

	FLinearColor Faded(FLinearColor Color, float Opacity)
	{
		Color.A *= FMath::Clamp(Opacity, 0.0f, 1.0f);
		return Color;
	}

	// Spaced capitals for the small line ("T H E   W A N D E R I N G")
	FString Spaced(const FString& Text)
	{
		FString Result;
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			Result.AppendChar(FChar::ToUpper(Text[Index]));
			if (Index + 1 < Text.Len())
			{
				Result.AppendChar(TEXT(' '));
			}
		}
		return Result;
	}
}

void UBeyondRegionBannerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

double UBeyondRegionBannerWidget::GetNow() const
{
	// Real time: the banner keeps fading while a menu pauses the game
	const UWorld* World = GetWorld();
	return World ? World->GetRealTimeSeconds() : FPlatformTime::Seconds();
}

void UBeyondRegionBannerWidget::ShowRegion(const FText& Title, const FText& Subtitle, const FText& Detail, const FText& Reward, FLinearColor Accent)
{
	BannerTitle = Title;
	BannerSubtitle = Subtitle;
	BannerDetail = Detail;
	BannerReward = Reward;
	BannerAccent = Accent;
	BannerStart = GetNow();
}

void UBeyondRegionBannerWidget::ShowToast(const FText& Title, const FText& Detail, FLinearColor Colour)
{
	const double Now = GetNow();
	Toasts.RemoveAll([this, Now](const FToast& Toast) { return Now - Toast.Start >= ToastDuration; });
	FToast& Toast = Toasts.InsertDefaulted_GetRef(0);
	Toast.Title = Title;
	Toast.Detail = Detail;
	Toast.Colour = Colour;
	Toast.Start = Now;
	if (Toasts.Num() > 4)
	{
		Toasts.SetNum(4);
	}
}

bool UBeyondRegionBannerWidget::IsBannerShowing() const
{
	const double Age = GetNow() - BannerStart;
	return !BannerTitle.IsEmpty() && Age >= 0.0 && Age < GetBannerDuration();
}

TArray<FText> UBeyondRegionBannerWidget::GetToastTitles() const
{
	const double Now = GetNow();
	TArray<FText> Titles;
	for (const FToast& Toast : Toasts)
	{
		if (Now - Toast.Start < ToastDuration)
		{
			Titles.Add(Toast.Title);
		}
	}
	return Titles;
}

int32 UBeyondRegionBannerWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace BeyondRegionBannerLocal;
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 BackLayer = BaseLayer + 1;
	const int32 LineLayer = BaseLayer + 2;
	const int32 TextLayer = BaseLayer + 3;
	const FVector2f ScreenSize(AllottedGeometry.GetLocalSize());
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
	const double Now = GetNow();

	auto DrawBox = [&](const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Tint, int32 Layer)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft)), WhiteBrush,
			ESlateDrawEffect::None, Tint);
	};
	auto DrawLabel = [&](const FString& Text, const FSlateFontInfo& Font, const FVector2f& Position, const FLinearColor& Tint, float Align)
	{
		const FVector2f Size = MeasureBannerText(Text, Font);
		const FVector2f TopLeft(Position.X - Size.X * Align, Position.Y);
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft + FVector2f(2.0f, 2.0f))),
			Text, Font, ESlateDrawEffect::None, Faded(ShadowColor, Tint.A));
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer + 1, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft)),
			Text, Font, ESlateDrawEffect::None, Tint);
		return Size;
	};
	auto DrawRule = [&](const FVector2f& From, const FVector2f& To, const FLinearColor& Tint, float Thickness)
	{
		TArray<FVector2f> Points = { From, To };
		FSlateDrawElement::MakeLines(OutDrawElements, LineLayer, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Tint, true, Thickness);
	};

	//~ The banner
	const float Age = static_cast<float>(Now - BannerStart);
	const float Duration = GetBannerDuration();
	if (!BannerTitle.IsEmpty() && Age >= 0.0f && Age < Duration)
	{
		float Alpha = 1.0f;
		if (Age < FadeIn)
		{
			Alpha = Age / FadeIn;
		}
		else if (Age > Duration - FadeOut)
		{
			Alpha = (Duration - Age) / FadeOut;
		}
		// The rules grow out from the middle while it fades in
		const float Grow = FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(Age / (FadeIn * 1.4f), 0.0f, 1.0f), 3.0f);
		const FVector2f Centre(ScreenSize.X * BannerPosition.X, ScreenSize.Y * BannerPosition.Y);

		const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 40);
		const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
		const FSlateFontInfo DetailFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
		const FString Title = BannerTitle.ToString();
		const FVector2f TitleSize = MeasureBannerText(Title, TitleFont);

		// A soft dark band behind it
		const FVector2f BandSize(FMath::Max(TitleSize.X + 420.0f, 640.0f), TitleSize.Y + 92.0f);
		DrawBox(FVector2f(Centre.X - BandSize.X * 0.5f, Centre.Y - 36.0f), BandSize, Faded(PanelColor, Alpha), BackLayer);

		if (!BannerSubtitle.IsEmpty())
		{
			DrawLabel(Spaced(BannerSubtitle.ToString()), SmallFont, FVector2f(Centre.X, Centre.Y - 26.0f), Faded(GoldColor, Alpha), 0.5f);
		}
		DrawLabel(Title, TitleFont, Centre, Faded(TextColor, Alpha), 0.5f);

		// Thin rules either side of the name, a dot of the region's colour at their inner ends
		const float LineY = Centre.Y + TitleSize.Y * 0.55f;
		for (const float Side : { -1.0f, 1.0f })
		{
			const float Inner = Centre.X + Side * (TitleSize.X * 0.5f + 22.0f);
			const float Outer = Inner + Side * 170.0f * Grow;
			DrawRule(FVector2f(Inner, LineY), FVector2f(Outer, LineY), Faded(GoldColor, Alpha * 0.9f), 1.5f);
			DrawBox(FVector2f(Inner - 3.0f, LineY - 3.0f), FVector2f(6.0f, 6.0f), Faded(BannerAccent, Alpha), LineLayer);
		}

		float BelowY = Centre.Y + TitleSize.Y + 6.0f;
		if (!BannerDetail.IsEmpty())
		{
			BelowY += DrawLabel(BannerDetail.ToString(), DetailFont, FVector2f(Centre.X, BelowY), Faded(TextColor, Alpha * 0.85f), 0.5f).Y + 2.0f;
		}
		if (!BannerReward.IsEmpty())
		{
			DrawLabel(BannerReward.ToString(), DetailFont, FVector2f(Centre.X, BelowY), Faded(GoldColor, Alpha), 0.5f);
		}
	}

	//~ Toasts, left of centre-height, newest on top
	const FSlateFontInfo ToastFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	const FSlateFontInfo ToastDetailFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	float ToastTop = ScreenSize.Y * 0.38f;
	for (const FToast& Toast : Toasts)
	{
		const float ToastAge = static_cast<float>(Now - Toast.Start);
		if (ToastAge < 0.0f || ToastAge >= ToastDuration)
		{
			continue;
		}
		const float In = FMath::Clamp(ToastAge / 0.3f, 0.0f, 1.0f);
		const float Out = FMath::Clamp((ToastDuration - ToastAge) / 0.4f, 0.0f, 1.0f);
		const float Alpha = FMath::Min(In, Out);
		const FString Title = Toast.Title.ToString();
		const FString Detail = Toast.Detail.ToString();
		const FVector2f TitleSize = MeasureBannerText(Title, ToastFont);
		const FVector2f DetailSize = Detail.IsEmpty() ? FVector2f::ZeroVector : MeasureBannerText(Detail, ToastDetailFont);
		const FVector2f BoxSize(FMath::Max(TitleSize.X, DetailSize.X) + 40.0f, TitleSize.Y + DetailSize.Y + 16.0f);
		// Slides in from the left edge
		const float Left = 36.0f - (1.0f - FMath::InterpEaseOut(0.0f, 1.0f, In, 3.0f)) * 60.0f;
		const FVector2f TopLeft(Left, ToastTop);
		DrawBox(TopLeft, BoxSize, Faded(FLinearColor(PanelColor.R, PanelColor.G, PanelColor.B, 0.82f), Alpha), BackLayer);
		DrawBox(TopLeft, FVector2f(4.0f, BoxSize.Y), Faded(Toast.Colour, Alpha), LineLayer);
		DrawLabel(Title, ToastFont, TopLeft + FVector2f(18.0f, 7.0f), Faded(Toast.Colour, Alpha), 0.0f);
		if (!Detail.IsEmpty())
		{
			DrawLabel(Detail, ToastDetailFont, TopLeft + FVector2f(18.0f, 9.0f + TitleSize.Y), Faded(TextColor, Alpha * 0.8f), 0.0f);
		}
		ToastTop += BoxSize.Y + 8.0f;
	}
	return TextLayer + 2;
}
