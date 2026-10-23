// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondSkillTreeWidget.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "Characters/BeyondCharacterBase.h"
#include "Engine/Texture2D.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Progression/BeyondSkillTreeComponent.h"
#include "Sound/SoundBase.h"
#include "UI/BeyondCharacterProfile.h"
#include "UI/BeyondMenuPanels.h"
#include "UI/BeyondUIArt.h"

#define LOCTEXT_NAMESPACE "BeyondSkillTree"

namespace BeyondSkillTreeLocal
{
	// The graph's area on the page
	constexpr float GraphCentreX = 1040.0f;
	constexpr float GraphTop = 232.0f;
	constexpr float GraphBottom = 900.0f;
	constexpr float GraphHalfWidth = 330.0f;
	constexpr float ResetWindow = 2.5f;

	// The right column
	const FVector2f StatsTopLeft(1404.0f, 118.0f);
	const FVector2f StatsSize(486.0f, 382.0f);
	const FVector2f CardTopLeft(1404.0f, 514.0f);
	const FVector2f CardSize(486.0f, 474.0f);

	bool IsActiveNode(const FBeyondSkillNode& Node)
	{
		return Node.Effects.ContainsByPredicate([](const FBeyondSkillEffect& Effect)
		{
			return Effect.Type == EBeyondSkillEffectType::GrantAbility || Effect.Type == EBeyondSkillEffectType::DuoPower;
		});
	}

	bool IsUpgradeNode(const FBeyondSkillNode& Node)
	{
		return Node.Effects.ContainsByPredicate([](const FBeyondSkillEffect& Effect) { return Effect.Type == EBeyondSkillEffectType::AbilityRank; });
	}
}

UBeyondSkillTreeWidget::UBeyondSkillTreeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The SkillTreeSystem pack's icons for stat nodes, plus the ability bar's for health
	StatIcons.Add(EBeyondSkillStat::Strength, TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SkillTreeSystem/Textures/T_Sword.T_Sword"))));
	StatIcons.Add(EBeyondSkillStat::Arcana, TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SkillTreeSystem/Textures/T_MagicWand.T_MagicWand"))));
	StatIcons.Add(EBeyondSkillStat::MaxStamina, TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SkillTreeSystem/Textures/T_PersonSimpleRun.T_PersonSimpleRun"))));
	StatIcons.Add(EBeyondSkillStat::MaxHealth, TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/Blueprints/Widgets/Images/health-increase.health-increase"))));
	UnlockSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/energy-charge-up.energy-charge-up")));
}

void UBeyondSkillTreeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LoadArt();
	RefreshTabs();
}

void UBeyondSkillTreeWidget::LoadArt()
{
	for (const TPair<EBeyondSkillStat, TSoftObjectPtr<UTexture2D>>& Pair : StatIcons)
	{
		if (UTexture2D* Texture = Pair.Value.IsNull() ? nullptr : Pair.Value.LoadSynchronous())
		{
			LoadedTextures.AddUnique(Texture);
		}
	}
}

void UBeyondSkillTreeWidget::RefreshTabs()
{
	const UBeyondSkillTreeComponent* Previous = GetActiveTree();
	Tabs.Reset();

	const ABeyondPlayerController* PC = GetBeyondPlayerController();
	if (!PC || !PC->PartyComponent)
	{
		return;
	}

	// Demigods in duo order (Conduit, Striker, anyone else), then the shared duo tree: the same order as the header
	for (ABeyondCharacterBase* Member : GetPartyInDuoOrder())
	{
		if (UBeyondSkillTreeComponent* Tree = Member->GetSkillTreeComponent())
		{
			FTreeTab& Tab = Tabs.AddDefaulted_GetRef();
			Tab.Title = Member->GetCharacterDisplayName();
			Tab.Tree = Tree;
			Tab.Character = Member;
		}
	}
	if (PC->DuoSkillTree && PC->DuoSkillTree->Tree)
	{
		FTreeTab& Tab = Tabs.AddDefaulted_GetRef();
		Tab.Title = LOCTEXT("DuoTab", "Duo");
		Tab.Tree = PC->DuoSkillTree.Get();
	}

	ActiveTab = 0;
	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		if (Tabs[Index].Tree.Get() == Previous)
		{
			ActiveTab = Index;
		}
	}
}

void UBeyondSkillTreeWidget::SelectTab(int32 Index)
{
	if (Tabs.IsValidIndex(Index) && Index != ActiveTab)
	{
		ActiveTab = Index;
		CancelHold();
		HoveredNode = NAME_None;
		SelectedNode = NAME_None;
		ResetArmedAge = -1.0f;
	}
}

int32 UBeyondSkillTreeWidget::FindTabFor(const AActor* Character) const
{
	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		if (Character && Tabs[Index].Character.Get() == Character)
		{
			return Index;
		}
	}
	return 0;
}

UBeyondSkillTreeComponent* UBeyondSkillTreeWidget::GetActiveTree() const
{
	return Tabs.IsValidIndex(ActiveTab) ? Tabs[ActiveTab].Tree.Get() : nullptr;
}

const FBeyondSkillNode* UBeyondSkillTreeWidget::FindActiveNode(FName NodeId) const
{
	const UBeyondSkillTreeComponent* Tree = GetActiveTree();
	return Tree && Tree->Tree && !NodeId.IsNone() ? Tree->Tree->FindNode(NodeId) : nullptr;
}

