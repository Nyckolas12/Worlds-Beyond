// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondProgressWidget.h"
#include "Characters/BeyondCharacterBase.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Items/BeyondInventoryComponent.h"
#include "Items/BeyondItemLibrary.h"
#include "Items/BeyondLootDrop.h"
#include "Kismet/GameplayStatics.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	// Bar fill speed in bar-widths per second while catching up
	constexpr float ProgressBarFillSpeed = 1.6f;
	constexpr float ProgressNoticeDuration = 2.8f;
	constexpr float ProgressToastFade = 0.3f;

	FVector2f MeasureProgressText(const FString& Text, const FSlateFontInfo& Font)
	{
		// Rough size when there is no renderer to ask (-nullrhi automation runs)
		FSlateRenderer* Renderer = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetRenderer() : nullptr;
		if (!Renderer)
		{
			return FVector2f(Text.Len() * Font.Size * 0.6f, Font.Size * 1.2f);
		}
		const TSharedRef<FSlateFontMeasure> Measure = Renderer->GetFontMeasureService();
		return FVector2f(Measure->Measure(Text, Font));
	}

	FLinearColor ProgressWithAlpha(FLinearColor Color, float Opacity)
	{
		Color.A *= FMath::Clamp(Opacity, 0.0f, 1.0f);
		return Color;
	}
}

UBeyondProgressWidget::UBeyondProgressWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PillBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	PillBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	PillBrush.TintColor = FSlateColor(FLinearColor::White);
}

void UBeyondProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Never eats clicks
	SetVisibility(ESlateVisibility::HitTestInvisible);
	BindToParty();
	ReadLeader();
	DisplayedLevel = TargetLevel;
	DisplayedFraction = TargetFraction;
}

void UBeyondProgressWidget::NativeDestruct()
{
	if (UBeyondPartyComponent* Party = BoundParty.Get())
	{
		Party->OnMemberLevelUp.RemoveDynamic(this, &ThisClass::HandleMemberLevelUp);
		Party->OnExperienceAwarded.RemoveDynamic(this, &ThisClass::HandleExperienceAwarded);
	}
	if (UBeyondInventoryComponent* Inventory = BoundInventory.Get())
	{
		Inventory->OnItemAdded.RemoveDynamic(this, &ThisClass::HandleItemAdded);
	}
	BoundParty.Reset();
	BoundInventory.Reset();
	Super::NativeDestruct();
}

void UBeyondProgressWidget::BindToParty()
{
	const APlayerController* PC = GetOwningPlayer();
	UBeyondInventoryComponent* Inventory = PC ? PC->FindComponentByClass<UBeyondInventoryComponent>() : nullptr;
	if (Inventory && Inventory != BoundInventory.Get())
	{
		Inventory->OnItemAdded.AddUniqueDynamic(this, &ThisClass::HandleItemAdded);
		BoundInventory = Inventory;
	}

	UBeyondPartyComponent* Party = PC ? PC->FindComponentByClass<UBeyondPartyComponent>() : nullptr;
	if (!Party || Party == BoundParty.Get())
	{
		return;
	}
	Party->OnMemberLevelUp.AddUniqueDynamic(this, &ThisClass::HandleMemberLevelUp);
	Party->OnExperienceAwarded.AddUniqueDynamic(this, &ThisClass::HandleExperienceAwarded);
	BoundParty = Party;
}

void UBeyondProgressWidget::HandleMemberLevelUp(ABeyondCharacterBase* Member, int32 NewLevel)
{
	ShowLevelUpBanner(Member, NewLevel);
}

void UBeyondProgressWidget::HandleExperienceAwarded(ABeyondCharacterBase* Victim, float InExperience)
{
	ShowExperienceGain(InExperience);
}

void UBeyondProgressWidget::HandleItemAdded(const FBeyondItemInstance& Item)
{
	ShowLootToast(Item);
}

