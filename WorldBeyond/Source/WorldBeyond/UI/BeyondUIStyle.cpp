// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondUIStyle.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/PackageName.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/BeyondUIArt.h"
#include "UI/BeyondUIStyleSettings.h"

#define LOCTEXT_NAMESPACE "BeyondUI"

namespace BeyondUIStyleLocal
{
	// A soft reference whose package is on disk (so a missing asset doesn't log a load failure every session)
	template <typename T>
	T* LoadIfPresent(const TSoftObjectPtr<T>& Soft)
	{
		if (Soft.IsNull())
		{
			return nullptr;
		}
		if (T* Loaded = Soft.Get())
		{
			return Loaded;
		}
		const FString PackageName = Soft.ToSoftObjectPath().GetLongPackageName();
		if (PackageName.IsEmpty() || !FPackageName::DoesPackageExist(PackageName))
		{
			return nullptr;
		}
		return Soft.LoadSynchronous();
	}

	const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
	}

	bool IsHeadingStyle(EBeyondFont Style)
	{
		return Style == EBeyondFont::Display || Style == EBeyondFont::Heading || Style == EBeyondFont::HeadingBold || Style == EBeyondFont::Number
			|| Style == EBeyondFont::Key;
	}
}

//~ UBeyondUISubsystem

UBeyondUISubsystem* UBeyondUISubsystem::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UBeyondUISubsystem>() : nullptr;
}

void UBeyondUISubsystem::EnsureLoaded()
{
	if (bLoaded)
	{
		return;
	}
	bLoaded = true;
	const UBeyondUIStyleSettings* Settings = UBeyondUIStyleSettings::Get();
	HeadingFont = BeyondUIStyleLocal::LoadIfPresent(Settings->HeadingFont);
	BodyFont = BeyondUIStyleLocal::LoadIfPresent(Settings->BodyFont);
	ArtSet = BeyondUIStyleLocal::LoadIfPresent(Settings->ArtSet);
}

void UBeyondUISubsystem::Reload()
{
	bLoaded = false;
	HeadingFont = nullptr;
	BodyFont = nullptr;
	ArtSet = nullptr;
}

UFont* UBeyondUISubsystem::GetHeadingFont()
{
	EnsureLoaded();
	return HeadingFont;
}

UFont* UBeyondUISubsystem::GetBodyFont()
{
	EnsureLoaded();
	return BodyFont;
}

const UBeyondUIArtSet* UBeyondUISubsystem::GetArtSet()
{
	EnsureLoaded();
	return ArtSet ? ArtSet.Get() : GetDefault<UBeyondUIArtSet>();
}

UTexture2D* UBeyondUISubsystem::LoadTexture(const TSoftObjectPtr<UTexture2D>& Texture)
{
	UTexture2D* Loaded = BeyondUIStyleLocal::LoadIfPresent(Texture);
	if (Loaded)
	{
		Textures.AddUnique(Loaded);
	}
	return Loaded;
}

const FSlateBrush* UBeyondUISubsystem::GetTextureBrush(UTexture2D* Texture)
{
	if (!Texture)
	{
		return nullptr;
	}
	if (const TUniquePtr<FSlateBrush>* Existing = TextureBrushes.Find(Texture))
	{
		return Existing->Get();
	}
	Textures.AddUnique(Texture);
	TUniquePtr<FSlateBrush> NewBrush = MakeUnique<FSlateBrush>();
	NewBrush->SetResourceObject(Texture);
	NewBrush->DrawAs = ESlateBrushDrawType::Image;
	NewBrush->ImageSize = FVector2f(static_cast<float>(Texture->GetSurfaceWidth()), static_cast<float>(Texture->GetSurfaceHeight()));
	const FSlateBrush* Result = NewBrush.Get();
	TextureBrushes.Add(Texture, MoveTemp(NewBrush));
	return Result;
}

const FSlateBrush* UBeyondUISubsystem::GetRoundedBrush(float Radius)
{
	const int32 Key = Radius <= 0.0f ? -1 : FMath::RoundToInt(Radius);
	if (const TUniquePtr<FSlateBrush>* Existing = RoundedBrushes.Find(Key))
	{
		return Existing->Get();
	}
	TUniquePtr<FSlateBrush> NewBrush = MakeUnique<FSlateBrush>();
	NewBrush->DrawAs = ESlateBrushDrawType::RoundedBox;
	NewBrush->TintColor = FSlateColor(FLinearColor::White);
	if (Key < 0)
	{
		NewBrush->OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	}
	else
	{
		NewBrush->OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		NewBrush->OutlineSettings.CornerRadii = FVector4(Key, Key, Key, Key);
	}
	const FSlateBrush* Result = NewBrush.Get();
	RoundedBrushes.Add(Key, MoveTemp(NewBrush));
	return Result;
}

//~ Colours

FLinearColor BeyondUI::FromHex(uint32 RGB, float Alpha)
{
	FLinearColor Color(FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF, 255));
	Color.A = Alpha;
	return Color;
}

FLinearColor BeyondUI::WithAlpha(FLinearColor Color, float Opacity)
{
	Color.A *= FMath::Clamp(Opacity, 0.0f, 1.0f);
	return Color;
}

const BeyondUI::FPalette& BeyondUI::Palette()
{
	static const FPalette Colors = []
	{
		FPalette P;
		P.Backdrop = FromHex(0x070B16);
		P.Panel = FromHex(0x0A1122, 0.9f);
		P.PanelTop = FromHex(0x131D36, 0.9f);
		P.PanelRaised = FromHex(0x16203A, 0.95f);
		P.Gold = FromHex(0xC9A45C);
		P.GoldBright = FromHex(0xEACB80);
		P.GoldDim = FromHex(0x7A6438);
		P.Silver = FromHex(0x9AA4B8);
		P.Text = FromHex(0xE9E2CF);
		P.TextDim = FromHex(0xA7AEBD);
		P.TextFaint = FromHex(0x606879);
		P.Blue = FromHex(0x3D8BFF);
		P.BlueBright = FromHex(0x9CC9FF);
		P.BlueDeep = FromHex(0x163A92);
		P.Amber = FromHex(0xF2B441);
		P.Violet = FromHex(0x9B6BFF);
		P.Plus = FromHex(0x7BD88F);
		P.Minus = FromHex(0xE5675A);
		P.Health = FromHex(0x4FC16A);
		P.Stamina = FromHex(0xD8AE48);
		P.Shadow = FLinearColor(0.0f, 0.0f, 0.0f, 0.75f);
		return P;
	}();
	return Colors;
}