bool UBeyondSkillTreeWidget::IsDuoLoadout(const FBeyondSkillNode& Node) const
{
	const UBeyondDuoSkillTreeComponent* Duo = Cast<UBeyondDuoSkillTreeComponent>(GetActiveTree());
	if (!Duo || Duo->GetRank(Node.Id) <= 0)
	{
		return false;
	}
	const TSubclassOf<UGameplayAbility> Loadout = Duo->GetDuoLoadout();
	return Node.Effects.ContainsByPredicate([&Loadout](const FBeyondSkillEffect& Effect)
	{
		return Effect.Type == EBeyondSkillEffectType::DuoPower && Effect.Ability == Loadout;
	});
}

UTexture2D* UBeyondSkillTreeWidget::GetNodeIcon(const FBeyondSkillNode& Node) const
{
	if (Node.Icon)
	{
		return Node.Icon;
	}
	for (const FBeyondSkillEffect& Effect : Node.Effects)
	{
		if (Effect.Ability)
		{
			if (const UBeyondGameplayAbility* Ability = Cast<UBeyondGameplayAbility>(Effect.Ability->GetDefaultObject()); Ability && Ability->Icon)
			{
				return Ability->Icon;
			}
		}
		if (Effect.Type == EBeyondSkillEffectType::Stat)
		{
			if (const TSoftObjectPtr<UTexture2D>* Soft = StatIcons.Find(Effect.Stat))
			{
				if (UTexture2D* Texture = Soft->Get())
				{
					return Texture;
				}
			}
		}
	}
	return nullptr;
}

FName UBeyondSkillTreeWidget::GetDetailNode() const
{
	if (FindActiveNode(HoveredNode))
	{
		return HoveredNode;
	}
	if (FindActiveNode(SelectedNode))
	{
		return SelectedNode;
	}
	const UBeyondSkillTreeComponent* Tree = GetActiveTree();
	if (!Tree || !Tree->Tree || Tree->Tree->Nodes.IsEmpty())
	{
		return NAME_None;
	}
	for (const FBeyondSkillNode& Node : Tree->Tree->Nodes)
	{
		FText Unused;
		if (Tree->CanUnlock(Node.Id, Unused))
		{
			return Node.Id;
		}
	}
	return Tree->Tree->Nodes[0].Id;
}

void UBeyondSkillTreeWidget::SelectNode(FName NodeId)
{
	if (FindActiveNode(NodeId))
	{
		SelectedNode = NodeId;
	}
}

FVector2f UBeyondSkillTreeWidget::GetNodeDesignCenter(FName NodeId) const
{
	using namespace BeyondSkillTreeLocal;
	const UBeyondSkillTreeComponent* Tree = GetActiveTree();
	const FBeyondSkillNode* Node = FindActiveNode(NodeId);
	if (!Tree || !Tree->Tree || !Node)
	{
		return FVector2f(-1000.0f, -1000.0f);
	}
	// Fit the tree's grid (columns from the centre, rows from the top) into the graph area, centred both ways
	float MaxRow = 0.0f;
	float MaxColumn = 0.0f;
	for (const FBeyondSkillNode& Other : Tree->Tree->Nodes)
	{
		MaxRow = FMath::Max(MaxRow, static_cast<float>(Other.Position.Y));
		MaxColumn = FMath::Max(MaxColumn, FMath::Abs(static_cast<float>(Other.Position.X)));
	}
	const float StepX = MaxColumn > 0.0f ? FMath::Min(175.0f, (GraphHalfWidth - NodeRadius - 20.0f) / MaxColumn) : 0.0f;
	const float Available = GraphBottom - GraphTop - NodeRadius * 2.0f - 30.0f;
	const float StepY = MaxRow > 0.0f ? FMath::Min(150.0f, Available / MaxRow) : 0.0f;
	const float Top = GraphTop + NodeRadius + (Available - MaxRow * StepY) * 0.5f;
	return FVector2f(GraphCentreX + static_cast<float>(Node->Position.X) * StepX, Top + static_cast<float>(Node->Position.Y) * StepY);
}

FName UBeyondSkillTreeWidget::HitTestNode(const FVector2f& Design) const
{
	const UBeyondSkillTreeComponent* Tree = GetActiveTree();
	if (!Tree || !Tree->Tree)
	{
		return NAME_None;
	}
	for (const FBeyondSkillNode& Node : Tree->Tree->Nodes)
	{
		if (FVector2f::Distance(Design, GetNodeDesignCenter(Node.Id)) <= NodeRadius + 6.0f)
		{
			return Node.Id;
		}
	}
	return NAME_None;
}

//~ Actions

bool UBeyondSkillTreeWidget::BeginHold(FName NodeId)
{
	UBeyondSkillTreeComponent* Tree = GetActiveTree();
	FText Reason;
	if (!Tree || !Tree->CanUnlock(NodeId, Reason))
	{
		ShowMessage(Reason.IsEmpty() ? LOCTEXT("CannotUnlock", "Can't unlock that yet") : Reason, true);
		PlayDeniedSound();
		HoldNode = NAME_None;
		return false;
	}
	SelectedNode = NodeId;
	HoldNode = NodeId;
	HoldTime = 0.0f;
	return true;
}

void UBeyondSkillTreeWidget::CancelHold()
{
	HoldNode = NAME_None;
	HoldTime = 0.0f;
}

