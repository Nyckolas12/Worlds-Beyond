// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondCrosshairWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr int32 CrosshairRingSegments = 40;
}

void UBeyondCrosshairWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Never eats clicks
	SetVisibility(ESlateVisibility::HitTestInvisible);
	CurrentGap = Gap;
}

void UBeyondCrosshairWidget::SetAimState(bool bOverHostile, bool bAiming, float Speed01)
{
	bHostile = bOverHostile;
	bAimingNow = bAiming;
	Speed = FMath::Clamp(Speed01, 0.0f, 1.0f);
}

void UBeyondCrosshairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	HostileBlend = FMath::FInterpTo(HostileBlend, bHostile ? 1.0f : 0.0f, InDeltaTime, BlendSpeed);

	float WantedGap = Gap + MovingGap * Speed;
	if (bAimingNow || bHostile)
	{
		WantedGap *= FocusedGapScale;
	}
	CurrentGap = FMath::FInterpTo(CurrentGap, WantedGap, InDeltaTime, BlendSpeed);
}

int32 UBeyondCrosshairWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 ShadowLayer = BaseLayer + 1;
	const int32 ShapeLayer = BaseLayer + 2;

	const FVector2f Centre = FVector2f(AllottedGeometry.GetLocalSize()) * 0.5f;
	const FLinearColor Color = FMath::Lerp(IdleColor, HostileColor, HostileBlend);
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));

	auto DrawBox = [&](const FVector2f& Middle, float Size, const FLinearColor& Tint, int32 Layer)
	{
		const FVector2f BoxSize(Size, Size);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(Middle - BoxSize * 0.5f)),
			WhiteBrush, ESlateDrawEffect::None, Tint);
	};
	auto DrawLine = [&](TArray<FVector2f> Points, const FLinearColor& Tint, float LineThickness, int32 Layer)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points),
			ESlateDrawEffect::None, Tint, true, LineThickness);
	};

	// Centre dot
	DrawBox(Centre, DotSize + 2.0f, ShadowColor, ShadowLayer);
	DrawBox(Centre, DotSize, Color, ShapeLayer);

	// Four ticks
	static const FVector2f Directions[] = { FVector2f(1.0f, 0.0f), FVector2f(-1.0f, 0.0f), FVector2f(0.0f, 1.0f), FVector2f(0.0f, -1.0f) };
	for (const FVector2f& Direction : Directions)
	{
		TArray<FVector2f> Tick = { Centre + Direction * CurrentGap, Centre + Direction * (CurrentGap + TickLength) };
		DrawLine(Tick, ShadowColor, Thickness + 2.0f, ShadowLayer);
		DrawLine(MoveTemp(Tick), Color, Thickness, ShapeLayer);
	}

	// Ring: faint when idle, solid and tighter over an enemy
	if (RingRadius > 0.0f)
	{
		const float Radius = RingRadius * FMath::Lerp(1.0f, 0.8f, HostileBlend) + (CurrentGap - Gap);
		TArray<FVector2f> Ring;
		Ring.Reserve(CrosshairRingSegments + 1);
		for (int32 Index = 0; Index <= CrosshairRingSegments; ++Index)
		{
			const float Angle = 2.0f * PI * Index / CrosshairRingSegments;
			Ring.Add(Centre + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FLinearColor RingColor = Color;
		RingColor.A *= FMath::Lerp(0.35f, 1.0f, HostileBlend);
		FLinearColor RingShadow = ShadowColor;
		RingShadow.A *= FMath::Lerp(0.5f, 1.0f, HostileBlend);
		DrawLine(Ring, RingShadow, Thickness + 1.5f, ShadowLayer);
		DrawLine(MoveTemp(Ring), RingColor, FMath::Max(1.0f, Thickness - 0.5f), ShapeLayer);
	}

	return ShapeLayer;
}
