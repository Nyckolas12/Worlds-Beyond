// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondMenuWidget.h"
#include "Characters/BeyondCharacterBase.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Sound/SoundBase.h"
#include "UI/BeyondCharacterProfile.h"
#include "UI/BeyondUIArt.h"
#include "UI/BeyondUIStyleSettings.h"

#define LOCTEXT_NAMESPACE "BeyondMenu"

namespace BeyondMenuLocal
{
	constexpr float FadeInSeconds = 0.18f;
	constexpr float MessageSeconds = 3.0f;

	FText GetPageName(EBeyondMenuPage Page)
	{
		switch (Page)
		{
		case EBeyondMenuPage::SkillTree: return LOCTEXT("SkillTreePage", "Skill Tree");
		case EBeyondMenuPage::Armor: return LOCTEXT("ArmorPage", "Armor");
		case EBeyondMenuPage::Overview: return LOCTEXT("OverviewPage", "Overview");
		case EBeyondMenuPage::Quests: return LOCTEXT("QuestsPage", "Quests");
		case EBeyondMenuPage::WorldMap: return LOCTEXT("MapPage", "World Map");
		default: return LOCTEXT("PausePage", "Paused");
		}
	}

	EBeyondGlyph GetPageGlyph(EBeyondMenuPage Page)
	{
		switch (Page)
		{
		case EBeyondMenuPage::SkillTree: return EBeyondGlyph::SkillTree;
		case EBeyondMenuPage::Armor: return EBeyondGlyph::Armor;
		case EBeyondMenuPage::Overview: return EBeyondGlyph::Overview;
		case EBeyondMenuPage::Quests: return EBeyondGlyph::Quests;
		default: return EBeyondGlyph::Map;
		}
	}

	FKey GetPageKey(EBeyondMenuPage Page)
	{
		switch (Page)
		{
		case EBeyondMenuPage::SkillTree: return EKeys::K;
		case EBeyondMenuPage::Armor: return EKeys::I;
		case EBeyondMenuPage::Overview: return EKeys::C;
		case EBeyondMenuPage::Quests: return EKeys::J;
		case EBeyondMenuPage::WorldMap: return EKeys::M;
		default: return EKeys::P;
		}
	}
}

UBeyondMenuWidget::UBeyondMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

const TArray<EBeyondMenuPage>& UBeyondMenuWidget::GetSidebarPages()
{
	static const TArray<EBeyondMenuPage> Pages = { EBeyondMenuPage::SkillTree, EBeyondMenuPage::Armor, EBeyondMenuPage::Overview, EBeyondMenuPage::Quests };
	return Pages;
}

void UBeyondMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
	BeyondUI::Preload();
	RebuildHeaderTabs();
}

void UBeyondMenuWidget::NotifyOpened()
{
	RebuildHeaderTabs();
	OpenAge = 0.0f;
}

void UBeyondMenuWidget::RebuildHeaderTabs()
{
	HeaderTabs.Reset();
	BuildHeaderTabs(HeaderTabs);
}

void UBeyondMenuWidget::BuildHeaderTabs(TArray<FBeyondMenuTab>& OutTabs) const
{
	const ABeyondPlayerController* PC = GetBeyondPlayerController();
	for (ABeyondCharacterBase* Member : GetPartyInDuoOrder())
	{
		FBeyondMenuTab& Tab = OutTabs.AddDefaulted_GetRef();
		Tab.Title = Member->GetCharacterDisplayName();
		Tab.Character = Member;
		if (Member->Profile)
		{
			Tab.Accent = Member->Profile->AccentColor;
		}
	}
	FBeyondMenuTab& Duo = OutTabs.AddDefaulted_GetRef();
	Duo.Title = LOCTEXT("DuoTab", "Duo");
	Duo.bDuo = true;
	Duo.Accent = PC && PC->DuoProfile ? PC->DuoProfile->AccentColor : BeyondUI::Palette().Violet;
}

int32 UBeyondMenuWidget::GetActiveHeaderTab() const
{
	const int32 Tab = GetPageTab();
	return HeaderTabs.IsValidIndex(Tab) ? Tab : 0;
}