bool UBeyondSkillTreeWidget::EquipDuoPowerFrom(FName NodeId)
{
	UBeyondDuoSkillTreeComponent* Duo = Cast<UBeyondDuoSkillTreeComponent>(GetActiveTree());
	const FBeyondSkillNode* Node = FindActiveNode(NodeId);
	if (!Duo || !Node)
	{
		return false;
	}

	// A duo-power node puts its power on G; an upgrade node of an available power (Heaven's Wrath) puts that one back
	const TArray<TSubclassOf<UGameplayAbility>> Available = Duo->GetAvailableDuoPowers();
	for (const FBeyondSkillEffect& Effect : Node->Effects)
	{
		const bool bPowerNode = Effect.Type == EBeyondSkillEffectType::DuoPower;
		const bool bUpgradeNode = Effect.Type == EBeyondSkillEffectType::AbilityRank;
		if ((bPowerNode || bUpgradeNode) && Effect.Ability && Available.Contains(Effect.Ability))
		{
			if (Duo->SetDuoLoadout(Effect.Ability))
			{
				ShowMessage(FText::Format(LOCTEXT("OnDuoSlot", "{0} is on G"), Node->DisplayName));
				PlayUISound(UnlockSound);
				return true;
			}
		}
		else if (bPowerNode)
		{
			ShowMessage(LOCTEXT("DuoLocked", "Unlock this duo power first"), true);
			return false;
		}
	}
	return false;
}

void UBeyondSkillTreeWidget::HandleResetKey()
{
	UBeyondSkillTreeComponent* Tree = GetActiveTree();
	if (!Tree)
	{
		return;
	}
	const int32 Spent = Tree->GetSpentPoints();
	if (Spent <= 0)
	{
		ShowMessage(LOCTEXT("NothingToReset", "Nothing to reset"), true);
		return;
	}
	if (ResetArmedAge >= 0.0f && ResetArmedAge < BeyondSkillTreeLocal::ResetWindow)
	{
		const int32 Refunded = Tree->ResetTree();
		ResetArmedAge = -1.0f;
		ShowMessage(FText::Format(LOCTEXT("Refunded", "Tree reset: {0} {1} refunded"), FText::AsNumber(Refunded), Tree->GetCurrencyName()));
		return;
	}
	ResetArmedAge = 0.0f;
	ShowMessage(FText::Format(LOCTEXT("ConfirmReset", "Press R again to reset this tree ({0} {1} back)"), FText::AsNumber(Spent), Tree->GetCurrencyName()), true);
}

void UBeyondSkillTreeWidget::CloseMenu()
{
	CancelHold();
	Super::CloseMenu();
}

void UBeyondSkillTreeWidget::AdvancePage(float DeltaSeconds)
{
	if (ResetArmedAge >= 0.0f)
	{
		ResetArmedAge += DeltaSeconds;
		if (ResetArmedAge > BeyondSkillTreeLocal::ResetWindow)
		{
			ResetArmedAge = -1.0f;
		}
	}
	if (FlashAge >= 0.0f)
	{
		FlashAge += DeltaSeconds;
		if (FlashAge > 0.6f)
		{
			FlashAge = -1.0f;
		}
	}

	if (HoldNode.IsNone())
	{
		return;
	}
	HoldTime += DeltaSeconds;
	if (HoldTime < HoldDuration)
	{
		return;
	}

	const FName Unlocking = HoldNode;
	CancelHold();
	UBeyondSkillTreeComponent* Tree = GetActiveTree();
	const FBeyondSkillNode* Node = FindActiveNode(Unlocking);
	if (Tree && Node && Tree->Unlock(Unlocking))
	{
		FlashNode = Unlocking;
		FlashAge = 0.0f;
		PlayUISound(UnlockSound, 1.0f);
		const int32 Rank = Tree->GetRank(Unlocking);
		ShowMessage(Node->MaxRank > 1
			? FText::Format(LOCTEXT("UnlockedRank", "{0}: rank {1} of {2}"), Node->DisplayName, FText::AsNumber(Rank), FText::AsNumber(Node->MaxRank))
			: FText::Format(LOCTEXT("Unlocked", "{0} unlocked"), Node->DisplayName));
	}
	else
	{
		FText Reason;
		if (Tree)
		{
			Tree->CanUnlock(Unlocking, Reason);
		}
		ShowMessage(Reason, true);
		PlayDeniedSound();
	}
}

//~ Input

bool UBeyondSkillTreeWidget::HandlePageKey(const FKey& Key)
{
	if (Key == EKeys::R)
	{
		HandleResetKey();
		return true;
	}
	if (Key == EKeys::F || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		BeginHold(GetDetailNode());
		return true;
	}
	if (Key == EKeys::G)
	{
		EquipDuoPowerFrom(GetDetailNode());
		return true;
	}
	return false;
}

bool UBeyondSkillTreeWidget::HandlePageKeyUp(const FKey& Key)
{
	if (Key == EKeys::F || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		CancelHold();
		return true;
	}
	return false;
}

bool UBeyondSkillTreeWidget::HandlePageClick(const FVector2f& Design, const FKey& Button)
{
	const FName Node = HitTestNode(Design);
	if (Button == EKeys::LeftMouseButton)
	{
		if (!Node.IsNone())
		{
			SelectedNode = Node;
			BeginHold(Node);
		}
		return true;
	}
	if (Button == EKeys::RightMouseButton && !Node.IsNone())
	{
		EquipDuoPowerFrom(Node);
		return true;
	}
	return false;
}

