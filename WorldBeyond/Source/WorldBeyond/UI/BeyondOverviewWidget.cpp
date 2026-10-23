// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondOverviewWidget.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "Engine/Texture2D.h"
#include "Items/BeyondEquipmentComponent.h"
#include "Items/BeyondInventoryComponent.h"
#include "Items/BeyondItemLibrary.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Progression/BeyondSkillTreeComponent.h"
#include "Progression/BeyondStatEffects.h"
#include "UI/BeyondCharacterProfile.h"
#include "UI/BeyondUIArt.h"
#include "World/BeyondRegionDefinition.h"
#include "World/BeyondWorldSubsystem.h"

#define LOCTEXT_NAMESPACE "BeyondOverview"

namespace BeyondOverviewLocal
{
	const FVector2f PartyTopLeft(300.0f, 118.0f);
	const FVector2f PartySize(226.0f, 664.0f);
	constexpr float CardHeight = 196.0f;
	constexpr float CardGap = 12.0f;
	const FVector2f CentreTopLeft(540.0f, 118.0f);
	const FVector2f CentreSize(850.0f, 664.0f);
	const FVector2f RightTopLeft(1404.0f, 118.0f);
	constexpr float RightWidth = 486.0f;
	constexpr float BottomTop = 796.0f;
	constexpr float BottomHeight = 192.0f;

	const TCHAR* const BossIds[] = { TEXT("gorehide"), TEXT("veyla"), TEXT("kaelthar"), TEXT("hrimgar") };

	FVector2f GetCardTopLeft(int32 Index)
	{
		return PartyTopLeft + FVector2f(13.0f, 50.0f + Index * (CardHeight + CardGap));
	}

	// "GA_Duo_HeavensJudgment_C" -> "Heavens Judgment" (until abilities carry display names)
	FText GetAbilityName(const TSubclassOf<UGameplayAbility>& Ability)
	{
		if (!Ability)
		{
			return FText::GetEmpty();
		}
		if (const UBeyondGameplayAbility* Named = Cast<UBeyondGameplayAbility>(Ability->GetDefaultObject()); Named && !Named->DisplayName.IsEmpty())
		{
			return Named->DisplayName;
		}
		FString Name = Ability->GetName();
		Name.RemoveFromEnd(TEXT("_C"));
		for (const TCHAR* Prefix : { TEXT("BeyondGA_"), TEXT("GA_Duo_"), TEXT("GA_"), TEXT("Duo_") })
		{
			Name.RemoveFromStart(Prefix);
		}
		Name.ReplaceInline(TEXT("_"), TEXT(" "));
		FString Spaced;
		for (int32 Index = 0; Index < Name.Len(); ++Index)
		{
			if (Index > 0 && FChar::IsUpper(Name[Index]) && FChar::IsLower(Name[Index - 1]))
			{
				Spaced.AppendChar(TEXT(' '));
			}
			Spaced.AppendChar(Name[Index]);
		}
		Spaced.ReplaceInline(TEXT("Heavens "), TEXT("Heaven's "));
		return FText::FromString(Spaced);
	}
}

