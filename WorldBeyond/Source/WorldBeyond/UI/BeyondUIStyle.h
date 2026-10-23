// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondElement.h"
#include "Fonts/SlateFontInfo.h"
#include "Items/BeyondItemTypes.h"
#include "Layout/Geometry.h"
#include "Styling/SlateBrush.h"
#include "Subsystems/EngineSubsystem.h"
#include "UI/BeyondUIGlyphs.h"
#include "BeyondUIStyle.generated.h"

class FSlateWindowElementList;
class UBeyondUIArtSet;
class UFont;
class UTexture2D;

/** Text styles of the menus and HUD (Plan 6) */
UENUM()
enum class EBeyondFont : uint8
{
	// Big spaced small caps: "CHARACTER", page titles (Cinzel)
	Display,
	// Section headers and tabs (Cinzel, spaced)
	Heading,
	HeadingBold,
	// Figures (Cinzel's lining numerals)
	Number,
	// Key caps (Cinzel Bold)
	Key,
	// Running text (EB Garamond)
	Body,
	BodyItalic,
	BodyBold
};

/**
 * Holds what the UI paints with for the whole session: the two font families, the art set, loaded textures and the
 * brushes made for them (so brush pointers stay valid while Slate paints). Everything loads on first use.
 */
UCLASS()
class WORLDBEYOND_API UBeyondUISubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	// Null only before the engine is up
	static UBeyondUISubsystem* Get();

	UFont* GetHeadingFont();
	UFont* GetBodyFont();
	const UBeyondUIArtSet* GetArtSet();

	// Loads a texture and keeps it loaded
	UTexture2D* LoadTexture(const TSoftObjectPtr<UTexture2D>& Texture);

	// A stable image brush for a texture (null for null)
	const FSlateBrush* GetTextureBrush(UTexture2D* Texture);

	// A rounded-box brush (Radius <= 0: pill, half the height)
	const FSlateBrush* GetRoundedBrush(float Radius);

	// Forget fonts and art so they load again (after a pass made new ones)
	void Reload();

private:
	void EnsureLoaded();

	UPROPERTY(Transient)
	TObjectPtr<UFont> HeadingFont;

	UPROPERTY(Transient)
	TObjectPtr<UFont> BodyFont;

	UPROPERTY(Transient)
	TObjectPtr<UBeyondUIArtSet> ArtSet;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> Textures;

	TMap<const UTexture2D*, TUniquePtr<FSlateBrush>> TextureBrushes;
	TMap<int32, TUniquePtr<FSlateBrush>> RoundedBrushes;
	bool bLoaded = false;
};

namespace BeyondUI
{
	//~ Colours (the mockups' sRGB values, converted once)

	struct FPalette
	{
		FLinearColor Backdrop;
		FLinearColor Panel;
		FLinearColor PanelTop;
		FLinearColor PanelRaised;
		FLinearColor Gold;
		FLinearColor GoldBright;
		FLinearColor GoldDim;
		FLinearColor Silver;
		FLinearColor Text;
		FLinearColor TextDim;
		FLinearColor TextFaint;
		FLinearColor Blue;
		FLinearColor BlueBright;
		FLinearColor BlueDeep;
		FLinearColor Amber;
		FLinearColor Violet;
		FLinearColor Plus;
		FLinearColor Minus;
		FLinearColor Health;
		FLinearColor Stamina;
		FLinearColor Shadow;
	};

	WORLDBEYOND_API const FPalette& Palette();
	WORLDBEYOND_API FLinearColor FromHex(uint32 RGB, float Alpha = 1.0f);
	WORLDBEYOND_API FLinearColor WithAlpha(FLinearColor Color, float Opacity);
	WORLDBEYOND_API FLinearColor TierColor(EBeyondItemTier Tier);
	WORLDBEYOND_API FLinearColor ElementColor(EBeyondElement Element);
	WORLDBEYOND_API FText ElementName(EBeyondElement Element);
	WORLDBEYOND_API EBeyondGlyph ElementGlyph(EBeyondElement Element);
	WORLDBEYOND_API EBeyondGlyph SlotGlyph(EBeyondItemSlot ItemSlot, bool bStaff = false);

	//~ Text