bool UBeyondSkillTreeWidget::HandlePageMouseUp(const FVector2f& Design, const FKey& Button)
{
	if (Button == EKeys::LeftMouseButton)
	{
		CancelHold();
		return true;
	}
	return false;
}

void UBeyondSkillTreeWidget::HandlePageHover(const FVector2f& Design)
{
	HoveredNode = HitTestNode(Design);
	if (!HoldNode.IsNone() && HoveredNode != HoldNode && !HoveredNode.IsNone())
	{
		CancelHold();
	}
}

void UBeyondSkillTreeWidget::GetPageKeyHints(TArray<FBeyondKeyHint>& OutHints) const
{
	OutHints.Add({ TEXT("LMB"), LOCTEXT("SelectNode", "Select Node"), EKeys::LeftMouseButton });
	OutHints.Add({ TEXT("F"), LOCTEXT("UpgradeHold", "Upgrade (Hold)"), EKeys::F });
	OutHints.Add({ TEXT("R"), LOCTEXT("ResetRefund", "Reset (Refund)"), EKeys::R });
	if (Cast<UBeyondDuoSkillTreeComponent>(GetActiveTree()))
	{
		OutHints.Add({ TEXT("G"), LOCTEXT("PutOnG", "Put on Duo Slot"), EKeys::G });
	}
	OutHints.Add({ TEXT("Q  E"), LOCTEXT("SwitchCharacter", "Switch Character"), EKeys::E });
}

bool UBeyondSkillTreeWidget::GetCounter(FBeyondMenuCounter& OutCounter) const
{
	const UBeyondSkillTreeComponent* Tree = GetActiveTree();
	if (!Tree)
	{
		return false;
	}
	UBeyondUISubsystem* UI = UBeyondUISubsystem::Get();
	const UBeyondUIArtSet* Art = UI ? UI->GetArtSet() : nullptr;
	const bool bDuo = Cast<UBeyondDuoSkillTreeComponent>(Tree) != nullptr;
	OutCounter.Label = Tree->GetCurrencyName();
	FBeyondMenuCounter::FValue& Value = OutCounter.Values.AddDefaulted_GetRef();
	Value.Text = FString::FromInt(Tree->GetAvailablePoints());
	Value.Icon = Art ? LoadTexture(bDuo ? Art->BondPointIcon : Art->SkillPointIcon) : nullptr;
	Value.Glyph = bDuo ? EBeyondGlyph::BondPoint : EBeyondGlyph::SkillPoint;
	Value.Tint = BeyondUI::Palette().GoldBright;
	return true;
}

//~ Painting

void UBeyondSkillTreeWidget::PaintPage(BeyondUI::FPainter& Paint) const
{
	using namespace BeyondUI;
	using namespace BeyondSkillTreeLocal;
	const FPalette& P = Palette();
	const UBeyondSkillTreeComponent* Tree = GetActiveTree();
	if (!Tree || !Tree->Tree)
	{
		Paint.Text(LOCTEXT("NoTree", "No skill tree assigned: run Scripts/Migration/migrate_pass8.py").ToString(), Font(EBeyondFont::Body, 22.0f),
			FVector2f(1100.0f, 500.0f), P.Minus, 0.5f, 30);
		return;
	}
	const UBeyondSkillTreeAsset& Asset = *Tree->Tree;
	const UBeyondCharacterProfile* Profile = GetProfileFor(ActiveTab);
	const FLinearColor Accent = GetAccent();

	//~ The demigod's art on the left, fading into the page
	const FVector2f ArtTopLeft(300.0f, 118.0f);
	const FVector2f ArtSize(480.0f, 872.0f);
	if (UTexture2D* FullArt = Profile ? LoadTexture(Profile->FullArt) : nullptr)
	{
		Paint.ImageCover(FullArt, ArtTopLeft, ArtSize, FLinearColor(0.75f, 0.8f, 0.95f, 0.95f), 20, 0.15f);
		Paint.GradientH(ArtTopLeft + FVector2f(ArtSize.X * 0.55f, 0.0f), FVector2f(ArtSize.X * 0.45f, ArtSize.Y), WithAlpha(P.Backdrop, 0.0f), WithAlpha(P.Backdrop, 0.95f), 21);
	}
	else if (Profile)
	{
		Paint.Glyph(ElementGlyph(Profile->PrimaryElement), FVector2f(545.0f, 560.0f), 300.0f, WithAlpha(Accent, 0.1f), 20, 3.0f);
	}

	//~ Title: emblem, tree name, motto
	const FVector2f EmblemCentre(736.0f, 162.0f);
	if (UTexture2D* Emblem = Profile ? LoadTexture(Profile->Emblem) : nullptr)
	{
		Paint.ImageCentred(Emblem, EmblemCentre, FVector2f(84.0f, 84.0f), FLinearColor::White, 22);
	}
	else
	{
		Paint.Ring(EmblemCentre, 30.0f, WithAlpha(P.Gold, 0.7f), 1.5f, 22);
		Paint.Sparkle(EmblemCentre, 34.0f, P.GoldBright, 22);
	}
	Paint.Text(Asset.TreeName.ToString(), Font(EBeyondFont::Display, 38.0f), FVector2f(790.0f, 122.0f), P.GoldBright, 0.0f, 24);
	const FText Motto = Profile && !Profile->Motto.IsEmpty() ? Profile->Motto : Asset.Subtitle;
	Paint.Text(Motto.ToString(), Font(EBeyondFont::Body, 21.0f), FVector2f(792.0f, 174.0f), P.TextDim, 0.0f, 24);

	//~ The rune circle under the graph
	const float GraphMidY = (GraphTop + GraphBottom) * 0.5f + 10.0f;
	Paint.RuneCircle(FVector2f(GraphCentreX, GraphMidY), 345.0f, WithAlpha(P.Gold, 0.55f), 20);

	//~ Connections, under the nodes
	for (const FBeyondSkillNode& Node : Asset.Nodes)
	{
		const FVector2f To = GetNodeDesignCenter(Node.Id);
		for (const FName& Required : Node.Requires)
		{
			if (!Asset.FindNode(Required))
			{
				continue;
			}
			const FVector2f From = GetNodeDesignCenter(Required);
			const FVector2f Direction = (To - From).GetSafeNormal();
			const FVector2f A = From + Direction * NodeRadius;
			const FVector2f B = To - Direction * NodeRadius;
			const bool bFromTaken = Tree->GetRank(Required) > 0;
			const bool bToTaken = Tree->GetRank(Node.Id) > 0;
			if (bFromTaken && bToTaken)
			{
				Paint.Line(A, B, WithAlpha(Accent, 0.3f), 10.0f, 24);
				Paint.Line(A, B, FMath::Lerp(Accent, FLinearColor::White, 0.35f), 2.5f, 25);
			}
			else if (bFromTaken)
			{
				Paint.Line(A, B, WithAlpha(P.Gold, 0.6f), 2.0f, 25);
			}
			else
			{
				Paint.Line(A, B, WithAlpha(P.Silver, 0.2f), 1.5f, 25);
			}
		}
	}

	//~ Nodes
	for (const FBeyondSkillNode& Node : Asset.Nodes)
	{
		PaintNode(Paint, *Tree, Node);
	}

	PaintSidePanels(Paint, *Tree);
}

