// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondWorldMapWidget.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "World/BeyondRegionDefinition.h"
#include "World/BeyondRegionVolume.h"
#include "World/BeyondWorldInfo.h"
#include "World/BeyondWorldMapData.h"

#define LOCTEXT_NAMESPACE "BeyondWorldMap"

namespace BeyondWorldMapLocal
{
	FVector2f MeasureMapText(const FString& Text, const FSlateFontInfo& Font)
	{
		FSlateRenderer* Renderer = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetRenderer() : nullptr;
		if (!Renderer)
		{
			return FVector2f(Text.Len() * Font.Size * 0.6f, Font.Size * 1.2f);
		}
		return FVector2f(Renderer->GetFontMeasureService()->Measure(Text, Font));
	}

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}

	FString SpacedCaps(const FString& Text)
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

	// Screen pixels within which a click picks a marker
	constexpr float PickRadius = 22.0f;
}

UBeyondWorldMapWidget::UBeyondWorldMapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	RoundBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	RoundBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	RoundBrush.TintColor = FSlateColor(FLinearColor::White);
}

void UBeyondWorldMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
	Refresh();
}

void UBeyondWorldMapWidget::Refresh()
{
	const UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this);
	const FName Selected = GetSelectedId();
	Markers = WorldSubsystem ? WorldSubsystem->GetMapMarkers() : TArray<FBeyondMapMarker>();
	bConfirming = false;
	Message = FText::GetEmpty();

	// Regions: outlines and fog
	Outlines.Reset();
	if (WorldSubsystem)
	{
		const UBeyondPartyComponent* Party = UBeyondWorldSubsystem::GetParty(this);
		for (const ABeyondRegionVolume* Volume : WorldSubsystem->GetRegionVolumes())
		{
			if (!Volume->Region || Volume->Region->Kind != EBeyondRegionKind::Region)
			{
				continue;
			}
			FRegionOutline& Outline = Outlines.AddDefaulted_GetRef();
			Outline.Id = Volume->Region->RegionId;
			Outline.Points = Volume->GetPolygon();
			Outline.Colour = Volume->Region->MapColour;
			Outline.bDiscovered = Party && Party->IsRegionDiscovered(Outline.Id);
		}
	}

	// The picture and the rectangle it covers; without one, a square around everything there is
	const ABeyondWorldInfo* Info = WorldSubsystem ? WorldSubsystem->GetWorldInfo() : nullptr;
	const UBeyondWorldMapData* Data = Info ? Info->MapData.Get() : nullptr;
	MapTexture = Data ? Data->MapTexture.Get() : nullptr;
	FogMasks.Reset();
	if (Data)
	{
		for (const TPair<FName, TObjectPtr<UTexture2D>>& Mask : Data->RegionFogMasks)
		{
			FogMasks.Add(Mask.Key, Mask.Value);
		}
	}
	Title = Info && !Info->WorldName.IsEmpty() ? Info->WorldName : (Data && !Data->Title.IsEmpty() ? Data->Title : LOCTEXT("Title", "The Wandering Dominion"));

	if (Data)
	{
		WorldMin = Data->WorldMin;
		WorldMax = Data->WorldMax;
	}
	else
	{
		FBox2D Box(ForceInit);
		for (const FBeyondMapMarker& Marker : Markers)
		{
			Box += FVector2D(Marker.Location.X, Marker.Location.Y);
		}
		for (const FRegionOutline& Outline : Outlines)
		{
			for (const FVector2D& Point : Outline.Points)
			{
				Box += Point;
			}
		}
		if (!Box.bIsValid)
		{
			Box = FBox2D(FVector2D(-5000.0), FVector2D(5000.0));
		}
		const FVector2D Centre = Box.GetCenter();
		const double Half = FMath::Max(FMath::Max(Box.GetExtent().X, Box.GetExtent().Y) * 1.2, 2000.0);
		WorldMin = Centre - FVector2D(Half);
		WorldMax = Centre + FVector2D(Half);
	}

	// Start on the party, zoomed in a little on a big map
	CurrentPlace = FText::GetEmpty();
	if (WorldSubsystem)
	{
		const UBeyondRegionDefinition* Village = WorldSubsystem->GetCurrentVillage();
		const UBeyondRegionDefinition* Region = WorldSubsystem->GetCurrentRegion();
		CurrentPlace = Village ? Village->GetDisplayNameOrId() : (Region ? Region->GetDisplayNameOrId() : FText::GetEmpty());
	}
	Zoom = Data ? 2.0f : 1.0f;
	ViewCentre = (WorldMin + WorldMax) * 0.5;
	for (const FBeyondMapMarker& Marker : Markers)
	{
		if (Marker.Kind == EBeyondMapMarkerKind::Leader)
		{
			ViewCentre = FVector2D(Marker.Location.X, Marker.Location.Y);
		}
	}
	ClampView();

	SelectedIndex = INDEX_NONE;
	HoveredIndex = INDEX_NONE;
	if (!Selected.IsNone())
	{
		SelectMarker(Selected);
	}
}