FBeyondOverviewModel UBeyondOverviewWidget::BuildModel() const
{
	using namespace BeyondOverviewLocal;
	FBeyondOverviewModel Model;
	const ABeyondPlayerController* PC = GetBeyondPlayerController();
	const UBeyondPartyComponent* Party = PC ? PC->PartyComponent.Get() : nullptr;
	const int32 Tab = GetActiveHeaderTab();
	const bool bDuo = IsDuoHeaderTab(Tab);
	ABeyondCharacterBase* Character = GetHeaderCharacter(Tab);
	const UBeyondCharacterProfile* Profile = GetProfileFor(Tab);

	//~ Party cards
	for (int32 Index = 0; Index < HeaderTabs.Num(); ++Index)
	{
		FBeyondOverviewModel::FPartyCard& Card = Model.Party.AddDefaulted_GetRef();
		Card.Name = HeaderTabs[Index].Title;
		Card.bDuo = HeaderTabs[Index].bDuo;
		const ABeyondCharacterBase* Member = HeaderTabs[Index].Character.Get();
		Card.Level = Member ? Member->GetCharacterLevel() : Party ? Party->GetPartyLevel() : 1;
		if (const UBeyondCharacterProfile* CardProfile = GetProfileFor(Index))
		{
			Card.Portrait = LoadTexture(CardProfile->CardPortrait);
		}
	}

	//~ Who is shown
	Model.Name = bDuo ? LOCTEXT("Duo", "Angel & Ji-Woong") : Character ? Character->GetCharacterDisplayName() : FText::GetEmpty();
	if (Profile)
	{
		Model.ClassTitle = Profile->ClassTitle;
		Model.Motto = Profile->Motto;
		Model.Element = Profile->PrimaryElement;
	}
	if (Character)
	{
		Model.Level = BeyondMenuPanels::GetLevelInfo(Character);
		if (const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		{
			Model.Health = ASC->GetNumericAttribute(UCharacterAttributeSet::GetCurrentHealthAttribute());
			Model.MaxHealth = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());
			Model.Stamina = ASC->GetNumericAttribute(UCharacterAttributeSet::GetCurrentStaminaAttribute());
			Model.MaxStamina = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxStaminaAttribute());
		}
		Model.Stats = BeyondMenuPanels::GetCombatStats(Character);
	}
	else if (Party)
	{
		Model.Level.Level = Party->GetPartyLevel();
		Model.Level.ExperienceToNext = -1.0f;
		Model.Health = Party->GetBond();
		Model.MaxHealth = Party->MaxBond;
		FBeyondStatRow& Points = Model.Stats.AddDefaulted_GetRef();
		Points.Glyph = EBeyondGlyph::BondPoint;
		Points.Name = LOCTEXT("BondPoints", "Bond Points");
		Points.Value = Party->GetBondPoints();
		FBeyondStatRow& Gain = Model.Stats.AddDefaulted_GetRef();
		Gain.Glyph = EBeyondGlyph::Up;
		Gain.Name = LOCTEXT("BondGain", "Bond Gain");
		Gain.Value = 100.0f * (Party->GetBondGainMultiplier() - 1.0f);
		Gain.bPercent = true;
	}

	//~ Affinity: the four region elements around the demigod's own
	const UAbilitySystemComponent* AffinityASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	for (const EBeyondElement Element : { EBeyondElement::Verdant, EBeyondElement::Shadow, EBeyondElement::Frost, EBeyondElement::Flame })
	{
		EBeyondSkillStat Stat;
		const float Value = AffinityASC && BeyondStats::GetAffinityStat(Element, Stat) && AffinityASC->HasAttributeSetForAttribute(BeyondStats::GetStatAttribute(Stat))
			? AffinityASC->GetNumericAttribute(BeyondStats::GetStatAttribute(Stat)) : 0.0f;
		Model.Affinity.Emplace(Element, Value);
	}

	//~ Passives: set bonuses being worn, then skill nodes taken that work on their own
	if (const UBeyondEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr)
	{
		for (const UBeyondArmorSet* Set : Equipment->GetWornSets())
		{
			const int32 Pieces = Equipment->GetSetPieceCount(Set);
			FBeyondOverviewModel::FLine& Line = Model.Passives.AddDefaulted_GetRef();
			Line.Glyph = EBeyondGlyph::Star;
			Line.Title = FText::Format(LOCTEXT("SetPassive", "{0} ({1} / {2})"), Set->SetName, FText::AsNumber(Pieces), FText::AsNumber(UBeyondArmorSet::FullSetPieces));
			TArray<FString> TwoPiece;
			for (const FBeyondItemStat& Stat : Set->TwoPieceStats)
			{
				TwoPiece.Add(UBeyondItemLibrary::FormatStat(Stat).ToString());
			}
			Line.Text = Pieces >= UBeyondArmorSet::FullSetPieces ? Set->FullSetDescription : FText::FromString(FString::Join(TwoPiece, TEXT(", ")));
		}
	}
	const UBeyondSkillTreeComponent* Tree = Character ? Character->GetSkillTreeComponent() : PC ? PC->DuoSkillTree.Get() : nullptr;
	if (Tree && Tree->Tree)
	{
		for (const FBeyondSkillNode& Node : Tree->Tree->Nodes)
		{
			const int32 Rank = Tree->GetRank(Node.Id);
			const bool bGrantsAbility = Node.Effects.ContainsByPredicate([](const FBeyondSkillEffect& Effect)
			{
				return Effect.Type == EBeyondSkillEffectType::GrantAbility || Effect.Type == EBeyondSkillEffectType::DuoPower;
			});
			if (Rank <= 0 || bGrantsAbility || Model.Passives.Num() >= 4)
			{
				continue;
			}
			FBeyondOverviewModel::FLine& Line = Model.Passives.AddDefaulted_GetRef();
			Line.Icon = Node.Icon;
			Line.Glyph = Model.Element != EBeyondElement::None ? BeyondUI::ElementGlyph(Model.Element) : EBeyondGlyph::Star;
			Line.Title = Node.MaxRank > 1 ? FText::Format(LOCTEXT("NodeRank", "{0}  {1}/{2}"), Node.DisplayName, FText::AsNumber(Rank), FText::AsNumber(Node.MaxRank))
				: Node.DisplayName;
			TArray<FString> Effects;
			for (const FText& Effect : UBeyondSkillTreeAsset::DescribeEffects(Node, Rank))
			{
				Effects.Add(Effect.ToString());
			}
			Line.Text = FText::FromString(FString::Join(Effects, TEXT(", ")));
		}
	}

	//~ Bond (its levels come later; the meter and the duo tree's effects for now)
	if (Party)
	{
		Model.BondExperience = Party->GetBond();
		Model.BondExperienceToNext = Party->MaxBond;
		if (const float Faster = 100.0f * (Party->GetBondGainMultiplier() - 1.0f); Faster > 0.5f)
		{
			Model.BondPerks.Add(FText::Format(LOCTEXT("BondGainPerk", "The Bond meter fills {0} faster."), FText::FromString(BeyondUI::FormatPercent(Faster))));
		}
		Model.BondPerks.Add(FText::Format(LOCTEXT("BondPointsPerk", "{0} Bond Points to spend on the duo tree."), FText::AsNumber(Party->GetBondPoints())));
		Model.BondPerks.Add(LOCTEXT("BondDuoPerk", "Full meter: the duo move is ready on G."));
	}
	if (const UBeyondDuoSkillTreeComponent* Duo = PC ? PC->DuoSkillTree.Get() : nullptr)
	{
		TArray<TSubclassOf<UGameplayAbility>> Powers = Duo->GetAvailableDuoPowers();
		Powers.Remove(nullptr);
		for (const TSubclassOf<UGameplayAbility>& Power : Powers)
		{
			FBeyondOverviewModel::FLine& Line = Model.DuoAbilities.AddDefaulted_GetRef();
			Line.Title = BeyondOverviewLocal::GetAbilityName(Power);
			Line.Value = FString::Printf(TEXT("Lv. %d"), 1 + Duo->GetAbilityRankBonus(Power));
			if (const UBeyondGameplayAbility* Ability = Cast<UBeyondGameplayAbility>(Power->GetDefaultObject()))
			{
				Line.Icon = Ability->Icon;
			}
			Line.Glyph = EBeyondGlyph::Bond;
		}
	}

	//~ Resources (the wallet comes later; points and the bag for now)
	UBeyondUISubsystem* UI = UBeyondUISubsystem::Get();
	const UBeyondUIArtSet* Art = UI ? UI->GetArtSet() : nullptr;
	if (const UBeyondSkillTreeComponent* OwnTree = Character ? Character->GetSkillTreeComponent() : nullptr)
	{
		FBeyondOverviewModel::FLine& Line = Model.Resources.AddDefaulted_GetRef();
		Line.Glyph = EBeyondGlyph::SkillPoint;
		Line.Icon = Art ? LoadTexture(Art->SkillPointIcon) : nullptr;
		Line.Title = LOCTEXT("SkillPoints", "Skill Points");
		Line.Value = FString::FromInt(OwnTree->GetAvailablePoints());
	}
	if (Party)
	{
		FBeyondOverviewModel::FLine& Line = Model.Resources.AddDefaulted_GetRef();
		Line.Glyph = EBeyondGlyph::BondPoint;
		Line.Icon = Art ? LoadTexture(Art->BondPointIcon) : nullptr;
		Line.Title = LOCTEXT("BondPointsResource", "Bond Points");
		Line.Value = FString::FromInt(Party->GetBondPoints());
	}
	if (const UBeyondInventoryComponent* Inventory = PC ? PC->InventoryComponent.Get() : nullptr)
	{
		FBeyondOverviewModel::FLine& Line = Model.Resources.AddDefaulted_GetRef();
		Line.Glyph = EBeyondGlyph::Grid;
		Line.Title = LOCTEXT("BagResource", "Bag");
		Line.Value = FString::Printf(TEXT("%d / %d"), Inventory->GetItemCount(), Inventory->Capacity);
	}

	//~ Loadout
	if (const UBeyondEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr)
	{
		for (const EBeyondItemSlot ItemSlot : { EBeyondItemSlot::Weapon, EBeyondItemSlot::Helm, EBeyondItemSlot::Chest, EBeyondItemSlot::Gauntlets, EBeyondItemSlot::Boots })
		{
			FBeyondItemInstance Worn;
			Model.Loadout.Emplace(ItemSlot, Equipment->GetEquipped(ItemSlot, Worn) ? TOptional<FBeyondItemInstance>(Worn) : TOptional<FBeyondItemInstance>());
		}
	}

	//~ Journey (the quest log comes later): where the party is
	const UBeyondWorldSubsystem* World = UBeyondWorldSubsystem::Get(this);
	Model.QuestKind = LOCTEXT("Journey", "Journey");
	if (const UBeyondRegionDefinition* Region = World ? World->GetCurrentRegion() : nullptr)
	{
		Model.QuestTitle = Region->GetDisplayNameOrId();
		Model.QuestObjective = LOCTEXT("ExploreRegion", "Explore the region and its villages.");
	}
	else
	{
		Model.QuestTitle = LOCTEXT("TheWilds", "The Wandering Dominion");
		Model.QuestObjective = LOCTEXT("FindWay", "Find your way through the wilds.");
	}
	Model.QuestChapter = LOCTEXT("QuestLogSoon", "The quest log opens with the next update");

	//~ Records
	int32 Areas = 0;
	int32 AreasFound = 0;
	if (World)
	{
		for (const FBeyondMapMarker& Marker : World->GetMapMarkers())
		{
			if (Marker.Kind == EBeyondMapMarkerKind::Region || Marker.Kind == EBeyondMapMarkerKind::Village || Marker.Kind == EBeyondMapMarkerKind::Area)
			{
				++Areas;
				AreasFound += Party && Party->IsRegionDiscovered(Marker.Id) ? 1 : 0;
			}
		}
	}
	int32 Bosses = 0;
	for (const TCHAR* BossId : BossIds)
	{
		Bosses += Party && Party->IsBossDefeated(BossId) ? 1 : 0;
	}
	auto AddRecord = [&Model](EBeyondGlyph Glyph, const FText& Title, const FString& Value)
	{
		FBeyondOverviewModel::FLine& Line = Model.Records.AddDefaulted_GetRef();
		Line.Glyph = Glyph;
		Line.Title = Title;
		Line.Value = Value;
	};
	AddRecord(EBeyondGlyph::Hourglass, LOCTEXT("Playtime", "Playtime"), BeyondUI::FormatPlaytime(GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0));
	AddRecord(EBeyondGlyph::Compass, LOCTEXT("Areas", "Areas Discovered"), FString::Printf(TEXT("%d / %d"), AreasFound, Areas));
	AddRecord(EBeyondGlyph::Quests, LOCTEXT("Level", "Party Level"), FString::FromInt(Party ? Party->GetPartyLevel() : 1));
	AddRecord(EBeyondGlyph::Skull, LOCTEXT("Bosses", "Bosses Defeated"), FString::Printf(TEXT("%d / %d"), Bosses, static_cast<int32>(UE_ARRAY_COUNT(BossIds))));
	return Model;
}

