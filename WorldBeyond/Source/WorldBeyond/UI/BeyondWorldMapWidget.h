// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "World/BeyondWorldSubsystem.h"
#include "BeyondWorldMapWidget.generated.h"

class UTexture2D;

/**
 * The world map (Plan 5, M), drawn in C++ like the other menus: the map's picture (DA_WorldMap; plain parchment fitted
 * to the markers when the map has none), regions not found yet under fog, villages, places, boss arenas (ticked once
 * beaten), waystones (bright once attuned) and the party. Wheel / Q E zoom, drag pans, arrows / WASD / the d-pad hop
 * between markers. Picking an attuned waystone and pressing F / Enter / clicking it again asks, a second time travels
 * (not in a fight, a talk, with a demigod down or inside a sealed arena). M / Esc close it, I and K switch screens.
 * Opened paused by ABeyondPlayerController.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondWorldMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBeyondWorldMapWidget(const FObjectInitializer& ObjectInitializer);

	// Reads the markers and the map again (on every open)
	UFUNCTION(BlueprintCallable, Category = "Map")
	void Refresh();

	// Selects the marker with this id (waystones, villages, places, arenas); false if there is none to select
	UFUNCTION(BlueprintCallable, Category = "Map")
	bool SelectMarker(FName Id);

	// On an attuned waystone: the first call asks, the second travels (and closes the map)
	UFUNCTION(BlueprintCallable, Category = "Map")
	bool ConfirmSelection();

	UFUNCTION(BlueprintCallable, Category = "Map")
	void CancelConfirm();

	UFUNCTION(BlueprintPure, Category = "Map")
	FName GetSelectedId() const;

	UFUNCTION(BlueprintPure, Category = "Map")
	bool IsConfirming() const { return bConfirming; }

	// Why the last travel was refused
	UFUNCTION(BlueprintPure, Category = "Map")
	FText GetMessage() const { return Message; }

	UFUNCTION(BlueprintPure, Category = "Map")
	TArray<FBeyondMapMarker> GetMarkers() const { return Markers; }

	UFUNCTION(BlueprintPure, Category = "Map")
	UTexture2D* GetMapTexture() const { return MapTexture; }

	// 1 shows the whole map
	UFUNCTION(BlueprintCallable, Category = "Map")
	void SetZoom(float NewZoom);

	UFUNCTION(BlueprintPure, Category = "Map")
	float GetZoom() const { return Zoom; }

	UFUNCTION(BlueprintCallable, Category = "Map")
	void Close();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "1"))
	float MaxZoom = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor BackdropColor = FLinearColor(0.0f, 0.0f, 0.02f, 0.72f);

	// The map without a picture
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor ParchmentColor = FLinearColor(0.78f, 0.7f, 0.53f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor InkColor = FLinearColor(0.16f, 0.11f, 0.07f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor FogColor = FLinearColor(0.07f, 0.07f, 0.09f, 0.78f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor GoldColor = FLinearColor(1.0f, 0.78f, 0.3f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor TextColor = FLinearColor(0.97f, 0.96f, 0.92f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor PanelColor = FLinearColor(0.02f, 0.03f, 0.07f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor WarningColor = FLinearColor(1.0f, 0.42f, 0.35f, 1.0f);

protected:
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	struct FRegionOutline
	{
		FName Id;
		TArray<FVector2D> Points;
		FLinearColor Colour = FLinearColor::White;
		bool bDiscovered = false;
	};

	// The square the map is drawn in, for a widget of LocalSize
	void GetMapRect(const FVector2f& LocalSize, FVector2f& OutTopLeft, FVector2f& OutSize) const;
	FVector2D GetViewSpan() const;
	FVector2f WorldToLocal(const FVector& World, const FVector2f& LocalSize) const;
	FVector2D LocalToWorld(const FVector2f& Local, const FVector2f& LocalSize) const;
	void ClampView();
	bool IsSelectable(const FBeyondMapMarker& Marker) const;
	int32 FindMarkerAt(const FVector2f& Local, const FVector2f& LocalSize) const;
	void MoveSelection(const FVector2D& Direction);
	void SelectIndex(int32 Index);

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MapTexture;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> FogMasks;

	TArray<FBeyondMapMarker> Markers;
	TArray<FRegionOutline> Outlines;
	FText Title;
	FText CurrentPlace;

	FVector2D WorldMin = FVector2D(-10000.0);
	FVector2D WorldMax = FVector2D(10000.0);
	FVector2D ViewCentre = FVector2D::ZeroVector;
	float Zoom = 1.0f;

	int32 SelectedIndex = INDEX_NONE;
	int32 HoveredIndex = INDEX_NONE;
	bool bConfirming = false;
	FText Message;

	bool bMouseDown = false;
	bool bDragged = false;
	FVector2f MouseDownPosition = FVector2f::ZeroVector;
	FVector2D ViewCentreAtMouseDown = FVector2D::ZeroVector;
	FVector2f LastLocalSize = FVector2f(1920.0f, 1080.0f);

	FSlateBrush RoundBrush;
};
