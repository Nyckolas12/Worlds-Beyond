// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondPauseMenuWidget.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Player/BeyondPlayerController.h"

#define LOCTEXT_NAMESPACE "BeyondPauseMenu"

namespace BeyondPauseLocal
{
	const FVector2f PanelTopLeft(640.0f, 186.0f);
	const FVector2f PanelSize(640.0f, 720.0f);
	constexpr float EntryTop = 342.0f;
	constexpr float EntryHeight = 64.0f;
	constexpr float EntryGap = 10.0f;
	constexpr float ConfirmWindow = 3.0f;

	const EBeyondPauseEntry Entries[] = { EBeyondPauseEntry::Resume, EBeyondPauseEntry::Character, EBeyondPauseEntry::Quests, EBeyondPauseEntry::WorldMap,
		EBeyondPauseEntry::Settings, EBeyondPauseEntry::ReturnToTitle, EBeyondPauseEntry::Quit };

	FText GetEntryName(EBeyondPauseEntry Entry)
	{
		switch (Entry)
		{
		case EBeyondPauseEntry::Resume: return LOCTEXT("Resume", "Resume");
		case EBeyondPauseEntry::Character: return LOCTEXT("Character", "Character");
		case EBeyondPauseEntry::Quests: return LOCTEXT("Quests", "Quests");
		case EBeyondPauseEntry::WorldMap: return LOCTEXT("WorldMap", "World Map");
		case EBeyondPauseEntry::Settings: return LOCTEXT("Settings", "Settings");
		case EBeyondPauseEntry::ReturnToTitle: return LOCTEXT("ReturnToTitle", "Return to Title");
		default: return LOCTEXT("Quit", "Quit Game");
		}
	}

	EBeyondGlyph GetEntryGlyph(EBeyondPauseEntry Entry)
	{
		switch (Entry)
		{
		case EBeyondPauseEntry::Resume: return EBeyondGlyph::Up;
		case EBeyondPauseEntry::Character: return EBeyondGlyph::Overview;
		case EBeyondPauseEntry::Quests: return EBeyondGlyph::Quests;
		case EBeyondPauseEntry::WorldMap: return EBeyondGlyph::Map;
		case EBeyondPauseEntry::Settings: return EBeyondGlyph::Compass;
		case EBeyondPauseEntry::ReturnToTitle: return EBeyondGlyph::Star;
		default: return EBeyondGlyph::Cross;
		}
	}
}

UBeyondPauseMenuWidget::UBeyondPauseMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TitleLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/Blueprints/Widgets/MainMenu/Lvl_MainMenu.Lvl_MainMenu")));
}

FText UBeyondPauseMenuWidget::GetTitle() const
{
	return LOCTEXT("Title", "Paused");
}

int32 UBeyondPauseMenuWidget::GetEntryCount() const
{
	return UE_ARRAY_COUNT(BeyondPauseLocal::Entries);
}

EBeyondPauseEntry UBeyondPauseMenuWidget::GetSelectedEntry() const
{
	return BeyondPauseLocal::Entries[FMath::Clamp(Selected, 0, GetEntryCount() - 1)];
}

void UBeyondPauseMenuWidget::ActivateEntry(EBeyondPauseEntry Entry)
{
	ABeyondPlayerController* PC = GetBeyondPlayerController();
	const bool bConfirmed = ArmedEntry == Entry && ArmedAge >= 0.0f && ArmedAge < BeyondPauseLocal::ConfirmWindow;
	switch (Entry)
	{
	case EBeyondPauseEntry::Resume:
		CloseMenu();
		break;
	case EBeyondPauseEntry::Character:
		if (PC)
		{
			PC->OpenMenuPage(EBeyondMenuPage::Overview);
		}
		break;
	case EBeyondPauseEntry::Quests:
		if (PC)
		{
			PC->OpenMenuPage(EBeyondMenuPage::Quests);
		}
		break;
	case EBeyondPauseEntry::WorldMap:
		if (PC)
		{
			PC->OpenMenuPage(EBeyondMenuPage::WorldMap);
		}
		break;
	case EBeyondPauseEntry::Settings:
		ShowMessage(LOCTEXT("SettingsSoon", "Settings come in a later update"), true);
		PlayDeniedSound();
		break;
	case EBeyondPauseEntry::ReturnToTitle:
	case EBeyondPauseEntry::Quit:
		if (!bConfirmed)
		{
			ArmedEntry = Entry;
			ArmedAge = 0.0f;
			ShowMessage(Entry == EBeyondPauseEntry::Quit ? LOCTEXT("ConfirmQuit", "Press again to quit (progress is saved as you play)")
				: LOCTEXT("ConfirmTitle", "Press again to return to the title screen"), true);
			break;
		}
		ArmedAge = -1.0f;
		CloseMenu();
		if (Entry == EBeyondPauseEntry::Quit)
		{
			UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
		}
		else if (!TitleLevel.IsNull())
		{
			UGameplayStatics::OpenLevelBySoftObjectPtr(this, TitleLevel);
		}
		break;
	}
}

void UBeyondPauseMenuWidget::AdvancePage(float DeltaSeconds)
{
	if (ArmedAge >= 0.0f)
	{
		ArmedAge += DeltaSeconds;
		if (ArmedAge > BeyondPauseLocal::ConfirmWindow)
		{
			ArmedAge = -1.0f;
		}
	}
}