//~ Selection and travel

bool UBeyondWorldMapWidget::IsSelectable(const FBeyondMapMarker& Marker) const
{
	switch (Marker.Kind)
	{
	case EBeyondMapMarkerKind::Waystone:
	case EBeyondMapMarkerKind::Village:
	case EBeyondMapMarkerKind::Place:
	case EBeyondMapMarkerKind::Arena:
		return Marker.bDiscovered;
	default:
		return false;
	}
}

void UBeyondWorldMapWidget::SelectIndex(int32 Index)
{
	if (Index != SelectedIndex)
	{
		bConfirming = false;
		Message = FText::GetEmpty();
	}
	SelectedIndex = Markers.IsValidIndex(Index) ? Index : INDEX_NONE;
}

bool UBeyondWorldMapWidget::SelectMarker(FName Id)
{
	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		if (Markers[Index].Id == Id && IsSelectable(Markers[Index]))
		{
			SelectIndex(Index);
			return true;
		}
	}
	return false;
}

FName UBeyondWorldMapWidget::GetSelectedId() const
{
	return Markers.IsValidIndex(SelectedIndex) ? Markers[SelectedIndex].Id : NAME_None;
}

void UBeyondWorldMapWidget::CancelConfirm()
{
	bConfirming = false;
}

bool UBeyondWorldMapWidget::ConfirmSelection()
{
	if (!Markers.IsValidIndex(SelectedIndex))
	{
		return false;
	}
	const FBeyondMapMarker Marker = Markers[SelectedIndex];
	if (Marker.Kind != EBeyondMapMarkerKind::Waystone || !Marker.bActive)
	{
		Message = Marker.Kind == EBeyondMapMarkerKind::Waystone ? LOCTEXT("NotAttuned", "Attune this waystone first (walk up to it and press F)")
			: LOCTEXT("OnlyWaystones", "You can only travel to attuned waystones");
		return false;
	}
	UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this);
	FText WhyNot;
	if (!WorldSubsystem || !WorldSubsystem->CanFastTravel(Marker.Id, WhyNot))
	{
		Message = WhyNot;
		bConfirming = false;
		return false;
	}
	if (!bConfirming)
	{
		bConfirming = true;
		Message = FText::GetEmpty();
		return true;
	}
	bConfirming = false;
	// Closing first unpauses the game, then the party goes
	Close();
	return WorldSubsystem->FastTravelTo(Marker.Id);
}

void UBeyondWorldMapWidget::Close()
{
	bConfirming = false;
	if (ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetOwningPlayer()))
	{
		PC->CloseWorldMap();
	}
	else
	{
		RemoveFromParent();
	}
}

void UBeyondWorldMapWidget::MoveSelection(const FVector2D& Direction)
{
	// The nearest selectable marker roughly that way from the selection (or the middle of the view)
	const FVector2D From = Markers.IsValidIndex(SelectedIndex)
		? FVector2D(Markers[SelectedIndex].Location.X, Markers[SelectedIndex].Location.Y) : ViewCentre;
	int32 Best = INDEX_NONE;
	double BestScore = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		if (Index == SelectedIndex || !IsSelectable(Markers[Index]))
		{
			continue;
		}
		const FVector2D To = FVector2D(Markers[Index].Location.X, Markers[Index].Location.Y) - From;
		const double Distance = To.Size();
		if (Distance < 1.0)
		{
			continue;
		}
		const double Dot = FVector2D::DotProduct(To / Distance, Direction);
		if (Dot < 0.35)
		{
			continue;
		}
		const double Score = Distance * (2.0 - Dot);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Index;
		}
	}
	if (Best != INDEX_NONE)
	{
		SelectIndex(Best);
		// Keep it in view
		const FVector2D Location(Markers[Best].Location.X, Markers[Best].Location.Y);
		const FVector2D Half = GetViewSpan() * 0.4;
		if (FMath::Abs(Location.X - ViewCentre.X) > Half.X || FMath::Abs(Location.Y - ViewCentre.Y) > Half.Y)
		{
			ViewCentre = Location;
			ClampView();
		}
	}
}