void UBeyondSkillTreeWidget::PaintNode(const BeyondUI::FPainter& Paint, const UBeyondSkillTreeComponent& Tree, const FBeyondSkillNode& Node) const
{
	using namespace BeyondUI;
	const FPalette& P = Palette();
	const FLinearColor Accent = GetAccent();
	const FVector2f C = GetNodeDesignCenter(Node.Id);
	const float R = NodeRadius;
	const EBeyondSkillNodeState State = Tree.GetNodeState(Node.Id);
	const int32 Rank = Tree.GetRank(Node.Id);
	FText Unused;
	const bool bCanBuy = Tree.CanUnlock(Node.Id, Unused);
	const bool bSelected = Node.Id == SelectedNode || (SelectedNode.IsNone() && Node.Id == GetDetailNode() && HoveredNode.IsNone());
	const float Pulse = 0.5f + 0.5f * FMath::Sin(AnimTime * 4.0f);

	if (State == EBeyondSkillNodeState::Locked)
	{
		Paint.Diamond(C, R * 0.95f, R * 0.95f, FromHex(0x0C1220, 0.96f), 30);
		Paint.DiamondOutline(C, R * 0.95f, R * 0.95f, WithAlpha(P.Silver, 0.5f), 1.5f, 31);
		Paint.DiamondOutline(C, R * 0.78f, R * 0.78f, WithAlpha(P.Silver, 0.16f), 1.0f, 31);
		Paint.Glyph(EBeyondGlyph::Lock, C, R * 0.8f, WithAlpha(P.Silver, 0.85f), 32, 1.6f);
	}
	else
	{
		const bool bTaken = Rank > 0;
		if (bTaken)
		{
			Paint.Glow(C, R * 2.5f, WithAlpha(Accent, 0.42f), 28);
			Paint.Disc(C, R, FMath::Lerp(P.BlueDeep, Accent, 0.55f), 30);
			Paint.Disc(C, R * 0.82f, FMath::Lerp(P.Backdrop, Accent, 0.35f), 30);
			Paint.Ring(C, R, FMath::Lerp(Accent, FLinearColor::White, 0.45f), 2.5f, 31);
			if (State == EBeyondSkillNodeState::Maxed)
			{
				Paint.Ring(C, R + 5.0f, WithAlpha(P.GoldBright, 0.75f), 1.5f, 31);
			}
		}
		else
		{
			Paint.Glow(C, R * 2.2f, WithAlpha(P.GoldBright, bCanBuy ? 0.18f + 0.22f * Pulse : 0.1f), 28);
			Paint.Disc(C, R, FromHex(0x0D1426, 0.98f), 30);
			Paint.Ring(C, R, WithAlpha(P.GoldBright, bCanBuy ? 0.95f : 0.5f), 2.5f, 31);
			Paint.Ring(C, R + 5.0f, WithAlpha(P.Gold, 0.35f), 1.0f, 31);
		}
		if (UTexture2D* Icon = GetNodeIcon(Node))
		{
			Paint.ImageCentred(Icon, C, FVector2f(R, R) * 1.3f, WithAlpha(FLinearColor::White, bTaken ? 1.0f : 0.8f), 32);
		}
		else
		{
			const FString Letter = Node.DisplayName.ToString().Left(1).ToUpper();
			const FSlateFontInfo LetterFont = Font(EBeyondFont::Display, R * 0.8f);
			Paint.Text(Letter, LetterFont, C - FVector2f(0.0f, MeasureText(Letter, LetterFont).Y * 0.5f), P.Text, 0.5f, 32);
		}
	}

	// Selection, hover, hold progress, unlock flash, the duo power on G
	if (bSelected)
	{
		Paint.Ring(C, R + 9.0f, P.GoldBright, 2.0f, 33);
		Paint.Diamond(C + FVector2f(R * 0.72f, -R * 0.72f), 6.0f, 6.0f, P.GoldBright, 34);
	}
	if (Node.Id == HoveredNode)
	{
		Paint.Ring(C, R + 9.0f, WithAlpha(FLinearColor::White, 0.55f), 1.5f, 33);
	}
	if (Node.Id == HoldNode && HoldDuration > 0.0f)
	{
		Paint.Ring(C, R + 10.0f, P.GoldBright, 5.0f, 34, HoldTime / HoldDuration);
	}
	if (Node.Id == FlashNode && FlashAge >= 0.0f)
	{
		const float Alpha = FlashAge / 0.6f;
		Paint.Ring(C, R + 6.0f + 44.0f * Alpha, WithAlpha(P.GoldBright, 1.0f - Alpha), 4.0f * (1.0f - Alpha) + 1.0f, 34);
	}
	if (IsDuoLoadout(Node))
	{
		Paint.KeyCap(TEXT("G"), C + FVector2f(R * 0.5f, -R * 1.2f), 35, 22.0f);
	}

	// Rank pill and name below
	const FString RankText = FString::Printf(TEXT("%d/%d"), Rank, Node.MaxRank);
	const FSlateFontInfo RankFont = Font(EBeyondFont::Number, 15.0f);
	const FVector2f RankSize = MeasureText(RankText, RankFont);
	const FVector2f PillTopLeft(C.X - RankSize.X * 0.5f - 9.0f, C.Y + R - 9.0f);
	const FVector2f PillSize(RankSize.X + 18.0f, 21.0f);
	Paint.Rounded(PillTopLeft, PillSize, 4.0f, FromHex(0x070B16, 0.95f), 35);
	Paint.RectOutline(PillTopLeft, PillSize, WithAlpha(Rank > 0 ? P.GoldBright : P.GoldDim, 0.85f), 1.0f, 35);
	Paint.Text(RankText, RankFont, FVector2f(C.X, PillTopLeft.Y + (PillSize.Y - RankSize.Y) * 0.5f), State == EBeyondSkillNodeState::Maxed ? P.GoldBright : P.Text,
		0.5f, 36, false);
	Paint.Text(Node.DisplayName.ToString(), Font(EBeyondFont::Body, 19.0f), FVector2f(C.X, C.Y + R + 14.0f),
		State == EBeyondSkillNodeState::Locked ? P.TextDim : P.Text, 0.5f, 36);
}