FLinearColor BeyondUI::TierColor(EBeyondItemTier Tier)
{
	switch (Tier)
	{
	case EBeyondItemTier::Uncommon: return FromHex(0x6CCB6F);
	case EBeyondItemTier::Rare: return FromHex(0x4A8CFF);
	case EBeyondItemTier::Epic: return FromHex(0xA867FF);
	case EBeyondItemTier::Legendary: return FromHex(0xE8B04A);
	default: return FromHex(0xBFC3CC);
	}
}

FLinearColor BeyondUI::ElementColor(EBeyondElement Element)
{
	switch (Element)
	{
	case EBeyondElement::Storm: return FromHex(0x5AA8FF);
	case EBeyondElement::Radiant: return FromHex(0xFFD36B);
	case EBeyondElement::Eclipse: return FromHex(0xB58CFF);
	case EBeyondElement::Verdant: return FromHex(0x6FCF7A);
	case EBeyondElement::Shadow: return FromHex(0x9C6BD8);
	case EBeyondElement::Frost: return FromHex(0x9FE3FF);
	case EBeyondElement::Flame: return FromHex(0xFF7A3D);
	default: return Palette().Silver;
	}
}

FText BeyondUI::ElementName(EBeyondElement Element)
{
	switch (Element)
	{
	case EBeyondElement::Storm: return LOCTEXT("Storm", "Storm");
	case EBeyondElement::Radiant: return LOCTEXT("Radiant", "Radiant");
	case EBeyondElement::Eclipse: return LOCTEXT("Eclipse", "Eclipse");
	case EBeyondElement::Verdant: return LOCTEXT("Verdant", "Verdant");
	case EBeyondElement::Shadow: return LOCTEXT("Shadow", "Shadow");
	case EBeyondElement::Frost: return LOCTEXT("Frost", "Frost");
	case EBeyondElement::Flame: return LOCTEXT("Flame", "Flame");
	default: return LOCTEXT("NoElement", "None");
	}
}

EBeyondGlyph BeyondUI::ElementGlyph(EBeyondElement Element)
{
	switch (Element)
	{
	case EBeyondElement::Storm: return EBeyondGlyph::Storm;
	case EBeyondElement::Radiant: return EBeyondGlyph::Radiant;
	case EBeyondElement::Eclipse: return EBeyondGlyph::Eclipse;
	case EBeyondElement::Verdant: return EBeyondGlyph::Verdant;
	case EBeyondElement::Shadow: return EBeyondGlyph::Shadow;
	case EBeyondElement::Frost: return EBeyondGlyph::Frost;
	case EBeyondElement::Flame: return EBeyondGlyph::Flame;
	default: return EBeyondGlyph::Star;
	}
}

EBeyondGlyph BeyondUI::SlotGlyph(EBeyondItemSlot ItemSlot, bool bStaff)
{
	switch (ItemSlot)
	{
	case EBeyondItemSlot::Helm: return EBeyondGlyph::Helm;
	case EBeyondItemSlot::Chest: return EBeyondGlyph::Chest;
	case EBeyondItemSlot::Gauntlets: return EBeyondGlyph::Gauntlets;
	case EBeyondItemSlot::Boots: return EBeyondGlyph::Boots;
	default: return bStaff ? EBeyondGlyph::Staff : EBeyondGlyph::Sword;
	}
}

//~ Text

void BeyondUI::Preload()
{
	if (UBeyondUISubsystem* UI = UBeyondUISubsystem::Get())
	{
		UI->GetHeadingFont();
		UI->GetBodyFont();
		UI->GetArtSet();
	}
}

FSlateFontInfo BeyondUI::Font(EBeyondFont Style, float Size)
{
	UBeyondUISubsystem* UI = UBeyondUISubsystem::Get();
	const bool bHeading = BeyondUIStyleLocal::IsHeadingStyle(Style);
	UFont* Family = UI ? (bHeading ? UI->GetHeadingFont() : UI->GetBodyFont()) : nullptr;

	FName Typeface = TEXT("Regular");
	int32 Spacing = 0;
	switch (Style)
	{
	case EBeyondFont::Display: Spacing = 160; break;
	case EBeyondFont::Heading: Spacing = 90; break;
	case EBeyondFont::HeadingBold: Typeface = TEXT("Bold"); Spacing = 70; break;
	case EBeyondFont::Number: Spacing = 20; break;
	case EBeyondFont::Key: Typeface = TEXT("Bold"); break;
	case EBeyondFont::BodyItalic: Typeface = TEXT("Italic"); break;
	case EBeyondFont::BodyBold: Typeface = TEXT("Bold"); break;
	default: break;
	}

	FSlateFontInfo Info = Family ? FSlateFontInfo(Family, Size, Typeface) : FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	Info.LetterSpacing = Spacing;
	return Info;
}

FVector2f BeyondUI::MeasureText(const FString& Text, const FSlateFontInfo& InFont)
{
	FSlateRenderer* Renderer = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetRenderer() : nullptr;
	if (Renderer)
	{
		return FVector2f(Renderer->GetFontMeasureService()->Measure(Text, InFont));
	}
	// No renderer (-nullrhi tests): Cinzel is wide, Garamond narrow
	float Factor = 0.6f;
	if (InFont.FontObject)
	{
		const FString Family = InFont.FontObject->GetName();
		Factor = Family.Contains(TEXT("Cinzel")) ? 0.74f : Family.Contains(TEXT("Garamond")) ? 0.48f : 0.6f;
	}
	const float PerCharacter = InFont.Size * (Factor + InFont.LetterSpacing / 1000.0f);
	return FVector2f(Text.Len() * PerCharacter, InFont.Size * 1.3f);
}

TArray<FString> BeyondUI::WrapText(const FString& Text, const FSlateFontInfo& InFont, float MaxWidth)
{
	TArray<FString> Lines;
	TArray<FString> Paragraphs;
	Text.ParseIntoArray(Paragraphs, TEXT("\n"), false);
	for (const FString& ParagraphText : Paragraphs)
	{
		TArray<FString> Words;
		ParagraphText.ParseIntoArray(Words, TEXT(" "), true);
		FString Current;
		for (const FString& Word : Words)
		{
			const FString Candidate = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
			if (!Current.IsEmpty() && MeasureText(Candidate, InFont).X > MaxWidth)
			{
				Lines.Add(Current);
				Current = Word;
			}
			else
			{
				Current = Candidate;
			}
		}
		Lines.Add(Current);
	}
	return Lines;
}

