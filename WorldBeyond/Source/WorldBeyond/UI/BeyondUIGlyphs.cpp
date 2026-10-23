// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondUIGlyphs.h"

namespace BeyondUIGlyphLocal
{
	using FStroke = TArray<FVector2f>;

	FStroke Arc(const FVector2f& Centre, float Radius, float FromDegrees, float ToDegrees, int32 Segments = 16)
	{
		FStroke Points;
		Points.Reserve(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = FMath::DegreesToRadians(FMath::Lerp(FromDegrees, ToDegrees, static_cast<float>(Index) / Segments));
			Points.Add(Centre + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		return Points;
	}

	FStroke Circle(const FVector2f& Centre, float Radius, int32 Segments = 20)
	{
		return Arc(Centre, Radius, 0.0f, 360.0f, Segments);
	}

	FStroke Poly(std::initializer_list<FVector2f> Points, bool bClosed = false)
	{
		FStroke Stroke(Points);
		if (bClosed && Stroke.Num() > 0)
		{
			const FVector2f First = Stroke[0];
			Stroke.Add(First);
		}
		return Stroke;
	}

	FStroke Diamond(const FVector2f& Centre, float HalfWidth, float HalfHeight)
	{
		return Poly({ Centre + FVector2f(0.0f, -HalfHeight), Centre + FVector2f(HalfWidth, 0.0f), Centre + FVector2f(0.0f, HalfHeight),
			Centre + FVector2f(-HalfWidth, 0.0f) }, true);
	}

	// A four-pointed star outline
	FStroke Sparkle(const FVector2f& Centre, float Radius, float Waist)
	{
		FStroke Points;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const float Angle = FMath::DegreesToRadians(-90.0f + Index * 45.0f);
			const float R = (Index % 2 == 0) ? Radius : Radius * Waist;
			Points.Add(Centre + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * R);
		}
		const FVector2f First = Points[0];
		Points.Add(First);
		return Points;
	}
}

TArray<TArray<FVector2f>> BeyondUI::GetGlyphStrokes(EBeyondGlyph Glyph)
{
	using namespace BeyondUIGlyphLocal;
	TArray<FStroke> S;
	switch (Glyph)
	{
	//~ Equipment
	case EBeyondGlyph::Helm:
		S.Add(Arc(FVector2f(0.0f, 0.06f), 0.3f, 180.0f, 360.0f));
		S.Add(Poly({ { -0.3f, 0.06f }, { -0.3f, 0.3f }, { -0.1f, 0.3f }, { -0.06f, 0.08f } }));
		S.Add(Poly({ { 0.3f, 0.06f }, { 0.3f, 0.3f }, { 0.1f, 0.3f }, { 0.06f, 0.08f } }));
		S.Add(Poly({ { -0.3f, 0.06f }, { 0.3f, 0.06f } }));
		S.Add(Poly({ { 0.0f, -0.24f }, { 0.0f, 0.26f } }));
		break;
	case EBeyondGlyph::Chest:
	case EBeyondGlyph::Armor:
		S.Add(Poly({ { -0.12f, -0.32f }, { -0.38f, -0.2f }, { -0.3f, 0.02f }, { -0.25f, 0.34f }, { 0.25f, 0.34f }, { 0.3f, 0.02f }, { 0.38f, -0.2f },
			{ 0.12f, -0.32f }, { 0.0f, -0.2f } }, true));
		S.Add(Poly({ { 0.0f, -0.2f }, { 0.0f, 0.34f } }));
		S.Add(Poly({ { -0.22f, 0.1f }, { 0.22f, 0.1f } }));
		break;
	case EBeyondGlyph::Gauntlets:
		S.Add(Poly({ { -0.2f, 0.36f }, { -0.2f, 0.14f }, { 0.2f, 0.14f }, { 0.2f, 0.36f } }, true));
		S.Add(Poly({ { -0.2f, 0.14f }, { -0.2f, -0.1f }, { 0.2f, -0.1f }, { 0.2f, 0.14f } }));
		for (const float X : { -0.15f, -0.05f, 0.05f, 0.15f })
		{
			S.Add(Poly({ { X, -0.1f }, { X, -0.34f } }));
		}
		S.Add(Poly({ { -0.2f, 0.04f }, { -0.36f, -0.1f } }));
		break;
	case EBeyondGlyph::Boots:
		S.Add(Poly({ { -0.2f, -0.36f }, { 0.05f, -0.36f }, { 0.05f, 0.06f }, { 0.34f, 0.17f }, { 0.34f, 0.34f }, { -0.2f, 0.34f } }, true));
		S.Add(Poly({ { -0.2f, -0.2f }, { 0.05f, -0.2f } }));
		break;
	case EBeyondGlyph::Sword:
		S.Add(Poly({ { -0.05f, 0.14f }, { -0.05f, -0.32f }, { 0.0f, -0.42f }, { 0.05f, -0.32f }, { 0.05f, 0.14f } }));
		S.Add(Poly({ { -0.2f, 0.14f }, { 0.2f, 0.14f } }));
		S.Add(Poly({ { 0.0f, 0.14f }, { 0.0f, 0.34f } }));
		S.Add(Circle(FVector2f(0.0f, 0.38f), 0.04f, 8));
		break;
	case EBeyondGlyph::Staff:
		S.Add(Poly({ { 0.0f, -0.18f }, { 0.0f, 0.42f } }));
		S.Add(Circle(FVector2f(0.0f, -0.28f), 0.09f, 12));
		S.Add(Poly({ { -0.15f, -0.4f }, { -0.1f, -0.2f }, { 0.0f, -0.15f }, { 0.1f, -0.2f }, { 0.15f, -0.4f } }));
		break;
	case EBeyondGlyph::Ring:
		S.Add(Circle(FVector2f(0.0f, 0.1f), 0.24f, 24));
		S.Add(Diamond(FVector2f(0.0f, -0.2f), 0.1f, 0.1f));
		break;
	case EBeyondGlyph::Amulet:
		S.Add(Poly({ { -0.26f, -0.42f }, { 0.0f, -0.08f }, { 0.26f, -0.42f } }));
		S.Add(Diamond(FVector2f(0.0f, 0.14f), 0.15f, 0.24f));
		S.Add(Diamond(FVector2f(0.0f, 0.14f), 0.05f, 0.08f));
		break;

	//~ Stats
	case EBeyondGlyph::Heart:
		S.Add(Poly({ { 0.0f, 0.34f }, { -0.34f, 0.0f } }));
		S.Last().Append(Arc(FVector2f(-0.17f, -0.1f), 0.18f, 150.0f, 330.0f, 10));
		S.Last().Append(Arc(FVector2f(0.17f, -0.1f), 0.18f, 210.0f, 390.0f, 10));
		S.Last().Add(FVector2f(0.34f, 0.0f));
		S.Last().Add(FVector2f(0.0f, 0.34f));
		break;
	case EBeyondGlyph::Stamina:
		for (const float Y : { -0.2f, 0.0f, 0.2f })
		{
			const float Inset = Y == 0.0f ? 0.0f : 0.08f;
			S.Add(Poly({ { -0.38f + Inset, Y }, { -0.12f, Y - 0.06f }, { 0.12f, Y + 0.06f }, { 0.38f - Inset, Y } }));
		}
		break;
	case EBeyondGlyph::Attack:
		S.Add(Poly({ { -0.32f, 0.32f }, { 0.3f, -0.3f }, { 0.38f, -0.38f }, { 0.36f, -0.26f } }));
		S.Add(Poly({ { -0.3f, 0.12f }, { -0.12f, 0.3f } }));
		S.Add(Poly({ { -0.32f, 0.32f }, { -0.4f, 0.4f } }));
		break;
	case EBeyondGlyph::Magic:
		S.Add(Poly({ { -0.36f, 0.36f }, { 0.12f, -0.12f } }));
		S.Add(Sparkle(FVector2f(0.2f, -0.2f), 0.2f, 0.3f));
		break;
	case EBeyondGlyph::Shield:
		S.Add(Poly({ { 0.0f, -0.4f }, { 0.32f, -0.28f }, { 0.3f, 0.06f }, { 0.0f, 0.4f }, { -0.3f, 0.06f }, { -0.32f, -0.28f } }, true));
		S.Add(Poly({ { 0.0f, -0.28f }, { 0.0f, 0.26f } }));
		break;
	case EBeyondGlyph::Crit:
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const float Angle = FMath::DegreesToRadians(Index * 45.0f);
			const float Inner = Index % 2 == 0 ? 0.1f : 0.16f;
			const float Outer = Index % 2 == 0 ? 0.42f : 0.3f;
			const FVector2f Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			S.Add(Poly({ Dir * Inner, Dir * Outer }));
		}
		break;
	case EBeyondGlyph::CritDamage:
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const float Angle = FMath::DegreesToRadians(Index * 45.0f + 22.5f);
			const FVector2f Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			S.Add(Poly({ Dir * 0.24f, Dir * 0.42f }));
		}
		S.Add(Sparkle(FVector2f::ZeroVector, 0.2f, 0.35f));
		break;
	case EBeyondGlyph::Mastery:
		S.Add(Circle(FVector2f(0.0f, -0.13f), 0.17f));
		S.Add(Circle(FVector2f(-0.15f, 0.12f), 0.17f));
		S.Add(Circle(FVector2f(0.15f, 0.12f), 0.17f));
		break;