void UBeyondSkillTreeWidget::PaintSidePanels(const BeyondUI::FPainter& Paint, const UBeyondSkillTreeComponent& Tree) const
{
	using namespace BeyondUI;
	using namespace BeyondSkillTreeLocal;
	const FPalette& P = Palette();

	//~ Level and combat stats
	Paint.Panel(StatsTopLeft, StatsSize, 40);
	const float Inner = StatsSize.X - 52.0f;
	const FVector2f Left = StatsTopLeft + FVector2f(26.0f, 18.0f);
	const ABeyondCharacterBase* Character = Tabs.IsValidIndex(ActiveTab) ? Tabs[ActiveTab].Character.Get() : nullptr;
	if (Character)
	{
		const float LevelHeight = BeyondMenuPanels::PaintLevel(Paint, BeyondMenuPanels::GetLevelInfo(Character), Left, Inner, 42);
		const float StatsTop = Paint.SectionHeader(LOCTEXT("CombatStats", "Combat Stats").ToString(), Left + FVector2f(0.0f, LevelHeight + 6.0f), Inner, 42);
		BeyondMenuPanels::PaintStats(Paint, BeyondMenuPanels::GetCombatStats(Character), FVector2f(Left.X, StatsTop), Inner, 28.0f, 42, false, 18.0f);
	}
	else
	{
		// The duo tree: party level, Bond Points and what the tree adds to the Bond meter
		FBeyondLevelInfo Party;
		Party.Level = Tree.GetTreeLevel();
		Party.ExperienceToNext = -1.0f;
		const float LevelHeight = BeyondMenuPanels::PaintLevel(Paint, Party, Left, Inner, 42, LOCTEXT("PartyLevel", "Party Level"));
		const float StatsTop = Paint.SectionHeader(LOCTEXT("DuoStats", "Bond").ToString(), Left + FVector2f(0.0f, LevelHeight + 6.0f), Inner, 42);
		TArray<FBeyondStatRow> Rows;
		auto AddRow = [&Rows](EBeyondGlyph Glyph, const FText& Name, float Value, bool bPercent)
		{
			FBeyondStatRow& Row = Rows.AddDefaulted_GetRef();
			Row.Glyph = Glyph;
			Row.Name = Name;
			Row.Value = Value;
			Row.bPercent = bPercent;
		};
		AddRow(EBeyondGlyph::BondPoint, LOCTEXT("BondPoints", "Bond Points"), Tree.GetAvailablePoints(), false);
		AddRow(EBeyondGlyph::Up, LOCTEXT("BondGain", "Bond Gain"), 100.0f * Tree.GetEffectTotal(EBeyondSkillEffectType::BondGain), true);
		AddRow(EBeyondGlyph::Bond, LOCTEXT("BondEcho", "Bond Echo"), 100.0f * Tree.GetEffectTotal(EBeyondSkillEffectType::BondEcho), true);
		BeyondMenuPanels::PaintStats(Paint, Rows, FVector2f(Left.X, StatsTop), Inner, 28.0f, 42, false, 18.0f);
	}

	//~ The selected node's card
	Paint.Panel(CardTopLeft, CardSize, 40);
	if (const FBeyondSkillNode* Node = FindActiveNode(GetDetailNode()))
	{
		PaintNodeCard(Paint, Tree, *Node, CardTopLeft, CardSize);
	}
}