FText UBeyondMenuWidget::GetHeaderTabTitle(int32 Index) const
{
	return HeaderTabs.IsValidIndex(Index) ? HeaderTabs[Index].Title : FText::GetEmpty();
}

void UBeyondMenuWidget::SelectHeaderTab(int32 Index)
{
	if (HeaderTabs.IsEmpty())
	{
		RebuildHeaderTabs();
	}
	if (HeaderTabs.IsValidIndex(Index) && Index != GetActiveHeaderTab())
	{
		OnHeaderTabSelected(Index);
		PlaySelectSound();
	}
}

int32 UBeyondMenuWidget::FindHeaderTabFor(const AActor* Character) const
{
	for (int32 Index = 0; Index < HeaderTabs.Num(); ++Index)
	{
		if (Character ? HeaderTabs[Index].Character.Get() == Character : HeaderTabs[Index].bDuo)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UBeyondMenuWidget::GetNavigationTab() const
{
	if (UsesCharacterTabs())
	{
		return GetActiveHeaderTab();
	}
	const ABeyondPlayerController* PC = GetBeyondPlayerController();
	return PC ? PC->GetLastMenuCharacterTab() : INDEX_NONE;
}

void UBeyondMenuWidget::CycleHeaderTab(int32 Direction)
{
	if (HeaderTabs.Num() > 1)
	{
		SelectHeaderTab((GetActiveHeaderTab() + Direction + HeaderTabs.Num()) % HeaderTabs.Num());
	}
}

ABeyondPlayerController* UBeyondMenuWidget::GetBeyondPlayerController() const
{
	return Cast<ABeyondPlayerController>(GetOwningPlayer());
}

TArray<ABeyondCharacterBase*> UBeyondMenuWidget::GetPartyInDuoOrder() const
{
	TArray<ABeyondCharacterBase*> Members;
	const ABeyondPlayerController* PC = GetBeyondPlayerController();
	if (PC && PC->PartyComponent)
	{
		Members = PC->PartyComponent->GetMembers();
		Members.RemoveAll([](const ABeyondCharacterBase* Member) { return Member == nullptr; });
		Members.StableSort([](const ABeyondCharacterBase& A, const ABeyondCharacterBase& B)
		{
			auto Order = [](EBeyondDuoRole Role) { return Role == EBeyondDuoRole::Conduit ? 0 : Role == EBeyondDuoRole::Striker ? 1 : 2; };
			return Order(A.DuoRole) < Order(B.DuoRole);
		});
	}
	return Members;
}

ABeyondCharacterBase* UBeyondMenuWidget::GetHeaderCharacter(int32 Index) const
{
	return HeaderTabs.IsValidIndex(Index) ? HeaderTabs[Index].Character.Get() : nullptr;
}

bool UBeyondMenuWidget::IsDuoHeaderTab(int32 Index) const
{
	return HeaderTabs.IsValidIndex(Index) && HeaderTabs[Index].bDuo;
}

const UBeyondCharacterProfile* UBeyondMenuWidget::GetProfileFor(int32 HeaderTab) const
{
	if (IsDuoHeaderTab(HeaderTab))
	{
		const ABeyondPlayerController* PC = GetBeyondPlayerController();
		return PC ? PC->DuoProfile.Get() : nullptr;
	}
	const ABeyondCharacterBase* Character = GetHeaderCharacter(HeaderTab);
	return Character ? Character->Profile.Get() : nullptr;
}

FLinearColor UBeyondMenuWidget::GetAccent() const
{
	const int32 Tab = GetActiveHeaderTab();
	return HeaderTabs.IsValidIndex(Tab) ? HeaderTabs[Tab].Accent : BeyondUI::Palette().Blue;
}

FText UBeyondMenuWidget::GetTitle() const
{
	return LOCTEXT("CharacterTitle", "Character");
}

void UBeyondMenuWidget::ShowMessage(const FText& Text, bool bWarning)
{
	Message = Text;
	bMessageWarning = bWarning;
	MessageAge = 0.0f;
}

void UBeyondMenuWidget::PlayUISound(const TSoftObjectPtr<USoundBase>& Sound, float Volume) const
{
	if (USoundBase* Loaded = Sound.IsNull() ? nullptr : Sound.LoadSynchronous())
	{
		// UI sounds keep playing while the game is paused
		UGameplayStatics::PlaySound2D(this, Loaded, Volume, 1.0f, 0.0f, nullptr, nullptr, true);
	}
}

void UBeyondMenuWidget::PlaySelectSound() const
{
	PlayUISound(UBeyondUIStyleSettings::Get()->SelectSound, 0.5f);
}

void UBeyondMenuWidget::PlayDeniedSound() const
{
	PlayUISound(DeniedSound, 0.7f);
}

UTexture2D* UBeyondMenuWidget::LoadTexture(const TSoftObjectPtr<UTexture2D>& Texture) const
{
	UBeyondUISubsystem* UI = UBeyondUISubsystem::Get();
	return UI ? UI->LoadTexture(Texture) : nullptr;
}

bool UBeyondMenuWidget::IsCursorIn(const FVector2f& TopLeft, const FVector2f& Size) const
{
	return CursorDesign.X >= TopLeft.X && CursorDesign.Y >= TopLeft.Y && CursorDesign.X <= TopLeft.X + Size.X && CursorDesign.Y <= TopLeft.Y + Size.Y;
}

void UBeyondMenuWidget::CloseMenu()
{
	if (ABeyondPlayerController* PC = GetBeyondPlayerController())
	{
		PC->CloseMenuPage(GetPage());
	}
	else
	{
		RemoveFromParent();
	}
}

//~ Timers

void UBeyondMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	LastLocalSize = FVector2f(MyGeometry.GetLocalSize());
	AdvanceAnimation(InDeltaTime);
}

void UBeyondMenuWidget::AdvanceAnimation(float DeltaSeconds)
{
	AnimTime += DeltaSeconds;
	OpenAge += DeltaSeconds;
	if (MessageAge >= 0.0f)
	{
		MessageAge += DeltaSeconds;
	}
	AdvancePage(DeltaSeconds);
}

//~ Layout and hit tests

FVector2f UBeyondMenuWidget::GetHeaderTabTopLeft(int32 Index) const
{
	using namespace BeyondMenuLayout;
	const int32 Count = FMath::Max(HeaderTabs.Num(), 1);
	const float Total = Count * HeaderTabWidth + (Count - 1) * HeaderTabGap;
	return FVector2f(960.0f - Total * 0.5f + Index * (HeaderTabWidth + HeaderTabGap), HeaderTabTop);
}

int32 UBeyondMenuWidget::HitTestHeaderTab(const FVector2f& Design) const
{
	if (!ShowsHeaderTabs())
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < HeaderTabs.Num(); ++Index)
	{
		const FVector2f TopLeft = GetHeaderTabTopLeft(Index);
		if (Design.X >= TopLeft.X && Design.X <= TopLeft.X + BeyondMenuLayout::HeaderTabWidth && Design.Y >= TopLeft.Y
			&& Design.Y <= TopLeft.Y + BeyondMenuLayout::HeaderTabHeight)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UBeyondMenuWidget::HitTestSidebar(const FVector2f& Design) const
{
	using namespace BeyondMenuLayout;
	if (!ShowsSidebar())
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < GetSidebarPages().Num(); ++Index)
	{
		const float Top = SidebarTop + Index * (SidebarEntryHeight + SidebarGap);
		if (Design.X >= SidebarLeft && Design.X <= SidebarLeft + SidebarWidth && Design.Y >= Top && Design.Y <= Top + SidebarEntryHeight)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FVector2f UBeyondMenuWidget::ToDesign(const FGeometry& Geometry, const FVector2D& ScreenPosition)
{
	LastLocalSize = FVector2f(Geometry.GetLocalSize());
	return GetDesignSpace().ToDesign(FVector2f(Geometry.AbsoluteToLocal(ScreenPosition)));
}

TArray<FBeyondKeyHint> UBeyondMenuWidget::GetKeyHints() const
{
	TArray<FBeyondKeyHint> Hints;
	GetPageKeyHints(Hints);
	Hints.Add({ TEXT("Esc"), LOCTEXT("Close", "Close"), EKeys::Escape });
	return Hints;
}

//~ Input

FReply UBeyondMenuWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	CursorDesign = ToDesign(InGeometry, InMouseEvent.GetScreenSpacePosition());
	const int32 NewTab = HitTestHeaderTab(CursorDesign);
	const int32 NewSidebar = HitTestSidebar(CursorDesign);
	if ((NewTab != INDEX_NONE && NewTab != HoveredHeaderTab) || (NewSidebar != INDEX_NONE && NewSidebar != HoveredSidebar))
	{
		PlayUISound(UBeyondUIStyleSettings::Get()->HoverSound, 0.25f);
	}
	HoveredHeaderTab = NewTab;
	HoveredSidebar = NewSidebar;
	HoveredKeyHint = INDEX_NONE;
	for (int32 Index = 0; Index < KeyHintBoxes.Num(); ++Index)
	{
		if (KeyHintBoxes[Index].Key.IsInside(CursorDesign))
		{
			HoveredKeyHint = Index;
		}
	}
	HandlePageHover(CursorDesign);
	return FReply::Handled();
}

FReply UBeyondMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	CursorDesign = ToDesign(InGeometry, InMouseEvent.GetScreenSpacePosition());
	const FKey Button = InMouseEvent.GetEffectingButton();
	if (Button == EKeys::LeftMouseButton)
	{
		if (const int32 Tab = HitTestHeaderTab(CursorDesign); Tab != INDEX_NONE)
		{
			SelectHeaderTab(Tab);
			return FReply::Handled();
		}
		if (const int32 Entry = HitTestSidebar(CursorDesign); Entry != INDEX_NONE)
		{
			const EBeyondMenuPage Page = GetSidebarPages()[Entry];
			if (Page != GetPage())
			{
				if (ABeyondPlayerController* PC = GetBeyondPlayerController())
				{
					PC->OpenMenuPage(Page, GetNavigationTab());
				}
			}
			return FReply::Handled();
		}
		for (const TPair<FBox2f, FKey>& Box : KeyHintBoxes)
		{
			if (Box.Key.IsInside(CursorDesign))
			{
				PressKey(Box.Value);
				return FReply::Handled();
			}
		}
	}
	HandlePageClick(CursorDesign, Button);
	return FReply::Handled();
}

FReply UBeyondMenuWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	CursorDesign = ToDesign(InGeometry, InMouseEvent.GetScreenSpacePosition());
	HandlePageMouseUp(CursorDesign, InMouseEvent.GetEffectingButton());
	return FReply::Handled();
}