//~ Input

int32 UBeyondOverviewWidget::HitTestPartyCard(const FVector2f& Design) const
{
	using namespace BeyondOverviewLocal;
	for (int32 Index = 0; Index < HeaderTabs.Num() && Index < 3; ++Index)
	{
		const FVector2f TopLeft = GetCardTopLeft(Index);
		if (Design.X >= TopLeft.X && Design.X <= TopLeft.X + 200.0f && Design.Y >= TopLeft.Y && Design.Y <= TopLeft.Y + CardHeight)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UBeyondOverviewWidget::HandlePageHover(const FVector2f& Design)
{
	HoveredCard = HitTestPartyCard(Design);
}

bool UBeyondOverviewWidget::HandlePageClick(const FVector2f& Design, const FKey& Button)
{
	const int32 Card = HitTestPartyCard(Design);
	if (Button == EKeys::LeftMouseButton && Card != INDEX_NONE)
	{
		SelectHeaderTab(Card);
		return true;
	}
	return false;
}

void UBeyondOverviewWidget::GetPageKeyHints(TArray<FBeyondKeyHint>& OutHints) const
{
	OutHints.Add({ TEXT("LMB"), LOCTEXT("SelectHint", "Select"), EKeys::LeftMouseButton });
	OutHints.Add({ TEXT("Q  E"), LOCTEXT("SwitchCharacter", "Switch Character"), EKeys::E });
	OutHints.Add({ TEXT("K"), LOCTEXT("SkillTreeHint", "Skill Tree"), EKeys::K });
	OutHints.Add({ TEXT("I"), LOCTEXT("ArmorHint", "Armor"), EKeys::I });
	OutHints.Add({ TEXT("J"), LOCTEXT("QuestsHint", "Quests"), EKeys::J });
}

bool UBeyondOverviewWidget::GetCounter(FBeyondMenuCounter& OutCounter) const
{
	const ABeyondPlayerController* PC = GetBeyondPlayerController();
	if (!PC || !PC->PartyComponent)
	{
		return false;
	}
	OutCounter.Label = LOCTEXT("Resources", "Resources");
	FBeyondMenuCounter::FValue& Bond = OutCounter.Values.AddDefaulted_GetRef();
	Bond.Glyph = EBeyondGlyph::BondPoint;
	Bond.Tint = BeyondUI::Palette().BlueBright;
	Bond.Text = FString::FromInt(PC->PartyComponent->GetBondPoints());
	return true;
}

//~ Painting

void UBeyondOverviewWidget::PaintPage(BeyondUI::FPainter& Paint) const
{
	using namespace BeyondUI;
	using namespace BeyondOverviewLocal;
	const FPalette& P = Palette();
	const FBeyondOverviewModel Model = BuildModel();
	const int32 Active = GetActiveHeaderTab();
	const FLinearColor Accent = GetAccent();

	//~ Party cards
	Paint.Panel(PartyTopLeft, PartySize, 20);
	Paint.SectionHeader(LOCTEXT("Party", "Party").ToString(), PartyTopLeft + FVector2f(18.0f, 14.0f), PartySize.X - 36.0f, 22, 16.0f);
	for (int32 Index = 0; Index < Model.Party.Num() && Index < 3; ++Index)
	{
		const FBeyondOverviewModel::FPartyCard& Card = Model.Party[Index];
		const FVector2f TopLeft = GetCardTopLeft(Index);
		const FVector2f Size(200.0f, CardHeight);
		const bool bSelected = Index == Active;
		Paint.GradientV(TopLeft, Size, FromHex(0x1A2440, 0.95f), FromHex(0x070B16, 0.95f), 22);
		if (Card.Portrait)
		{
			Paint.ImageCover(Card.Portrait, TopLeft, Size, FLinearColor::White, 23, 0.2f);
		}
		else
		{
			const UBeyondCharacterProfile* Profile = GetProfileFor(Index);
			Paint.Glyph(Card.bDuo ? EBeyondGlyph::Eclipse : Profile ? ElementGlyph(Profile->PrimaryElement) : EBeyondGlyph::Star, TopLeft + FVector2f(100.0f, 84.0f), 110.0f,
				WithAlpha(HeaderTabs.IsValidIndex(Index) ? HeaderTabs[Index].Accent : Accent, 0.45f), 23, 2.0f);
		}
		Paint.GradientV(TopLeft + FVector2f(0.0f, Size.Y * 0.55f), FVector2f(Size.X, Size.Y * 0.45f), WithAlpha(P.Backdrop, 0.0f), WithAlpha(P.Backdrop, 0.92f), 24);
		Paint.Text(FString::Printf(TEXT("Lv. %d"), Card.Level), Font(EBeyondFont::Number, 20.0f), TopLeft + FVector2f(14.0f, Size.Y - 62.0f), P.Text, 0.0f, 26);
		Paint.Text(Card.Name.ToString().ToUpper(), Font(EBeyondFont::Heading, 19.0f), TopLeft + FVector2f(14.0f, Size.Y - 36.0f), P.Text, 0.0f, 26);
		Paint.RectOutline(TopLeft, Size, bSelected ? P.GoldBright : WithAlpha(P.Silver, Index == HoveredCard ? 0.8f : 0.4f), bSelected ? 2.0f : 1.0f, 27);
		if (bSelected)
		{
			Paint.Sparkle(TopLeft + FVector2f(Size.X - 4.0f, 4.0f), 10.0f, P.GoldBright, 28);
			Paint.Glow(TopLeft + Size * 0.5f, 150.0f, WithAlpha(P.GoldBright, 0.08f), 21);
		}
	}

	//~ Centre: emblem, name, class, level
	Paint.Panel(CentreTopLeft, CentreSize, 20);
	const UBeyondCharacterProfile* Profile = GetProfileFor(Active);
	const FVector2f EmblemCentre = CentreTopLeft + FVector2f(76.0f, 74.0f);
	if (UTexture2D* Emblem = Profile ? LoadTexture(Profile->Emblem) : nullptr)
	{
		Paint.ImageCentred(Emblem, EmblemCentre, FVector2f(104.0f, 104.0f), FLinearColor::White, 22);
	}
	else
	{
		Paint.Ring(EmblemCentre, 34.0f, WithAlpha(P.Gold, 0.6f), 1.5f, 22);
		Paint.Sparkle(EmblemCentre, 40.0f, P.GoldBright, 22);
	}
	const float NameLeft = CentreTopLeft.X + 140.0f;
	Paint.Text(Model.Name.ToString(), Font(EBeyondFont::Display, 34.0f), FVector2f(NameLeft, CentreTopLeft.Y + 22.0f), P.GoldBright, 0.0f, 24);
	if (!Model.ClassTitle.IsEmpty())
	{
		Paint.Glyph(ElementGlyph(Model.Element), FVector2f(NameLeft + 14.0f, CentreTopLeft.Y + 88.0f), 24.0f, ElementColor(Model.Element), 24, 1.6f);
		Paint.Text(Model.ClassTitle.ToString(), Font(EBeyondFont::Heading, 20.0f), FVector2f(NameLeft + 36.0f, CentreTopLeft.Y + 74.0f), ElementColor(Model.Element), 0.0f, 24);
		Paint.Text(Model.Motto.ToString(), Font(EBeyondFont::Body, 18.0f), FVector2f(NameLeft + 36.0f, CentreTopLeft.Y + 102.0f), P.TextDim, 0.0f, 24);
	}
	const FBeyondLevelInfo& Level = Model.Level;
	const FString LevelText = FString::Printf(TEXT("Lv. %d"), Level.Level);
	Paint.Text(LevelText, Font(EBeyondFont::Number, 34.0f), FVector2f(CentreTopLeft.X + CentreSize.X - 28.0f, CentreTopLeft.Y + 20.0f), P.Text, 1.0f, 24);
	if (Level.ExperienceToNext > 0.0f)
	{
		Paint.Text(FString::Printf(TEXT("%s / %s"), *FormatNumber(FMath::FloorToInt64(Level.Experience)), *FormatNumber(FMath::RoundToInt64(Level.ExperienceToNext))),
			Font(EBeyondFont::Number, 16.0f), FVector2f(CentreTopLeft.X + CentreSize.X - 28.0f, CentreTopLeft.Y + 72.0f), P.TextDim, 1.0f, 24);
		Paint.Bar(FVector2f(CentreTopLeft.X + CentreSize.X - 260.0f, CentreTopLeft.Y + 100.0f), FVector2f(232.0f, 7.0f), Level.Experience / Level.ExperienceToNext,
			P.GoldBright, 24);
	}
	Paint.Separator(FVector2f(CentreTopLeft.X + 24.0f, CentreTopLeft.Y + 138.0f), CentreSize.X - 48.0f, 22, 0.6f);

	//~ Vitals
	const float ColumnTop = CentreTopLeft.Y + 158.0f;
	const float VitalsLeft = CentreTopLeft.X + 28.0f;
	auto PaintVital = [&](float Top, EBeyondGlyph Glyph, const FText& Label, float Value, float Max, const FLinearColor& Fill)
	{
		Paint.Glyph(Glyph, FVector2f(VitalsLeft + 12.0f, Top + 14.0f), 24.0f, P.Text, 24, 1.5f);
		Paint.Text(Label.ToString(), Font(EBeyondFont::Body, 19.0f), FVector2f(VitalsLeft + 36.0f, Top), P.Text, 0.0f, 24);
		Paint.Text(FormatNumber(FMath::RoundToInt64(Max)), Font(EBeyondFont::Number, 24.0f), FVector2f(VitalsLeft + 260.0f, Top - 4.0f), P.Text, 1.0f, 24);
		Paint.Bar(FVector2f(VitalsLeft + 36.0f, Top + 36.0f), FVector2f(224.0f, 9.0f), Max > 0.0f ? Value / Max : 0.0f, Fill, 24, false);
		Paint.Text(FString::Printf(TEXT("%s / %s"), *FormatNumber(FMath::RoundToInt64(Value)), *FormatNumber(FMath::RoundToInt64(Max))), Font(EBeyondFont::Number, 14.0f),
			FVector2f(VitalsLeft + 260.0f, Top + 50.0f), P.TextDim, 1.0f, 24);
	};
	if (IsDuoHeaderTab(Active))
	{
		PaintVital(ColumnTop, EBeyondGlyph::Bond, LOCTEXT("BondMeter", "Bond"), Model.Health, Model.MaxHealth, P.Violet);
	}
	else
	{
		PaintVital(ColumnTop, EBeyondGlyph::Heart, LOCTEXT("HP", "HP"), Model.Health, Model.MaxHealth, P.Health);
		PaintVital(ColumnTop + 84.0f, EBeyondGlyph::Stamina, LOCTEXT("Stamina", "Stamina"), Model.Stamina, Model.MaxStamina, P.Stamina);
	}

	//~ Main stats
	const FVector2f StatsLeft(CentreTopLeft.X + 318.0f, ColumnTop);
	const float StatsTop = Paint.SectionHeader(LOCTEXT("MainStats", "Main Stats").ToString(), StatsLeft, 270.0f, 22, 16.0f);
	BeyondMenuPanels::PaintStats(Paint, Model.Stats, FVector2f(StatsLeft.X, StatsTop), 270.0f, 30.0f, 24, false, 18.0f);

	//~ Elemental affinity: the demigod's element in the middle, the four region elements around it
	const FVector2f AffinityLeft(CentreTopLeft.X + 612.0f, ColumnTop);
	Paint.SectionHeader(LOCTEXT("Affinity", "Elemental Affinity").ToString(), AffinityLeft, 214.0f, 22, 15.0f);
	const FVector2f Hub = AffinityLeft + FVector2f(107.0f, 140.0f);
	Paint.Ring(Hub, 74.0f, WithAlpha(P.Gold, 0.4f), 1.0f, 23);
	Paint.Glow(Hub, 60.0f, WithAlpha(ElementColor(Model.Element), 0.35f), 23);
	Paint.Sparkle(Hub, 22.0f, ElementColor(Model.Element), 25);
	const FVector2f Spots[] = { FVector2f(0.0f, -74.0f), FVector2f(74.0f, 0.0f), FVector2f(0.0f, 74.0f), FVector2f(-74.0f, 0.0f) };
	for (int32 Index = 0; Index < Model.Affinity.Num() && Index < 4; ++Index)
	{
		const EBeyondElement Element = Model.Affinity[Index].Key;
		const FVector2f Centre = Hub + Spots[Index];
		Paint.Line(Hub + Spots[Index] * 0.32f, Centre - Spots[Index].GetSafeNormal() * 26.0f, WithAlpha(P.Gold, 0.4f), 1.0f, 23);
		Paint.Disc(Centre, 26.0f, FMath::Lerp(P.Backdrop, ElementColor(Element), 0.25f), 24);
		Paint.Ring(Centre, 26.0f, ElementColor(Element), 1.5f, 25);
		UBeyondUISubsystem* UI = UBeyondUISubsystem::Get();
		const UBeyondUIArtSet* Art = UI ? UI->GetArtSet() : nullptr;
		const TSoftObjectPtr<UTexture2D>* Icon = Art ? Art->ElementIcons.Find(Element) : nullptr;
		if (UTexture2D* Texture = Icon ? LoadTexture(*Icon) : nullptr)
		{
			Paint.ImageCentred(Texture, Centre, FVector2f(40.0f, 40.0f), FLinearColor::White, 26);
		}
		else
		{
			Paint.Glyph(ElementGlyph(Element), Centre, 28.0f, ElementColor(Element), 26, 1.6f);
		}
		const float Value = Model.Affinity[Index].Value;
		const FVector2f LabelAt = Index == 0 ? Centre + FVector2f(34.0f, -12.0f) : Index == 1 ? Centre + FVector2f(-12.0f, 30.0f)
			: Index == 2 ? Centre + FVector2f(34.0f, -12.0f) : Centre + FVector2f(-12.0f, 30.0f);
		Paint.Text(FormatPercent(Value), Font(EBeyondFont::Number, 14.0f), LabelAt, FMath::IsNearlyZero(Value) ? P.TextFaint : P.Text, 0.0f, 26);
	}

	//~ Active passives
	const float PassivesTop = Paint.SectionHeader(LOCTEXT("ActivePassives", "Active Passives").ToString(), FVector2f(CentreTopLeft.X + 28.0f, CentreTopLeft.Y + 396.0f),
		CentreSize.X - 56.0f, 22, 16.0f);
	if (Model.Passives.IsEmpty())
	{
		Paint.Text(LOCTEXT("NoPassives", "Wear set pieces and learn passive skills to see them here.").ToString(), Font(EBeyondFont::BodyItalic, 18.0f),
			FVector2f(CentreTopLeft.X + 40.0f, PassivesTop + 8.0f), P.TextDim, 0.0f, 24);
	}
	for (int32 Index = 0; Index < Model.Passives.Num() && Index < 4; ++Index)
	{
		const FBeyondOverviewModel::FLine& Line = Model.Passives[Index];
		const FVector2f RowTopLeft(CentreTopLeft.X + 28.0f, PassivesTop + Index * 58.0f);
		const FVector2f RowSize(CentreSize.X - 56.0f, 52.0f);
		Paint.GradientH(RowTopLeft, RowSize, FromHex(0x141D33, 0.85f), FromHex(0x0B1020, 0.6f), 23);
		Paint.RectOutline(RowTopLeft, RowSize, WithAlpha(P.Silver, 0.25f), 1.0f, 24);
		const FVector2f IconCentre = RowTopLeft + FVector2f(30.0f, 26.0f);
		Paint.Disc(IconCentre, 20.0f, FMath::Lerp(P.BlueDeep, Accent, 0.35f), 24);
		Paint.Ring(IconCentre, 20.0f, FMath::Lerp(Accent, FLinearColor::White, 0.4f), 1.2f, 25);
		if (Line.Icon)
		{
			Paint.ImageCentred(Line.Icon, IconCentre, FVector2f(30.0f, 30.0f), FLinearColor::White, 26);
		}
		else
		{
			Paint.Glyph(Line.Glyph, IconCentre, 22.0f, P.Text, 26, 1.4f);
		}
		Paint.Text(Line.Title.ToString(), Font(EBeyondFont::BodyBold, 18.0f), RowTopLeft + FVector2f(64.0f, 3.0f), P.Text, 0.0f, 25);
		Paint.Text(Line.Text.ToString(), Font(EBeyondFont::Body, 16.0f), RowTopLeft + FVector2f(64.0f, 27.0f), P.TextDim, 0.0f, 25);
	}

	//~ Right: Bond status, duo abilities, resources
	const FVector2f BondTopLeft = RightTopLeft;
	const FVector2f BondSize(RightWidth, 214.0f);
	Paint.Panel(BondTopLeft, BondSize, 20);
	float Y = Paint.SectionHeader(LOCTEXT("BondStatus", "Bond Status").ToString(), BondTopLeft + FVector2f(24.0f, 14.0f), RightWidth - 48.0f, 22, 16.0f);
	const FVector2f Crescent(BondTopLeft.X + 66.0f, Y + 40.0f);
	UBeyondUISubsystem* UI = UBeyondUISubsystem::Get();
	const UBeyondUIArtSet* Art = UI ? UI->GetArtSet() : nullptr;
	if (UTexture2D* BondEmblem = Art ? LoadTexture(Art->BondEmblem) : nullptr)
	{
		Paint.ImageCentred(BondEmblem, Crescent, FVector2f(86.0f, 86.0f), FLinearColor::White, 24);
	}
	else
	{
		Paint.Glow(Crescent, 50.0f, WithAlpha(P.Violet, 0.4f), 23);
		Paint.Glyph(EBeyondGlyph::Bond, Crescent, 62.0f, P.GoldBright, 24, 2.2f);
	}
	Paint.Text(LOCTEXT("BondMeterLabel", "Bond Meter").ToString(), Font(EBeyondFont::Heading, 18.0f), FVector2f(BondTopLeft.X + 128.0f, Y + 6.0f), P.Text, 0.0f, 24);
	Paint.Text(FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(Model.BondExperience), FMath::RoundToInt(Model.BondExperienceToNext)), Font(EBeyondFont::Number, 17.0f),
		FVector2f(BondTopLeft.X + RightWidth - 24.0f, Y + 8.0f), P.Text, 1.0f, 24);
	Paint.Bar(FVector2f(BondTopLeft.X + 128.0f, Y + 40.0f), FVector2f(RightWidth - 152.0f, 7.0f),
		Model.BondExperienceToNext > 0.0f ? Model.BondExperience / Model.BondExperienceToNext : 0.0f, P.Blue, 24, false);
	float PerkY = Y + 86.0f;
	for (const FText& Perk : Model.BondPerks)
	{
		Paint.Sparkle(FVector2f(BondTopLeft.X + 34.0f, PerkY + 11.0f), 7.0f, P.BlueBright, 24);
		Paint.Text(Perk.ToString(), Font(EBeyondFont::Body, 17.0f), FVector2f(BondTopLeft.X + 52.0f, PerkY), P.Text, 0.0f, 24);
		PerkY += 24.0f;
	}

	const FVector2f DuoTopLeft = RightTopLeft + FVector2f(0.0f, BondSize.Y + 12.0f);
	const FVector2f DuoSize(RightWidth, 206.0f);
	Paint.Panel(DuoTopLeft, DuoSize, 20);
	Y = Paint.SectionHeader(LOCTEXT("DuoAbilities", "Duo Abilities").ToString(), DuoTopLeft + FVector2f(24.0f, 14.0f), RightWidth - 48.0f, 22, 16.0f);
	for (int32 Index = 0; Index < Model.DuoAbilities.Num() && Index < 3; ++Index)
	{
		const FBeyondOverviewModel::FLine& Line = Model.DuoAbilities[Index];
		const FVector2f Centre(DuoTopLeft.X + 86.0f + Index * 156.0f, Y + 50.0f);
		Paint.Glow(Centre, 70.0f, WithAlpha(P.Violet, 0.35f), 23);
		Paint.Disc(Centre, 40.0f, FMath::Lerp(P.BlueDeep, P.Violet, 0.4f), 24);
		Paint.Ring(Centre, 40.0f, P.BlueBright, 1.8f, 25);
		if (Line.Icon)
		{
			Paint.ImageCentred(Line.Icon, Centre, FVector2f(56.0f, 56.0f), FLinearColor::White, 26);
		}
		else
		{
			Paint.Glyph(Line.Glyph, Centre, 42.0f, P.Text, 26, 1.8f);
		}
		Paint.Text(Line.Title.ToString(), Font(EBeyondFont::Body, 17.0f), FVector2f(Centre.X, Centre.Y + 48.0f), P.Text, 0.5f, 26);
		Paint.Text(Line.Value, Font(EBeyondFont::Number, 16.0f), FVector2f(Centre.X, Centre.Y + 70.0f), P.GoldBright, 0.5f, 26);
	}

	const FVector2f ResourcesTopLeft = DuoTopLeft + FVector2f(0.0f, DuoSize.Y + 12.0f);
	const FVector2f ResourcesSize(RightWidth, CentreTopLeft.Y + CentreSize.Y - ResourcesTopLeft.Y);
	Paint.Panel(ResourcesTopLeft, ResourcesSize, 20);
	Y = Paint.SectionHeader(LOCTEXT("KeyResources", "Key Resources").ToString(), ResourcesTopLeft + FVector2f(24.0f, 14.0f), RightWidth - 48.0f, 22, 16.0f);
	for (const FBeyondOverviewModel::FLine& Line : Model.Resources)
	{
		const FVector2f IconCentre(ResourcesTopLeft.X + 40.0f, Y + 14.0f);
		if (Line.Icon)
		{
			Paint.ImageCentred(Line.Icon, IconCentre, FVector2f(30.0f, 30.0f), FLinearColor::White, 24);
		}
		else
		{
			Paint.Glyph(Line.Glyph, IconCentre, 24.0f, P.GoldBright, 24, 1.6f);
		}
		Paint.Text(Line.Title.ToString(), Font(EBeyondFont::Body, 19.0f), FVector2f(ResourcesTopLeft.X + 66.0f, Y), P.Text, 0.0f, 24);
		Paint.Text(Line.Value, Font(EBeyondFont::Number, 19.0f), FVector2f(ResourcesTopLeft.X + RightWidth - 24.0f, Y), P.Text, 1.0f, 24);
		Y += 32.0f;
	}

	//~ Bottom: loadout, journey, record
	const FVector2f LoadoutTopLeft(300.0f, BottomTop);
	Paint.Panel(LoadoutTopLeft, FVector2f(600.0f, BottomHeight), 20);
	Paint.SectionHeader(LOCTEXT("Loadout", "Equipment Loadout").ToString(), LoadoutTopLeft + FVector2f(22.0f, 12.0f), 556.0f, 22, 16.0f);
	for (int32 Index = 0; Index < Model.Loadout.Num(); ++Index)
	{
		const FVector2f Centre(LoadoutTopLeft.X + 76.0f + Index * 112.0f, LoadoutTopLeft.Y + 100.0f);
		const TOptional<FBeyondItemInstance>& Worn = Model.Loadout[Index].Value;
		FItemFrameStyle Style;
		Style.bEmpty = !Worn.IsSet();
		Style.Glyph = SlotGlyph(Model.Loadout[Index].Key);
		if (Worn.IsSet())
		{
			Style.Tier = Worn->Tier;
			if (const UBeyondItemDefinition* Definition = Worn->GetDefinition())
			{
				Style.Icon = Definition->Icon;
			}
		}
		Paint.ItemFrame(Centre, 74.0f, Style, 24);
		Paint.Text(UBeyondItemLibrary::GetSlotName(Model.Loadout[Index].Key).ToString(), Font(EBeyondFont::Body, 16.0f), FVector2f(Centre.X, Centre.Y + 42.0f), P.TextDim, 0.5f, 26);
	}

	const FVector2f QuestTopLeft(914.0f, BottomTop);
	const FVector2f QuestSize(560.0f, BottomHeight);
	Paint.Panel(QuestTopLeft, QuestSize, 20);
	Paint.SectionHeader(LOCTEXT("QuestProgress", "Quest Progress").ToString(), QuestTopLeft + FVector2f(22.0f, 12.0f), QuestSize.X - 44.0f, 22, 16.0f);
	const FVector2f ThumbTopLeft = QuestTopLeft + FVector2f(22.0f, 50.0f);
	const FVector2f ThumbSize(210.0f, 118.0f);
	Paint.GradientV(ThumbTopLeft, ThumbSize, FromHex(0x1B2A4A), FromHex(0x0A0F1C), 22);
	if (Model.QuestThumbnail)
	{
		Paint.ImageCover(Model.QuestThumbnail, ThumbTopLeft, ThumbSize, FLinearColor::White, 23);
	}
	else
	{
		Paint.Glyph(EBeyondGlyph::Compass, ThumbTopLeft + ThumbSize * 0.5f, 60.0f, WithAlpha(P.Gold, 0.5f), 23, 1.6f);
	}
	Paint.RectOutline(ThumbTopLeft, ThumbSize, WithAlpha(P.Gold, 0.5f), 1.0f, 24);
	const float QuestLeft = ThumbTopLeft.X + ThumbSize.X + 22.0f;
	Paint.Diamond(FVector2f(QuestLeft + 8.0f, ThumbTopLeft.Y + 12.0f), 7.0f, 10.0f, P.GoldBright, 24);
	Paint.Text(Model.QuestKind.ToString(), Font(EBeyondFont::Body, 16.0f), FVector2f(QuestLeft + 26.0f, ThumbTopLeft.Y), P.GoldBright, 0.0f, 24);
	Paint.TextFit(Model.QuestTitle.ToString(), EBeyondFont::Heading, 19.0f, FVector2f(QuestLeft + 26.0f, ThumbTopLeft.Y + 24.0f), QuestSize.X - 290.0f, P.Text, 0.0f, 24);
	Paint.Paragraph(Model.QuestObjective.ToString(), Font(EBeyondFont::Body, 16.0f), FVector2f(QuestLeft + 26.0f, ThumbTopLeft.Y + 56.0f), QuestSize.X - 290.0f, 20.0f,
		P.TextDim, 24, 2);
	Paint.Text(Model.QuestChapter.ToString(), Font(EBeyondFont::BodyItalic, 15.0f), FVector2f(ThumbTopLeft.X, ThumbTopLeft.Y + ThumbSize.Y + 4.0f), P.TextDim, 0.0f, 24);

	const FVector2f RecordTopLeft(1488.0f, BottomTop);
	const FVector2f RecordSize(402.0f, BottomHeight);
	Paint.Panel(RecordTopLeft, RecordSize, 20);
	Y = Paint.SectionHeader(LOCTEXT("Record", "Playtime & Record").ToString(), RecordTopLeft + FVector2f(22.0f, 12.0f), RecordSize.X - 44.0f, 22, 16.0f);
	for (const FBeyondOverviewModel::FLine& Line : Model.Records)
	{
		Paint.Glyph(Line.Glyph, FVector2f(RecordTopLeft.X + 34.0f, Y + 12.0f), 20.0f, P.Text, 24, 1.4f);
		Paint.Text(Line.Title.ToString(), Font(EBeyondFont::Body, 17.0f), FVector2f(RecordTopLeft.X + 56.0f, Y), P.Text, 0.0f, 24);
		Paint.Text(Line.Value, Font(EBeyondFont::Number, 17.0f), FVector2f(RecordTopLeft.X + RecordSize.X - 22.0f, Y), P.Text, 1.0f, 24);
		Y += 29.0f;
	}
}

#undef LOCTEXT_NAMESPACE