void UBeyondWorldMapWidget::SetZoom(float NewZoom)
{
	Zoom = FMath::Clamp(NewZoom, 1.0f, MaxZoom);
	ClampView();
}

//~ Geometry

void UBeyondWorldMapWidget::GetMapRect(const FVector2f& LocalSize, FVector2f& OutTopLeft, FVector2f& OutSize) const
{
	const float Side = FMath::Max(FMath::Min(LocalSize.X - 120.0f, LocalSize.Y - 190.0f), 100.0f);
	OutSize = FVector2f(Side, Side);
	OutTopLeft = FVector2f((LocalSize.X - Side) * 0.5f, 92.0f + (LocalSize.Y - 190.0f - Side) * 0.5f);
}

FVector2D UBeyondWorldMapWidget::GetViewSpan() const
{
	return (WorldMax - WorldMin) / FMath::Max(Zoom, 1.0f);
}

void UBeyondWorldMapWidget::ClampView()
{
	const FVector2D Half = GetViewSpan() * 0.5;
	ViewCentre.X = FMath::Clamp(ViewCentre.X, WorldMin.X + Half.X, WorldMax.X - Half.X);
	ViewCentre.Y = FMath::Clamp(ViewCentre.Y, WorldMin.Y + Half.Y, WorldMax.Y - Half.Y);
}

FVector2f UBeyondWorldMapWidget::WorldToLocal(const FVector& World, const FVector2f& LocalSize) const
{
	FVector2f TopLeft, Size;
	GetMapRect(LocalSize, TopLeft, Size);
	const FVector2D Span = GetViewSpan();
	const FVector2D ViewMin = ViewCentre - Span * 0.5;
	return FVector2f(TopLeft.X + static_cast<float>((World.X - ViewMin.X) / Span.X) * Size.X,
		TopLeft.Y + static_cast<float>((World.Y - ViewMin.Y) / Span.Y) * Size.Y);
}

FVector2D UBeyondWorldMapWidget::LocalToWorld(const FVector2f& Local, const FVector2f& LocalSize) const
{
	FVector2f TopLeft, Size;
	GetMapRect(LocalSize, TopLeft, Size);
	const FVector2D Span = GetViewSpan();
	const FVector2D ViewMin = ViewCentre - Span * 0.5;
	return FVector2D(ViewMin.X + (Local.X - TopLeft.X) / Size.X * Span.X, ViewMin.Y + (Local.Y - TopLeft.Y) / Size.Y * Span.Y);
}

int32 UBeyondWorldMapWidget::FindMarkerAt(const FVector2f& Local, const FVector2f& LocalSize) const
{
	int32 Best = INDEX_NONE;
	float BestDistance = BeyondWorldMapLocal::PickRadius;
	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		if (!IsSelectable(Markers[Index]))
		{
			continue;
		}
		const float Distance = FVector2f::Distance(WorldToLocal(Markers[Index].Location, LocalSize), Local);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Index;
		}
	}
	return Best;
}

//~ Input