	// Loads the fonts and the art set now (menus call it on construct so the first paint doesn't hitch)
	WORLDBEYOND_API void Preload();
	WORLDBEYOND_API FSlateFontInfo Font(EBeyondFont Style, float Size);
	// Size of Text in Font; without a renderer (tests under -nullrhi) a per-family estimate
	WORLDBEYOND_API FVector2f MeasureText(const FString& Text, const FSlateFontInfo& Font);
	// Text split at spaces into lines no wider than MaxWidth
	WORLDBEYOND_API TArray<FString> WrapText(const FString& Text, const FSlateFontInfo& Font, float MaxWidth);
	// 20,340
	WORLDBEYOND_API FString FormatNumber(int64 Value);
	// 18% (Decimals 1: 4.5%)
	WORLDBEYOND_API FString FormatPercent(float Percent, int32 Decimals = 0);
	// 32:18:45
	WORLDBEYOND_API FString FormatPlaytime(double Seconds);

	//~ Layout: everything is placed on a 1920 x 1080 page, scaled uniformly to the screen and centred

	struct WORLDBEYOND_API FDesignSpace
	{
		FVector2f DesignSize = FVector2f(1920.0f, 1080.0f);
		FVector2f LocalSize = FVector2f(1920.0f, 1080.0f);
		float Scale = 1.0f;
		FVector2f Offset = FVector2f::ZeroVector;

		// The reference resolution from the UI settings
		static FDesignSpace Fit(const FVector2f& InLocalSize);
		static FDesignSpace Fit(const FVector2f& InLocalSize, const FVector2f& InDesignSize);

		FVector2f ToDesign(const FVector2f& Local) const;
		FVector2f ToLocal(const FVector2f& Design) const;
		// A child geometry measured in design units
		FGeometry MakeGeometry(const FGeometry& Allotted) const;
		// The whole widget in design units (reaches past the page on wider or taller screens)
		FVector2f BleedTopLeft() const;
		FVector2f BleedSize() const;
	};

	//~ Painting

	/** How an item square looks */
	struct FItemFrameStyle
	{
		EBeyondItemTier Tier = EBeyondItemTier::Common;
		bool bEmpty = false;
		bool bSelected = false;
		bool bHovered = false;
		bool bLocked = false;
		bool bJunk = false;
		// Drawn in the corner when bShowUpgrade
		int32 UpgradeLevel = 0;
		bool bShowUpgrade = false;
		// The star badge in the top-left corner
		bool bBadge = true;
		UTexture2D* Icon = nullptr;
		// Drawn when there is no icon
		EBeyondGlyph Glyph = EBeyondGlyph::None;
		float Opacity = 1.0f;
	};

	/**
	 * Draws the shared look into a widget's paint (coordinates in the geometry it is given, usually the design space).
	 * Layer arguments are offsets from the base layer; GetMaxLayer is what NativePaint should return.
	 */
	class WORLDBEYOND_API FPainter
	{
	public:
		FPainter(const FGeometry& InGeometry, FSlateWindowElementList& InElements, int32 InBaseLayer, float InOpacity = 1.0f);

		const FGeometry& GetGeometry() const { return Geometry; }
		int32 GetMaxLayer() const { return MaxLayer; }
		float AnimTime = 0.0f;

		//~ Shapes
		void Box(const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Color, int32 Layer) const;
		void Brush(const FSlateBrush* InBrush, const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Tint, int32 Layer) const;
		void Image(UTexture2D* Texture, const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Tint, int32 Layer) const;
		void ImageCentred(UTexture2D* Texture, const FVector2f& Centre, const FVector2f& Size, const FLinearColor& Tint, int32 Layer) const;
		// Fills the rectangle keeping the picture's aspect (cropping the overflow); FocusY 0 keeps the top, 0.5 the middle
		void ImageCover(UTexture2D* Texture, const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Tint, int32 Layer, float FocusY = 0.5f) const;
		void Rounded(const FVector2f& TopLeft, const FVector2f& Size, float Radius, const FLinearColor& Color, int32 Layer) const;
		void Disc(const FVector2f& Centre, float Radius, const FLinearColor& Color, int32 Layer) const;
		void GradientH(const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Left, const FLinearColor& Right, int32 Layer) const;
		void GradientV(const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Top, const FLinearColor& Bottom, int32 Layer) const;
		// Clear at both ends, Middle in the centre
		void GradientFadeH(const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Middle, int32 Layer) const;
		void Line(const FVector2f& From, const FVector2f& To, const FLinearColor& Color, float Thickness, int32 Layer) const;
		void Polyline(TArray<FVector2f> Points, const FLinearColor& Color, float Thickness, int32 Layer, bool bClosed = false) const;
		void RectOutline(const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Color, float Thickness, int32 Layer) const;
		// Clockwise from StartAngle (radians, 0 = right, -PI/2 = top)
		void Ring(const FVector2f& Centre, float Radius, const FLinearColor& Color, float Thickness, int32 Layer, float Fraction = 1.0f,
			float StartAngle = -HALF_PI) const;
		// A box of LocalSize centred on Centre, transformed by Matrix (rotation, scale, shear)
		void Transformed(const FSlateBrush* InBrush, const FVector2f& Centre, const FVector2f& LocalSize, const FMatrix2x2& Matrix, const FLinearColor& Color,
			int32 Layer) const;
		void Diamond(const FVector2f& Centre, float HalfWidth, float HalfHeight, const FLinearColor& Color, int32 Layer) const;
		void DiamondOutline(const FVector2f& Centre, float HalfWidth, float HalfHeight, const FLinearColor& Color, float Thickness, int32 Layer) const;
		// A four-pointed star (the mockups' spark)
		void Sparkle(const FVector2f& Centre, float Radius, const FLinearColor& Color, int32 Layer) const;
		// Top edge shifted right by Skew / 2, bottom edge left by Skew / 2
		void Parallelogram(const FVector2f& TopLeft, const FVector2f& Size, float Skew, const FLinearColor& Top, const FLinearColor& Bottom, int32 Layer) const;
		void ParallelogramOutline(const FVector2f& TopLeft, const FVector2f& Size, float Skew, const FLinearColor& Color, float Thickness, int32 Layer) const;
		void Glow(const FVector2f& Centre, float Radius, const FLinearColor& Color, int32 Layer) const;
		void Glyph(EBeyondGlyph InGlyph, const FVector2f& Centre, float Size, const FLinearColor& Color, int32 Layer, float Thickness = 0.0f) const;

