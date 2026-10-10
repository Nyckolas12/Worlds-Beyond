// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondBossBarWidget.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "BeyondGameplayTags.h"
#include "Characters/BeyondCharacterBase.h"
#include "Enemies/BeyondBossCharacter.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr float BossChipSpeed = 0.35f;
	constexpr float BossBarSpacing = 62.0f;
	constexpr float BossAnnouncementFade = 0.4f;

	FVector2f MeasureBossText(const FString& Text, const FSlateFontInfo& Font)
	{
		FSlateRenderer* Renderer = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetRenderer() : nullptr;
		if (!Renderer)
		{
			return FVector2f(Text.Len() * Font.Size * 0.6f, Font.Size * 1.2f);
		}
		return FVector2f(Renderer->GetFontMeasureService()->Measure(Text, Font));
	}
}

void UBeyondBossBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBeyondBossBarWidget::SetBosses(const TArray<ABeyondCharacterBase*>& Bosses)
{
	TArray<FEntry> Updated;
	for (ABeyondCharacterBase* Boss : Bosses)
	{
		if (!Boss || Updated.Num() >= 2)
		{
			continue;
		}
		FEntry Entry;
		if (const FEntry* Previous = Entries.FindByPredicate([Boss](const FEntry& Old) { return Old.Boss.Get() == Boss; }))
		{
			Entry = *Previous;
		}
		else
		{
			Entry.Health = UBeyondCombatLibrary::GetActorHealthPercent(Boss);
			Entry.Chip = Entry.Health;
		}
		Entry.Boss = Boss;
		Updated.Add(Entry);
	}
	Entries = MoveTemp(Updated);
}

TArray<ABeyondCharacterBase*> UBeyondBossBarWidget::GetBosses() const
{
	TArray<ABeyondCharacterBase*> Result;
	for (const FEntry& Entry : Entries)
	{
		if (ABeyondCharacterBase* Boss = Entry.Boss.Get())
		{
			Result.Add(Boss);
		}
	}
	return Result;
}

void UBeyondBossBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;

	for (FEntry& Entry : Entries)
	{
		const ABeyondCharacterBase* Boss = Entry.Boss.Get();
		if (!Boss)
		{
			continue;
		}
		const ABeyondEnemyCharacter* Enemy = Cast<ABeyondEnemyCharacter>(Boss);
		Entry.Name = (Enemy ? Enemy->GetEnemyName() : Boss->GetCharacterDisplayName()).ToString().ToUpper();
		Entry.bMiniBoss = Boss->Rank == EBeyondEnemyRank::MiniBoss;
		Entry.Health = FMath::Clamp(UBeyondCombatLibrary::GetActorHealthPercent(Boss), 0.0f, 1.0f);
		// The chip waits a moment, then slides down to the health
		Entry.Chip = Entry.Chip > Entry.Health ? FMath::Max(Entry.Health, Entry.Chip - BossChipSpeed * InDeltaTime) : Entry.Health;
		const UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
		Entry.bInvulnerable = ASC && ASC->HasMatchingGameplayTag(BeyondTags::State_Invincible);

		if (const ABeyondBossCharacter* BossCharacter = Cast<ABeyondBossCharacter>(Boss))
		{
			Entry.Title = BossCharacter->GetBossTitle().ToString();
			Entry.Notches = BossCharacter->GetPhaseThresholds();
			Entry.Phase = BossCharacter->GetPhaseIndex();
			Entry.PhaseCount = BossCharacter->GetPhaseCount();
			FText Announcement;
			float Age = 0.0f;
			if (BossCharacter->GetAnnouncement(Announcement, Age))
			{
				Entry.Announcement = Announcement.ToString();
				Entry.AnnouncementAlpha = FMath::Clamp(Age / BossAnnouncementFade, 0.0f, 1.0f)
					* FMath::Clamp((3.5f - Age) / BossAnnouncementFade, 0.0f, 1.0f);
			}
			else
			{
				Entry.AnnouncementAlpha = 0.0f;
			}
		}
	}
}