FReply UBeyondMenuWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	CursorDesign = ToDesign(InGeometry, InMouseEvent.GetScreenSpacePosition());
	HandlePageWheel(CursorDesign, InMouseEvent.GetWheelDelta());
	return FReply::Handled();
}

FReply UBeyondMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.IsRepeat())
	{
		return FReply::Handled();
	}
	return PressKey(InKeyEvent.GetKey()) ? FReply::Handled() : Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UBeyondMenuWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	return HandlePageKeyUp(InKeyEvent.GetKey()) ? FReply::Handled() : Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

bool UBeyondMenuWidget::PressKey(const FKey& Key)
{
	if (HandleFrameKey(Key))
	{
		return true;
	}
	if (HandlePageKey(Key))
	{
		return true;
	}
	// Tabs last, so a page can use the arrows for itself
	if (Key == EKeys::Q || Key == EKeys::Left || Key == EKeys::Gamepad_LeftShoulder)
	{
		CycleHeaderTab(-1);
		return true;
	}
	if (Key == EKeys::E || Key == EKeys::Right || Key == EKeys::Gamepad_RightShoulder)
	{
		CycleHeaderTab(1);
		return true;
	}
	return false;
}

bool UBeyondMenuWidget::HandleFrameKey(const FKey& Key)
{
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right || Key == BeyondMenuLocal::GetPageKey(GetPage()))
	{
		CloseMenu();
		return true;
	}
	ABeyondPlayerController* PC = GetBeyondPlayerController();
	for (const EBeyondMenuPage Page : { EBeyondMenuPage::SkillTree, EBeyondMenuPage::Armor, EBeyondMenuPage::Overview, EBeyondMenuPage::Quests,
		EBeyondMenuPage::WorldMap })
	{
		if (Key == BeyondMenuLocal::GetPageKey(Page))
		{
			if (PC)
			{
				PC->OpenMenuPage(Page, GetNavigationTab());
			}
			return true;
		}
	}
	if (ShowsHeaderTabs())
	{
		const FKey Numbers[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Numbers); ++Index)
		{
			if (Key == Numbers[Index] && HeaderTabs.IsValidIndex(Index))
			{
				SelectHeaderTab(Index);
				return true;
			}
		}
	}
	return false;
}

