// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondQuestLogWidget.h"

#define LOCTEXT_NAMESPACE "BeyondQuestLog"

namespace BeyondQuestLogLocal
{
	const FVector2f ListTopLeft(320.0f, 118.0f);
	const FVector2f ListSize(500.0f, 870.0f);
	const FVector2f DetailTopLeft(836.0f, 118.0f);
	const FVector2f DetailSize(1054.0f, 870.0f);
	constexpr float EntryTop = 186.0f;
	constexpr float EntryHeight = 76.0f;
	constexpr float EntryGap = 8.0f;

	struct FChapter
	{
		const TCHAR* Chapter;
		const TCHAR* Title;
		const TCHAR* Summary;
	};

	// The main story's chapters (their quests come with the quest system)
	const FChapter Chapters[] = {
		{ TEXT("Chapter 1"), TEXT("Storm over Mossbrook"), TEXT("Something howls in the woods around Mossbrook, and the village elder wants to talk.") },
		{ TEXT("Chapter 2"), TEXT("The Pale Alpha"), TEXT("A great pale wolf leads the forest's packs. The standing stones remember where it hunts.") },
		{ TEXT("Chapter 3"), TEXT("The Hollow Voice"), TEXT("A voice calls from the corrupted woods, and the blight answers it.") },
		{ TEXT("Chapter 4"), TEXT("Rime and Ruin"), TEXT("Beyond Kingsfork the passes freeze over, and a troll king holds the heights.") },
		{ TEXT("Chapter 5"), TEXT("The Molten Crown"), TEXT("The caldera burns. Something colossal stirs beneath the ash.") },
		{ TEXT("Chapter 6"), TEXT("Fractured Skies"), TEXT("An ancient spire splits the sky. The storm and the sun meet there.") },
	};
}

void UBeyondQuestLogWidget::BuildHeaderTabs(TArray<FBeyondMenuTab>& OutTabs) const
{
	const FLinearColor Accent = BeyondUI::Palette().Blue;
	for (const FText& Title : { LOCTEXT("All", "All"), LOCTEXT("Main", "Main Story"), LOCTEXT("Side", "Side Quests") })
	{
		FBeyondMenuTab& Tab = OutTabs.AddDefaulted_GetRef();
		Tab.Title = Title;
		Tab.Accent = Accent;
	}
}

FText UBeyondQuestLogWidget::GetTitle() const
{
	return LOCTEXT("Title", "Quests");
}