FString BeyondUI::FormatNumber(int64 Value)
{
	const bool bNegative = Value < 0;
	FString Digits = FString::Printf(TEXT("%lld"), bNegative ? -Value : Value);
	for (int32 Index = Digits.Len() - 3; Index > 0; Index -= 3)
	{
		Digits.InsertAt(Index, TEXT(','));
	}
	return bNegative ? TEXT("-") + Digits : Digits;
}

FString BeyondUI::FormatPercent(float Percent, int32 Decimals)
{
	return Decimals <= 0 ? FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Percent)) : FString::Printf(TEXT("%.*f%%"), Decimals, Percent);
}

FString BeyondUI::FormatPlaytime(double Seconds)
{
	const int64 Total = FMath::Max<int64>(0, static_cast<int64>(Seconds));
	return FString::Printf(TEXT("%lld:%02lld:%02lld"), Total / 3600, (Total / 60) % 60, Total % 60);
}

//~ Design space

BeyondUI::FDesignSpace BeyondUI::FDesignSpace::Fit(const FVector2f& InLocalSize)
{
	const FVector2D Reference = UBeyondUIStyleSettings::Get()->ReferenceResolution;
	return Fit(InLocalSize, FVector2f(static_cast<float>(FMath::Max(Reference.X, 1.0)), static_cast<float>(FMath::Max(Reference.Y, 1.0))));
}

BeyondUI::FDesignSpace BeyondUI::FDesignSpace::Fit(const FVector2f& InLocalSize, const FVector2f& InDesignSize)
{
	FDesignSpace Space;
	Space.DesignSize = InDesignSize;
	Space.LocalSize = FVector2f(FMath::Max(InLocalSize.X, 1.0f), FMath::Max(InLocalSize.Y, 1.0f));
	Space.Scale = FMath::Min(Space.LocalSize.X / InDesignSize.X, Space.LocalSize.Y / InDesignSize.Y);
	Space.Offset = (Space.LocalSize - InDesignSize * Space.Scale) * 0.5f;
	return Space;
}

FVector2f BeyondUI::FDesignSpace::ToDesign(const FVector2f& Local) const
{
	return (Local - Offset) / Scale;
}

FVector2f BeyondUI::FDesignSpace::ToLocal(const FVector2f& Design) const
{
	return Design * Scale + Offset;
}

FGeometry BeyondUI::FDesignSpace::MakeGeometry(const FGeometry& Allotted) const
{
	return Allotted.MakeChild(DesignSize, FSlateLayoutTransform(Scale, Offset));
}

FVector2f BeyondUI::FDesignSpace::BleedTopLeft() const
{
	return -Offset / Scale;
}

FVector2f BeyondUI::FDesignSpace::BleedSize() const
{
	return LocalSize / Scale;
}

//~ Painter

BeyondUI::FPainter::FPainter(const FGeometry& InGeometry, FSlateWindowElementList& InElements, int32 InBaseLayer, float InOpacity)
	: Geometry(InGeometry)
	, Elements(InElements)
	, BaseLayer(InBaseLayer)
	, Opacity(InOpacity)
	, MaxLayer(InBaseLayer)
{
}

void BeyondUI::FPainter::Track(int32 Layer) const
{
	MaxLayer = FMath::Max(MaxLayer, BaseLayer + Layer + 1);
}

FLinearColor BeyondUI::FPainter::Faded(const FLinearColor& Color) const
{
	return WithAlpha(Color, Opacity);
}

FPaintGeometry BeyondUI::FPainter::PaintGeometry(const FVector2f& TopLeft, const FVector2f& Size) const
{
	return Geometry.ToPaintGeometry(FVector2f(FMath::Max(Size.X, 0.0f), FMath::Max(Size.Y, 0.0f)), FSlateLayoutTransform(TopLeft));
}

void BeyondUI::FPainter::Box(const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Color, int32 Layer) const
{
	Brush(BeyondUIStyleLocal::WhiteBrush(), TopLeft, Size, Color, Layer);
}

void BeyondUI::FPainter::Brush(const FSlateBrush* InBrush, const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Tint, int32 Layer) const
{
	if (!InBrush || Tint.A <= 0.0f || Size.X <= 0.0f || Size.Y <= 0.0f)
	{
		return;
	}
	FSlateDrawElement::MakeBox(Elements, BaseLayer + Layer, PaintGeometry(TopLeft, Size), InBrush, ESlateDrawEffect::None, Faded(Tint));
	Track(Layer);
}

void BeyondUI::FPainter::Image(UTexture2D* Texture, const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Tint, int32 Layer) const
{
	UBeyondUISubsystem* UI = UBeyondUISubsystem::Get();
	Brush(UI ? UI->GetTextureBrush(Texture) : nullptr, TopLeft, Size, Tint, Layer);
}

void BeyondUI::FPainter::ImageCentred(UTexture2D* Texture, const FVector2f& Centre, const FVector2f& Size, const FLinearColor& Tint, int32 Layer) const
{
	Image(Texture, Centre - Size * 0.5f, Size, Tint, Layer);
}

void BeyondUI::FPainter::ImageCover(UTexture2D* Texture, const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Tint, int32 Layer, float FocusY) const
{
	if (!Texture || Size.X <= 0.0f || Size.Y <= 0.0f)
	{
		return;
	}
	FVector2f TextureSize(static_cast<float>(Texture->GetSurfaceWidth()), static_cast<float>(Texture->GetSurfaceHeight()));
	if (TextureSize.X <= 1.0f || TextureSize.Y <= 1.0f)
	{
		TextureSize = Size;
	}
	const float Fit = FMath::Max(Size.X / TextureSize.X, Size.Y / TextureSize.Y);
	const FVector2f Drawn = TextureSize * Fit;
	const FVector2f Position = TopLeft + FVector2f((Size.X - Drawn.X) * 0.5f, (Size.Y - Drawn.Y) * FMath::Clamp(FocusY, 0.0f, 1.0f));
	Elements.PushClip(FSlateClippingZone(PaintGeometry(TopLeft, Size)));
	Image(Texture, Position, Drawn, Tint, Layer);
	Elements.PopClip();
}

void BeyondUI::FPainter::Rounded(const FVector2f& TopLeft, const FVector2f& Size, float Radius, const FLinearColor& Color, int32 Layer) const
{
	UBeyondUISubsystem* UI = UBeyondUISubsystem::Get();
	Brush(UI ? UI->GetRoundedBrush(Radius) : BeyondUIStyleLocal::WhiteBrush(), TopLeft, Size, Color, Layer);
}