FReply UBeyondWorldMapWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetOwningPlayer());
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		if (bConfirming)
		{
			CancelConfirm();
		}
		else
		{
			Close();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::M || Key == EKeys::Gamepad_Special_Left || Key == EKeys::Gamepad_Special_Right)
	{
		Close();
		return FReply::Handled();
	}
	if (Key == EKeys::I && PC)
	{
		PC->OpenInventory();
		return FReply::Handled();
	}
	if (Key == EKeys::K && PC)
	{
		PC->OpenSkillTree();
		return FReply::Handled();
	}
	if (Key == EKeys::F || Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		ConfirmSelection();
		return FReply::Handled();
	}
	if (Key == EKeys::Q || Key == EKeys::Hyphen || Key == EKeys::Subtract || Key == EKeys::Gamepad_LeftShoulder)
	{
		SetZoom(Zoom / 1.4f);
		return FReply::Handled();
	}
	if (Key == EKeys::E || Key == EKeys::Equals || Key == EKeys::Add || Key == EKeys::Gamepad_RightShoulder)
	{
		SetZoom(Zoom * 1.4f);
		return FReply::Handled();
	}
	FVector2D Direction = FVector2D::ZeroVector;
	if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left) { Direction = FVector2D(-1.0, 0.0); }
	if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right) { Direction = FVector2D(1.0, 0.0); }
	if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up) { Direction = FVector2D(0.0, -1.0); }
	if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down) { Direction = FVector2D(0.0, 1.0); }
	if (!Direction.IsZero())
	{
		MoveSelection(Direction);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UBeyondWorldMapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		CancelConfirm();
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bMouseDown = true;
		bDragged = false;
		MouseDownPosition = FVector2f(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
		ViewCentreAtMouseDown = ViewCentre;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UBeyondWorldMapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !bMouseDown)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}
	bMouseDown = false;
	if (!bDragged)
	{
		const FVector2f Local(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
		const int32 Hit = FindMarkerAt(Local, FVector2f(InGeometry.GetLocalSize()));
		if (Hit != INDEX_NONE && Hit == SelectedIndex)
		{
			// Clicking the selected waystone asks, clicking again travels
			ConfirmSelection();
		}
		else if (Hit != INDEX_NONE)
		{
			SelectIndex(Hit);
			if (Markers[Hit].Kind == EBeyondMapMarkerKind::Waystone && Markers[Hit].bActive)
			{
				ConfirmSelection();
			}
		}
		else
		{
			SelectIndex(INDEX_NONE);
		}
	}
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UBeyondWorldMapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2f LocalSize(InGeometry.GetLocalSize());
	const FVector2f Local(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
	LastLocalSize = LocalSize;
	if (bMouseDown)
	{
		const FVector2f Delta = Local - MouseDownPosition;
		if (Delta.Size() > 6.0f)
		{
			bDragged = true;
		}
		if (bDragged)
		{
			FVector2f TopLeft, Size;
			GetMapRect(LocalSize, TopLeft, Size);
			const FVector2D Span = GetViewSpan();
			ViewCentre = ViewCentreAtMouseDown - FVector2D(Delta.X / Size.X * Span.X, Delta.Y / Size.Y * Span.Y);
			ClampView();
		}
		return FReply::Handled();
	}
	HoveredIndex = FindMarkerAt(Local, LocalSize);
	return FReply::Handled();
}

FReply UBeyondWorldMapWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Zoom around the cursor
	const FVector2f LocalSize(InGeometry.GetLocalSize());
	const FVector2f Local(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
	const FVector2D Before = LocalToWorld(Local, LocalSize);
	SetZoom(Zoom * (InMouseEvent.GetWheelDelta() > 0.0f ? 1.25f : 0.8f));
	const FVector2D After = LocalToWorld(Local, LocalSize);
	ViewCentre += Before - After;
	ClampView();
	return FReply::Handled();
}

//~ Drawing

int32 UBeyondWorldMapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace BeyondWorldMapLocal;
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 BackLayer = BaseLayer + 1;
	const int32 MapLayer = BaseLayer + 2;
	const int32 FogLayer = BaseLayer + 3;
	const int32 LineLayer = BaseLayer + 4;
	const int32 MarkLayer = BaseLayer + 5;
	const int32 TextLayer = BaseLayer + 7;
	const FVector2f LocalSize(AllottedGeometry.GetLocalSize());
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));

	FVector2f MapTopLeft, MapSize;
	GetMapRect(LocalSize, MapTopLeft, MapSize);
	const FSlateRect MapClip(MapTopLeft.X, MapTopLeft.Y, MapTopLeft.X + MapSize.X, MapTopLeft.Y + MapSize.Y);

	auto Box = [&](const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Tint, int32 Layer, const FSlateBrush* Brush = nullptr)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft)),
			Brush ? Brush : WhiteBrush, ESlateDrawEffect::None, Tint);
	};
	auto Dot = [&](const FVector2f& Centre, float Diameter, const FLinearColor& Tint, int32 Layer)
	{
		Box(Centre - FVector2f(Diameter * 0.5f), FVector2f(Diameter), Tint, Layer, &RoundBrush);
	};
	auto Lines = [&](TArray<FVector2f> Points, const FLinearColor& Tint, float Thickness, int32 Layer)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Tint, true, Thickness);
	};
	auto Label = [&](const FString& Text, const FSlateFontInfo& Font, const FVector2f& Position, const FLinearColor& Tint, float Align, const FLinearColor& Shadow)
	{
		const FVector2f Size = MeasureMapText(Text, Font);
		const FVector2f TopLeft(Position.X - Size.X * Align, Position.Y);
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft + FVector2f(1.5f, 1.5f))),
			Text, Font, ESlateDrawEffect::None, Shadow);
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer + 1, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft)),
			Text, Font, ESlateDrawEffect::None, Tint);
		return Size;
	};
	auto Diamond = [&](const FVector2f& Centre, float Radius, const FLinearColor& Tint, float Thickness, int32 Layer)
	{
		Lines({ Centre + FVector2f(0.0f, -Radius), Centre + FVector2f(Radius, 0.0f), Centre + FVector2f(0.0f, Radius), Centre + FVector2f(-Radius, 0.0f),
			Centre + FVector2f(0.0f, -Radius) }, Tint, Thickness, Layer);
	};
	auto InView = [&](const FVector2f& Point)
	{
		return Point.X >= MapClip.Left && Point.X <= MapClip.Right && Point.Y >= MapClip.Top && Point.Y <= MapClip.Bottom;
	};

	// Backdrop and frame
	Box(FVector2f::ZeroVector, LocalSize, BackdropColor, BackLayer);
	Box(MapTopLeft - FVector2f(6.0f), MapSize + FVector2f(12.0f), WithAlpha(GoldColor, 0.55f), BackLayer);

	// The picture (or parchment), showing the zoomed part
	const FVector2D Span = GetViewSpan();
	const FVector2D Full = WorldMax - WorldMin;
	const FVector2D UVMin = (ViewCentre - Span * 0.5 - WorldMin) / Full;
	const FVector2D UVMax = (ViewCentre + Span * 0.5 - WorldMin) / Full;
	if (MapTexture)
	{
		FSlateBrush MapBrush;
		MapBrush.SetResourceObject(MapTexture);
		MapBrush.ImageSize = FVector2D(MapTexture->GetSizeX(), MapTexture->GetSizeY());
		MapBrush.SetUVRegion(FBox2f(FVector2f(UVMin), FVector2f(UVMax)));
		Box(MapTopLeft, MapSize, FLinearColor::White, MapLayer, &MapBrush);
	}
	else
	{
		Box(MapTopLeft, MapSize, ParchmentColor, MapLayer);
	}

	OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry.GetLayoutBoundingRect(MapClip)));

	// Fog over regions not found yet: their baked mask, or a dim outline without one
	for (const FRegionOutline& Outline : Outlines)
	{
		const TObjectPtr<UTexture2D>* Mask = FogMasks.Find(Outline.Id);
		if (!Outline.bDiscovered && Mask && *Mask)
		{
			FSlateBrush MaskBrush;
			MaskBrush.SetResourceObject(*Mask);
			MaskBrush.SetUVRegion(FBox2f(FVector2f(UVMin), FVector2f(UVMax)));
			Box(MapTopLeft, MapSize, FogColor, FogLayer, &MaskBrush);
		}
		if (Outline.Points.Num() >= 3)
		{
			TArray<FVector2f> Points;
			for (const FVector2D& Point : Outline.Points)
			{
				Points.Add(WorldToLocal(FVector(Point.X, Point.Y, 0.0), LocalSize));
			}
			Points.Add(Points[0]);
			const FLinearColor Tint = Outline.bDiscovered ? WithAlpha(Outline.Colour, 0.8f) : WithAlpha(InkColor, 0.35f);
			Lines(MoveTemp(Points), Tint, Outline.bDiscovered ? 2.0f : 1.0f, LineLayer);
		}
	}

	const FSlateFontInfo RegionFont = FCoreStyle::GetDefaultFontStyle("Bold", 15);
	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const FLinearColor LabelShadow(0.0f, 0.0f, 0.0f, 0.6f);
	const FLinearColor InkLabel = MapTexture ? TextColor : InkColor;
	const FLinearColor InkShadow = MapTexture ? LabelShadow : FLinearColor(1.0f, 0.95f, 0.85f, 0.35f);

	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		const FBeyondMapMarker& Marker = Markers[Index];
		const FVector2f At = WorldToLocal(Marker.Location, LocalSize);
		if (!InView(At))
		{
			continue;
		}
		const bool bSelected = Index == SelectedIndex;
		const bool bHovered = Index == HoveredIndex;
		const bool bShowLabel = bSelected || bHovered || Zoom >= 2.5f;
		switch (Marker.Kind)
		{
		case EBeyondMapMarkerKind::Region:
		{
			const FString Text = Marker.bDiscovered ? SpacedCaps(Marker.Label.ToString()) : FString(TEXT("? ? ?"));
			const FVector2f Size = Label(Text, RegionFont, At, WithAlpha(InkLabel, Marker.bDiscovered ? 0.95f : 0.5f), 0.5f, InkShadow);
			if (Marker.bDiscovered && !Marker.Detail.IsEmpty())
			{
				Label(Marker.Detail.ToString(), SmallFont, At + FVector2f(0.0f, Size.Y), WithAlpha(InkLabel, 0.8f), 0.5f, InkShadow);
			}
			break;
		}
		case EBeyondMapMarkerKind::Village:
		case EBeyondMapMarkerKind::Area:
			if (Marker.bDiscovered)
			{
				Dot(At, Marker.Kind == EBeyondMapMarkerKind::Village ? 11.0f : 7.0f, InkColor, MarkLayer);
				Dot(At, Marker.Kind == EBeyondMapMarkerKind::Village ? 7.0f : 4.0f, Marker.Colour, MarkLayer + 1);
				Label(Marker.Label.ToString(), LabelFont, At + FVector2f(0.0f, 8.0f), InkLabel, 0.5f, InkShadow);
			}
			break;
		case EBeyondMapMarkerKind::Waystone:
			if (Marker.bDiscovered)
			{
				if (Marker.bActive)
				{
					Dot(At, 20.0f, WithAlpha(Marker.Colour, 0.35f), MarkLayer);
				}
				Diamond(At, 7.0f, Marker.bActive ? Marker.Colour : WithAlpha(InkColor, 0.7f), Marker.bActive ? 3.0f : 2.0f, MarkLayer + 1);
				if (bShowLabel)
				{
					Label(Marker.Label.ToString(), SmallFont, At + FVector2f(0.0f, 10.0f), InkLabel, 0.5f, InkShadow);
				}
			}
			break;
		case EBeyondMapMarkerKind::Place:
			if (Marker.bDiscovered)
			{
				Box(At - FVector2f(4.0f), FVector2f(8.0f), InkColor, MarkLayer);
				Box(At - FVector2f(2.5f), FVector2f(5.0f), Marker.Colour, MarkLayer + 1);
				if (bShowLabel)
				{
					Label(Marker.Label.ToString(), SmallFont, At + FVector2f(0.0f, 7.0f), InkLabel, 0.5f, InkShadow);
				}
			}
			else if (Marker.bActive)
			{
				Label(TEXT("?"), LabelFont, At - FVector2f(0.0f, 8.0f), WithAlpha(InkLabel, 0.7f), 0.5f, InkShadow);
			}
			break;
		case EBeyondMapMarkerKind::Arena:
			if (Marker.bDiscovered)
			{
				const FLinearColor Tint = Marker.bActive ? FLinearColor(0.45f, 0.45f, 0.45f, 1.0f) : Marker.Colour;
				Dot(At, 16.0f, InkColor, MarkLayer);
				Dot(At, 12.0f, Tint, MarkLayer + 1);
				if (Marker.bActive)
				{
					// Beaten: a tick
					Lines({ At + FVector2f(-4.0f, 0.0f), At + FVector2f(-1.0f, 3.5f), At + FVector2f(4.5f, -4.0f) }, TextColor, 2.0f, MarkLayer + 2);
				}
				else
				{
					// Crossed swords
					Lines({ At + FVector2f(-4.0f, -4.0f), At + FVector2f(4.0f, 4.0f) }, TextColor, 2.0f, MarkLayer + 2);
					Lines({ At + FVector2f(4.0f, -4.0f), At + FVector2f(-4.0f, 4.0f) }, TextColor, 2.0f, MarkLayer + 2);
				}
				if (bShowLabel)
				{
					Label(Marker.Label.ToString(), SmallFont, At + FVector2f(0.0f, 9.0f), InkLabel, 0.5f, InkShadow);
				}
			}
			break;
		case EBeyondMapMarkerKind::Leader:
		{
			const float Radians = FMath::DegreesToRadians(Marker.Yaw);
			const FVector2f Forward(FMath::Cos(Radians), FMath::Sin(Radians));
			const FVector2f Side(-Forward.Y, Forward.X);
			Dot(At, 22.0f, WithAlpha(Marker.Colour, 0.3f), MarkLayer);
			Lines({ At + Forward * 11.0f, At - Forward * 7.0f + Side * 7.0f, At - Forward * 3.0f, At - Forward * 7.0f - Side * 7.0f, At + Forward * 11.0f },
				Marker.Colour, 3.0f, MarkLayer + 2);
			break;
		}
		case EBeyondMapMarkerKind::Companion:
			Dot(At, 9.0f, InkColor, MarkLayer);
			Dot(At, 6.0f, Marker.Colour, MarkLayer + 1);
			break;
		}
		if (bSelected || (bHovered && IsSelectable(Marker)))
		{
			const float Ring = bSelected ? 26.0f : 22.0f;
			Diamond(At, Ring * 0.5f + 3.0f, WithAlpha(GoldColor, bSelected ? 1.0f : 0.6f), 1.5f, MarkLayer + 3);
		}
	}

	OutDrawElements.PopClip();

	// Title and where the party is
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 22);
	const FSlateFontInfo InfoFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
	Label(SpacedCaps(Title.ToString()), TitleFont, FVector2f(LocalSize.X * 0.5f, 26.0f), GoldColor, 0.5f, LabelShadow);
	if (!CurrentPlace.IsEmpty())
	{
		Label(FText::Format(LOCTEXT("YouAreIn", "You are in {0}"), CurrentPlace).ToString(), InfoFont, FVector2f(LocalSize.X * 0.5f, 62.0f), TextColor, 0.5f, LabelShadow);
	}

	// The selection: what it is and what F does
	const float PanelTop = MapTopLeft.Y + MapSize.Y + 14.0f;
	if (Markers.IsValidIndex(SelectedIndex))
	{
		const FBeyondMapMarker& Marker = Markers[SelectedIndex];
		FString Line = Marker.Label.ToString();
		if (!Marker.Detail.IsEmpty())
		{
			Line += FString::Printf(TEXT("  ·  %s"), *Marker.Detail.ToString());
		}
		FText Hint;
		if (Marker.Kind == EBeyondMapMarkerKind::Waystone)
		{
			Hint = bConfirming ? FText::Format(LOCTEXT("Ask", "Travel to {0}?   [F] Yes   [Esc] No"), Marker.Label)
				: Marker.bActive ? LOCTEXT("CanTravel", "Attuned   ·   [F] Travel") : LOCTEXT("NotAttunedHint", "Not attuned");
		}
		else if (Marker.Kind == EBeyondMapMarkerKind::Arena)
		{
			Hint = Marker.bActive ? LOCTEXT("Beaten", "Defeated") : LOCTEXT("Danger", "A powerful foe waits here");
		}
		const FVector2f LineSize = MeasureMapText(Line, InfoFont);
		const FVector2f HintSize = Hint.IsEmpty() ? FVector2f::ZeroVector : MeasureMapText(Hint.ToString(), InfoFont);
		const FVector2f PanelSize(FMath::Max(LineSize.X, HintSize.X) + 48.0f, LineSize.Y + HintSize.Y + 20.0f);
		Box(FVector2f(LocalSize.X * 0.5f - PanelSize.X * 0.5f, PanelTop), PanelSize, PanelColor, BackLayer + 1);
		Label(Line, InfoFont, FVector2f(LocalSize.X * 0.5f, PanelTop + 8.0f), TextColor, 0.5f, LabelShadow);
		if (!Hint.IsEmpty())
		{
			Label(Hint.ToString(), InfoFont, FVector2f(LocalSize.X * 0.5f, PanelTop + 10.0f + LineSize.Y), bConfirming ? GoldColor : WithAlpha(TextColor, 0.8f), 0.5f, LabelShadow);
		}
	}
	if (!Message.IsEmpty())
	{
		Label(Message.ToString(), InfoFont, FVector2f(LocalSize.X * 0.5f, PanelTop + 64.0f), WarningColor, 0.5f, LabelShadow);
	}
	Label(LOCTEXT("Keys", "[M] Close    [Wheel / Q E] Zoom    [Drag] Pan    [Arrows] Pick    [F / Click] Travel").ToString(), SmallFont,
		FVector2f(24.0f, LocalSize.Y - 28.0f), WithAlpha(TextColor, 0.7f), 0.0f, LabelShadow);
	return TextLayer + 2;
}

#undef LOCTEXT_NAMESPACE
