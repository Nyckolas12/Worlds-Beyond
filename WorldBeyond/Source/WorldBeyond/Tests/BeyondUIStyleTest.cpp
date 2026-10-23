// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Font.h"
#include "Misc/AutomationTest.h"
#include "UI/BeyondUIStyle.h"
#include "UI/BeyondUIStyleSettings.h"

/**
 * Plan 6 foundation (no world needed): the 1920 x 1080 design space fits any screen and maps both ways, text measuring
 * works with and without a renderer, wrapping, number / percent / playtime formatting, the palette's tier colours and
 * the fonts migrate_pass20.py makes (skipped until the pass has run).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondUIStyleTest, "WorldsBeyond.UI.Style",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondUIStyleTest::RunTest(const FString& Parameters)
{
	using namespace BeyondUI;
	const FVector2f Design(1920.0f, 1080.0f);

	// Design space: uniform scale, centred, round trip
	struct FCase { FVector2f Local; float Scale; };
	const FCase Cases[] = { { FVector2f(1280.0f, 720.0f), 2.0f / 3.0f }, { FVector2f(1920.0f, 1080.0f), 1.0f }, { FVector2f(2560.0f, 1080.0f), 1.0f },
		{ FVector2f(3840.0f, 2160.0f), 2.0f }, { FVector2f(1920.0f, 1440.0f), 1.0f } };
	for (const FCase& Case : Cases)
	{
		const FDesignSpace Space = FDesignSpace::Fit(Case.Local, Design);
		const FString Name = FString::Printf(TEXT("%.0fx%.0f"), Case.Local.X, Case.Local.Y);
		TestTrue(Name + TEXT(": scale"), FMath::IsNearlyEqual(Space.Scale, Case.Scale, 0.001f));
		const FVector2f Page = Design * Space.Scale;
		TestTrue(Name + TEXT(": page centred"), FMath::IsNearlyEqual(Space.Offset.X * 2.0f + Page.X, Case.Local.X, 0.01f)
			&& FMath::IsNearlyEqual(Space.Offset.Y * 2.0f + Page.Y, Case.Local.Y, 0.01f));
		const FVector2f Point(431.0f, 777.0f);
		TestTrue(Name + TEXT(": round trip"), Space.ToDesign(Space.ToLocal(Point)).Equals(Point, 0.01f));
		TestTrue(Name + TEXT(": bleed covers the widget"), Space.ToLocal(Space.BleedTopLeft()).Equals(FVector2f::ZeroVector, 0.01f)
			&& Space.ToLocal(Space.BleedTopLeft() + Space.BleedSize()).Equals(Case.Local, 0.01f));
	}

	// Text: measures something in every font, wraps, keeps every word
	for (const EBeyondFont Style : { EBeyondFont::Display, EBeyondFont::Heading, EBeyondFont::Number, EBeyondFont::Body, EBeyondFont::BodyItalic })
	{
		const FVector2f Size = MeasureText(TEXT("Combat Stats"), Font(Style, 18.0f));
		TestTrue(FString::Printf(TEXT("Font %d measures"), static_cast<int32>(Style)), Size.X > 20.0f && Size.Y > 8.0f);
	}
	const FString Long = TEXT("After using a skill, gain a shield equal to 3% of Max Health for 8s.");
	const FSlateFontInfo Body = Font(EBeyondFont::Body, 16.0f);
	const TArray<FString> Lines = WrapText(Long, Body, 200.0f);
	TestTrue(TEXT("Long text wraps"), Lines.Num() > 1);
	TestEqual(TEXT("Wrapping keeps every word"), FString::Join(Lines, TEXT(" ")), Long);
	for (const FString& Line : Lines)
	{
		TestTrue(TEXT("Wrapped lines fit (single words may not)"), MeasureText(Line, Body).X <= 200.0f || !Line.Contains(TEXT(" ")));
	}

	// Formatting
	TestEqual(TEXT("Thousands"), FormatNumber(20340), FString(TEXT("20,340")));
	TestEqual(TEXT("Millions"), FormatNumber(1234567), FString(TEXT("1,234,567")));
	TestEqual(TEXT("Small"), FormatNumber(12), FString(TEXT("12")));
	TestEqual(TEXT("Negative"), FormatNumber(-4500), FString(TEXT("-4,500")));
	TestEqual(TEXT("Percent"), FormatPercent(18.0f), FString(TEXT("18%")));
	TestEqual(TEXT("Percent with decimals"), FormatPercent(4.5f, 1), FString(TEXT("4.5%")));
	TestEqual(TEXT("Playtime"), FormatPlaytime(32 * 3600 + 18 * 60 + 45), FString(TEXT("32:18:45")));

	// Palette
	TestTrue(TEXT("Legendary is gold, Epic purple, Rare blue"), TierColor(EBeyondItemTier::Legendary).R > TierColor(EBeyondItemTier::Legendary).B
		&& TierColor(EBeyondItemTier::Epic).B > TierColor(EBeyondItemTier::Epic).G && TierColor(EBeyondItemTier::Rare).B > TierColor(EBeyondItemTier::Rare).R);
	TestEqual(TEXT("Every glyph but None has strokes"), GetGlyphStrokes(EBeyondGlyph::None).Num(), 0);
	for (int32 Index = 1; Index <= static_cast<int32>(EBeyondGlyph::Flame); ++Index)
	{
		TestTrue(FString::Printf(TEXT("Glyph %d has strokes"), Index), GetGlyphStrokes(static_cast<EBeyondGlyph>(Index)).Num() > 0);
	}

	// Fonts from migrate_pass20.py
	if (UBeyondUISubsystem* UI = UBeyondUISubsystem::Get())
	{
		UI->Reload();
		const bool bHasFonts = !UBeyondUIStyleSettings::Get()->HeadingFont.IsNull() && UI->GetHeadingFont() && UI->GetBodyFont();
		if (bHasFonts)
		{
			TestEqual(TEXT("Cinzel has Regular and Bold"), UI->GetHeadingFont()->GetInternalCompositeFont().DefaultTypeface.Fonts.Num(), 2);
			TestEqual(TEXT("EB Garamond has Regular, Italic and Bold"), UI->GetBodyFont()->GetInternalCompositeFont().DefaultTypeface.Fonts.Num(), 3);
			TestTrue(TEXT("Headings use Cinzel"), Font(EBeyondFont::Heading, 16.0f).FontObject == UI->GetHeadingFont());
			TestTrue(TEXT("Body text uses EB Garamond"), Font(EBeyondFont::Body, 16.0f).FontObject == UI->GetBodyFont());
		}
		else
		{
			AddInfo(TEXT("UI fonts not made yet (run migrate_pass20.py): the engine font is used"));
		}
	}
	return true;
}

#endif