int32 UBeyondPauseMenuWidget::HitTestEntry(const FVector2f& Design) const
{
	using namespace BeyondPauseLocal;
	if (Design.X < PanelTopLeft.X + 60.0f || Design.X > PanelTopLeft.X + PanelSize.X - 60.0f)
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < GetEntryCount(); ++Index)
	{
		const float Top = EntryTop + Index * (EntryHeight + EntryGap);
		if (Design.Y >= Top && Design.Y <= Top + EntryHeight)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UBeyondPauseMenuWidget::HandlePageHover(const FVector2f& Design)
{
	const int32 Entry = HitTestEntry(Design);
	if (Entry != INDEX_NONE)
	{
		Selected = Entry;
	}
}

bool UBeyondPauseMenuWidget::HandlePageClick(const FVector2f& Design, const FKey& Button)
{
	const int32 Entry = HitTestEntry(Design);
	if (Button == EKeys::LeftMouseButton && Entry != INDEX_NONE)
	{
		Selected = Entry;
		PlaySelectSound();
		ActivateEntry(GetSelectedEntry());
		return true;
	}
	return false;
}

bool UBeyondPauseMenuWidget::HandlePageKey(const FKey& Key)
{
	if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up)
	{
		Selected = (Selected + GetEntryCount() - 1) % GetEntryCount();
		return true;
	}
	if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down)
	{
		Selected = (Selected + 1) % GetEntryCount();
		return true;
	}
	if (Key == EKeys::F || Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		PlaySelectSound();
		ActivateEntry(GetSelectedEntry());
		return true;
	}
	return false;
}

void UBeyondPauseMenuWidget::GetPageKeyHints(TArray<FBeyondKeyHint>& OutHints) const
{
	OutHints.Add({ TEXT("W  S"), LOCTEXT("Choose", "Choose"), EKeys::Down });
	OutHints.Add({ TEXT("F"), LOCTEXT("Confirm", "Confirm"), EKeys::F });
	OutHints.Add({ TEXT("C"), LOCTEXT("CharacterHint", "Character"), EKeys::C });
	OutHints.Add({ TEXT("M"), LOCTEXT("MapHint", "World Map"), EKeys::M });
}

void UBeyondPauseMenuWidget::PaintPage(BeyondUI::FPainter& Paint) const
{
	using namespace BeyondUI;
	using namespace BeyondPauseLocal;
	const FPalette& P = Palette();
	Paint.Panel(PanelTopLeft, PanelSize, 20);
	const FVector2f Crest(PanelTopLeft.X + PanelSize.X * 0.5f, PanelTopLeft.Y + 50.0f);
	Paint.Ring(Crest, 30.0f, WithAlpha(P.Gold, 0.6f), 1.5f, 22);
	Paint.Sparkle(Crest, 34.0f, P.GoldBright, 22);
	Paint.Text(LOCTEXT("PanelTitle", "Worlds Beyond").ToString(), Font(EBeyondFont::Display, 30.0f), FVector2f(Crest.X, PanelTopLeft.Y + 88.0f), P.GoldBright, 0.5f, 22);
	Paint.Separator(FVector2f(PanelTopLeft.X + 60.0f, EntryTop - 18.0f), PanelSize.X - 120.0f, 22, 0.8f);

	for (int32 Index = 0; Index < GetEntryCount(); ++Index)
	{
		const EBeyondPauseEntry Entry = Entries[Index];
		const FVector2f TopLeft(PanelTopLeft.X + 70.0f, EntryTop + Index * (EntryHeight + EntryGap));
		const FVector2f Size(PanelSize.X - 140.0f, EntryHeight);
		const bool bSelected = Index == Selected;
		const bool bArmed = ArmedEntry == Entry && ArmedAge >= 0.0f;
		const bool bDisabled = Entry == EBeyondPauseEntry::Settings;
		if (bSelected)
		{
			Paint.Parallelogram(TopLeft, Size, 18.0f, WithAlpha(bArmed ? P.Minus : P.Blue, 0.75f), WithAlpha(P.BlueDeep, 0.8f), 22);
			Paint.ParallelogramOutline(TopLeft, Size, 18.0f, WithAlpha(P.BlueBright, 0.9f), 1.4f, 23);
			Paint.Diamond(FVector2f(TopLeft.X - 22.0f, TopLeft.Y + Size.Y * 0.5f), 7.0f, 9.0f, P.GoldBright, 24);
		}
		else
		{
			Paint.Parallelogram(TopLeft, Size, 18.0f, FromHex(0x141C30, 0.85f), FromHex(0x0A0F1C, 0.85f), 22);
			Paint.ParallelogramOutline(TopLeft, Size, 18.0f, WithAlpha(P.Silver, 0.35f), 1.0f, 23);
		}
		const FLinearColor LabelColor = bDisabled ? P.TextFaint : bSelected ? FLinearColor::White : P.Text;
		Paint.Glyph(GetEntryGlyph(Entry), TopLeft + FVector2f(44.0f, Size.Y * 0.5f), 26.0f, LabelColor, 24, 1.6f);
		Paint.Text(GetEntryName(Entry).ToString(), Font(EBeyondFont::Heading, 22.0f), TopLeft + FVector2f(82.0f, 16.0f), LabelColor, 0.0f, 24);
	}
}

#undef LOCTEXT_NAMESPACE