	//~ Currencies
	case EBeyondGlyph::Coin:
		S.Add(Circle(FVector2f::ZeroVector, 0.36f, 24));
		S.Add(Circle(FVector2f::ZeroVector, 0.26f, 20));
		S.Add(Sparkle(FVector2f::ZeroVector, 0.14f, 0.35f));
		break;
	case EBeyondGlyph::Crystal:
		S.Add(Poly({ { 0.0f, -0.42f }, { 0.2f, -0.1f }, { 0.0f, 0.42f }, { -0.2f, -0.1f } }, true));
		S.Add(Poly({ { -0.2f, -0.1f }, { 0.2f, -0.1f } }));
		S.Add(Poly({ { 0.0f, -0.42f }, { 0.0f, 0.42f } }));
		break;
	case EBeyondGlyph::Shard:
		S.Add(Poly({ { -0.06f, -0.4f }, { 0.16f, 0.0f }, { -0.02f, 0.38f }, { -0.2f, 0.02f } }, true));
		S.Add(Poly({ { 0.16f, -0.24f }, { 0.34f, 0.04f }, { 0.22f, 0.24f } }));
		break;
	case EBeyondGlyph::Fragment:
		S.Add(Arc(FVector2f(-0.04f, 0.0f), 0.32f, 60.0f, 300.0f, 18));
		S.Last().Append(Arc(FVector2f(0.1f, 0.0f), 0.24f, 240.0f, 120.0f, 14));
		S.Add(Sparkle(FVector2f(0.26f, -0.2f), 0.12f, 0.3f));
		break;
	case EBeyondGlyph::SkillPoint:
	case EBeyondGlyph::Star:
		S.Add(Sparkle(FVector2f::ZeroVector, 0.44f, 0.28f));
		break;
	case EBeyondGlyph::BondPoint:
	case EBeyondGlyph::Bond:
		S.Add(Arc(FVector2f(-0.04f, 0.0f), 0.34f, 50.0f, 310.0f, 18));
		S.Last().Append(Arc(FVector2f(0.12f, 0.0f), 0.26f, 250.0f, 110.0f, 14));
		S.Add(Sparkle(FVector2f(0.18f, 0.0f), 0.13f, 0.3f));
		break;