void UBeyondSkillTreeWidget::PaintNodeCard(const BeyondUI::FPainter& Paint, const UBeyondSkillTreeComponent& Tree, const FBeyondSkillNode& Node,
	const FVector2f& TopLeft, const FVector2f& Size) const
{
	using namespace BeyondUI;
	const FPalette& P = Palette();
	const FLinearColor Accent = GetAccent();
	const int32 Rank = Tree.GetRank(Node.Id);
	const float Left = TopLeft.X + 24.0f;
	const float Width = Size.X - 48.0f;

	// Banner: icon disc, name, rank, tags
	const FVector2f IconCentre(Left + 44.0f, TopLeft.Y + 62.0f);
	Paint.Glow(IconCentre, 80.0f, WithAlpha(Accent, 0.35f), 42);
	Paint.Disc(IconCentre, 42.0f, FMath::Lerp(P.BlueDeep, Accent, 0.4f), 43);
	Paint.Ring(IconCentre, 42.0f, FMath::Lerp(Accent, FLinearColor::White, 0.4f), 2.0f, 44);
	if (UTexture2D* Icon = GetNodeIcon(Node))
	{
		Paint.ImageCentred(Icon, IconCentre, FVector2f(56.0f, 56.0f), FLinearColor::White, 45);
	}
	const float TitleLeft = Left + 104.0f;
	Paint.Text(Node.DisplayName.ToString(), Font(EBeyondFont::Display, 25.0f), FVector2f(TitleLeft, TopLeft.Y + 20.0f), P.GoldBright, 0.0f, 45);
	Paint.Text(FString::Printf(TEXT("Lv. %d / %d"), Rank, Node.MaxRank), Font(EBeyondFont::Number, 18.0f), FVector2f(TitleLeft, TopLeft.Y + 58.0f), P.Text, 0.0f, 45);

	TArray<FText> Tags;
	Tags.Add(BeyondSkillTreeLocal::IsActiveNode(Node) ? LOCTEXT("ActiveSkill", "Active Skill")
		: BeyondSkillTreeLocal::IsUpgradeNode(Node) ? LOCTEXT("Upgrade", "Upgrade") : LOCTEXT("Passive", "Passive"));
	if (const UBeyondCharacterProfile* Profile = GetProfileFor(ActiveTab); Profile && Profile->PrimaryElement != EBeyondElement::None
		&& (BeyondSkillTreeLocal::IsActiveNode(Node) || BeyondSkillTreeLocal::IsUpgradeNode(Node)))
	{
		Tags.Add(ElementName(Profile->PrimaryElement));
	}
	float TagX = TitleLeft;
	const FSlateFontInfo TagFont = Font(EBeyondFont::Body, 16.0f);
	for (int32 Index = 0; Index < Tags.Num(); ++Index)
	{
		const FString Tag = Tags[Index].ToString();
		const FVector2f TagSize = MeasureText(Tag, TagFont) + FVector2f(20.0f, 4.0f);
		const FVector2f TagTopLeft(TagX, TopLeft.Y + 88.0f);
		Paint.Rounded(TagTopLeft, TagSize, 0.0f, Index == 0 ? WithAlpha(Accent, 0.45f) : FromHex(0x141C30, 0.95f), 44);
		Paint.Text(Tag, TagFont, TagTopLeft + FVector2f(TagSize.X * 0.5f, 2.0f), P.Text, 0.5f, 45, false);
		TagX += TagSize.X + 8.0f;
	}

	// Description
	float Y = TopLeft.Y + 128.0f;
	if (!Node.Description.IsEmpty())
	{
		Y += Paint.Paragraph(Node.Description.ToString(), Font(EBeyondFont::Body, 19.0f), FVector2f(Left, Y), Width, 24.0f, P.Text, 45, 3) + 6.0f;
	}
	Paint.Separator(FVector2f(Left, Y + 4.0f), Width, 44, 0.6f);
	Y += 16.0f;

	// What each rank gives; the current one lit
	const FSlateFontInfo RankFont = Font(EBeyondFont::Heading, 17.0f);
	const FSlateFontInfo EffectFont = Font(EBeyondFont::Body, 18.0f);
	const float ButtonTop = TopLeft.Y + Size.Y - 78.0f;
	for (int32 Level = 1; Level <= Node.MaxRank && Y < ButtonTop - 30.0f; ++Level)
	{
		const bool bCurrent = Level == FMath::Max(Rank, 1);
		const bool bReached = Level <= Rank;
		const FVector2f Bullet(Left + 8.0f, Y + 12.0f);
		if (bCurrent)
		{
			Paint.Diamond(Bullet, 7.0f, 7.0f, P.GoldBright, 45);
			Paint.Diamond(Bullet, 3.0f, 3.0f, P.Backdrop, 46);
		}
		else
		{
			Paint.DiamondOutline(Bullet, 6.0f, 6.0f, WithAlpha(P.Silver, 0.6f), 1.2f, 45);
		}
		Paint.Text(FString::Printf(TEXT("Lv. %d"), Level), RankFont, FVector2f(Left + 26.0f, Y), bCurrent ? P.GoldBright : P.TextDim, 0.0f, 45);
		TArray<FString> Effects;
		for (const FText& Effect : UBeyondSkillTreeAsset::DescribeEffects(Node, Level))
		{
			Effects.Add(Effect.ToString());
		}
		const FLinearColor EffectColor = bReached || bCurrent ? P.Text : WithAlpha(P.TextDim, 0.8f);
		const float Height = Paint.Paragraph(FString::Join(Effects, TEXT(". ")), EffectFont, FVector2f(Left + 92.0f, Y), Width - 92.0f, 23.0f, EffectColor, 45, 2);
		Y += FMath::Max(Height, 23.0f) + 10.0f;
	}

	// Cost and the UPGRADE button
	FText Reason;
	const bool bCanBuy = Tree.CanUnlock(Node.Id, Reason);
	const bool bMaxed = Rank >= Node.MaxRank;
	if (!bCanBuy && !bMaxed && !Reason.IsEmpty())
	{
		Paint.Text(Reason.ToString(), Font(EBeyondFont::BodyItalic, 17.0f), FVector2f(Left, ButtonTop - 26.0f), P.Minus, 0.0f, 45);
	}
	// "Cost * 1 Skill Point" (the currency's name loses its s for one point)
	FString Currency = Tree.GetCurrencyName().ToString();
	if (Node.Cost == 1 && Currency.EndsWith(TEXT("s")))
	{
		Currency.LeftChopInline(1);
	}
	const FSlateFontInfo CostFont = Font(EBeyondFont::Body, 17.0f);
	const FString CostText = FString::Printf(TEXT("%d %s"), Node.Cost, *Currency);
	const FVector2f CostTopLeft(Left, ButtonTop + 6.0f);
	const FVector2f CostSize(FMath::Max(190.0f, 86.0f + MeasureText(CostText, CostFont).X), 46.0f);
	Paint.Box(CostTopLeft, CostSize, FromHex(0x0A0F1C, 0.9f), 44);
	Paint.RectOutline(CostTopLeft, CostSize, WithAlpha(P.Gold, 0.5f), 1.0f, 45);
	Paint.Text(LOCTEXT("Cost", "Cost").ToString(), CostFont, CostTopLeft + FVector2f(12.0f, 11.0f), P.TextDim, 0.0f, 45);
	Paint.Sparkle(CostTopLeft + FVector2f(64.0f, 23.0f), 9.0f, P.GoldBright, 45);
	Paint.Text(CostText, CostFont, CostTopLeft + FVector2f(78.0f, 11.0f), P.Text, 0.0f, 45);

	const FVector2f ButtonTopLeft(Left + CostSize.X + 20.0f, ButtonTop);
	const FVector2f ButtonSize(Width - CostSize.X - 20.0f, 58.0f);
	const float Skew = 18.0f;
	const bool bHolding = !HoldNode.IsNone() && HoldNode == Node.Id;
	if (bMaxed)
	{
		Paint.Parallelogram(ButtonTopLeft, ButtonSize, Skew, FromHex(0x1A1F2C, 0.9f), FromHex(0x0E121C, 0.9f), 44);
		Paint.ParallelogramOutline(ButtonTopLeft, ButtonSize, Skew, WithAlpha(P.GoldBright, 0.6f), 1.2f, 45);
		Paint.Text(LOCTEXT("Maxed", "Maxed").ToString(), Font(EBeyondFont::Heading, 22.0f), ButtonTopLeft + FVector2f(ButtonSize.X * 0.5f, 14.0f), P.GoldBright, 0.5f, 46);
		return;
	}
	Paint.Parallelogram(ButtonTopLeft, ButtonSize, Skew, bCanBuy ? FromHex(0x2A2416, 0.95f) : FromHex(0x161A24, 0.9f), FromHex(0x0E0D0A, 0.95f), 44);
	if (bHolding && HoldDuration > 0.0f)
	{
		const float Fill = FMath::Clamp(HoldTime / HoldDuration, 0.0f, 1.0f);
		Paint.Parallelogram(ButtonTopLeft, FVector2f(ButtonSize.X * Fill, ButtonSize.Y), Skew, WithAlpha(P.GoldBright, 0.55f), WithAlpha(P.Gold, 0.35f), 45);
	}
	Paint.ParallelogramOutline(ButtonTopLeft, ButtonSize, Skew, WithAlpha(bCanBuy ? P.GoldBright : P.Silver, bCanBuy ? 0.95f : 0.4f), 1.6f, 46);
	Paint.ParallelogramOutline(ButtonTopLeft + FVector2f(0.0f, 4.0f), ButtonSize - FVector2f(0.0f, 8.0f), Skew * (ButtonSize.Y - 8.0f) / ButtonSize.Y,
		WithAlpha(P.Gold, 0.3f), 1.0f, 46);
	const float CapWidth = Paint.KeyCap(TEXT("F"), ButtonTopLeft + FVector2f(22.0f, 15.0f), 47);
	Paint.Text(LOCTEXT("UpgradeButton", "Upgrade").ToString(), Font(EBeyondFont::Heading, 23.0f), ButtonTopLeft + FVector2f(22.0f + CapWidth + (ButtonSize.X - CapWidth - 22.0f) * 0.5f, 14.0f),
		bCanBuy ? P.Text : P.TextDim, 0.5f, 47);
}

#undef LOCTEXT_NAMESPACE