void BeyondUI::FPainter::Disc(const FVector2f& Centre, float Radius, const FLinearColor& Color, int32 Layer) const
{
	Rounded(Centre - FVector2f(Radius, Radius), FVector2f(Radius, Radius) * 2.0f, 0.0f, Color, Layer);
}

void BeyondUI::FPainter::GradientH(const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Left, const FLinearColor& Right, int32 Layer) const
{
	if (Size.X <= 0.0f || Size.Y <= 0.0f || (Left.A <= 0.0f && Right.A <= 0.0f))
	{
		return;
	}
	TArray<FSlateGradientStop> Stops;
	Stops.Emplace(FVector2f::ZeroVector, Faded(Left));
	Stops.Emplace(Size, Faded(Right));
	// Orient_Vertical: the colour changes from left to right
	FSlateDrawElement::MakeGradient(Elements, BaseLayer + Layer, PaintGeometry(TopLeft, Size), MoveTemp(Stops), Orient_Vertical);
	Track(Layer);
}

void BeyondUI::FPainter::GradientV(const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Top, const FLinearColor& Bottom, int32 Layer) const
{
	if (Size.X <= 0.0f || Size.Y <= 0.0f || (Top.A <= 0.0f && Bottom.A <= 0.0f))
	{
		return;
	}
	TArray<FSlateGradientStop> Stops;
	Stops.Emplace(FVector2f::ZeroVector, Faded(Top));
	Stops.Emplace(Size, Faded(Bottom));
	FSlateDrawElement::MakeGradient(Elements, BaseLayer + Layer, PaintGeometry(TopLeft, Size), MoveTemp(Stops), Orient_Horizontal);
	Track(Layer);
}

void BeyondUI::FPainter::GradientFadeH(const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Middle, int32 Layer) const
{
	const FVector2f Half(Size.X * 0.5f, Size.Y);
	GradientH(TopLeft, Half, WithAlpha(Middle, 0.0f), Middle, Layer);
	GradientH(TopLeft + FVector2f(Half.X, 0.0f), Half, Middle, WithAlpha(Middle, 0.0f), Layer);
}

void BeyondUI::FPainter::Line(const FVector2f& From, const FVector2f& To, const FLinearColor& Color, float Thickness, int32 Layer) const
{
	Polyline({ From, To }, Color, Thickness, Layer);
}

void BeyondUI::FPainter::Polyline(TArray<FVector2f> Points, const FLinearColor& Color, float Thickness, int32 Layer, bool bClosed) const
{
	if (Points.Num() < 2 || Color.A <= 0.0f)
	{
		return;
	}
	if (bClosed)
	{
		const FVector2f First = Points[0];
		Points.Add(First);
	}
	FSlateDrawElement::MakeLines(Elements, BaseLayer + Layer, Geometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Faded(Color), true, Thickness);
	Track(Layer);
}

void BeyondUI::FPainter::RectOutline(const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Color, float Thickness, int32 Layer) const
{
	Polyline({ TopLeft, TopLeft + FVector2f(Size.X, 0.0f), TopLeft + Size, TopLeft + FVector2f(0.0f, Size.Y) }, Color, Thickness, Layer, true);
}

void BeyondUI::FPainter::Ring(const FVector2f& Centre, float Radius, const FLinearColor& Color, float Thickness, int32 Layer, float Fraction, float StartAngle) const
{
	const float Clamped = FMath::Clamp(Fraction, 0.0f, 1.0f);
	if (Clamped <= 0.0f || Radius <= 0.0f)
	{
		return;
	}
	const int32 Segments = FMath::Clamp(FMath::CeilToInt(FMath::Max(16.0f, Radius * 0.6f) * Clamped), 2, 160);
	TArray<FVector2f> Points;
	Points.Reserve(Segments + 1);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float Angle = StartAngle + 2.0f * PI * Clamped * Index / Segments;
		Points.Add(Centre + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}
	Polyline(MoveTemp(Points), Color, Thickness, Layer);
}

void BeyondUI::FPainter::Transformed(const FSlateBrush* InBrush, const FVector2f& Centre, const FVector2f& LocalSize, const FMatrix2x2& Matrix,
	const FLinearColor& Color, int32 Layer) const
{
	if (!InBrush || Color.A <= 0.0f)
	{
		return;
	}
	const FPaintGeometry Paint = Geometry.ToPaintGeometry(LocalSize, FSlateLayoutTransform(Centre - LocalSize * 0.5f), FSlateRenderTransform(Matrix),
		FVector2f(0.5f, 0.5f));
	FSlateDrawElement::MakeBox(Elements, BaseLayer + Layer, Paint, InBrush, ESlateDrawEffect::None, Faded(Color));
	Track(Layer);
}

void BeyondUI::FPainter::Diamond(const FVector2f& Centre, float HalfWidth, float HalfHeight, const FLinearColor& Color, int32 Layer) const
{
	// A unit square turned 45 degrees and stretched: corners land on the four points
	Transformed(BeyondUIStyleLocal::WhiteBrush(), Centre, FVector2f(1.0f, 1.0f), FMatrix2x2(HalfWidth, HalfHeight, -HalfWidth, HalfHeight), Color, Layer);
	if (HalfWidth >= 4.0f)
	{
		// Boxes aren't antialiased: soften the edges with a line
		DiamondOutline(Centre, HalfWidth, HalfHeight, Color, 1.0f, Layer);
	}
}

void BeyondUI::FPainter::DiamondOutline(const FVector2f& Centre, float HalfWidth, float HalfHeight, const FLinearColor& Color, float Thickness, int32 Layer) const
{
	Polyline({ Centre + FVector2f(0.0f, -HalfHeight), Centre + FVector2f(HalfWidth, 0.0f), Centre + FVector2f(0.0f, HalfHeight), Centre + FVector2f(-HalfWidth, 0.0f) },
		Color, Thickness, Layer, true);
}

void BeyondUI::FPainter::Sparkle(const FVector2f& Centre, float Radius, const FLinearColor& Color, int32 Layer) const
{
	Glow(Centre, Radius * 1.6f, WithAlpha(Color, 0.45f), Layer);
	Diamond(Centre, Radius * 0.24f, Radius, Color, Layer + 1);
	Diamond(Centre, Radius, Radius * 0.24f, Color, Layer + 1);
	Disc(Centre, Radius * 0.18f, WithAlpha(FLinearColor::White, Color.A), Layer + 1);
}