	//~ Menu pages
	case EBeyondGlyph::SkillTree:
		S.Add(Circle(FVector2f(0.0f, -0.28f), 0.1f, 12));
		S.Add(Circle(FVector2f(-0.28f, 0.26f), 0.1f, 12));
		S.Add(Circle(FVector2f(0.28f, 0.26f), 0.1f, 12));
		S.Add(Poly({ { 0.0f, -0.18f }, { 0.0f, 0.02f }, { -0.28f, 0.02f }, { -0.28f, 0.16f } }));
		S.Add(Poly({ { 0.0f, 0.02f }, { 0.28f, 0.02f }, { 0.28f, 0.16f } }));
		break;
	case EBeyondGlyph::Overview:
		S.Add(Poly({ { -0.28f, -0.4f }, { 0.14f, -0.4f }, { 0.28f, -0.26f }, { 0.28f, 0.4f }, { -0.28f, 0.4f } }, true));
		for (const float Y : { -0.16f, 0.0f, 0.16f })
		{
			S.Add(Poly({ { -0.16f, Y }, { 0.16f, Y } }));
		}
		break;
	case EBeyondGlyph::Quests:
		S.Add(Poly({ { -0.26f, -0.36f }, { 0.3f, -0.36f }, { 0.3f, 0.3f }, { -0.2f, 0.3f } }));
		S.Add(Arc(FVector2f(-0.26f, -0.3f), 0.07f, 270.0f, 90.0f, 8));
		S.Add(Poly({ { -0.26f, -0.36f }, { -0.26f, 0.36f } }));
		S.Last().Append(Arc(FVector2f(-0.2f, 0.36f), 0.06f, 180.0f, 360.0f, 6));
		S.Add(Poly({ { -0.12f, -0.18f }, { 0.18f, -0.18f } }));
		S.Add(Poly({ { -0.12f, -0.02f }, { 0.18f, -0.02f } }));
		S.Add(Poly({ { -0.12f, 0.14f }, { 0.08f, 0.14f } }));
		break;
	case EBeyondGlyph::Map:
		S.Add(Poly({ { -0.4f, -0.3f }, { -0.14f, -0.38f }, { 0.14f, -0.3f }, { 0.4f, -0.38f }, { 0.4f, 0.3f }, { 0.14f, 0.38f }, { -0.14f, 0.3f },
			{ -0.4f, 0.38f } }, true));
		S.Add(Poly({ { -0.14f, -0.38f }, { -0.14f, 0.3f } }));
		S.Add(Poly({ { 0.14f, -0.3f }, { 0.14f, 0.38f } }));
		break;