void UBeyondProgressWidget::ShowLootToast(const FBeyondItemInstance& Item)
{
	FLootToast Toast;
	Toast.Label = UBeyondItemLibrary::GetItemName(Item).ToString();
	Toast.Color = UBeyondItemLibrary::GetTierColor(Item.Tier);
	const UBeyondItemDefinition* Definition = Item.GetDefinition();
	Toast.Detail = FString::Printf(TEXT("%s  %s"), *UBeyondItemLibrary::GetTierName(Item.Tier).ToString(),
		Definition ? *UBeyondItemLibrary::GetSlotName(Definition->Slot).ToString() : TEXT(""));
	if (Definition && Definition->Set)
	{
		Toast.Detail += FString::Printf(TEXT("  \u00B7  %s"), *Definition->Set->SetName.ToString());
	}
	Toasts.Insert(Toast, 0);
	if (Toasts.Num() > MaxToasts)
	{
		Toasts.SetNum(MaxToasts);
	}
}

void UBeyondProgressWidget::ShowNotice(const FText& Text)
{
	Notice = Text;
	NoticeAge = 0.0f;
}

void UBeyondProgressWidget::ReadPickupTarget()
{
	const ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetOwningPlayer());
	const ABeyondLootDrop* Drop = PC ? PC->FindPickupTarget() : nullptr;
	const FString Label = Drop ? Drop->GetLabel().ToString() : FString();
	if (Label != PromptLabel)
	{
		PromptAge = 0.0f;
	}
	PromptLabel = Label;
	PromptColor = Drop ? UBeyondItemLibrary::GetTierColor(Drop->GetItem().Tier) : FLinearColor::White;
}

void UBeyondProgressWidget::ShowLevelUpBanner(ABeyondCharacterBase* Member, int32 NewLevel)
{
	const FString Name = Member ? Member->GetCharacterDisplayName().ToString() : FString();

	// Both demigods share EXP, so they usually level together: one banner naming both
	const bool bMerge = BannerAge >= 0.0f && BannerAge < 0.5f && BannerLevel == NewLevel;
	if (!bMerge)
	{
		BannerNames.Reset();
		BannerAge = 0.0f;
		if (LevelUpSound)
		{
			UGameplayStatics::PlaySound2D(this, LevelUpSound);
		}
	}
	if (!Name.IsEmpty())
	{
		BannerNames.AddUnique(Name);
	}
	BannerLevel = NewLevel;

	const int32 Points = Member ? Member->GetSkillPoints() : 0;
	const FString Who = BannerNames.IsEmpty() ? FString() : FString::Join(BannerNames, TEXT(" & ")) + TEXT("  \u00B7  ");
	BannerSubtitle = FString::Printf(TEXT("%sLevel %d  \u00B7  %d skill point%s"), *Who, NewLevel, Points, Points == 1 ? TEXT("") : TEXT("s"));
}

void UBeyondProgressWidget::ShowExperienceGain(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}
	// Kills in quick succession add up
	GainAmount = GainAge >= 0.0f && GainAge < GainDuration * 0.6f ? GainAmount + Amount : Amount;
	GainAge = 0.0f;
}

void UBeyondProgressWidget::ReadLeader()
{
	ABeyondCharacterBase* Leader = Cast<ABeyondCharacterBase>(GetOwningPlayerPawn());
	if (!Leader)
	{
		return;
	}

	const bool bNewLeader = Leader != ShownLeader.Get();
	ShownLeader = Leader;
	LeaderName = Leader->GetCharacterDisplayName().ToString();
	TargetLevel = Leader->GetCharacterLevel();
	Experience = Leader->GetExperience();
	ExperienceNeeded = Leader->GetExperienceToNextLevel();
	TargetFraction = ExperienceNeeded > 0.0f ? FMath::Clamp(Experience / ExperienceNeeded, 0.0f, 1.0f) : 1.0f;
	SkillPoints = Leader->GetSkillPoints();

	// Swapping to the other demigod (or loading a save) snaps instead of animating
	if (bNewLeader || TargetLevel < DisplayedLevel || TargetLevel > DisplayedLevel + 1)
	{
		DisplayedLevel = TargetLevel;
		DisplayedFraction = TargetFraction;
	}
}

void UBeyondProgressWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	BindToParty();
	ReadLeader();
	ReadPickupTarget();
	AdvanceAnimation(InDeltaTime);
}