void BeyondUI::FPainter::Parallelogram(const FVector2f& TopLeft, const FVector2f& Size, float Skew, const FLinearColor& Top, const FLinearColor& Bottom,
	int32 Layer) const
{
	if (Size.X <= 0.0f || Size.Y <= 0.0f)
	{
		return;
	}
	// Shear x by y around the middle: the top edge moves right by Skew / 2
	const FMatrix2x2 Shear(1.0f, 0.0f, -Skew / Size.Y, 1.0f);
	const FPaintGeometry Paint = Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft), FSlateRenderTransform(Shear), FVector2f(0.5f, 0.5f));
	TArray<FSlateGradientStop> Stops;
	Stops.Emplace(FVector2f::ZeroVector, Faded(Top));
	Stops.Emplace(Size, Faded(Bottom));
	FSlateDrawElement::MakeGradient(Elements, BaseLayer + Layer, Paint, MoveTemp(Stops), Orient_Horizontal);
	Track(Layer);
}

void BeyondUI::FPainter::ParallelogramOutline(const FVector2f& TopLeft, const FVector2f& Size, float Skew, const FLinearColor& Color, float Thickness,
	int32 Layer) const
{
	const float Half = Skew * 0.5f;
	Polyline({ TopLeft + FVector2f(Half, 0.0f), TopLeft + FVector2f(Size.X + Half, 0.0f), TopLeft + FVector2f(Size.X - Half, Size.Y),
		TopLeft + FVector2f(-Half, Size.Y) }, Color, Thickness, Layer, true);
}

void BeyondUI::FPainter::Glow(const FVector2f& Centre, float Radius, const FLinearColor& Color, int32 Layer) const
{
	if (Color.A <= 0.0f || Radius <= 0.0f)
	{
		return;
	}
	UBeyondUISubsystem* UI = UBeyondUISubsystem::Get();
	const UBeyondUIArtSet* Art = UI ? UI->GetArtSet() : nullptr;
	UTexture2D* Texture = Art && UI ? UI->LoadTexture(Art->Glow) : nullptr;
	if (Texture)
	{
		ImageCentred(Texture, Centre, FVector2f(Radius, Radius) * 2.0f, Color, Layer);
		return;
	}
	// No glow texture: soft rings of discs
	for (const float Step : { 1.0f, 0.7f, 0.45f })
	{
		Disc(Centre, Radius * Step, WithAlpha(Color, 0.25f), Layer);
	}
}

void BeyondUI::FPainter::Glyph(EBeyondGlyph InGlyph, const FVector2f& Centre, float Size, const FLinearColor& Color, int32 Layer, float Thickness) const
{
	const float Width = Thickness > 0.0f ? Thickness : FMath::Max(1.3f, Size * 0.055f);
	for (const TArray<FVector2f>& Stroke : GetGlyphStrokes(InGlyph))
	{
		TArray<FVector2f> Points;
		Points.Reserve(Stroke.Num());
		for (const FVector2f& Point : Stroke)
		{
			Points.Add(Centre + Point * Size);
		}
		Polyline(MoveTemp(Points), Color, Width, Layer);
	}
}

FVector2f BeyondUI::FPainter::Text(const FString& String, const FSlateFontInfo& InFont, const FVector2f& Position, const FLinearColor& Color, float AlignX,
	int32 Layer, bool bShadow) const
{
	if (String.IsEmpty())
	{
		return FVector2f::ZeroVector;
	}
	const FVector2f Size = MeasureText(String, InFont);
	const FVector2f TopLeft(Position.X - Size.X * AlignX, Position.Y);
	if (bShadow)
	{
		FSlateDrawElement::MakeText(Elements, BaseLayer + Layer, PaintGeometry(TopLeft + FVector2f(1.5f, 1.5f), Size), String, InFont, ESlateDrawEffect::None,
			Faded(WithAlpha(FLinearColor::Black, 0.75f * Color.A)));
	}
	FSlateDrawElement::MakeText(Elements, BaseLayer + Layer + 1, PaintGeometry(TopLeft, Size), String, InFont, ESlateDrawEffect::None, Faded(Color));
	Track(Layer + 1);
	return Size;
}

FVector2f BeyondUI::FPainter::TextFit(const FString& String, EBeyondFont Style, float Size, const FVector2f& Position, float MaxWidth, const FLinearColor& Color,
	float AlignX, int32 Layer) const
{
	float FitSize = Size;
	FSlateFontInfo FitFont = Font(Style, FitSize);
	while (FitSize > Size * 0.6f && MeasureText(String, FitFont).X > MaxWidth)
	{
		FitSize -= 1.0f;
		FitFont = Font(Style, FitSize);
	}
	FString Shown = String;
	while (Shown.Len() > 1 && MeasureText(Shown, FitFont).X > MaxWidth)
	{
		Shown = Shown.LeftChop(2) + TEXT("\u2026");
		Shown.ReplaceInline(TEXT("\u2026\u2026"), TEXT("\u2026"));
	}
	return Text(Shown, FitFont, Position, Color, AlignX, Layer);
}

float BeyondUI::FPainter::Paragraph(const FString& String, const FSlateFontInfo& InFont, const FVector2f& TopLeft, float MaxWidth, float LineHeight,
	const FLinearColor& Color, int32 Layer, int32 MaxLines) const
{
	TArray<FString> Lines = WrapText(String, InFont, MaxWidth);
	if (MaxLines > 0 && Lines.Num() > MaxLines)
	{
		Lines.SetNum(MaxLines);
		Lines.Last() += TEXT("...");
	}
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		Text(Lines[Index], InFont, TopLeft + FVector2f(0.0f, Index * LineHeight), Color, 0.0f, Layer);
	}
	return Lines.Num() * LineHeight;
}

//~ Shared pieces

void BeyondUI::FPainter::Backdrop(const FDesignSpace& Space, UTexture2D* Picture, int32 Layer) const
{
	const FPalette& P = Palette();
	const FVector2f TopLeft = Space.BleedTopLeft();
	const FVector2f Size = Space.BleedSize();
	if (Picture)
	{
		Box(TopLeft, Size, P.Backdrop, Layer);
		ImageCover(Picture, TopLeft, Size, FLinearColor(0.8f, 0.85f, 1.0f, 1.0f), Layer, 0.4f);
		Box(TopLeft, Size, WithAlpha(P.Backdrop, 0.42f), Layer);
	}
	else
	{
		// The paused game shows through a deep navy veil
		Box(TopLeft, Size, WithAlpha(P.Backdrop, 0.88f), Layer);
		Glow(FVector2f(Space.DesignSize.X * 0.5f, Space.DesignSize.Y * 0.42f), Space.DesignSize.X * 0.55f, FromHex(0x1B2C5C, 0.35f), Layer);
	}
	// Vignette
	const FLinearColor Edge = WithAlpha(P.Backdrop, 0.85f);
	const FLinearColor Clear = WithAlpha(P.Backdrop, 0.0f);
	GradientH(TopLeft, FVector2f(Size.X * 0.22f, Size.Y), Edge, Clear, Layer);
	GradientH(TopLeft + FVector2f(Size.X * 0.78f, 0.0f), FVector2f(Size.X * 0.22f, Size.Y), Clear, Edge, Layer);
	GradientV(TopLeft, FVector2f(Size.X, Size.Y * 0.16f), WithAlpha(P.Backdrop, 0.7f), Clear, Layer);
	GradientV(TopLeft + FVector2f(0.0f, Size.Y * 0.74f), FVector2f(Size.X, Size.Y * 0.26f), Clear, WithAlpha(P.Backdrop, 0.92f), Layer);
}

