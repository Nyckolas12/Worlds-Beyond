// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondQuestLogWidget.h"
#include "Engine/Texture2D.h"
#include "Items/BeyondItemLibrary.h"
#include "Player/BeyondPlayerController.h"
#include "Quests/BeyondQuestComponent.h"
#include "UI/BeyondUIArt.h"

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
	constexpr int32 VisibleEntries = 9;

	FText KindLabel(const FBeyondQuest& Quest)
	{
		return Quest.Kind == EBeyondQuestKind::Main ? FText::Format(LOCTEXT("Chapter", "Chapter {0}"), FText::AsNumber(Quest.Chapter)) : LOCTEXT("SideQuest", "Side Quest");
	}
}

UBeyondQuestComponent* UBeyondQuestLogWidget::GetQuests() const
{
	const ABeyondPlayerController* PC = GetBeyondPlayerController();
	return PC ? PC->QuestComponent.Get() : nullptr;
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

void UBeyondQuestLogWidget::OnHeaderTabSelected(int32 Index)
{
	Super::OnHeaderTabSelected(Index);
	ScrollIndex = 0;
	ScrollToSelection();
}

TArray<FName> UBeyondQuestLogWidget::GetListedQuests() const
{
	TArray<FName> Listed;
	const UBeyondQuestComponent* Quests = GetQuests();
	const UBeyondQuestDatabase* Database = Quests ? Quests->GetDatabase() : nullptr;
	if (!Database)
	{
		return Listed;
	}
	for (const EBeyondQuestKind Kind : { EBeyondQuestKind::Main, EBeyondQuestKind::Side })
	{
		if ((Kind == EBeyondQuestKind::Main && GetFilter() == 2) || (Kind == EBeyondQuestKind::Side && GetFilter() == 1))
		{
			continue;
		}
		for (const FBeyondQuest& Quest : Database->Quests)
		{
			// The story's chapters are all listed (sealed until reached); side quests once they start
			if (Quest.Kind == Kind && (Kind == EBeyondQuestKind::Main || Quests->GetQuestState(Quest.QuestId) != EBeyondQuestState::Locked))
			{
				Listed.Add(Quest.QuestId);
			}
		}
	}
	return Listed;
}

FName UBeyondQuestLogWidget::GetSelectedQuest() const
{
	const TArray<FName> Listed = GetListedQuests();
	if (Listed.Contains(SelectedQuest))
	{
		return SelectedQuest;
	}
	// Nothing picked yet: the quest being followed, else the first listed
	const UBeyondQuestComponent* Quests = GetQuests();
	const FName Focus = Quests ? Quests->GetFocusQuest() : NAME_None;
	return Listed.Contains(Focus) ? Focus : (Listed.IsEmpty() ? NAME_None : Listed[0]);
}

void UBeyondQuestLogWidget::SelectQuest(FName QuestId)
{
	SelectedQuest = QuestId;
	ScrollToSelection();
}

void UBeyondQuestLogWidget::ScrollToSelection()
{
	const int32 Index = GetListedQuests().IndexOfByKey(GetSelectedQuest());
	if (Index != INDEX_NONE)
	{
		ScrollIndex = FMath::Clamp(ScrollIndex, Index - BeyondQuestLogLocal::VisibleEntries + 1, Index);
	}
	ScrollIndex = FMath::Clamp(ScrollIndex, 0, FMath::Max(GetListedQuests().Num() - BeyondQuestLogLocal::VisibleEntries, 0));
}

bool UBeyondQuestLogWidget::TrackSelectedQuest()
{
	UBeyondQuestComponent* Quests = GetQuests();
	const FName QuestId = GetSelectedQuest();
	if (!Quests || QuestId.IsNone())
	{
		return false;
	}
	if (Quests->GetTrackedQuest() == QuestId)
	{
		Quests->SetTrackedQuest(NAME_None);
		ShowMessage(LOCTEXT("Untracked", "No longer tracking it"));
		return true;
	}
	if (!Quests->SetTrackedQuest(QuestId))
	{
		ShowMessage(Quests->GetQuestState(QuestId) == EBeyondQuestState::Completed ? LOCTEXT("AlreadyDone", "That quest is complete")
			: LOCTEXT("NotStarted", "That quest hasn't started yet"), true);
		PlayDeniedSound();
		return false;
	}
	const FBeyondQuest* Quest = Quests->FindQuest(QuestId);
	ShowMessage(FText::Format(LOCTEXT("Tracking", "Tracking {0}"), Quest ? Quest->Title : FText::FromName(QuestId)));
	PlaySelectSound();
	return true;
}

//~ Input

int32 UBeyondQuestLogWidget::HitTestEntry(const FVector2f& Design) const
{
	using namespace BeyondQuestLogLocal;
	if (Design.X < ListTopLeft.X + 16.0f || Design.X > ListTopLeft.X + ListSize.X - 16.0f)
	{
		return INDEX_NONE;
	}
	const int32 Count = GetListedQuests().Num();
	for (int32 Row = 0; Row < VisibleEntries && ScrollIndex + Row < Count; ++Row)
	{
		const float Top = EntryTop + Row * (EntryHeight + EntryGap);
		if (Design.Y >= Top && Design.Y <= Top + EntryHeight)
		{
			return ScrollIndex + Row;
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
	const TArray<FName> Listed = GetListedQuests();
	if (Button == EKeys::LeftMouseButton && Listed.IsValidIndex(Entry))
	{
		SelectQuest(Listed[Entry]);
		PlaySelectSound();
		return true;
	}
	return false;
}

bool UBeyondQuestLogWidget::HandlePageWheel(const FVector2f& Design, float Delta)
{
	ScrollIndex = FMath::Clamp(ScrollIndex - FMath::Sign(static_cast<int32>(Delta)), 0, FMath::Max(GetListedQuests().Num() - BeyondQuestLogLocal::VisibleEntries, 0));
	HoveredEntry = HitTestEntry(Design);
	return true;
}

bool UBeyondQuestLogWidget::HandlePageKey(const FKey& Key)
{
	if (Key == EKeys::Up || Key == EKeys::Down)
	{
		const TArray<FName> Listed = GetListedQuests();
		const int32 Index = Listed.IndexOfByKey(GetSelectedQuest());
		if (!Listed.IsEmpty())
		{
			SelectQuest(Listed[FMath::Clamp(Index + (Key == EKeys::Down ? 1 : -1), 0, Listed.Num() - 1)]);
		}
		return true;
	}
	if (Key == EKeys::T || Key == EKeys::F)
	{
		TrackSelectedQuest();
		return true;
	}
	return false;
}

void UBeyondQuestLogWidget::GetPageKeyHints(TArray<FBeyondKeyHint>& OutHints) const
{
	const UBeyondQuestComponent* Quests = GetQuests();
	const bool bTracked = Quests && !GetSelectedQuest().IsNone() && Quests->GetTrackedQuest() == GetSelectedQuest();
	OutHints.Add({ TEXT("LMB"), LOCTEXT("Select", "Select"), EKeys::LeftMouseButton });
	OutHints.Add({ TEXT("T"), bTracked ? LOCTEXT("Untrack", "Stop Tracking") : LOCTEXT("Track", "Track"), EKeys::T });
	OutHints.Add({ TEXT("Q  E"), LOCTEXT("Filter", "Filter"), EKeys::E });
	OutHints.Add({ TEXT("C"), LOCTEXT("Overview", "Overview"), EKeys::C });
}

bool UBeyondQuestLogWidget::GetCounter(FBeyondMenuCounter& OutCounter) const
{
	const UBeyondQuestComponent* Quests = GetQuests();
	if (!Quests || !Quests->GetDatabase())
	{
		return false;
	}
	OutCounter.Label = LOCTEXT("StoryCounter", "Story");
	FBeyondMenuCounter::FValue& Value = OutCounter.Values.AddDefaulted_GetRef();
	Value.Glyph = EBeyondGlyph::Quests;
	Value.Tint = BeyondUI::Palette().GoldBright;
	Value.Text = FString::Printf(TEXT("%d / %d"), Quests->GetCompletedCount(EBeyondQuestKind::Main), Quests->GetQuestCount(EBeyondQuestKind::Main));
	return true;
}

//~ Painting

void UBeyondQuestLogWidget::PaintPage(BeyondUI::FPainter& Paint) const
{
	using namespace BeyondUI;
	using namespace BeyondQuestLogLocal;
	const TArray<FName> Listed = GetListedQuests();
	PaintList(Paint, Listed);

	Paint.Panel(DetailTopLeft, DetailSize, 20);
	const UBeyondQuestComponent* Quests = GetQuests();
	if (const FBeyondQuest* Quest = Quests ? Quests->FindQuest(GetSelectedQuest()) : nullptr)
	{
		PaintDetail(Paint, *Quest);
	}
	else
	{
		Paint.Glyph(EBeyondGlyph::Compass, DetailTopLeft + DetailSize * 0.5f - FVector2f(0.0f, 40.0f), 150.0f, WithAlpha(Palette().Gold, 0.2f), 22, 2.0f);
		Paint.Text(LOCTEXT("NoQuests", "The quest log fills as the story begins.").ToString(), Font(EBeyondFont::BodyItalic, 22.0f),
			DetailTopLeft + DetailSize * 0.5f + FVector2f(0.0f, 70.0f), Palette().TextDim, 0.5f, 22);
	}
}

void UBeyondQuestLogWidget::PaintList(const BeyondUI::FPainter& Paint, const TArray<FName>& Listed) const
{
	using namespace BeyondUI;
	using namespace BeyondQuestLogLocal;
	const FPalette& P = Palette();
	const UBeyondQuestComponent* Quests = GetQuests();
	const FName Selected = GetSelectedQuest();

	Paint.Panel(ListTopLeft, ListSize, 20);
	const FText Header = GetFilter() == 1 ? LOCTEXT("MainHeader", "Main Story") : GetFilter() == 2 ? LOCTEXT("SideHeader", "Side Quests") : LOCTEXT("AllHeader", "Journal");
	Paint.SectionHeader(Header.ToString(), ListTopLeft + FVector2f(24.0f, 18.0f), ListSize.X - 48.0f, 22, 18.0f);
	if (Listed.IsEmpty())
	{
		const FText Empty = GetFilter() == 2 ? LOCTEXT("NoSide", "No side quests yet. Villagers across the Dominion will have work for you.")
			: LOCTEXT("NoMain", "The story's chapters appear here.");
		Paint.Paragraph(Empty.ToString(), Font(EBeyondFont::BodyItalic, 19.0f), FVector2f(ListTopLeft.X + 28.0f, EntryTop + 8.0f), ListSize.X - 56.0f, 26.0f, P.TextDim, 22);
		return;
	}

	for (int32 Row = 0; Row < VisibleEntries && Listed.IsValidIndex(ScrollIndex + Row); ++Row)
	{
		const int32 Index = ScrollIndex + Row;
		const FBeyondQuest* Quest = Quests ? Quests->FindQuest(Listed[Index]) : nullptr;
		if (!Quest)
		{
			continue;
		}
		const EBeyondQuestState State = Quests->GetQuestState(Quest->QuestId);
		const bool bLocked = State == EBeyondQuestState::Locked;
		const FVector2f TopLeft(ListTopLeft.X + 18.0f, EntryTop + Row * (EntryHeight + EntryGap));
		const FVector2f Size(ListSize.X - 36.0f, EntryHeight);
		const bool bSelected = Quest->QuestId == Selected;
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

		// The state diamond: gold with a check once done, glowing blue while active, a padlock while sealed
		const FVector2f Mark = TopLeft + FVector2f(34.0f, Size.Y * 0.5f);
		if (State == EBeyondQuestState::Completed)
		{
			Paint.Diamond(Mark, 13.0f, 17.0f, WithAlpha(P.Gold, 0.9f), 24);
			Paint.Glyph(EBeyondGlyph::Check, Mark, 16.0f, P.Backdrop, 25, 2.2f);
		}
		else if (State == EBeyondQuestState::Active)
		{
			Paint.Glow(Mark, 26.0f, WithAlpha(P.BlueBright, 0.35f), 23);
			Paint.DiamondOutline(Mark, 12.0f, 16.0f, P.BlueBright, 1.8f, 24);
			Paint.Diamond(Mark, 5.0f, 7.0f, P.BlueBright, 25);
		}
		else
		{
			Paint.DiamondOutline(Mark, 12.0f, 16.0f, WithAlpha(P.Silver, 0.45f), 1.2f, 24);
			Paint.Glyph(EBeyondGlyph::Lock, Mark, 15.0f, WithAlpha(P.Silver, 0.6f), 25, 1.3f);
		}

		const FString Label = Quest->Location.IsEmpty() ? KindLabel(*Quest).ToString()
			: FString::Printf(TEXT("%s  ·  %s"), *KindLabel(*Quest).ToString(), *Quest->Location.ToString());
		Paint.Text(Label, Font(EBeyondFont::Body, 16.0f), TopLeft + FVector2f(64.0f, 10.0f), bLocked ? P.TextFaint : P.GoldBright, 0.0f, 24);
		Paint.TextFit(Quest->Title.ToString(), EBeyondFont::Heading, 20.0f, TopLeft + FVector2f(64.0f, 34.0f), Size.X - 110.0f,
			bLocked ? P.TextDim : bSelected ? FLinearColor::White : P.Text, 0.0f, 24);
		if (Quests->GetTrackedQuest() == Quest->QuestId)
		{
			Paint.Glyph(EBeyondGlyph::Compass, TopLeft + FVector2f(Size.X - 26.0f, Size.Y * 0.5f), 22.0f, P.GoldBright, 24, 1.6f);
		}
	}

	// More above / below
	if (ScrollIndex > 0)
	{
		Paint.Glyph(EBeyondGlyph::Up, FVector2f(ListTopLeft.X + ListSize.X * 0.5f, EntryTop - 14.0f), 16.0f, P.Gold, 24, 1.6f);
	}
	if (ScrollIndex + VisibleEntries < Listed.Num())
	{
		Paint.Text(FString::Printf(TEXT("+%d more"), Listed.Num() - ScrollIndex - VisibleEntries), Font(EBeyondFont::BodyItalic, 16.0f),
			FVector2f(ListTopLeft.X + ListSize.X * 0.5f, ListTopLeft.Y + ListSize.Y - 30.0f), P.TextDim, 0.5f, 24);
	}
}

void UBeyondQuestLogWidget::PaintDetail(const BeyondUI::FPainter& Paint, const FBeyondQuest& Quest) const
{
	using namespace BeyondUI;
	using namespace BeyondQuestLogLocal;
	const FPalette& P = Palette();
	const UBeyondQuestComponent* Quests = GetQuests();
	const EBeyondQuestState State = Quests->GetQuestState(Quest.QuestId);
	const bool bLocked = State == EBeyondQuestState::Locked;

	//~ The banner: the quest's picture with its chapter, title and place over the bottom
	const FVector2f BannerTopLeft = DetailTopLeft + FVector2f(28.0f, 28.0f);
	const FVector2f BannerSize(DetailSize.X - 56.0f, 360.0f);
	if (UTexture2D* Thumbnail = LoadTexture(Quest.Thumbnail))
	{
		Paint.ImageCover(Thumbnail, BannerTopLeft, BannerSize, bLocked ? FLinearColor(0.35f, 0.38f, 0.48f) : FLinearColor::White, 21);
	}
	else
	{
		Paint.GradientV(BannerTopLeft, BannerSize, FromHex(0x1C2C50), FromHex(0x070B16), 21);
		Paint.Glyph(EBeyondGlyph::Compass, BannerTopLeft + BannerSize * 0.5f, 160.0f, WithAlpha(P.Gold, 0.25f), 22, 2.0f);
	}
	Paint.GradientV(BannerTopLeft + FVector2f(0.0f, BannerSize.Y * 0.4f), FVector2f(BannerSize.X, BannerSize.Y * 0.6f), WithAlpha(P.Backdrop, 0.0f),
		WithAlpha(P.Backdrop, 0.92f), 23);
	Paint.RectOutline(BannerTopLeft, BannerSize, WithAlpha(P.Gold, 0.5f), 1.0f, 24);
	Paint.Text(KindLabel(Quest).ToString(), Font(EBeyondFont::Heading, 18.0f), BannerTopLeft + FVector2f(30.0f, BannerSize.Y - 112.0f), P.GoldBright, 0.0f, 25);
	Paint.TextFit(Quest.Title.ToString(), EBeyondFont::Display, 40.0f,
		BannerTopLeft + FVector2f(28.0f, BannerSize.Y - 86.0f), BannerSize.X - 260.0f, P.Text, 0.0f, 25);
	if (!Quest.Location.IsEmpty())
	{
		Paint.Text(Quest.Location.ToString(), Font(EBeyondFont::BodyItalic, 19.0f), BannerTopLeft + FVector2f(30.0f, BannerSize.Y - 34.0f), P.TextDim, 0.0f, 25);
	}

	// State pill, top right of the banner
	const bool bTracked = Quests->GetTrackedQuest() == Quest.QuestId;
	const FText StateText = State == EBeyondQuestState::Completed ? LOCTEXT("Completed", "Completed") : bLocked ? LOCTEXT("Sealed", "Sealed")
		: bTracked ? LOCTEXT("Tracked", "Tracked") : LOCTEXT("Active", "Active");
	const FLinearColor StateColor = State == EBeyondQuestState::Completed ? P.Gold : bLocked ? P.Silver : P.BlueBright;
	const FSlateFontInfo PillFont = Font(EBeyondFont::Heading, 16.0f);
	const FVector2f PillSize(MeasureText(StateText.ToString(), PillFont).X + 36.0f, 34.0f);
	const FVector2f PillTopLeft(BannerTopLeft.X + BannerSize.X - PillSize.X - 18.0f, BannerTopLeft.Y + 18.0f);
	Paint.Box(PillTopLeft, PillSize, WithAlpha(P.Backdrop, 0.85f), 25);
	Paint.RectOutline(PillTopLeft, PillSize, WithAlpha(StateColor, 0.9f), 1.2f, 26);
	Paint.Text(StateText.ToString(), PillFont, PillTopLeft + FVector2f(PillSize.X * 0.5f, 6.0f), StateColor, 0.5f, 26);

	//~ Summary
	const float Left = BannerTopLeft.X + 4.0f;
	const float Width = BannerSize.X - 8.0f;
	float Y = BannerTopLeft.Y + BannerSize.Y + 22.0f;
	Y += Paint.Paragraph(Quest.Summary.ToString(), Font(EBeyondFont::Body, 21.0f), FVector2f(Left, Y), Width, 28.0f, bLocked ? P.TextDim : P.Text, 22, 3) + 14.0f;

	//~ Objectives (sealed: what opens it)
	Y = Paint.SectionHeader((bLocked ? LOCTEXT("ToUnseal", "To Begin") : LOCTEXT("Objectives", "Objectives")).ToString(), FVector2f(BannerTopLeft.X, Y), BannerSize.X, 22, 18.0f);
	auto Line = [&Paint, &P, Left, Width](float& LineY, const FString& Text, bool bDone, const FString& Count)
	{
		const FVector2f Mark(Left + 12.0f, LineY + 13.0f);
		if (bDone)
		{
			Paint.Diamond(Mark, 8.0f, 9.0f, WithAlpha(P.Gold, 0.9f), 23);
			Paint.Glyph(EBeyondGlyph::Check, Mark, 11.0f, P.Backdrop, 24, 2.0f);
		}
		else
		{
			Paint.DiamondOutline(Mark, 7.0f, 8.0f, WithAlpha(P.Silver, 0.8f), 1.2f, 23);
		}
		Paint.TextFit(Text, EBeyondFont::Body, 19.0f, FVector2f(Left + 34.0f, LineY), Width - 140.0f, bDone ? P.TextDim : P.Text, 0.0f, 23);
		if (!Count.IsEmpty())
		{
			Paint.Text(Count, Font(EBeyondFont::Number, 18.0f), FVector2f(Left + Width, LineY), bDone ? P.TextDim : P.GoldBright, 1.0f, 23);
		}
		LineY += 30.0f;
	};
	if (bLocked)
	{
		for (const FName& Required : Quest.Requires)
		{
			const FBeyondQuest* Before = Quests->FindQuest(Required);
			Line(Y, FText::Format(LOCTEXT("FinishFirst", "Finish {0}"), Before ? Before->Title : FText::FromName(Required)).ToString(),
				Quests->GetQuestState(Required) == EBeyondQuestState::Completed, FString());
		}
		if (Quest.RequiredLevel > 1)
		{
			Line(Y, FText::Format(LOCTEXT("ReachLevelFirst", "Reach level {0}"), FText::AsNumber(Quest.RequiredLevel)).ToString(), false, FString());
		}
		if (!Quest.RequiredFlag.IsNone())
		{
			Line(Y, LOCTEXT("StoryFirst", "Continue the story").ToString(), false, FString());
		}
	}
	else
	{
		for (int32 Index = 0; Index < Quest.Objectives.Num(); ++Index)
		{
			const FBeyondQuestObjective& Objective = Quest.Objectives[Index];
			const FString Count = Objective.Count > 1 ? FString::Printf(TEXT("%d / %d"), Quests->GetObjectiveProgress(Quest.QuestId, Index), Objective.Count) : FString();
			Line(Y, Objective.Text.IsEmpty() ? Objective.Target.ToString() : Objective.Text.ToString(), Quests->IsObjectiveDone(Quest.QuestId, Index), Count);
		}
	}

	//~ Rewards
	const FBeyondQuestReward& Reward = Quest.Reward;
	Y += 8.0f;
	Y = Paint.SectionHeader(LOCTEXT("Rewards", "Rewards").ToString(), FVector2f(BannerTopLeft.X, Y), BannerSize.X, 22, 18.0f);
	float X = Left;
	UBeyondUISubsystem* UI = UBeyondUISubsystem::Get();
	const UBeyondUIArtSet* Art = UI ? UI->GetArtSet() : nullptr;
	auto Chip = [&](UTexture2D* Icon, EBeyondGlyph Glyph, const FLinearColor& Tint, const FString& Text)
	{
		const FSlateFontInfo ChipFont = Font(EBeyondFont::Body, 18.0f);
		const float ChipWidth = 52.0f + MeasureText(Text, ChipFont).X;
		if (X + ChipWidth > Left + Width)
		{
			return;
		}
		Paint.GradientV(FVector2f(X, Y), FVector2f(ChipWidth, 40.0f), FromHex(0x161E33, 0.92f), FromHex(0x0A0F1C, 0.92f), 22);
		Paint.RectOutline(FVector2f(X, Y), FVector2f(ChipWidth, 40.0f), WithAlpha(P.Gold, 0.4f), 1.0f, 23);
		if (Icon)
		{
			Paint.ImageCentred(Icon, FVector2f(X + 22.0f, Y + 20.0f), FVector2f(28.0f, 28.0f), FLinearColor::White, 24);
		}
		else
		{
			Paint.Glyph(Glyph, FVector2f(X + 22.0f, Y + 20.0f), 20.0f, Tint, 24, 1.5f);
		}
		Paint.Text(Text, ChipFont, FVector2f(X + 42.0f, Y + 8.0f), P.Text, 0.0f, 24);
		X += ChipWidth + 10.0f;
	};
	if (Reward.Experience > 0.0f)
	{
		Chip(nullptr, EBeyondGlyph::Star, P.GoldBright, FString::Printf(TEXT("%s EXP"), *FormatNumber(FMath::RoundToInt(Reward.Experience))));
	}
	for (const EBeyondCurrency Currency : FBeyondCurrencyBundle::All)
	{
		if (const int32 Amount = Reward.Currencies.Get(Currency); Amount > 0)
		{
			const TSoftObjectPtr<UTexture2D>* Icon = Art ? Art->CurrencyIcons.Find(Currency) : nullptr;
			Chip(Icon ? LoadTexture(*Icon) : nullptr, CurrencyGlyph(Currency), CurrencyColor(Currency), FString::Printf(TEXT("%s %s"), *FormatNumber(Amount),
				*UBeyondItemLibrary::GetCurrencyName(Currency).ToString()));
		}
	}
	if (Reward.BondExperience > 0.0f)
	{
		Chip(Art ? LoadTexture(Art->BondPointIcon) : nullptr, EBeyondGlyph::Bond, P.Violet, FString::Printf(TEXT("%d Bond EXP"), FMath::RoundToInt(Reward.BondExperience)));
	}
	for (const TSoftObjectPtr<UBeyondItemDefinition>& Entry : Reward.Items)
	{
		if (const UBeyondItemDefinition* Definition = Entry.LoadSynchronous())
		{
			Chip(Definition->Icon, SlotGlyph(Definition->Slot), TierColor(Reward.ItemTier), Definition->DisplayName.ToString());
		}
	}
	if (X == Left)
	{
		Paint.Text(LOCTEXT("NoReward", "The story itself").ToString(), Font(EBeyondFont::BodyItalic, 18.0f), FVector2f(Left, Y + 8.0f), P.TextDim, 0.0f, 22);
	}

	// Tracking, bottom right
	if (State == EBeyondQuestState::Active)
	{
		const FString TrackLabel = (bTracked ? LOCTEXT("StopTracking", "Stop Tracking") : LOCTEXT("TrackButton", "Track")).ToString();
		const float ButtonWidth = 64.0f + MeasureText(TrackLabel, Font(EBeyondFont::Body, 17.0f)).X;
		const FVector2f ButtonTopLeft(DetailTopLeft.X + DetailSize.X - ButtonWidth - 30.0f, DetailTopLeft.Y + DetailSize.Y - 64.0f);
		Paint.GradientV(ButtonTopLeft, FVector2f(ButtonWidth, 40.0f), FromHex(0x1B2A4D, 0.95f), FromHex(0x0A0F1C, 0.95f), 22);
		Paint.RectOutline(ButtonTopLeft, FVector2f(ButtonWidth, 40.0f), WithAlpha(P.BlueBright, 0.7f), 1.0f, 23);
		Paint.KeyHint(TEXT("T"), TrackLabel, ButtonTopLeft + FVector2f(10.0f, 6.0f), 24);
	}
}

#undef LOCTEXT_NAMESPACE