void UBeyondProgressWidget::AdvanceAnimation(float DeltaSeconds)
{
	AnimTime += DeltaSeconds;

	// The bar runs to the end, wraps to the new level, then settles on the current EXP
	if (DisplayedLevel < TargetLevel)
	{
		DisplayedFraction += ProgressBarFillSpeed * DeltaSeconds;
		if (DisplayedFraction >= 1.0f)
		{
			DisplayedLevel = TargetLevel;
			DisplayedFraction = 0.0f;
		}
	}
	else if (DisplayedFraction < TargetFraction)
	{
		DisplayedFraction = FMath::Min(TargetFraction, DisplayedFraction + ProgressBarFillSpeed * DeltaSeconds);
	}
	else
	{
		DisplayedFraction = TargetFraction;
	}

	if (BannerAge >= 0.0f)
	{
		BannerAge += DeltaSeconds;
		if (BannerAge >= GetBannerDuration())
		{
			BannerAge = -1.0f;
		}
	}
	if (GainAge >= 0.0f)
	{
		GainAge += DeltaSeconds;
		if (GainAge >= GainDuration)
		{
			GainAge = -1.0f;
		}
	}

	PromptAge += DeltaSeconds;
	for (FLootToast& Toast : Toasts)
	{
		Toast.Age += DeltaSeconds;
	}
	Toasts.RemoveAll([this](const FLootToast& Toast) { return Toast.Age >= ToastDuration; });
	if (NoticeAge >= 0.0f)
	{
		NoticeAge += DeltaSeconds;
		if (NoticeAge >= ProgressNoticeDuration)
		{
			NoticeAge = -1.0f;
		}
	}
}