int32 UBeyondQuestLogWidget::HitTestEntry(const FVector2f& Design) const
{
	using namespace BeyondQuestLogLocal;
	if (GetFilter() == 2 || Design.X < ListTopLeft.X + 16.0f || Design.X > ListTopLeft.X + ListSize.X - 16.0f)
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Chapters); ++Index)
	{
		const float Top = EntryTop + Index * (EntryHeight + EntryGap);
		if (Design.Y >= Top && Design.Y <= Top + EntryHeight)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UBeyondQuestLogWidget::HandlePageHover(const FVector2f& Design)
{
	HoveredEntry = HitTestEntry(Design);
}

bool UBeyondQuestLogWidget::HandlePageClick(const FVector2f& Design, const FKey& Button)
{
	const int32 Entry = HitTestEntry(Design);
	if (Button == EKeys::LeftMouseButton && Entry != INDEX_NONE)
	{
		SelectedEntry = Entry;
		PlaySelectSound();
		return true;
	}
	return false;
}

bool UBeyondQuestLogWidget::HandlePageKey(const FKey& Key)
{
	if (Key == EKeys::Up || Key == EKeys::Down)
	{
		SelectedEntry = FMath::Clamp(SelectedEntry + (Key == EKeys::Down ? 1 : -1), 0, static_cast<int32>(UE_ARRAY_COUNT(BeyondQuestLogLocal::Chapters)) - 1);
		return true;
	}
	return false;
}

void UBeyondQuestLogWidget::GetPageKeyHints(TArray<FBeyondKeyHint>& OutHints) const
{
	OutHints.Add({ TEXT("LMB"), LOCTEXT("Select", "Select"), EKeys::LeftMouseButton });
	OutHints.Add({ TEXT("Q  E"), LOCTEXT("Filter", "Filter"), EKeys::E });
	OutHints.Add({ TEXT("C"), LOCTEXT("Overview", "Overview"), EKeys::C });
}

void UBeyondQuestLogWidget::PaintPage(BeyondUI::FPainter& Paint) const
{
	using namespace BeyondUI;
	using namespace BeyondQuestLogLocal;
	const FPalette& P = Palette();

	//~ The list
	Paint.Panel(ListTopLeft, ListSize, 20);
	Paint.SectionHeader(GetFilter() == 2 ? LOCTEXT("SideHeader", "Side Quests").ToString() : LOCTEXT("MainHeader", "Main Story").ToString(),
		ListTopLeft + FVector2f(24.0f, 18.0f), ListSize.X - 48.0f, 22, 18.0f);
	if (GetFilter() == 2)
	{
		Paint.Paragraph(LOCTEXT("NoSide", "No side quests yet. Villagers across the Dominion will have work for you.").ToString(), Font(EBeyondFont::BodyItalic, 19.0f),
			FVector2f(ListTopLeft.X + 28.0f, EntryTop + 8.0f), ListSize.X - 56.0f, 26.0f, P.TextDim, 22);
	}
	else
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Chapters); ++Index)
		{
			const FVector2f TopLeft(ListTopLeft.X + 18.0f, EntryTop + Index * (EntryHeight + EntryGap));
			const FVector2f Size(ListSize.X - 36.0f, EntryHeight);
			const bool bSelected = Index == SelectedEntry;
			if (bSelected)
			{
				Paint.GradientH(TopLeft, Size, WithAlpha(P.Blue, 0.5f), WithAlpha(P.Blue, 0.05f), 22);
				Paint.RectOutline(TopLeft, Size, WithAlpha(P.BlueBright, 0.7f), 1.0f, 23);
				Paint.Diamond(FVector2f(TopLeft.X, TopLeft.Y + Size.Y * 0.5f), 6.0f, 8.0f, P.GoldBright, 24);
			}
			else
			{
				Paint.GradientH(TopLeft, Size, FromHex(0x141C30, Index == HoveredEntry ? 0.95f : 0.7f), FromHex(0x0A0F1C, 0.3f), 22);
				Paint.RectOutline(TopLeft, Size, WithAlpha(P.Silver, 0.22f), 1.0f, 23);
			}
			Paint.DiamondOutline(TopLeft + FVector2f(34.0f, Size.Y * 0.5f), 12.0f, 16.0f, WithAlpha(P.Gold, 0.8f), 1.4f, 24);
			Paint.Text(Chapters[Index].Chapter, Font(EBeyondFont::Body, 16.0f), TopLeft + FVector2f(64.0f, 10.0f), P.GoldBright, 0.0f, 24);
			Paint.Text(Chapters[Index].Title, Font(EBeyondFont::Heading, 20.0f), TopLeft + FVector2f(64.0f, 34.0f), bSelected ? FLinearColor::White : P.Text, 0.0f, 24);
		}
	}

	//~ The selected chapter
	Paint.Panel(DetailTopLeft, DetailSize, 20);
	const FChapter& Chapter = Chapters[FMath::Clamp(SelectedEntry, 0, static_cast<int32>(UE_ARRAY_COUNT(Chapters)) - 1)];
	const FVector2f BannerTopLeft = DetailTopLeft + FVector2f(28.0f, 28.0f);
	const FVector2f BannerSize(DetailSize.X - 56.0f, 380.0f);
	Paint.GradientV(BannerTopLeft, BannerSize, FromHex(0x1C2C50), FromHex(0x070B16), 21);
	Paint.Glyph(EBeyondGlyph::Compass, BannerTopLeft + BannerSize * 0.5f, 160.0f, WithAlpha(P.Gold, 0.25f), 22, 2.0f);
	Paint.GradientV(BannerTopLeft + FVector2f(0.0f, BannerSize.Y * 0.5f), FVector2f(BannerSize.X, BannerSize.Y * 0.5f), WithAlpha(P.Backdrop, 0.0f), WithAlpha(P.Backdrop, 0.9f), 23);
	Paint.RectOutline(BannerTopLeft, BannerSize, WithAlpha(P.Gold, 0.5f), 1.0f, 24);
	Paint.Text(Chapter.Chapter, Font(EBeyondFont::Heading, 18.0f), BannerTopLeft + FVector2f(30.0f, BannerSize.Y - 96.0f), P.GoldBright, 0.0f, 25);
	Paint.Text(Chapter.Title, Font(EBeyondFont::Display, 40.0f), BannerTopLeft + FVector2f(28.0f, BannerSize.Y - 68.0f), P.Text, 0.0f, 25);

	float Y = BannerTopLeft.Y + BannerSize.Y + 28.0f;
	Y += Paint.Paragraph(Chapter.Summary, Font(EBeyondFont::Body, 22.0f), FVector2f(BannerTopLeft.X + 4.0f, Y), BannerSize.X - 8.0f, 30.0f, P.Text, 22, 3) + 18.0f;
	Y = Paint.SectionHeader(LOCTEXT("Objectives", "Objectives").ToString(), FVector2f(BannerTopLeft.X, Y), BannerSize.X, 22, 18.0f);
	Paint.DiamondOutline(FVector2f(BannerTopLeft.X + 14.0f, Y + 14.0f), 7.0f, 7.0f, WithAlpha(P.Silver, 0.7f), 1.2f, 22);
	Paint.Text(LOCTEXT("Sealed", "The quest log opens with the next update: objectives, rewards and tracking.").ToString(), Font(EBeyondFont::BodyItalic, 19.0f),
		FVector2f(BannerTopLeft.X + 34.0f, Y), P.TextDim, 0.0f, 22);
}

#undef LOCTEXT_NAMESPACE
