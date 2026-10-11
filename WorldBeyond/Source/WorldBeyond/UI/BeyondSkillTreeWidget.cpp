// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondSkillTreeWidget.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "Characters/BeyondCharacterBase.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Progression/BeyondSkillTreeComponent.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "BeyondSkillTree"

namespace
{
	constexpr float SkillTreeTop = 230.0f;
	constexpr float SkillTabWidth = 180.0f;
	constexpr float SkillTabHeight = 40.0f;
	constexpr float SkillTabGap = 14.0f;
	constexpr float SkillTooltipWidth = 340.0f;
	constexpr int32 SkillRingSegments = 48;

	FVector2f MeasureTreeText(const FString& Text, const FSlateFontInfo& Font)
	{
		FSlateRenderer* Renderer = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetRenderer() : nullptr;
		if (!Renderer)
		{
			return FVector2f(Text.Len() * Font.Size * 0.6f, Font.Size * 1.2f);
		}
		return FVector2f(Renderer->GetFontMeasureService()->Measure(Text, Font));
	}

	FLinearColor TreeWithAlpha(FLinearColor Color, float Opacity)
	{
		Color.A *= FMath::Clamp(Opacity, 0.0f, 1.0f);
		return Color;
	}
}

UBeyondSkillTreeWidget::UBeyondSkillTreeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);

	PillBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	PillBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	PillBrush.TintColor = FSlateColor(FLinearColor::White);

	PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	PanelBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	PanelBrush.OutlineSettings.CornerRadii = FVector4(10.0f, 10.0f, 10.0f, 10.0f);
	PanelBrush.TintColor = FSlateColor(FLinearColor::White);

	// The SkillTreeSystem pack's art, plus the ability bar's icons for health and stamina
	BackgroundTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SkillTreeSystem/Textures/T_background.T_background")));
	SlotTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SkillTreeSystem/Textures/T_WhiteIconBase.T_WhiteIconBase")));
	LockTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SkillTreeSystem/Textures/T_Lock.T_Lock")));
	GlowTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SkillTreeSystem/Textures/T_RadialGradient.T_RadialGradient")));
	StatIcons.Add(EBeyondSkillStat::Strength, TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SkillTreeSystem/Textures/T_Sword.T_Sword"))));
	StatIcons.Add(EBeyondSkillStat::Arcana, TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SkillTreeSystem/Textures/T_MagicWand.T_MagicWand"))));
	StatIcons.Add(EBeyondSkillStat::MaxStamina, TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SkillTreeSystem/Textures/T_PersonSimpleRun.T_PersonSimpleRun"))));
	StatIcons.Add(EBeyondSkillStat::MaxHealth, TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/Blueprints/Widgets/Images/health-increase.health-increase"))));
	UnlockSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/energy-charge-up.energy-charge-up")));
}

void UBeyondSkillTreeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
	LoadArt();
	RefreshTabs();
}

void UBeyondSkillTreeWidget::LoadArt()
{
	auto LoadTexture = [this](const TSoftObjectPtr<UTexture2D>& Soft) -> UTexture2D*
	{
		UTexture2D* Texture = Soft.IsNull() ? nullptr : Soft.LoadSynchronous();
		if (Texture)
		{
			LoadedTextures.AddUnique(Texture);
		}
		return Texture;
	};
	Background = LoadTexture(BackgroundTexture);
	SlotFrame = LoadTexture(SlotTexture);
	Lock = LoadTexture(LockTexture);
	Glow = LoadTexture(GlowTexture);
	for (const TPair<EBeyondSkillStat, TSoftObjectPtr<UTexture2D>>& Pair : StatIcons)
	{
		LoadTexture(Pair.Value);
	}
}

void UBeyondSkillTreeWidget::RefreshTabs()
{
	const UBeyondSkillTreeComponent* Previous = GetActiveTree();
	Tabs.Reset();

	const ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetOwningPlayer());
	if (!PC || !PC->PartyComponent)
	{
		return;
	}

	// Demigods in duo order (Conduit, Striker, anyone else), then the shared duo tree
	TArray<ABeyondCharacterBase*> Members = PC->PartyComponent->GetMembers();
	Members.StableSort([](const ABeyondCharacterBase& A, const ABeyondCharacterBase& B)
	{
		auto Order = [](EBeyondDuoRole Role) { return Role == EBeyondDuoRole::Conduit ? 0 : Role == EBeyondDuoRole::Striker ? 1 : 2; };
		return Order(A.DuoRole) < Order(B.DuoRole);
	});
	for (ABeyondCharacterBase* Member : Members)
	{
		if (UBeyondSkillTreeComponent* Tree = Member ? Member->GetSkillTreeComponent() : nullptr)
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
	return Tree && Tree->Tree ? Tree->Tree->FindNode(NodeId) : nullptr;
}

FLinearColor UBeyondSkillTreeWidget::GetAccent() const
{
	const UBeyondSkillTreeComponent* Tree = GetActiveTree();
	return Tree && Tree->Tree ? Tree->Tree->AccentColor : FLinearColor(0.10f, 0.45f, 1.0f);
}

bool UBeyondSkillTreeWidget::IsDuoLoadout(const FBeyondSkillNode& Node) const
{
	const UBeyondDuoSkillTreeComponent* Duo = Cast<UBeyondDuoSkillTreeComponent>(GetActiveTree());
	if (!Duo || Duo->GetRank(Node.Id) <= 0)
	{
		return false;
	}
	const TSubclassOf<UGameplayAbility> Loadout = Duo->GetDuoLoadout();
	for (const FBeyondSkillEffect& Effect : Node.Effects)
	{
		if (Effect.Type == EBeyondSkillEffectType::DuoPower && Effect.Ability == Loadout)
		{
			return true;
		}
	}
	return false;
}

FLinearColor UBeyondSkillTreeWidget::GetNodeColor(const UBeyondSkillTreeComponent& Tree, const FBeyondSkillNode& Node) const
{
	switch (Tree.GetNodeState(Node.Id))
	{
	case EBeyondSkillNodeState::Maxed:
		return GoldColor;
	case EBeyondSkillNodeState::Acquired:
		return FMath::Lerp(GetAccent(), GoldColor, 0.55f);
	case EBeyondSkillNodeState::Available:
		return GetAccent();
	default:
		return LockedColor;
	}
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

const FSlateBrush* UBeyondSkillTreeWidget::GetTextureBrush(UTexture2D* Texture) const
{
	if (!Texture)
	{
		return nullptr;
	}
	if (const TUniquePtr<FSlateBrush>* Existing = TextureBrushes.Find(Texture))
	{
		return Existing->Get();
	}
	TUniquePtr<FSlateBrush> Brush = MakeUnique<FSlateBrush>();
	Brush->SetResourceObject(Texture);
	Brush->DrawAs = ESlateBrushDrawType::Image;
	const FSlateBrush* Result = Brush.Get();
	TextureBrushes.Add(Texture, MoveTemp(Brush));
	return Result;
}

FVector2f UBeyondSkillTreeWidget::GetTreeScale(const FVector2f& LocalSize) const
{
	// Shrink the grid when a tree is taller / wider than the screen
	FVector2f Scale(1.0f, 1.0f);
	const UBeyondSkillTreeComponent* Tree = GetActiveTree();
	if (!Tree || !Tree->Tree)
	{
		return Scale;
	}
	float MaxRow = 0.0f;
	float MaxColumn = 0.0f;
	for (const FBeyondSkillNode& Node : Tree->Tree->Nodes)
	{
		MaxRow = FMath::Max(MaxRow, static_cast<float>(Node.Position.Y));
		MaxColumn = FMath::Max(MaxColumn, FMath::Abs(static_cast<float>(Node.Position.X)));
	}
	const float AvailableHeight = LocalSize.Y - SkillTreeTop - 150.0f;
	const float AvailableHalfWidth = LocalSize.X * 0.5f - 120.0f;
	if (MaxRow > 0.0f && MaxRow * CellSize.Y > AvailableHeight)
	{
		Scale.Y = FMath::Max(0.4f, AvailableHeight / (MaxRow * CellSize.Y));
	}
	if (MaxColumn > 0.0f && MaxColumn * CellSize.X > AvailableHalfWidth)
	{
		Scale.X = FMath::Max(0.4f, AvailableHalfWidth / (MaxColumn * CellSize.X));
	}
	return Scale;
}

FVector2f UBeyondSkillTreeWidget::GetNodeCenter(const FBeyondSkillNode& Node, const FVector2f& LocalSize) const
{
	const FVector2f Scale = GetTreeScale(LocalSize);
	return FVector2f(LocalSize.X * 0.5f + static_cast<float>(Node.Position.X) * static_cast<float>(CellSize.X) * Scale.X,
		SkillTreeTop + static_cast<float>(Node.Position.Y) * static_cast<float>(CellSize.Y) * Scale.Y);
}

FName UBeyondSkillTreeWidget::HitTestNode(const FVector2f& Local, const FVector2f& LocalSize) const
{
	const UBeyondSkillTreeComponent* Tree = GetActiveTree();
	if (!Tree || !Tree->Tree)
	{
		return NAME_None;
	}
	for (const FBeyondSkillNode& Node : Tree->Tree->Nodes)
	{
		if (FVector2f::Distance(Local, GetNodeCenter(Node, LocalSize)) <= NodeRadius + 4.0f)
		{
			return Node.Id;
		}
	}
	return NAME_None;
}

FVector2f UBeyondSkillTreeWidget::GetTabCenter(int32 Index, const FVector2f& LocalSize) const
{
	const float Total = Tabs.Num() * SkillTabWidth + FMath::Max(Tabs.Num() - 1, 0) * SkillTabGap;
	const float Left = LocalSize.X * 0.5f - Total * 0.5f;
	return FVector2f(Left + Index * (SkillTabWidth + SkillTabGap) + SkillTabWidth * 0.5f, 62.0f);
}

int32 UBeyondSkillTreeWidget::HitTestTab(const FVector2f& Local, const FVector2f& LocalSize) const
{
	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		const FVector2f Centre = GetTabCenter(Index, LocalSize);
		if (FMath::Abs(Local.X - Centre.X) <= SkillTabWidth * 0.5f && FMath::Abs(Local.Y - Centre.Y) <= SkillTabHeight * 0.5f)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UBeyondSkillTreeWidget::ShowMessage(const FText& Text, bool bWarning)
{
	Message = Text;
	bMessageWarning = bWarning;
	MessageAge = 0.0f;
}

void UBeyondSkillTreeWidget::PlayUISound(const TSoftObjectPtr<USoundBase>& Sound) const
{
	if (USoundBase* Loaded = Sound.IsNull() ? nullptr : Sound.LoadSynchronous())
	{
		// UI sounds keep playing while the game is paused
		UGameplayStatics::PlaySound2D(this, Loaded, 1.0f, 1.0f, 0.0f, nullptr, nullptr, true);
	}
}

bool UBeyondSkillTreeWidget::BeginHold(FName NodeId)
{
	UBeyondSkillTreeComponent* Tree = GetActiveTree();
	FText Reason;
	if (!Tree || !Tree->CanUnlock(NodeId, Reason))
	{
		ShowMessage(Reason.IsEmpty() ? LOCTEXT("CannotUnlock", "Can't unlock that yet") : Reason, true);
		PlayUISound(DeniedSound);
		HoldNode = NAME_None;
		return false;
	}
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

void UBeyondSkillTreeWidget::CycleTab(int32 Direction)
{
	if (Tabs.Num() > 1)
	{
		SelectTab((ActiveTab + Direction + Tabs.Num()) % Tabs.Num());
	}
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
	if (ResetArmedAge >= 0.0f && ResetArmedAge < 2.5f)
	{
		const int32 Refunded = Tree->ResetTree();
		ResetArmedAge = -1.0f;
		ShowMessage(FText::Format(LOCTEXT("Refunded", "Tree reset: {0} {1} refunded"), FText::AsNumber(Refunded), Tree->GetCurrencyName()));
		return;
	}
	ResetArmedAge = 0.0f;
	ShowMessage(FText::Format(LOCTEXT("ConfirmReset", "Press R again to reset this tree ({0} {1} back)"), FText::AsNumber(Spent), Tree->GetCurrencyName()), true);
}

void UBeyondSkillTreeWidget::Close()
{
	CancelHold();
	if (ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetOwningPlayer()))
	{
		PC->CloseSkillTree();
	}
	else
	{
		RemoveFromParent();
	}
}

void UBeyondSkillTreeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	LastLocalSize = FVector2f(MyGeometry.GetLocalSize());
	AdvanceAnimation(InDeltaTime);
}

void UBeyondSkillTreeWidget::AdvanceAnimation(float DeltaSeconds)
{
	AnimTime += DeltaSeconds;
	if (MessageAge >= 0.0f)
	{
		MessageAge += DeltaSeconds;
	}
	if (ResetArmedAge >= 0.0f)
	{
		ResetArmedAge += DeltaSeconds;
		if (ResetArmedAge > 2.5f)
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
		PlayUISound(UnlockSound);
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
		PlayUISound(DeniedSound);
	}
}

FReply UBeyondSkillTreeWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	LastLocalSize = FVector2f(InGeometry.GetLocalSize());
	const FVector2f Local(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
	HoveredNode = HitTestNode(Local, LastLocalSize);
	if (!HoldNode.IsNone() && HoveredNode != HoldNode)
	{
		CancelHold();
	}
	return FReply::Handled();
}

FReply UBeyondSkillTreeWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	LastLocalSize = FVector2f(InGeometry.GetLocalSize());
	const FVector2f Local(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const int32 Tab = HitTestTab(Local, LastLocalSize);
		if (Tab != INDEX_NONE)
		{
			SelectTab(Tab);
			return FReply::Handled();
		}
		const FName Node = HitTestNode(Local, LastLocalSize);
		if (!Node.IsNone())
		{
			BeginHold(Node);
		}
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		const FName Node = HitTestNode(Local, LastLocalSize);
		if (!Node.IsNone())
		{
			EquipDuoPowerFrom(Node);
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UBeyondSkillTreeWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		CancelHold();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UBeyondSkillTreeWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::K || Key == EKeys::Gamepad_Special_Right)
	{
		Close();
		return FReply::Handled();
	}
	if (Key == EKeys::I)
	{
		// Straight over to the equipment & inventory screen
		CancelHold();
		if (ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetOwningPlayer()))
		{
			PC->OpenInventory();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::M)
	{
		// ...or the world map
		CancelHold();
		if (ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetOwningPlayer()))
		{
			PC->OpenWorldMap();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Q || Key == EKeys::Left || Key == EKeys::Gamepad_LeftShoulder)
	{
		CycleTab(-1);
		return FReply::Handled();
	}
	if (Key == EKeys::E || Key == EKeys::Right || Key == EKeys::Gamepad_RightShoulder)
	{
		CycleTab(1);
		return FReply::Handled();
	}
	if (Key == EKeys::R)
	{
		HandleResetKey();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

int32 UBeyondSkillTreeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 BackLayer = BaseLayer + 1;
	const int32 LineLayer = BaseLayer + 2;
	const int32 NodeLayer = BaseLayer + 3;
	const int32 IconLayer = BaseLayer + 5;
	const int32 TextLayer = BaseLayer + 7;
	const int32 TooltipLayer = BaseLayer + 10;

	const FVector2f LocalSize(AllottedGeometry.GetLocalSize());
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
	const FLinearColor Accent = GetAccent();

	auto DrawBrush = [&](const FSlateBrush* ShapeBrush, const FVector2f& ShapeTopLeft, const FVector2f& ShapeSize, const FLinearColor& ShapeTint, int32 ShapeLayer)
	{
		if (ShapeBrush)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, ShapeLayer, AllottedGeometry.ToPaintGeometry(ShapeSize, FSlateLayoutTransform(ShapeTopLeft)),
				ShapeBrush, ESlateDrawEffect::None, ShapeTint);
		}
	};
	auto DrawCentred = [&](const FSlateBrush* ShapeBrush, const FVector2f& Centre, float Diameter, const FLinearColor& ShapeTint, int32 ShapeLayer)
	{
		DrawBrush(ShapeBrush, Centre - FVector2f(Diameter, Diameter) * 0.5f, FVector2f(Diameter, Diameter), ShapeTint, ShapeLayer);
	};
	auto DrawLabel = [&](const FString& Label, const FSlateFontInfo& LabelFont, const FVector2f& LabelPosition, const FLinearColor& LabelTint, float Align, int32 LabelLayer)
	{
		const FVector2f LabelSize = MeasureTreeText(Label, LabelFont);
		const FVector2f LabelTopLeft(LabelPosition.X - LabelSize.X * Align, LabelPosition.Y);
		FSlateDrawElement::MakeText(OutDrawElements, LabelLayer, AllottedGeometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(LabelTopLeft + FVector2f(1.5f, 1.5f))),
			Label, LabelFont, ESlateDrawEffect::None, TreeWithAlpha(FLinearColor::Black, 0.7f * LabelTint.A));
		FSlateDrawElement::MakeText(OutDrawElements, LabelLayer + 1, AllottedGeometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(LabelTopLeft)),
			Label, LabelFont, ESlateDrawEffect::None, LabelTint);
		return LabelSize;
	};
	auto DrawRing = [&](const FVector2f& Centre, float Radius, float Fraction, const FLinearColor& RingTint, float Thickness, int32 RingLayer)
	{
		const int32 Segments = FMath::Max(2, FMath::CeilToInt(SkillRingSegments * FMath::Clamp(Fraction, 0.0f, 1.0f)));
		TArray<FVector2f> Points;
		Points.Reserve(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			// Clockwise from the top
			const float Angle = -HALF_PI + 2.0f * PI * FMath::Clamp(Fraction, 0.0f, 1.0f) * Index / Segments;
			Points.Add(Centre + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, RingLayer, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, RingTint, true, Thickness);
	};

	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 26);
	const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 15);
	const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);

	//~ Backdrop: the pack's background over a dark veil
	DrawBrush(WhiteBrush, FVector2f::ZeroVector, LocalSize, FLinearColor(0.0f, 0.0f, 0.02f, 0.86f), BackLayer);
	if (const FSlateBrush* BackgroundBrush = GetTextureBrush(Background))
	{
		DrawBrush(BackgroundBrush, FVector2f::ZeroVector, LocalSize, FLinearColor(0.55f, 0.6f, 0.8f, 0.35f), BackLayer);
	}

	//~ Tabs
	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		const FVector2f Centre = GetTabCenter(Index, LocalSize);
		const bool bActive = Index == ActiveTab;
		const FLinearColor TabColor = bActive ? Accent : FLinearColor(0.12f, 0.13f, 0.18f, 0.95f);
		DrawBrush(&PillBrush, Centre - FVector2f(SkillTabWidth, SkillTabHeight) * 0.5f - FVector2f(2.0f, 2.0f), FVector2f(SkillTabWidth + 4.0f, SkillTabHeight + 4.0f),
			TreeWithAlpha(GoldColor, bActive ? 0.9f : 0.3f), BackLayer);
		DrawBrush(&PillBrush, Centre - FVector2f(SkillTabWidth, SkillTabHeight) * 0.5f, FVector2f(SkillTabWidth, SkillTabHeight), TabColor, LineLayer);
		const FString TabTitle = Tabs[Index].Title.ToString().ToUpper();
		DrawLabel(TabTitle, HeaderFont, Centre - FVector2f(0.0f, MeasureTreeText(TabTitle, HeaderFont).Y * 0.5f), TextColor, 0.5f, TextLayer);
	}
	if (Tabs.Num() > 1)
	{
		DrawLabel(TEXT("Q"), SmallFont, GetTabCenter(0, LocalSize) - FVector2f(SkillTabWidth * 0.5f + 22.0f, 7.0f), TreeWithAlpha(TextColor, 0.6f), 0.5f, TextLayer);
		DrawLabel(TEXT("E"), SmallFont, GetTabCenter(Tabs.Num() - 1, LocalSize) + FVector2f(SkillTabWidth * 0.5f + 22.0f, -7.0f), TreeWithAlpha(TextColor, 0.6f), 0.5f, TextLayer);
	}

	const UBeyondSkillTreeComponent* Tree = GetActiveTree();
	if (!Tree || !Tree->Tree)
	{
		DrawLabel(LOCTEXT("NoTree", "No skill tree assigned: run Scripts/Migration/migrate_pass8.py").ToString(), HeaderFont,
			FVector2f(LocalSize.X * 0.5f, LocalSize.Y * 0.45f), WarningColor, 0.5f, TextLayer);
		return TooltipLayer + 2;
	}
	const UBeyondSkillTreeAsset& Asset = *Tree->Tree;

	//~ Title, points and level
	DrawLabel(Asset.TreeName.ToString(), TitleFont, FVector2f(LocalSize.X * 0.5f, 100.0f), GoldColor, 0.5f, TextLayer);
	DrawLabel(Asset.Subtitle.ToString(), BodyFont, FVector2f(LocalSize.X * 0.5f, 140.0f), TreeWithAlpha(TextColor, 0.75f), 0.5f, TextLayer);

	const FString PointsText = FString::Printf(TEXT("%s  %d"), *Tree->GetCurrencyName().ToString().ToUpper(), Tree->GetAvailablePoints());
	const FVector2f PointsSize = MeasureTreeText(PointsText, HeaderFont);
	const FVector2f PointsTopLeft(LocalSize.X - 60.0f - PointsSize.X - 24.0f, 44.0f);
	DrawBrush(&PillBrush, PointsTopLeft, PointsSize + FVector2f(24.0f, 12.0f), TreeWithAlpha(GoldColor, Tree->GetAvailablePoints() > 0 ? 0.95f : 0.35f), BackLayer);
	DrawLabel(PointsText, HeaderFont, PointsTopLeft + FVector2f(12.0f, 6.0f), FLinearColor(0.05f, 0.04f, 0.02f, 1.0f), 0.0f, TextLayer);
	const FString LevelText = Cast<UBeyondDuoSkillTreeComponent>(Tree)
		? FString::Printf(TEXT("Party level %d"), Tree->GetTreeLevel())
		: FString::Printf(TEXT("Level %d"), Tree->GetTreeLevel());
	DrawLabel(LevelText, BodyFont, FVector2f(LocalSize.X - 60.0f, 90.0f), TreeWithAlpha(TextColor, 0.8f), 1.0f, TextLayer);

	//~ Connections (under the nodes)
	for (const FBeyondSkillNode& Node : Asset.Nodes)
	{
		const FVector2f To = GetNodeCenter(Node, LocalSize);
		for (const FName& Required : Node.Requires)
		{
			const FBeyondSkillNode* From = Asset.FindNode(Required);
			if (!From)
			{
				continue;
			}
			const bool bLit = Tree->GetRank(Required) > 0;
			const FLinearColor LineColor = bLit ? FMath::Lerp(Accent, GoldColor, Tree->GetRank(Node.Id) > 0 ? 0.6f : 0.0f) : TreeWithAlpha(LockedColor, 0.6f);
			TArray<FVector2f> Line = { GetNodeCenter(*From, LocalSize), To };
			FSlateDrawElement::MakeLines(OutDrawElements, LineLayer, AllottedGeometry.ToPaintGeometry(), MoveTemp(Line), ESlateDrawEffect::None,
				LineColor, true, bLit ? 4.0f : 3.0f);
		}
	}

	//~ Nodes
	const FSlateBrush* FrameBrush = GetTextureBrush(SlotFrame);
	const FSlateBrush* GlowBrush = GetTextureBrush(Glow);
	const FSlateBrush* LockBrush = GetTextureBrush(Lock);
	for (const FBeyondSkillNode& Node : Asset.Nodes)
	{
		const FVector2f Centre = GetNodeCenter(Node, LocalSize);
		const EBeyondSkillNodeState State = Tree->GetNodeState(Node.Id);
		const FLinearColor NodeColor = GetNodeColor(*Tree, Node);
		const int32 Rank = Tree->GetRank(Node.Id);
		FText Unused;
		const bool bCanBuy = Tree->CanUnlock(Node.Id, Unused);
		const float Diameter = NodeRadius * 2.0f;

		// Glow: steady when taken, pulsing when it can be bought now
		if (GlowBrush && (Rank > 0 || bCanBuy))
		{
			const float Pulse = bCanBuy ? 0.5f + 0.5f * FMath::Sin(AnimTime * 4.0f) : 1.0f;
			DrawCentred(GlowBrush, Centre, Diameter * 2.4f, TreeWithAlpha(NodeColor, 0.25f + 0.25f * Pulse), LineLayer);
		}

		// Frame (the pack's slot), dark core, icon
		if (FrameBrush)
		{
			DrawCentred(FrameBrush, Centre, Diameter + 8.0f, NodeColor, NodeLayer);
		}
		else
		{
			DrawCentred(&PillBrush, Centre, Diameter + 6.0f, NodeColor, NodeLayer);
		}
		DrawCentred(&PillBrush, Centre, Diameter - 6.0f, FLinearColor(PanelColor.R, PanelColor.G, PanelColor.B, 1.0f), NodeLayer + 1);

		const FLinearColor IconTint = State == EBeyondSkillNodeState::Locked ? FLinearColor(0.45f, 0.45f, 0.5f, 0.8f) : FLinearColor::White;
		if (const FSlateBrush* IconBrush = GetTextureBrush(GetNodeIcon(Node)))
		{
			DrawCentred(IconBrush, Centre, Diameter * 0.62f, IconTint, IconLayer);
		}
		else
		{
			const FString Glyph = Node.DisplayName.ToString().Left(1).ToUpper();
			DrawLabel(Glyph, TitleFont, Centre - FVector2f(0.0f, MeasureTreeText(Glyph, TitleFont).Y * 0.5f), IconTint, 0.5f, IconLayer);
		}
		if (State == EBeyondSkillNodeState::Locked && LockBrush)
		{
			DrawCentred(LockBrush, Centre + FVector2f(NodeRadius * 0.62f, NodeRadius * 0.62f), NodeRadius * 0.7f, FLinearColor(0.85f, 0.85f, 0.9f, 1.0f), IconLayer + 1);
		}

		// Hover ring, hold progress, unlock flash
		if (Node.Id == HoveredNode)
		{
			DrawRing(Centre, NodeRadius + 7.0f, 1.0f, TreeWithAlpha(FLinearColor::White, 0.75f), 2.0f, IconLayer);
		}
		if (Node.Id == HoldNode && HoldDuration > 0.0f)
		{
			DrawRing(Centre, NodeRadius + 7.0f, HoldTime / HoldDuration, GoldColor, 5.0f, IconLayer + 1);
		}
		if (Node.Id == FlashNode && FlashAge >= 0.0f)
		{
			const float Alpha = FlashAge / 0.6f;
			DrawRing(Centre, NodeRadius + 6.0f + 40.0f * Alpha, 1.0f, TreeWithAlpha(GoldColor, 1.0f - Alpha), 4.0f * (1.0f - Alpha) + 1.0f, IconLayer + 1);
		}

		// The duo power on G
		if (IsDuoLoadout(Node))
		{
			const FVector2f BadgeCentre = Centre + FVector2f(NodeRadius * 0.75f, -NodeRadius * 0.75f);
			DrawCentred(&PillBrush, BadgeCentre, 24.0f, GoldColor, IconLayer + 2);
			DrawLabel(TEXT("G"), SmallFont, BadgeCentre - FVector2f(0.0f, 7.0f), FLinearColor(0.05f, 0.04f, 0.02f, 1.0f), 0.5f, TextLayer + 2);
		}

		// Rank pill and name under the node
		const FString RankText = FString::Printf(TEXT("%d/%d"), Rank, Node.MaxRank);
		const FVector2f RankSize = MeasureTreeText(RankText, SmallFont);
		const FVector2f RankTopLeft(Centre.X - RankSize.X * 0.5f - 7.0f, Centre.Y + NodeRadius - 6.0f);
		DrawBrush(&PillBrush, RankTopLeft, RankSize + FVector2f(14.0f, 4.0f), TreeWithAlpha(Rank > 0 ? NodeColor : FLinearColor(0.1f, 0.1f, 0.14f, 1.0f), 0.95f), IconLayer + 1);
		DrawLabel(RankText, SmallFont, FVector2f(Centre.X, RankTopLeft.Y + 2.0f), Rank > 0 ? FLinearColor(0.05f, 0.04f, 0.02f, 1.0f) : TextColor, 0.5f, TextLayer);
		DrawLabel(Node.DisplayName.ToString(), SmallFont, FVector2f(Centre.X, Centre.Y + NodeRadius + 14.0f),
			TreeWithAlpha(TextColor, State == EBeyondSkillNodeState::Locked ? 0.55f : 0.95f), 0.5f, TextLayer);
	}

	//~ Tooltip for the hovered node
	if (const FBeyondSkillNode* Hovered = Asset.FindNode(HoveredNode))
	{
		const int32 Rank = Tree->GetRank(Hovered->Id);
		TArray<TPair<FString, FLinearColor>> Lines;
		Lines.Emplace(FString::Printf(TEXT("Rank %d / %d"), Rank, Hovered->MaxRank), TreeWithAlpha(TextColor, 0.7f));
		for (const FText& Effect : UBeyondSkillTreeAsset::DescribeEffects(*Hovered, FMath::Max(Rank, 1)))
		{
			Lines.Emplace(Effect.ToString(), FMath::Lerp(Accent, FLinearColor::White, 0.45f));
		}
		if (Rank > 0 && Rank < Hovered->MaxRank)
		{
			Lines.Emplace(TEXT("Next rank:"), TreeWithAlpha(TextColor, 0.6f));
			for (const FText& Effect : UBeyondSkillTreeAsset::DescribeEffects(*Hovered, Rank + 1))
			{
				Lines.Emplace(FString(TEXT("  ")) + Effect.ToString(), TreeWithAlpha(TextColor, 0.85f));
			}
		}
		if (!Hovered->Description.IsEmpty())
		{
			Lines.Emplace(Hovered->Description.ToString(), TreeWithAlpha(TextColor, 0.8f));
		}
		FText Reason;
		if (Tree->CanUnlock(Hovered->Id, Reason))
		{
			Lines.Emplace(FString::Printf(TEXT("Hold the left mouse button: %d %s"), Hovered->Cost, *Tree->GetCurrencyName().ToString()), GoldColor);
		}
		else
		{
			Lines.Emplace(Reason.ToString(), Rank >= Hovered->MaxRank ? GoldColor : WarningColor);
		}
		if (Cast<UBeyondDuoSkillTreeComponent>(Tree) && Rank > 0)
		{
			for (const FBeyondSkillEffect& Effect : Hovered->Effects)
			{
				if (Effect.Type == EBeyondSkillEffectType::DuoPower)
				{
					Lines.Emplace(IsDuoLoadout(*Hovered) ? FString(TEXT("On G")) : FString(TEXT("Right-click: put it on G")), GoldColor);
					break;
				}
			}
		}

		const float LineHeight = 18.0f;
		const float Height = 46.0f + Lines.Num() * LineHeight + 12.0f;
		const FVector2f Anchor = GetNodeCenter(*Hovered, LocalSize);
		float Left = Anchor.X + NodeRadius + 24.0f;
		if (Left + SkillTooltipWidth > LocalSize.X - 20.0f)
		{
			Left = Anchor.X - NodeRadius - 24.0f - SkillTooltipWidth;
		}
		const float Top = FMath::Clamp(Anchor.Y - 40.0f, 20.0f, LocalSize.Y - Height - 20.0f);
		DrawBrush(&PanelBrush, FVector2f(Left - 2.0f, Top - 2.0f), FVector2f(SkillTooltipWidth + 4.0f, Height + 4.0f), TreeWithAlpha(Accent, 0.9f), TooltipLayer);
		DrawBrush(&PanelBrush, FVector2f(Left, Top), FVector2f(SkillTooltipWidth, Height), PanelColor, TooltipLayer + 1);
		DrawLabel(Hovered->DisplayName.ToString(), HeaderFont, FVector2f(Left + 16.0f, Top + 12.0f), GoldColor, 0.0f, TooltipLayer + 2);
		float LineTop = Top + 44.0f;
		for (const TPair<FString, FLinearColor>& Line : Lines)
		{
			DrawLabel(Line.Key, BodyFont, FVector2f(Left + 16.0f, LineTop), Line.Value, 0.0f, TooltipLayer + 2);
			LineTop += LineHeight;
		}
	}

	//~ Feedback line and controls
	if (MessageAge >= 0.0f && MessageAge < 3.0f && !Message.IsEmpty())
	{
		const float Alpha = MessageAge < 2.4f ? 1.0f : 1.0f - (MessageAge - 2.4f) / 0.6f;
		DrawLabel(Message.ToString(), HeaderFont, FVector2f(LocalSize.X * 0.5f, LocalSize.Y - 92.0f),
			TreeWithAlpha(bMessageWarning ? WarningColor : GoldColor, Alpha), 0.5f, TooltipLayer);
	}
	const FString Controls = Cast<UBeyondDuoSkillTreeComponent>(Tree)
		? FString(TEXT("Hold LMB: unlock    RMB: put a duo power on G    Q / E: switch tree    R: reset    I: equipment    K / Esc: close"))
		: FString(TEXT("Hold LMB: unlock    Q / E: switch tree    R: reset (refund)    I: equipment    K / Esc: close"));
	DrawLabel(Controls, BodyFont, FVector2f(LocalSize.X * 0.5f, LocalSize.Y - 46.0f), TreeWithAlpha(TextColor, 0.65f), 0.5f, TextLayer);

	return TooltipLayer + 3;
}

#undef LOCTEXT_NAMESPACE