void BeyondUI::FPainter::Panel(const FVector2f& TopLeft, const FVector2f& Size, int32 Layer, float FillOpacity) const
{
	const FPalette& P = Palette();
	GradientV(TopLeft, Size, WithAlpha(P.PanelTop, P.PanelTop.A * FillOpacity), WithAlpha(P.Panel, P.Panel.A * FillOpacity), Layer);
	// A faint light along the top edge
	GradientFadeH(TopLeft + FVector2f(0.0f, 1.0f), FVector2f(Size.X, 1.0f), WithAlpha(P.GoldBright, 0.35f), Layer + 1);
	RectOutline(TopLeft, Size, WithAlpha(P.Gold, 0.42f), 1.0f, Layer + 1);
	RectOutline(TopLeft + FVector2f(4.0f, 4.0f), Size - FVector2f(8.0f, 8.0f), WithAlpha(P.Gold, 0.1f), 1.0f, Layer + 1);

	// Corner ornaments: a bright bracket, a smaller inner one and a diamond on the corner
	const float Arm = FMath::Min(24.0f, FMath::Min(Size.X, Size.Y) * 0.22f);
	const FVector2f Corners[] = { TopLeft, TopLeft + FVector2f(Size.X, 0.0f), TopLeft + Size, TopLeft + FVector2f(0.0f, Size.Y) };
	const FVector2f Inward[] = { FVector2f(1.0f, 1.0f), FVector2f(-1.0f, 1.0f), FVector2f(-1.0f, -1.0f), FVector2f(1.0f, -1.0f) };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FVector2f C = Corners[Index];
		const FVector2f D = Inward[Index];
		Polyline({ C + FVector2f(D.X * Arm, 0.0f), C, C + FVector2f(0.0f, D.Y * Arm) }, WithAlpha(P.GoldBright, 0.9f), 1.6f, Layer + 2);
		const FVector2f Inner = C + D * 5.0f;
		Polyline({ Inner + FVector2f(D.X * Arm * 0.45f, 0.0f), Inner, Inner + FVector2f(0.0f, D.Y * Arm * 0.45f) }, WithAlpha(P.Gold, 0.55f), 1.0f, Layer + 2);
		Diamond(C, 3.5f, 3.5f, P.GoldBright, Layer + 3);
	}
}

float BeyondUI::FPainter::SectionHeader(const FString& Title, const FVector2f& TopLeft, float Width, int32 Layer, float Size) const
{
	const FPalette& P = Palette();
	const FVector2f TextSize = Text(Title, Font(EBeyondFont::Heading, Size), TopLeft, P.GoldBright, 0.0f, Layer + 1);
	const float RuleY = TopLeft.Y + TextSize.Y + 3.0f;
	GradientH(FVector2f(TopLeft.X, RuleY), FVector2f(Width, 1.0f), WithAlpha(P.Gold, 0.8f), WithAlpha(P.Gold, 0.0f), Layer);
	Diamond(FVector2f(TopLeft.X, RuleY + 0.5f), 3.0f, 3.0f, P.GoldBright, Layer + 1);
	return RuleY + 10.0f;
}

void BeyondUI::FPainter::Separator(const FVector2f& Left, float Width, int32 Layer, float InOpacity) const
{
	const FPalette& P = Palette();
	const FVector2f Middle(Left.X + Width * 0.5f, Left.Y);
	GradientH(Left, FVector2f(Width * 0.5f - 8.0f, 1.0f), WithAlpha(P.Gold, 0.0f), WithAlpha(P.Gold, 0.7f * InOpacity), Layer);
	GradientH(FVector2f(Middle.X + 8.0f, Left.Y), FVector2f(Width * 0.5f - 8.0f, 1.0f), WithAlpha(P.Gold, 0.7f * InOpacity), WithAlpha(P.Gold, 0.0f), Layer);
	DiamondOutline(Middle + FVector2f(0.0f, 0.5f), 4.0f, 4.0f, WithAlpha(P.GoldBright, InOpacity), 1.0f, Layer);
}

void BeyondUI::FPainter::SkewTab(const FVector2f& TopLeft, const FVector2f& Size, const FString& Label, bool bSelected, bool bHovered, const FLinearColor& Accent,
	int32 Layer) const
{
	const FPalette& P = Palette();
	const float Skew = Size.Y * 0.34f;
	const FVector2f Centre = TopLeft + Size * 0.5f;
	FLinearColor LabelColor = bHovered ? P.Text : P.TextDim;
	if (bSelected)
	{
		Glow(Centre, Size.X * 0.62f, WithAlpha(Accent, 0.32f), Layer);
		Parallelogram(TopLeft, Size, Skew, FMath::Lerp(Accent, FLinearColor::White, 0.18f), FMath::Lerp(Accent, P.BlueDeep, 0.65f), Layer + 1);
		// Light across the top half
		Parallelogram(TopLeft + FVector2f(Skew * 0.25f, 2.0f), FVector2f(Size.X, Size.Y * 0.45f), Skew * 0.45f, WithAlpha(FLinearColor::White, 0.22f),
			WithAlpha(FLinearColor::White, 0.0f), Layer + 2);
		ParallelogramOutline(TopLeft, Size, Skew, WithAlpha(P.BlueBright, 0.95f), 1.6f, Layer + 3);
		ParallelogramOutline(TopLeft + FVector2f(0.0f, 3.0f), Size - FVector2f(0.0f, 6.0f), Skew * (Size.Y - 6.0f) / Size.Y, WithAlpha(P.GoldBright, 0.35f), 1.0f,
			Layer + 3);
		Sparkle(TopLeft + FVector2f(Size.X + Skew * 0.5f - 4.0f, 1.0f), 10.0f, P.GoldBright, Layer + 4);
		LabelColor = FLinearColor::White;
	}
	else
	{
		Parallelogram(TopLeft, Size, Skew, FromHex(0x161E33, 0.94f), FromHex(0x0A0F1D, 0.94f), Layer + 1);
		ParallelogramOutline(TopLeft, Size, Skew, WithAlpha(P.Silver, bHovered ? 0.85f : 0.45f), 1.2f, Layer + 3);
	}
	const FSlateFontInfo LabelFont = Font(EBeyondFont::Heading, 19.0f);
	const FVector2f LabelSize = MeasureText(Label, LabelFont);
	Text(Label, LabelFont, FVector2f(Centre.X, Centre.Y - LabelSize.Y * 0.5f), LabelColor, 0.5f, Layer + 5);
}