int32 UBeyondBossBarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 BackLayer = BaseLayer + 1;
	const int32 FillLayer = BaseLayer + 2;
	const int32 TopLayer = BaseLayer + 3;
	const int32 TextLayer = BaseLayer + 4;

	const FVector2f Screen(AllottedGeometry.GetLocalSize());
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
	const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", 15);
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Italic", 10);
	const FSlateFontInfo BannerFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);

	auto DrawBox = [&](const FVector2f& TopLeft, const FVector2f& Size, const FLinearColor& Tint, int32 Layer)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft)),
			WhiteBrush, ESlateDrawEffect::None, Tint);
	};
	auto DrawBossText = [&](const FString& Text, const FSlateFontInfo& Font, const FVector2f& Centre, float Top, const FLinearColor& Tint)
	{
		const FVector2f Size = MeasureBossText(Text, Font);
		const FVector2f TopLeft(Centre.X - Size.X * 0.5f, Top);
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft + FVector2f(1.5f, 1.5f))),
			Text, Font, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, Tint.A * 0.85f));
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer + 1, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft)),
			Text, Font, ESlateDrawEffect::None, Tint);
		return Size;
	};

	const float Width = FMath::Min(BarWidth, Screen.X * 0.7f);
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FEntry& Entry = Entries[Index];
		const float NameTop = TopMargin + Index * BossBarSpacing;
		const FVector2f Centre(Screen.X * 0.5f, NameTop);

		// Name, title beside it in smaller italics
		const FVector2f NameSize = DrawBossText(Entry.Name, NameFont, FVector2f(Centre.X - (Entry.Title.IsEmpty() ? 0.0f : 60.0f), 0.0f), NameTop, FLinearColor::White);
		if (!Entry.Title.IsEmpty())
		{
			const FVector2f TitleSize = MeasureBossText(Entry.Title, TitleFont);
			DrawBossText(Entry.Title, TitleFont, FVector2f(Centre.X - 60.0f + NameSize.X * 0.5f + 8.0f + TitleSize.X * 0.5f, 0.0f),
				NameTop + NameSize.Y - TitleSize.Y - 1.0f, TitleColor);
		}

		const FVector2f BarTopLeft(Centre.X - Width * 0.5f, NameTop + NameSize.Y + 3.0f);
		const FVector2f BarSize(Width, BarHeight);
		const FLinearColor Fill = Entry.bMiniBoss ? MiniBossColor : HealthColor;

		// Frame, back, chip, health
		DrawBox(BarTopLeft - FVector2f(2.0f, 2.0f), BarSize + FVector2f(4.0f, 4.0f), FLinearColor(0.75f, 0.6f, 0.35f, 0.9f), BackLayer);
		DrawBox(BarTopLeft, BarSize, FLinearColor(0.02f, 0.02f, 0.03f, 0.9f), BackLayer);
		DrawBox(BarTopLeft, FVector2f(Width * Entry.Chip, BarHeight), FLinearColor(1.0f, 0.85f, 0.55f, 0.9f), FillLayer);
		DrawBox(BarTopLeft, FVector2f(Width * Entry.Health, BarHeight), Fill, FillLayer);

		// Can't be hurt (changing phase): a pale pulsing sheen over the bar
		if (Entry.bInvulnerable)
		{
			const float Pulse = 0.25f + 0.2f * FMath::Sin(Time * 8.0f);
			DrawBox(BarTopLeft, BarSize, FLinearColor(1.0f, 0.95f, 0.75f, Pulse), TopLayer);
		}

		// Where the next phases start
		for (const float Notch : Entry.Notches)
		{
			const float X = BarTopLeft.X + Width * FMath::Clamp(Notch, 0.0f, 1.0f);
			DrawBox(FVector2f(X - 1.5f, BarTopLeft.Y - 4.0f), FVector2f(3.0f, BarHeight + 8.0f), FLinearColor(1.0f, 1.0f, 1.0f, 0.95f), TopLayer);
		}

		// Phase pips at the right end
		for (int32 Pip = 0; Pip < Entry.PhaseCount && Entry.PhaseCount > 1; ++Pip)
		{
			const FVector2f PipTopLeft(BarTopLeft.X + Width + 10.0f + Pip * 13.0f, BarTopLeft.Y + 1.0f);
			DrawBox(PipTopLeft, FVector2f(9.0f, BarHeight - 2.0f), Pip <= Entry.Phase ? Fill : FLinearColor(0.2f, 0.2f, 0.2f, 0.8f), FillLayer);
		}

		if (Entry.AnnouncementAlpha > 0.0f && !Entry.Announcement.IsEmpty())
		{
			DrawBossText(Entry.Announcement, BannerFont, FVector2f(Centre.X, 0.0f), BarTopLeft.Y + BarHeight + 8.0f,
				FLinearColor(1.0f, 0.75f, 0.35f, Entry.AnnouncementAlpha));
		}
	}
	return TextLayer + 1;
}