//~ Painting

int32 UBeyondMenuWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace BeyondUI;
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FDesignSpace Space = FDesignSpace::Fit(FVector2f(AllottedGeometry.GetLocalSize()));
	const FGeometry Design = Space.MakeGeometry(AllottedGeometry);

	FPainter Frame(Design, OutDrawElements, BaseLayer);
	Frame.AnimTime = AnimTime;
	PaintFrame(Frame, Space);

	// The page fades in when the menu opens or changes page
	const float Fade = FMath::Clamp(OpenAge / BeyondMenuLocal::FadeInSeconds, 0.0f, 1.0f);
	FPainter Page(Design, OutDrawElements, BaseLayer, FMath::Lerp(0.25f, 1.0f, Fade));
	Page.AnimTime = AnimTime;
	PaintPage(Page);
	return FMath::Max(Frame.GetMaxLayer(), Page.GetMaxLayer());
}

void UBeyondMenuWidget::PaintFrame(BeyondUI::FPainter& Paint, const BeyondUI::FDesignSpace& Space) const
{
	using namespace BeyondUI;
	using namespace BeyondMenuLayout;
	const FPalette& P = Palette();
	UBeyondUISubsystem* UI = UBeyondUISubsystem::Get();
	const UBeyondUIArtSet* Art = UI ? UI->GetArtSet() : nullptr;

	Paint.Backdrop(Space, Art ? LoadTexture(Art->MenuBackdrop) : nullptr, 0);

	//~ Emblem and title
	const FVector2f EmblemCentre(66.0f, 58.0f);
	if (UTexture2D* Emblem = Art ? LoadTexture(Art->CharacterEmblem) : nullptr)
	{
		Paint.ImageCentred(Emblem, EmblemCentre, FVector2f(76.0f, 76.0f), FLinearColor::White, 4);
	}
	else
	{
		Paint.Ring(EmblemCentre, 22.0f, WithAlpha(P.Gold, 0.55f), 1.0f, 3);
		Paint.Sparkle(EmblemCentre, 26.0f, P.GoldBright, 4);
	}
	const FString Title = GetTitle().ToString();
	const FVector2f TitleSize = Paint.Text(Title, Font(EBeyondFont::Display, 28.0f), FVector2f(106.0f, 36.0f), P.GoldBright, 0.0f, 6);
	Paint.GradientH(FVector2f(106.0f, 80.0f), FVector2f(FMath::Max(TitleSize.X + 40.0f, 180.0f), 1.0f), WithAlpha(P.Gold, 0.7f), WithAlpha(P.Gold, 0.0f), 4);

	//~ Header tabs flanked by Q / E
	if (ShowsHeaderTabs() && HeaderTabs.Num() > 0)
	{
		const int32 Active = GetActiveHeaderTab();
		for (int32 Index = 0; Index < HeaderTabs.Num(); ++Index)
		{
			Paint.SkewTab(GetHeaderTabTopLeft(Index), FVector2f(HeaderTabWidth, HeaderTabHeight), HeaderTabs[Index].Title.ToString().ToUpper(),
				Index == Active, Index == HoveredHeaderTab, HeaderTabs[Index].Accent, 4);
		}
		if (HeaderTabs.Num() > 1)
		{
			const FVector2f First = GetHeaderTabTopLeft(0);
			const FVector2f Last = GetHeaderTabTopLeft(HeaderTabs.Num() - 1);
			Paint.KeyCap(TEXT("Q"), FVector2f(First.X - 62.0f, HeaderTabTop + 11.0f), 6);
			Paint.KeyCap(TEXT("E"), FVector2f(Last.X + HeaderTabWidth + 34.0f, HeaderTabTop + 11.0f), 6);
		}
	}

	//~ Counter (top right)
	FBeyondMenuCounter Counter;
	if (GetCounter(Counter))
	{
		float Right = 1880.0f;
		const FSlateFontInfo ValueFont = Font(EBeyondFont::Number, 26.0f);
		for (int32 Index = Counter.Values.Num() - 1; Index >= 0; --Index)
		{
			const FBeyondMenuCounter::FValue& Value = Counter.Values[Index];
			const FVector2f Size = Paint.Text(Value.Text, ValueFont, FVector2f(Right, 38.0f), P.Text, 1.0f, 6);
			Right -= Size.X + 22.0f;
			if (Value.Icon)
			{
				Paint.ImageCentred(Value.Icon, FVector2f(Right, 56.0f), FVector2f(34.0f, 34.0f), FLinearColor::White, 6);
			}
			else
			{
				Paint.Glyph(Value.Glyph, FVector2f(Right, 56.0f), 28.0f, Value.Tint, 6, 1.8f);
				if (Value.Glyph == EBeyondGlyph::SkillPoint || Value.Glyph == EBeyondGlyph::Star)
				{
					Paint.Sparkle(FVector2f(Right, 56.0f), 14.0f, Value.Tint, 6);
				}
			}
			Right -= 34.0f;
		}
		Paint.Text(Counter.Label.ToString().ToUpper(), Font(EBeyondFont::Heading, 15.0f), FVector2f(Right, 46.0f), P.TextDim, 1.0f, 6);
	}

	//~ Sidebar
	if (ShowsSidebar())
	{
		const TArray<EBeyondMenuPage>& Pages = GetSidebarPages();
		const float Bottom = SidebarTop + Pages.Num() * (SidebarEntryHeight + SidebarGap);
		Paint.GradientV(FVector2f(SidebarLeft - 8.0f, SidebarTop - 20.0f), FVector2f(1.0f, Bottom - SidebarTop + 40.0f), WithAlpha(P.Gold, 0.0f), WithAlpha(P.Gold, 0.4f), 3);
		for (int32 Index = 0; Index < Pages.Num(); ++Index)
		{
			const EBeyondMenuPage Entry = Pages[Index];
			const FVector2f TopLeft(SidebarLeft, SidebarTop + Index * (SidebarEntryHeight + SidebarGap));
			const FVector2f Size(SidebarWidth, SidebarEntryHeight);
			const bool bSelected = Entry == GetPage();
			const bool bHovered = Index == HoveredSidebar;
			const float MidY = TopLeft.Y + Size.Y * 0.5f;
			if (bSelected)
			{
				Paint.GradientH(TopLeft, Size, WithAlpha(P.Blue, 0.62f), WithAlpha(P.Blue, 0.0f), 4);
				Paint.GradientH(TopLeft, FVector2f(Size.X, 1.0f), WithAlpha(P.BlueBright, 0.8f), WithAlpha(P.BlueBright, 0.0f), 5);
				Paint.GradientH(TopLeft + FVector2f(0.0f, Size.Y - 1.0f), FVector2f(Size.X, 1.0f), WithAlpha(P.BlueBright, 0.8f), WithAlpha(P.BlueBright, 0.0f), 5);
				Paint.Glow(FVector2f(SidebarLeft - 8.0f, MidY), 22.0f, WithAlpha(P.GoldBright, 0.6f), 5);
				Paint.Diamond(FVector2f(SidebarLeft - 8.0f, MidY), 7.0f, 9.0f, P.GoldBright, 6);
			}
			else if (bHovered)
			{
				Paint.GradientH(TopLeft, Size, WithAlpha(FLinearColor::White, 0.09f), WithAlpha(FLinearColor::White, 0.0f), 4);
			}
			else
			{
				Paint.GradientH(TopLeft, Size, FromHex(0x121A2E, 0.55f), FromHex(0x121A2E, 0.0f), 4);
			}
			const FLinearColor LabelColor = bSelected ? FLinearColor::White : bHovered ? P.Text : P.TextDim;
			Paint.Glyph(BeyondMenuLocal::GetPageGlyph(Entry), FVector2f(TopLeft.X + 36.0f, MidY), 30.0f, LabelColor, 6, 1.7f);
			const FSlateFontInfo LabelFont = Font(EBeyondFont::Body, 23.0f);
			const FString Label = BeyondMenuLocal::GetPageName(Entry).ToString();
			Paint.Text(Label, LabelFont, FVector2f(TopLeft.X + 70.0f, MidY - MeasureText(Label, LabelFont).Y * 0.5f), LabelColor, 0.0f, 6);
		}
	}

	//~ Message line
	if (MessageAge >= 0.0f && MessageAge < BeyondMenuLocal::MessageSeconds && !Message.IsEmpty())
	{
		const float Alpha = MessageAge < BeyondMenuLocal::MessageSeconds - 0.6f ? 1.0f : (BeyondMenuLocal::MessageSeconds - MessageAge) / 0.6f;
		const FSlateFontInfo MessageFont = Font(EBeyondFont::Body, 20.0f);
		const FString Text = Message.ToString();
		const FVector2f Size = MeasureText(Text, MessageFont);
		Paint.GradientFadeH(FVector2f(960.0f - Size.X * 0.5f - 120.0f, KeyBarTop - 52.0f), FVector2f(Size.X + 240.0f, 34.0f), WithAlpha(P.Backdrop, 0.85f * Alpha), 80);
		Paint.Text(Text, MessageFont, FVector2f(960.0f, KeyBarTop - 48.0f), WithAlpha(bMessageWarning ? P.Minus : P.GoldBright, Alpha), 0.5f, 81);
	}

	//~ Key bar
	const FVector2f BleedLeft = Space.BleedTopLeft();
	const float BarLeft = FMath::Max(BleedLeft.X + 40.0f, -200.0f);
	const float BarRight = FMath::Min(BleedLeft.X + Space.BleedSize().X - 40.0f, 2120.0f);
	Paint.Separator(FVector2f(BarLeft, KeyBarTop - 6.0f), BarRight - BarLeft, 6, 0.8f);
	KeyHintBoxes.Reset();
	const TArray<FBeyondKeyHint> Hints = GetKeyHints();
	float X = 40.0f;
	for (int32 Index = 0; Index < Hints.Num(); ++Index)
	{
		const FBeyondKeyHint& Hint = Hints[Index];
		const bool bLast = Index == Hints.Num() - 1;
		// Esc / Close sits at the right end
		FVector2f TopLeft(X, KeyBarTop + 12.0f);
		if (bLast)
		{
			const FSlateFontInfo LabelFont = Font(EBeyondFont::Body, 17.0f);
			const float Width = 52.0f + MeasureText(Hint.Label.ToString(), LabelFont).X;
			TopLeft.X = 1880.0f - Width;
		}
		const float Opacity = Index == HoveredKeyHint ? 1.0f : 0.85f;
		const float Width = Paint.KeyHint(Hint.Cap, Hint.Label.ToString(), TopLeft, 8, Opacity);
		KeyHintBoxes.Emplace(FBox2f(TopLeft - FVector2f(4.0f, 4.0f), TopLeft + FVector2f(Width + 4.0f, 32.0f)), Hint.Key);
		X += Width + 38.0f;
	}
}

#undef LOCTEXT_NAMESPACE