float BeyondUI::FPainter::KeyCap(const FString& Key, const FVector2f& TopLeft, int32 Layer, float Height, float InOpacity) const
{
	const FPalette& P = Palette();
	const FSlateFontInfo KeyFont = Font(EBeyondFont::Key, Height * 0.48f);
	const FVector2f TextSize = MeasureText(Key, KeyFont);
	const float Width = FMath::Max(Height, TextSize.X + 14.0f);
	const FVector2f Size(Width, Height);
	GradientV(TopLeft, Size, FromHex(0x1B2338, 0.95f * InOpacity), FromHex(0x0B1020, 0.95f * InOpacity), Layer);
	RectOutline(TopLeft, Size, WithAlpha(P.Silver, 0.8f * InOpacity), 1.0f, Layer + 1);
	RectOutline(TopLeft + FVector2f(2.0f, 2.0f), Size - FVector2f(4.0f, 4.0f), WithAlpha(FLinearColor::White, 0.07f * InOpacity), 1.0f, Layer + 1);
	Text(Key, KeyFont, FVector2f(TopLeft.X + Width * 0.5f, TopLeft.Y + (Height - TextSize.Y) * 0.5f), WithAlpha(P.Text, InOpacity), 0.5f, Layer + 1, false);
	return Width;
}

float BeyondUI::FPainter::KeyHint(const FString& Key, const FString& Label, const FVector2f& TopLeft, int32 Layer, float InOpacity) const
{
	const FPalette& P = Palette();
	const float CapWidth = KeyCap(Key, TopLeft, Layer, 28.0f, InOpacity);
	const FSlateFontInfo LabelFont = Font(EBeyondFont::Body, 17.0f);
	const FVector2f LabelSize = MeasureText(Label, LabelFont);
	Text(Label, LabelFont, FVector2f(TopLeft.X + CapWidth + 10.0f, TopLeft.Y + (28.0f - LabelSize.Y) * 0.5f), WithAlpha(P.Text, 0.9f * InOpacity), 0.0f, Layer);
	return CapWidth + 10.0f + LabelSize.X;
}

void BeyondUI::FPainter::ItemFrame(const FVector2f& Centre, float Size, const FItemFrameStyle& Style, int32 Layer) const
{
	const FPalette& P = Palette();
	const float Alpha = Style.Opacity;
	const FVector2f BoxSize(Size, Size);
	const FVector2f TopLeft = Centre - BoxSize * 0.5f;

	if (Style.bEmpty)
	{
		GradientV(TopLeft, BoxSize, FromHex(0x101727, 0.75f * Alpha), FromHex(0x080C17, 0.75f * Alpha), Layer);
		RectOutline(TopLeft, BoxSize, WithAlpha(P.Silver, (Style.bHovered ? 0.6f : 0.22f) * Alpha), 1.0f, Layer + 1);
		if (Style.Glyph != EBeyondGlyph::None)
		{
			Glyph(Style.Glyph, Centre, Size * 0.62f, WithAlpha(P.TextFaint, 0.8f * Alpha), Layer + 2);
		}
		if (Style.bSelected)
		{
			RectOutline(TopLeft - FVector2f(3.0f, 3.0f), BoxSize + FVector2f(6.0f, 6.0f), WithAlpha(P.GoldBright, Alpha), 2.0f, Layer + 3);
		}
		return;
	}

	const FLinearColor Tier = TierColor(Style.Tier);
	const FLinearColor Deep = FMath::Lerp(P.Backdrop, Tier, 0.32f);
	if (Style.bSelected || Style.bHovered)
	{
		Glow(Centre, Size * 0.95f, WithAlpha(Tier, (Style.bSelected ? 0.45f : 0.28f) * Alpha), Layer);
	}
	GradientV(TopLeft, BoxSize, WithAlpha(Deep, 0.95f * Alpha), WithAlpha(P.Backdrop, 0.95f * Alpha), Layer + 1);
	Glow(Centre + FVector2f(0.0f, Size * 0.08f), Size * 0.55f, WithAlpha(Tier, 0.22f * Alpha), Layer + 1);

	if (Style.Icon)
	{
		ImageCentred(Style.Icon, Centre, BoxSize * 0.86f, WithAlpha(FLinearColor::White, Alpha), Layer + 2);
	}
	else if (Style.Glyph != EBeyondGlyph::None)
	{
		Glyph(Style.Glyph, Centre, Size * 0.62f, WithAlpha(FMath::Lerp(Tier, FLinearColor::White, 0.35f), Alpha), Layer + 2);
	}

	// Rarity frame: a bright outer line, a soft inner one
	RectOutline(TopLeft, BoxSize, WithAlpha(Tier, 0.95f * Alpha), 1.6f, Layer + 3);
	RectOutline(TopLeft + FVector2f(3.0f, 3.0f), BoxSize - FVector2f(6.0f, 6.0f), WithAlpha(Tier, 0.3f * Alpha), 1.0f, Layer + 3);
	GradientV(TopLeft + FVector2f(1.0f, 1.0f), FVector2f(Size - 2.0f, Size * 0.3f), WithAlpha(FLinearColor::White, 0.08f * Alpha),
		WithAlpha(FLinearColor::White, 0.0f), Layer + 3);
	if (Style.bBadge && Style.Tier >= EBeyondItemTier::Rare)
	{
		Sparkle(TopLeft + FVector2f(7.0f, 7.0f), 7.0f, WithAlpha(Style.Tier == EBeyondItemTier::Legendary ? P.GoldBright : FMath::Lerp(Tier, FLinearColor::White, 0.5f),
			Alpha), Layer + 4);
	}
	if (Style.bSelected)
	{
		RectOutline(TopLeft - FVector2f(3.0f, 3.0f), BoxSize + FVector2f(6.0f, 6.0f), WithAlpha(P.GoldBright, Alpha), 2.0f, Layer + 4);
	}
	else if (Style.bHovered)
	{
		RectOutline(TopLeft - FVector2f(2.0f, 2.0f), BoxSize + FVector2f(4.0f, 4.0f), WithAlpha(FLinearColor::White, 0.45f * Alpha), 1.0f, Layer + 4);
	}

	if (Style.bShowUpgrade && Style.UpgradeLevel > 0)
	{
		const FSlateFontInfo UpgradeFont = Font(EBeyondFont::Number, FMath::Max(10.0f, Size * 0.17f));
		Text(FString::Printf(TEXT("+%d"), Style.UpgradeLevel), UpgradeFont, TopLeft + FVector2f(Size - 5.0f, Size - MeasureText(TEXT("+0"), UpgradeFont).Y - 2.0f),
			WithAlpha(P.Text, Alpha), 1.0f, Layer + 4);
	}
	if (Style.bLocked)
	{
		const FVector2f BadgeCentre = TopLeft + FVector2f(Size - 10.0f, 10.0f);
		Disc(BadgeCentre, 9.0f, WithAlpha(P.Backdrop, 0.85f * Alpha), Layer + 4);
		Glyph(EBeyondGlyph::Lock, BadgeCentre, 13.0f, WithAlpha(P.GoldBright, Alpha), Layer + 5, 1.3f);
	}
	if (Style.bJunk)
	{
		const FVector2f BadgeCentre = TopLeft + FVector2f(10.0f, Size - 10.0f);
		Disc(BadgeCentre, 9.0f, WithAlpha(P.Backdrop, 0.85f * Alpha), Layer + 4);
		Glyph(EBeyondGlyph::Cross, BadgeCentre, 12.0f, WithAlpha(P.Minus, Alpha), Layer + 5, 1.8f);
	}
}