int32 UBeyondProgressWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 BackLayer = BaseLayer + 1;
	const int32 FillLayer = BaseLayer + 2;
	const int32 TextLayer = BaseLayer + 3;

	const FVector2f ScreenSize(AllottedGeometry.GetLocalSize());
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));

	auto DrawShape = [&](const FSlateBrush* ShapeBrush, const FVector2f& ShapeTopLeft, const FVector2f& ShapeSize, const FLinearColor& ShapeTint, int32 ShapeLayer)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, ShapeLayer, AllottedGeometry.ToPaintGeometry(ShapeSize, FSlateLayoutTransform(ShapeTopLeft)),
			ShapeBrush, ESlateDrawEffect::None, ShapeTint);
	};
	auto DrawPill = [&](const FVector2f& PillTopLeft, const FVector2f& PillSize, const FLinearColor& PillTint, int32 PillLayer)
	{
		// Very short fills look wrong with round ends
		DrawShape(PillSize.X >= PillSize.Y ? &PillBrush : WhiteBrush, PillTopLeft, PillSize, PillTint, PillLayer);
	};
	auto DrawLabel = [&](const FString& Label, const FSlateFontInfo& LabelFont, const FVector2f& LabelPosition, const FLinearColor& LabelTint, float Align, int32 LabelLayer)
	{
		// Align: 0 left, 0.5 centred, 1 right of the position; a dark copy underneath keeps it readable
		const FVector2f LabelSize = MeasureProgressText(Label, LabelFont);
		const FVector2f LabelTopLeft(LabelPosition.X - LabelSize.X * Align, LabelPosition.Y);
		FSlateDrawElement::MakeText(OutDrawElements, LabelLayer, AllottedGeometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(LabelTopLeft + FVector2f(1.5f, 1.5f))),
			Label, LabelFont, ESlateDrawEffect::None, ProgressWithAlpha(ShadowColor, LabelTint.A));
		FSlateDrawElement::MakeText(OutDrawElements, LabelLayer + 1, AllottedGeometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(LabelTopLeft)),
			Label, LabelFont, ESlateDrawEffect::None, LabelTint);
		return LabelSize;
	};

	//~ Loot: pickup prompt, toasts, notices
	const FSlateFontInfo PromptFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
	const FSlateFontInfo ToastFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	const FSlateFontInfo ToastDetailFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	if (!PromptLabel.IsEmpty())
	{
		const float Alpha = FMath::Clamp(PromptAge / 0.15f, 0.0f, 1.0f);
		const FVector2f Centre = ScreenSize * FVector2f(PromptPosition);
		const FVector2f LabelSize = MeasureProgressText(PromptLabel, PromptFont);
		const float KeySize = LabelSize.Y + 8.0f;
		const float Total = KeySize + 12.0f + LabelSize.X;
		const FVector2f Left(Centre.X - Total * 0.5f, Centre.Y - KeySize * 0.5f);
		DrawShape(&PillBrush, Left - FVector2f(14.0f, 6.0f), FVector2f(Total + 28.0f, KeySize + 12.0f), ProgressWithAlpha(PanelColor, Alpha), BackLayer);
		DrawShape(&PillBrush, Left, FVector2f(KeySize, KeySize), ProgressWithAlpha(TextColor, Alpha), FillLayer);
		const FVector2f KeyTextSize = MeasureProgressText(TEXT("F"), PromptFont);
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer, AllottedGeometry.ToPaintGeometry(KeyTextSize,
			FSlateLayoutTransform(Left + (FVector2f(KeySize, KeySize) - KeyTextSize) * 0.5f)), TEXT("F"), PromptFont, ESlateDrawEffect::None,
			ProgressWithAlpha(FLinearColor(0.03f, 0.03f, 0.05f, 1.0f), Alpha));
		DrawLabel(PromptLabel, PromptFont, FVector2f(Left.X + KeySize + 12.0f, Centre.Y - LabelSize.Y * 0.5f), ProgressWithAlpha(PromptColor, Alpha), 0.0f, TextLayer);
	}

	float ToastBottom = ScreenSize.Y * 0.62f;
	for (const FLootToast& Toast : Toasts)
	{
		const float FadeIn = FMath::Clamp(Toast.Age / ProgressToastFade, 0.0f, 1.0f);
		const float FadeOut = FMath::Clamp((ToastDuration - Toast.Age) / ProgressToastFade, 0.0f, 1.0f);
		const float Alpha = FMath::Min(FadeIn, FadeOut);
		const FVector2f LabelSize = MeasureProgressText(Toast.Label, ToastFont);
		const FVector2f DetailSize = MeasureProgressText(Toast.Detail, ToastDetailFont);
		const FVector2f BoxSize(FMath::Max(LabelSize.X, DetailSize.X) + 40.0f, LabelSize.Y + DetailSize.Y + 14.0f);
		// Slides in from the right edge
		const float Right = ScreenSize.X - 36.0f + (1.0f - FMath::InterpEaseOut(0.0f, 1.0f, FadeIn, 3.0f)) * 60.0f;
		const FVector2f TopLeft(Right - BoxSize.X, ToastBottom - BoxSize.Y);
		DrawShape(WhiteBrush, TopLeft, BoxSize, ProgressWithAlpha(PanelColor, Alpha), BackLayer);
		DrawShape(WhiteBrush, TopLeft, FVector2f(4.0f, BoxSize.Y), ProgressWithAlpha(Toast.Color, Alpha), FillLayer);
		DrawLabel(Toast.Label, ToastFont, TopLeft + FVector2f(16.0f, 6.0f), ProgressWithAlpha(Toast.Color, Alpha), 0.0f, TextLayer);
		DrawLabel(Toast.Detail, ToastDetailFont, TopLeft + FVector2f(16.0f, 8.0f + LabelSize.Y), ProgressWithAlpha(TextColor, 0.75f * Alpha), 0.0f, TextLayer);
		ToastBottom -= BoxSize.Y + 8.0f;
	}

	if (NoticeAge >= 0.0f && !Notice.IsEmpty())
	{
		const float Alpha = NoticeAge < ProgressNoticeDuration - 0.5f ? 1.0f : (ProgressNoticeDuration - NoticeAge) / 0.5f;
		DrawLabel(Notice.ToString(), PromptFont, FVector2f(ScreenSize.X * 0.5f, ScreenSize.Y * FVector2f(PromptPosition).Y + 34.0f),
			ProgressWithAlpha(WarningColor, Alpha), 0.5f, TextLayer);
	}

	if (!ShownLeader.IsValid())
	{
		return TextLayer + 1;
	}

	//~ Badge + bar (bottom left by default)
	const FVector2f BadgeCentre = ScreenSize * FVector2f(PanelAnchor) + FVector2f(BadgeOffset);
	const float Badge = BadgeSize;
	const bool bBannerUp = IsBannerShowing();
	const float Pulse = bBannerUp ? 0.5f + 0.5f * FMath::Sin(AnimTime * 8.0f) : 0.0f;

	// Gold ring (a gold disc under a dark one), with a blue glow while the banner is up
	if (bBannerUp)
	{
		const float Glow = Badge + 10.0f + 6.0f * Pulse;
		DrawPill(BadgeCentre - FVector2f(Glow, Glow) * 0.5f, FVector2f(Glow, Glow), ProgressWithAlpha(BlueColor, 0.35f + 0.25f * Pulse), BackLayer);
	}
	DrawPill(BadgeCentre - FVector2f(Badge, Badge) * 0.5f, FVector2f(Badge, Badge), GoldColor, BackLayer);
	const float Inner = Badge - 6.0f;
	DrawPill(BadgeCentre - FVector2f(Inner, Inner) * 0.5f, FVector2f(Inner, Inner), FLinearColor(PanelColor.R, PanelColor.G, PanelColor.B, 1.0f), FillLayer);

	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
	const FSlateFontInfo LevelFont = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(Badge * 0.36f));
	const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	const FSlateFontInfo InfoFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);

	DrawLabel(TEXT("LV"), SmallFont, BadgeCentre + FVector2f(0.0f, -Badge * 0.36f), GoldColor, 0.5f, TextLayer);
	const FString LevelText = FString::FromInt(DisplayedLevel);
	const FVector2f LevelSize = MeasureProgressText(LevelText, LevelFont);
	DrawLabel(LevelText, LevelFont, BadgeCentre + FVector2f(0.0f, -LevelSize.Y * 0.42f), TextColor, 0.5f, TextLayer);

	// Name, skill points, bar and EXP to the right of the badge
	const FVector2f BarTopLeft(BadgeCentre.X + Badge * 0.5f + 12.0f, BadgeCentre.Y - BarHeight * 0.5f + 2.0f);
	const FVector2f NameSize = DrawLabel(LeaderName, NameFont, FVector2f(BarTopLeft.X, BarTopLeft.Y - 24.0f), TextColor, 0.0f, TextLayer);
	if (SkillPoints > 0)
	{
		const FString PointsText = FString::Printf(TEXT("+%d SP"), SkillPoints);
		const FVector2f PointsSize = MeasureProgressText(PointsText, SmallFont);
		const FVector2f PointsTopLeft(BarTopLeft.X + NameSize.X + 10.0f, BarTopLeft.Y - 22.0f);
		const FVector2f PointsPillSize(PointsSize.X + 14.0f, PointsSize.Y + 4.0f);
		DrawPill(PointsTopLeft, PointsPillSize, ProgressWithAlpha(GoldColor, 0.85f + 0.15f * FMath::Sin(AnimTime * 3.0f)), BackLayer);
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer, AllottedGeometry.ToPaintGeometry(PointsSize, FSlateLayoutTransform(PointsTopLeft + FVector2f(7.0f, 2.0f))),
			PointsText, SmallFont, ESlateDrawEffect::None, FLinearColor(0.05f, 0.04f, 0.02f, 1.0f));
	}

	DrawPill(BarTopLeft - FVector2f(2.0f, 2.0f), FVector2f(BarWidth + 4.0f, BarHeight + 4.0f), ProgressWithAlpha(GoldColor, 0.55f), BackLayer);
	DrawPill(BarTopLeft, FVector2f(BarWidth, BarHeight), PanelColor, FillLayer);
	if (DisplayedFraction > 0.0f)
	{
		// Blue at the start of a level, gold near the end
		const FLinearColor FillColor = FMath::Lerp(BlueColor, GoldColor, FMath::Clamp(DisplayedFraction * 1.2f, 0.0f, 1.0f));
		DrawPill(BarTopLeft, FVector2f(BarWidth * DisplayedFraction, BarHeight), FillColor, FillLayer + 1);
	}

	const FString ExperienceText = ExperienceNeeded > 0.0f
		? FString::Printf(TEXT("%.0f / %.0f EXP"), Experience, ExperienceNeeded)
		: FString(TEXT("MAX LEVEL"));
	DrawLabel(ExperienceText, InfoFont, FVector2f(BarTopLeft.X + BarWidth, BarTopLeft.Y + BarHeight + 4.0f), ProgressWithAlpha(TextColor, 0.8f), 1.0f, TextLayer);

	if (GainAge >= 0.0f)
	{
		const float Alpha = GainAge < 0.15f ? GainAge / 0.15f : 1.0f - FMath::Max(0.0f, GainAge - GainDuration * 0.5f) / (GainDuration * 0.5f);
		const FString GainText = FString::Printf(TEXT("+%.0f EXP"), GainAmount);
		DrawLabel(GainText, NameFont, FVector2f(BarTopLeft.X + BarWidth, BarTopLeft.Y - 26.0f - GainAge * 14.0f), ProgressWithAlpha(GoldColor, Alpha), 1.0f, TextLayer);
	}

	//~ Level-up banner (top centre)
	if (bBannerUp)
	{
		const float Duration = GetBannerDuration();
		float Alpha = 1.0f;
		if (BannerAge < BannerFadeIn)
		{
			Alpha = BannerAge / BannerFadeIn;
		}
		else if (BannerAge > Duration - BannerFadeOut)
		{
			Alpha = (Duration - BannerAge) / BannerFadeOut;
		}
		const float Slide = (1.0f - FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(BannerAge / BannerFadeIn, 0.0f, 1.0f), 3.0f)) * -18.0f;
		const FVector2f BannerAnchor(BannerPosition);
		const FVector2f Centre(ScreenSize.X * BannerAnchor.X, BannerAnchor.Y + Slide);

		const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 34);
		const FSlateFontInfo SubtitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 15);
		const FString Title(TEXT("LEVEL UP"));
		const FVector2f TitleSize = MeasureProgressText(Title, TitleFont);

		// Dark band behind, gold rules either side of the title
		const FVector2f BandSize(FMath::Max(TitleSize.X + 260.0f, 520.0f), TitleSize.Y + 48.0f);
		DrawShape(WhiteBrush, Centre - FVector2f(BandSize.X * 0.5f, 10.0f), BandSize, ProgressWithAlpha(PanelColor, Alpha * 0.7f), BackLayer);
		for (const float Side : { -1.0f, 1.0f })
		{
			const float InnerX = Centre.X + Side * (TitleSize.X * 0.5f + 18.0f);
			const float OuterX = Centre.X + Side * (TitleSize.X * 0.5f + 120.0f);
			const float LineY = Centre.Y + TitleSize.Y * 0.5f;
			TArray<FVector2f> Rule = { FVector2f(InnerX, LineY), FVector2f(OuterX, LineY) };
			FSlateDrawElement::MakeLines(OutDrawElements, FillLayer, AllottedGeometry.ToPaintGeometry(), MoveTemp(Rule),
				ESlateDrawEffect::None, ProgressWithAlpha(GoldColor, Alpha), true, 2.0f);
			const FVector2f Diamond(6.0f, 6.0f);
			DrawShape(WhiteBrush, FVector2f(InnerX, LineY) - Diamond * 0.5f, Diamond, ProgressWithAlpha(BlueColor, Alpha), FillLayer);
		}

		DrawLabel(Title, TitleFont, Centre, ProgressWithAlpha(GoldColor, Alpha), 0.5f, TextLayer);
		DrawLabel(BannerSubtitle, SubtitleFont, Centre + FVector2f(0.0f, TitleSize.Y + 2.0f), ProgressWithAlpha(TextColor, Alpha), 0.5f, TextLayer);
	}

	return TextLayer + 1;
}