		//~ Text (shadowed); AlignX 0 left, 0.5 centre, 1 right; returns the text's size
		FVector2f Text(const FString& String, const FSlateFontInfo& InFont, const FVector2f& Position, const FLinearColor& Color, float AlignX, int32 Layer,
			bool bShadow = true) const;
		// Text shrunk (down to 60%) to fit MaxWidth, ending in "..." when even that is too wide
		FVector2f TextFit(const FString& String, EBeyondFont Style, float Size, const FVector2f& Position, float MaxWidth, const FLinearColor& Color, float AlignX,
			int32 Layer) const;
		// Wrapped text; returns its height
		float Paragraph(const FString& String, const FSlateFontInfo& InFont, const FVector2f& TopLeft, float MaxWidth, float LineHeight,
			const FLinearColor& Color, int32 Layer, int32 MaxLines = 0) const;

		//~ The shared pieces

		// Full-screen picture under a navy veil and a vignette
		void Backdrop(const FDesignSpace& Space, UTexture2D* Picture, int32 Layer) const;
		// Navy panel with a gold hairline and corner ornaments
		void Panel(const FVector2f& TopLeft, const FVector2f& Size, int32 Layer, float FillOpacity = 1.0f) const;
		// Gold spaced title with a rule fading to the right; returns the y below it
		float SectionHeader(const FString& Title, const FVector2f& TopLeft, float Width, int32 Layer, float Size = 17.0f) const;
		// A gold rule with a diamond in the middle
		void Separator(const FVector2f& Left, float Width, int32 Layer, float Opacity = 1.0f) const;
		// The skewed character tab
		void SkewTab(const FVector2f& TopLeft, const FVector2f& Size, const FString& Label, bool bSelected, bool bHovered, const FLinearColor& Accent,
			int32 Layer) const;
		// A key in a small box; returns its width
		float KeyCap(const FString& Key, const FVector2f& TopLeft, int32 Layer, float Height = 28.0f, float Opacity = 1.0f) const;
		// Key cap and its label; returns the total width
		float KeyHint(const FString& Key, const FString& Label, const FVector2f& TopLeft, int32 Layer, float Opacity = 1.0f) const;
		void ItemFrame(const FVector2f& Centre, float Size, const FItemFrameStyle& Style, int32 Layer) const;
		// A bar with a diamond cap at each end and a spark at the head
		void Bar(const FVector2f& TopLeft, const FVector2f& Size, float Fraction, const FLinearColor& Fill, int32 Layer, bool bDiamondCaps = true) const;
		// Gold rings, ticks and compass diamonds (the Armor screen's circle, the skill graph's ground)
		void RuneCircle(const FVector2f& Centre, float Radius, const FLinearColor& Color, int32 Layer) const;

	private:
		FPaintGeometry PaintGeometry(const FVector2f& TopLeft, const FVector2f& Size) const;
		void Track(int32 Layer) const;
		FLinearColor Faded(const FLinearColor& Color) const;

		FGeometry Geometry;
		FSlateWindowElementList& Elements;
		int32 BaseLayer = 0;
		float Opacity = 1.0f;
		mutable int32 MaxLayer = 0;
	};
}