void BeyondUI::FPainter::Bar(const FVector2f& TopLeft, const FVector2f& Size, float Fraction, const FLinearColor& Fill, int32 Layer, bool bDiamondCaps) const
{
	const FPalette& P = Palette();
	const float Clamped = FMath::Clamp(Fraction, 0.0f, 1.0f);
	Box(TopLeft, Size, FromHex(0x04070E, 0.9f), Layer);
	RectOutline(TopLeft, Size, WithAlpha(P.GoldDim, 0.7f), 1.0f, Layer + 1);
	const FVector2f Inner = TopLeft + FVector2f(1.0f, 1.0f);
	const FVector2f FillSize((Size.X - 2.0f) * Clamped, FMath::Max(Size.Y - 2.0f, 1.0f));
	if (Clamped > 0.0f)
	{
		GradientH(Inner, FillSize, FMath::Lerp(Fill, P.Backdrop, 0.45f), Fill, Layer + 1);
		GradientV(Inner, FVector2f(FillSize.X, FillSize.Y * 0.5f), WithAlpha(FLinearColor::White, 0.22f), WithAlpha(FLinearColor::White, 0.0f), Layer + 2);
	}
	const float MidY = TopLeft.Y + Size.Y * 0.5f;
	if (Clamped > 0.0f && Clamped < 1.0f)
	{
		const FVector2f Head(Inner.X + FillSize.X, MidY);
		Glow(Head, Size.Y * 3.5f, WithAlpha(Fill, 0.55f), Layer + 2);
		Diamond(Head, Size.Y * 0.55f + 2.0f, Size.Y * 0.55f + 2.0f, FMath::Lerp(Fill, FLinearColor::White, 0.5f), Layer + 3);
	}
	if (bDiamondCaps)
	{
		const float Cap = Size.Y * 0.6f + 4.0f;
		Diamond(FVector2f(TopLeft.X, MidY), Cap, Cap, P.Gold, Layer + 3);
		Diamond(FVector2f(TopLeft.X, MidY), Cap * 0.45f, Cap * 0.45f, P.Backdrop, Layer + 4);
		DiamondOutline(FVector2f(TopLeft.X + Size.X, MidY), Cap * 0.8f, Cap * 0.8f, WithAlpha(P.Gold, 0.85f), 1.2f, Layer + 3);
	}
}

void BeyondUI::FPainter::RuneCircle(const FVector2f& Centre, float Radius, const FLinearColor& Color, int32 Layer) const
{
	UBeyondUISubsystem* UI = UBeyondUISubsystem::Get();
	const UBeyondUIArtSet* Art = UI ? UI->GetArtSet() : nullptr;
	if (UTexture2D* Runes = Art && UI ? UI->LoadTexture(Art->RuneCircle) : nullptr)
	{
		ImageCentred(Runes, Centre, FVector2f(Radius, Radius) * 2.2f, WithAlpha(Color, 0.14f), Layer);
	}
	Glow(Centre, Radius * 1.05f, WithAlpha(Color, 0.05f), Layer);

	Ring(Centre, Radius, WithAlpha(Color, 0.6f), 1.6f, Layer + 1);
	Ring(Centre, Radius * 0.955f, WithAlpha(Color, 0.32f), 1.0f, Layer + 1);
	for (int32 Index = 0; Index < 72; ++Index)
	{
		const float Angle = FMath::DegreesToRadians(Index * 5.0f);
		const FVector2f Dir(FMath::Cos(Angle), FMath::Sin(Angle));
		const bool bLong = Index % 6 == 0;
		Line(Centre + Dir * Radius * (bLong ? 0.93f : 0.955f), Centre + Dir * Radius * (bLong ? 1.025f : 1.0f), WithAlpha(Color, bLong ? 0.5f : 0.22f), 1.0f, Layer + 1);
	}
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const float Angle = FMath::DegreesToRadians(Index * 45.0f - 90.0f);
		const FVector2f Point = Centre + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
		const float Half = Index % 2 == 0 ? 6.0f : 4.0f;
		Diamond(Point, Half, Half, WithAlpha(Color, 0.85f), Layer + 2);
	}

	// Inner ring and an eight-pointed star
	const float InnerRadius = Radius * 0.72f;
	Ring(Centre, InnerRadius, WithAlpha(Color, 0.2f), 1.0f, Layer + 1);
	TArray<FVector2f> Star;
	for (int32 Index = 0; Index <= 8; ++Index)
	{
		const float Angle = FMath::DegreesToRadians((Index * 3 % 8) * 45.0f - 90.0f);
		Star.Add(Centre + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * InnerRadius);
	}
	Polyline(MoveTemp(Star), WithAlpha(Color, 0.1f), 1.0f, Layer + 1);
}

#undef LOCTEXT_NAMESPACE