	//~ Badges
	case EBeyondGlyph::Lock:
		S.Add(Poly({ { -0.26f, -0.04f }, { 0.26f, -0.04f }, { 0.26f, 0.38f }, { -0.26f, 0.38f } }, true));
		S.Add(Arc(FVector2f(0.0f, -0.12f), 0.17f, 180.0f, 360.0f, 10));
		S.Last().Insert(FVector2f(-0.17f, -0.04f), 0);
		S.Last().Add(FVector2f(0.17f, -0.04f));
		S.Add(Poly({ { 0.0f, 0.1f }, { 0.0f, 0.22f } }));
		break;
	case EBeyondGlyph::Grid:
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Column = 0; Column < 3; ++Column)
			{
				const FVector2f TopLeft(-0.36f + Column * 0.26f, -0.36f + Row * 0.26f);
				S.Add(Poly({ TopLeft, TopLeft + FVector2f(0.2f, 0.0f), TopLeft + FVector2f(0.2f, 0.2f), TopLeft + FVector2f(0.0f, 0.2f) }, true));
			}
		}
		break;
	case EBeyondGlyph::Check:
		S.Add(Poly({ { -0.32f, 0.02f }, { -0.08f, 0.26f }, { 0.34f, -0.24f } }));
		break;
	case EBeyondGlyph::Cross:
		S.Add(Poly({ { -0.28f, -0.28f }, { 0.28f, 0.28f } }));
		S.Add(Poly({ { -0.28f, 0.28f }, { 0.28f, -0.28f } }));
		break;
	case EBeyondGlyph::Up:
		S.Add(Poly({ { -0.3f, 0.02f }, { 0.0f, -0.28f }, { 0.3f, 0.02f } }));
		S.Add(Poly({ { -0.3f, 0.3f }, { 0.0f, 0.0f }, { 0.3f, 0.3f } }));
		break;
	case EBeyondGlyph::Hourglass:
		S.Add(Poly({ { -0.26f, -0.38f }, { 0.26f, -0.38f }, { -0.26f, 0.38f }, { 0.26f, 0.38f } }, true));
		break;
	case EBeyondGlyph::Compass:
		S.Add(Diamond(FVector2f::ZeroVector, 0.4f, 0.4f));
		S.Add(Poly({ { 0.0f, -0.4f }, { 0.0f, 0.4f } }));
		S.Add(Poly({ { -0.4f, 0.0f }, { 0.4f, 0.0f } }));
		break;
	case EBeyondGlyph::Skull:
		S.Add(Arc(FVector2f(0.0f, -0.06f), 0.3f, 150.0f, 390.0f, 18));
		S.Last().Append({ FVector2f(0.18f, 0.22f), FVector2f(0.18f, 0.36f), FVector2f(-0.18f, 0.36f), FVector2f(-0.18f, 0.22f) });
		{
			const FVector2f First = S.Last()[0];
			S.Last().Add(First);
		}
		S.Add(Circle(FVector2f(-0.12f, -0.04f), 0.07f, 8));
		S.Add(Circle(FVector2f(0.12f, -0.04f), 0.07f, 8));
		break;

	//~ Elements
	case EBeyondGlyph::Storm:
		S.Add(Poly({ { 0.08f, -0.42f }, { -0.2f, 0.04f }, { 0.02f, 0.04f }, { -0.1f, 0.42f }, { 0.24f, -0.08f }, { 0.02f, -0.08f } }, true));
		break;
	case EBeyondGlyph::Radiant:
		S.Add(Circle(FVector2f::ZeroVector, 0.16f, 16));
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const float Angle = FMath::DegreesToRadians(Index * 45.0f);
			const FVector2f Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			S.Add(Poly({ Dir * 0.24f, Dir * (Index % 2 == 0 ? 0.42f : 0.34f) }));
		}
		break;
	case EBeyondGlyph::Eclipse:
		S.Add(Circle(FVector2f::ZeroVector, 0.34f, 24));
		S.Add(Arc(FVector2f(0.12f, -0.06f), 0.28f, 110.0f, 250.0f, 12));
		break;
	case EBeyondGlyph::Verdant:
		S.Add(Poly({ { -0.3f, 0.34f }, { -0.3f, 0.0f } }));
		S.Last().Append(Arc(FVector2f(0.08f, 0.04f), 0.38f, 180.0f, 270.0f, 8));
		S.Last().Add(FVector2f(0.3f, -0.34f));
		S.Last().Append(Arc(FVector2f(-0.06f, -0.0f), 0.36f, -20.0f, 90.0f, 8));
		S.Last().Add(FVector2f(-0.3f, 0.34f));
		S.Add(Poly({ { -0.3f, 0.34f }, { 0.16f, -0.14f } }));
		break;
	case EBeyondGlyph::Shadow:
		S.Add(Arc(FVector2f::ZeroVector, 0.36f, 0.0f, 360.0f, 24));
		S.Add(Poly({ { -0.3f, 0.0f }, { -0.1f, -0.12f }, { 0.1f, -0.12f }, { 0.3f, 0.0f }, { 0.1f, 0.12f }, { -0.1f, 0.12f } }, true));
		S.Add(Circle(FVector2f::ZeroVector, 0.05f, 8));
		break;
	case EBeyondGlyph::Frost:
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const float Angle = FMath::DegreesToRadians(90.0f + Index * 60.0f);
			const FVector2f Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			S.Add(Poly({ -Dir * 0.42f, Dir * 0.42f }));
			for (const float Side : { -1.0f, 1.0f })
			{
				const FVector2f Tip = Dir * 0.28f * Side;
				const FVector2f Normal(-Dir.Y, Dir.X);
				S.Add(Poly({ Tip + Normal * 0.1f + Dir * 0.08f * Side, Tip, Tip - Normal * 0.1f + Dir * 0.08f * Side }));
			}
		}
		break;
	case EBeyondGlyph::Flame:
		S.Add(Poly({ { 0.0f, 0.4f }, { -0.26f, 0.22f }, { -0.28f, -0.04f }, { -0.12f, -0.22f }, { -0.06f, -0.04f }, { 0.04f, -0.42f }, { 0.22f, -0.16f },
			{ 0.28f, 0.14f }, { 0.18f, 0.32f } }, true));
		S.Add(Poly({ { 0.0f, 0.32f }, { -0.1f, 0.16f }, { 0.0f, 0.0f }, { 0.1f, 0.16f } }, true));
		break;
	default:
		break;
	}
	return S;
}
