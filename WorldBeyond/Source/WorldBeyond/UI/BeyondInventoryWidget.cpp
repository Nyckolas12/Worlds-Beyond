// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondInventoryWidget.h"
#include "AbilitySystemComponent.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Items/BeyondEquipmentComponent.h"
#include "Items/BeyondInventoryComponent.h"
#include "Items/BeyondItemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Progression/BeyondSkillTree.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "BeyondInventory"

namespace
{
	constexpr float InventoryTabWidth = 180.0f;
	constexpr float InventoryTabHeight = 40.0f;
	constexpr float InventoryTabGap = 14.0f;
	constexpr float InventoryCellGap = 6.0f;
	constexpr float InventoryTooltipWidth = 360.0f;
	constexpr float InventoryDiscardWindow = 2.5f;

	const EBeyondItemSlot InventorySlotOrder[] = { EBeyondItemSlot::Helm, EBeyondItemSlot::Chest, EBeyondItemSlot::Gauntlets, EBeyondItemSlot::Boots, EBeyondItemSlot::Weapon };
	const EBeyondSkillStat InventoryStatOrder[] = { EBeyondSkillStat::MaxHealth, EBeyondSkillStat::MaxStamina, EBeyondSkillStat::Strength, EBeyondSkillStat::Arcana, EBeyondSkillStat::Defense };

	FVector2f MeasureInventoryText(const FString& Text, const FSlateFontInfo& Font)
	{
		FSlateRenderer* Renderer = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetRenderer() : nullptr;
		if (!Renderer)
		{
			return FVector2f(Text.Len() * Font.Size * 0.6f, Font.Size * 1.2f);
		}
		return FVector2f(Renderer->GetFontMeasureService()->Measure(Text, Font));
	}

	FLinearColor InventoryWithAlpha(FLinearColor Color, float Opacity)
	{
		Color.A *= FMath::Clamp(Opacity, 0.0f, 1.0f);
		return Color;
	}

	// Splits Text into lines no wider than MaxWidth (at spaces)
	TArray<FString> WrapInventoryText(const FString& Text, const FSlateFontInfo& Font, float MaxWidth)
	{
		TArray<FString> Lines;
		TArray<FString> Words;
		Text.ParseIntoArray(Words, TEXT(" "), true);
		FString Current;
		for (const FString& Word : Words)
		{
			const FString Candidate = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
			if (!Current.IsEmpty() && MeasureInventoryText(Candidate, Font).X > MaxWidth)
			{
				Lines.Add(Current);
				Current = Word;
			}
			else
			{
				Current = Candidate;
			}
		}
		if (!Current.IsEmpty())
		{
			Lines.Add(Current);
		}
		return Lines;
	}

	FGameplayAttribute InventoryStatAttribute(EBeyondSkillStat Stat)
	{
		switch (Stat)
		{
		case EBeyondSkillStat::MaxHealth: return UCharacterAttributeSet::GetMaxHealthAttribute();
		case EBeyondSkillStat::MaxStamina: return UCharacterAttributeSet::GetMaxStaminaAttribute();
		case EBeyondSkillStat::Strength: return UCharacterAttributeSet::GetStrengthAttribute();
		case EBeyondSkillStat::Arcana: return UCharacterAttributeSet::GetArcanaAttribute();
		default: return UCharacterAttributeSet::GetDefenseAttribute();
		}
	}

	// Rough worth of an item's stats, for the "upgrade" marker (health and stamina come in bigger numbers)
	float InventoryItemScore(const FBeyondItemInstance& Item)
	{
		float Score = 0.0f;
		for (const FBeyondItemStat& Stat : UBeyondItemLibrary::GetItemStats(Item))
		{
			const float Weight = Stat.Stat == EBeyondSkillStat::MaxHealth ? 0.3f : Stat.Stat == EBeyondSkillStat::MaxStamina ? 0.5f : 1.0f;
			Score += Stat.Value * Weight;
		}
		return Score;
	}

	bool IsStaffTag(const FGameplayTag& Tag)
	{
		return Tag.ToString().Contains(TEXT("Staff"));
	}

	// Line drawings of the slots (coordinates in a 1 x 1 box around the centre)
	TArray<TArray<FVector2f>> GetInventoryGlyph(EBeyondItemSlot ItemSlot, bool bStaff)
	{
		TArray<TArray<FVector2f>> Lines;
		auto Circle = [](const FVector2f& Centre, float Radius, float From, float To, int32 Segments)
		{
			TArray<FVector2f> Points;
			for (int32 Index = 0; Index <= Segments; ++Index)
			{
				const float Angle = FMath::Lerp(From, To, static_cast<float>(Index) / Segments);
				Points.Add(Centre + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
			}
			return Points;
		};

		switch (ItemSlot)
		{
		case EBeyondItemSlot::Helm:
			Lines.Add(Circle(FVector2f(0.0f, 0.06f), 0.3f, PI, 2.0f * PI, 16));
			Lines.Add(TArray<FVector2f>{ FVector2f(-0.3f, 0.06f), FVector2f(-0.3f, 0.28f), FVector2f(-0.1f, 0.28f), FVector2f(-0.06f, 0.06f) });
			Lines.Add(TArray<FVector2f>{ FVector2f(0.3f, 0.06f), FVector2f(0.3f, 0.28f), FVector2f(0.1f, 0.28f), FVector2f(0.06f, 0.06f) });
			Lines.Add(TArray<FVector2f>{ FVector2f(-0.3f, 0.06f), FVector2f(0.3f, 0.06f) });
			Lines.Add(TArray<FVector2f>{ FVector2f(0.0f, 0.06f), FVector2f(0.0f, 0.24f) });
			break;
		case EBeyondItemSlot::Chest:
			Lines.Add(TArray<FVector2f>{ FVector2f(-0.12f, -0.3f), FVector2f(-0.36f, -0.2f), FVector2f(-0.28f, 0.0f), FVector2f(-0.24f, 0.32f), FVector2f(0.24f, 0.32f),
				FVector2f(0.28f, 0.0f), FVector2f(0.36f, -0.2f), FVector2f(0.12f, -0.3f), FVector2f(0.0f, -0.18f), FVector2f(-0.12f, -0.3f) });
			Lines.Add(TArray<FVector2f>{ FVector2f(0.0f, -0.18f), FVector2f(0.0f, 0.32f) });
			Lines.Add(TArray<FVector2f>{ FVector2f(-0.22f, 0.08f), FVector2f(0.22f, 0.08f) });
			break;
		case EBeyondItemSlot::Gauntlets:
			Lines.Add(TArray<FVector2f>{ FVector2f(-0.2f, 0.34f), FVector2f(-0.2f, 0.14f), FVector2f(0.2f, 0.14f), FVector2f(0.2f, 0.34f), FVector2f(-0.2f, 0.34f) });
			Lines.Add(TArray<FVector2f>{ FVector2f(-0.2f, 0.14f), FVector2f(-0.2f, -0.1f), FVector2f(0.2f, -0.1f), FVector2f(0.2f, 0.14f) });
			for (const float X : { -0.15f, -0.05f, 0.05f, 0.15f })
			{
				Lines.Add(TArray<FVector2f>{ FVector2f(X, -0.1f), FVector2f(X, -0.32f) });
			}
			Lines.Add(TArray<FVector2f>{ FVector2f(-0.2f, 0.04f), FVector2f(-0.34f, -0.08f) });
			break;
		case EBeyondItemSlot::Boots:
			Lines.Add(TArray<FVector2f>{ FVector2f(-0.2f, -0.34f), FVector2f(0.04f, -0.34f), FVector2f(0.04f, 0.06f), FVector2f(0.32f, 0.16f), FVector2f(0.32f, 0.32f),
				FVector2f(-0.2f, 0.32f), FVector2f(-0.2f, -0.34f) });
			Lines.Add(TArray<FVector2f>{ FVector2f(-0.2f, -0.2f), FVector2f(0.04f, -0.2f) });
			break;
		default:
			if (bStaff)
			{
				Lines.Add(TArray<FVector2f>{ FVector2f(0.0f, -0.18f), FVector2f(0.0f, 0.38f) });
				Lines.Add(Circle(FVector2f(0.0f, -0.27f), 0.09f, 0.0f, 2.0f * PI, 12));
				Lines.Add(TArray<FVector2f>{ FVector2f(-0.14f, -0.38f), FVector2f(-0.1f, -0.2f), FVector2f(0.0f, -0.16f), FVector2f(0.1f, -0.2f), FVector2f(0.14f, -0.38f) });
			}
			else
			{
				Lines.Add(TArray<FVector2f>{ FVector2f(-0.05f, 0.14f), FVector2f(-0.05f, -0.3f), FVector2f(0.0f, -0.4f), FVector2f(0.05f, -0.3f), FVector2f(0.05f, 0.14f) });
				Lines.Add(TArray<FVector2f>{ FVector2f(-0.2f, 0.14f), FVector2f(0.2f, 0.14f) });
				Lines.Add(TArray<FVector2f>{ FVector2f(0.0f, 0.14f), FVector2f(0.0f, 0.34f) });
				Lines.Add(Circle(FVector2f(0.0f, 0.38f), 0.04f, 0.0f, 2.0f * PI, 8));
			}
			break;
		}
		return Lines;
	}
}

UBeyondInventoryWidget::UBeyondInventoryWidget(const FObjectInitializer& ObjectInitializer)
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

	// The same pack art as the skill tree screen
	BackgroundTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SkillTreeSystem/Textures/T_background.T_background")));
	SlotTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SkillTreeSystem/Textures/T_WhiteIconBase.T_WhiteIconBase")));
	GlowTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SkillTreeSystem/Textures/T_RadialGradient.T_RadialGradient")));
	EquipSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/axe-block.axe-block")));
}

void UBeyondInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
	LoadArt();
	RefreshTabs();
}

void UBeyondInventoryWidget::LoadArt()
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
	Glow = LoadTexture(GlowTexture);
}

void UBeyondInventoryWidget::RefreshTabs()
{
	const ABeyondCharacterBase* Previous = GetShownCharacter();
	Tabs.Reset();

	const ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetOwningPlayer());
	if (!PC || !PC->PartyComponent)
	{
		return;
	}

	// Same order as the skill tree: Conduit, Striker, anyone else
	TArray<ABeyondCharacterBase*> Members = PC->PartyComponent->GetMembers();
	Members.StableSort([](const ABeyondCharacterBase& A, const ABeyondCharacterBase& B)
	{
		auto Order = [](EBeyondDuoRole Role) { return Role == EBeyondDuoRole::Conduit ? 0 : Role == EBeyondDuoRole::Striker ? 1 : 2; };
		return Order(A.DuoRole) < Order(B.DuoRole);
	});
	for (ABeyondCharacterBase* Member : Members)
	{
		if (Member && Member->GetEquipmentComponent())
		{
			FInventoryTab& Tab = Tabs.AddDefaulted_GetRef();
			Tab.Title = Member->GetCharacterDisplayName();
			Tab.Character = Member;
		}
	}

	ActiveTab = FMath::Max(FindTabFor(Previous), 0);
}

void UBeyondInventoryWidget::SelectTab(int32 Index)
{
	if (Tabs.IsValidIndex(Index) && Index != ActiveTab)
	{
		ActiveTab = Index;
		Hovered = FInventoryHit();
		FlashAge = -1.0f;
	}
}

int32 UBeyondInventoryWidget::FindTabFor(const AActor* Character) const
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

ABeyondCharacterBase* UBeyondInventoryWidget::GetShownCharacter() const
{
	return Tabs.IsValidIndex(ActiveTab) ? Tabs[ActiveTab].Character.Get() : nullptr;
}

UBeyondEquipmentComponent* UBeyondInventoryWidget::GetShownEquipment() const
{
	const ABeyondCharacterBase* Character = GetShownCharacter();
	return Character ? Character->GetEquipmentComponent() : nullptr;
}

UBeyondInventoryComponent* UBeyondInventoryWidget::GetInventory() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? PC->FindComponentByClass<UBeyondInventoryComponent>() : nullptr;
}

TArray<FBeyondItemInstance> UBeyondInventoryWidget::GetSortedBagItems() const
{
	TArray<FBeyondItemInstance> Items;
	if (const UBeyondInventoryComponent* Inventory = GetInventory())
	{
		Items = Inventory->GetItems();
	}
	Items.StableSort([](const FBeyondItemInstance& A, const FBeyondItemInstance& B)
	{
		const UBeyondItemDefinition* DefinitionA = A.GetDefinition();
		const UBeyondItemDefinition* DefinitionB = B.GetDefinition();
		const int32 SlotA = DefinitionA ? static_cast<int32>(DefinitionA->Slot) : 99;
		const int32 SlotB = DefinitionB ? static_cast<int32>(DefinitionB->Slot) : 99;
		if (SlotA != SlotB)
		{
			return SlotA < SlotB;
		}
		if (A.Tier != B.Tier)
		{
			return A.Tier > B.Tier;
		}
		return A.ItemLevel > B.ItemLevel;
	});
	return Items;
}

const FSlateBrush* UBeyondInventoryWidget::GetTextureBrush(UTexture2D* Texture) const
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

//~ Layout

FVector2f UBeyondInventoryWidget::GetTabCenter(int32 Index, const FVector2f& LocalSize) const
{
	const float Total = Tabs.Num() * InventoryTabWidth + FMath::Max(Tabs.Num() - 1, 0) * InventoryTabGap;
	const float Left = LocalSize.X * 0.5f - Total * 0.5f;
	return FVector2f(Left + Index * (InventoryTabWidth + InventoryTabGap) + InventoryTabWidth * 0.5f, 62.0f);
}

FVector2f UBeyondInventoryWidget::GetDollCenter(const FVector2f& LocalSize) const
{
	return FVector2f(FMath::Max(LocalSize.X * 0.2f + 40.0f, 260.0f), 370.0f);
}

FVector2f UBeyondInventoryWidget::GetSlotCenter(EBeyondItemSlot ItemSlot, const FVector2f& LocalSize) const
{
	// A cross: helm on top, gauntlets / chest / weapon across, boots below
	const float Step = SlotSize * 1.45f;
	FVector2f Offset = FVector2f::ZeroVector;
	switch (ItemSlot)
	{
	case EBeyondItemSlot::Helm: Offset = FVector2f(0.0f, -Step); break;
	case EBeyondItemSlot::Gauntlets: Offset = FVector2f(-Step, 0.0f); break;
	case EBeyondItemSlot::Weapon: Offset = FVector2f(Step, 0.0f); break;
	case EBeyondItemSlot::Boots: Offset = FVector2f(0.0f, Step); break;
	default: break;
	}
	return GetDollCenter(LocalSize) + Offset;
}

FVector2f UBeyondInventoryWidget::GetGridTopLeft(const FVector2f& LocalSize) const
{
	const float GridWidth = GridColumns * CellSize + (GridColumns - 1) * InventoryCellGap;
	return FVector2f(LocalSize.X - 80.0f - GridWidth, 200.0f);
}

FVector2f UBeyondInventoryWidget::GetCellTopLeft(int32 Index, const FVector2f& LocalSize) const
{
	const int32 Columns = FMath::Max(GridColumns, 1);
	return GetGridTopLeft(LocalSize) + FVector2f((Index % Columns) * (CellSize + InventoryCellGap), (Index / Columns) * (CellSize + InventoryCellGap));
}

UBeyondInventoryWidget::FInventoryHit UBeyondInventoryWidget::HitTest(const FVector2f& Local, const FVector2f& LocalSize) const
{
	FInventoryHit Hit;
	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		const FVector2f Centre = GetTabCenter(Index, LocalSize);
		if (FMath::Abs(Local.X - Centre.X) <= InventoryTabWidth * 0.5f && FMath::Abs(Local.Y - Centre.Y) <= InventoryTabHeight * 0.5f)
		{
			Hit.Tab = Index;
			return Hit;
		}
	}
	for (const EBeyondItemSlot ItemSlot : InventorySlotOrder)
	{
		const FVector2f Centre = GetSlotCenter(ItemSlot, LocalSize);
		if (FMath::Abs(Local.X - Centre.X) <= SlotSize * 0.5f && FMath::Abs(Local.Y - Centre.Y) <= SlotSize * 0.5f)
		{
			Hit.bSlot = true;
			Hit.ItemSlot = ItemSlot;
			return Hit;
		}
	}
	const TArray<FBeyondItemInstance> Items = GetSortedBagItems();
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const FVector2f TopLeft = GetCellTopLeft(Index, LocalSize);
		if (Local.X >= TopLeft.X && Local.Y >= TopLeft.Y && Local.X <= TopLeft.X + CellSize && Local.Y <= TopLeft.Y + CellSize)
		{
			Hit.BagItem = Items[Index].Id;
			return Hit;
		}
	}
	return Hit;
}

//~ Actions

void UBeyondInventoryWidget::ShowMessage(const FText& Text, bool bWarning)
{
	Message = Text;
	bMessageWarning = bWarning;
	MessageAge = 0.0f;
}

void UBeyondInventoryWidget::PlayUISound(const TSoftObjectPtr<USoundBase>& Sound) const
{
	if (USoundBase* Loaded = Sound.IsNull() ? nullptr : Sound.LoadSynchronous())
	{
		// UI sounds keep playing while the game is paused
		UGameplayStatics::PlaySound2D(this, Loaded, 0.7f, 1.0f, 0.0f, nullptr, nullptr, true);
	}
}

bool UBeyondInventoryWidget::EquipBagItem(FGuid ItemId)
{
	UBeyondEquipmentComponent* Equipment = GetShownEquipment();
	UBeyondInventoryComponent* Inventory = GetInventory();
	FBeyondItemInstance Item;
	if (!Equipment || !Inventory || !Inventory->FindItem(ItemId, Item))
	{
		return false;
	}

	FText Reason;
	if (!Equipment->CanEquip(Item, Reason))
	{
		ShowMessage(Reason, true);
		PlayUISound(DeniedSound);
		return false;
	}
	if (!Equipment->EquipFromInventory(ItemId))
	{
		return false;
	}

	const UBeyondItemDefinition* Definition = Item.GetDefinition();
	const ABeyondCharacterBase* Character = GetShownCharacter();
	FText Text = FText::Format(LOCTEXT("Equipped", "{0} equips {1}"), Character ? Character->GetCharacterDisplayName() : FText::GetEmpty(),
		UBeyondItemLibrary::GetItemName(Item));
	if (Definition && Definition->Set)
	{
		const int32 Pieces = Equipment->GetSetPieceCount(Definition->Set);
		if (Pieces == UBeyondArmorSet::FullSetPieces)
		{
			Text = FText::Format(LOCTEXT("SetComplete", "{0} set complete: {1}"), Definition->Set->SetName, Definition->Set->FullSetDescription);
		}
		else if (Pieces == UBeyondArmorSet::PartialSetPieces)
		{
			Text = FText::Format(LOCTEXT("SetTwo", "{0}: 2-piece bonus active"), Definition->Set->SetName);
		}
	}
	ShowMessage(Text);
	PlayUISound(EquipSound);
	if (Definition)
	{
		bFlashSlot = true;
		FlashSlot = Definition->Slot;
		FlashAge = 0.0f;
	}
	return true;
}

bool UBeyondInventoryWidget::UnequipSlot(EBeyondItemSlot ItemSlot)
{
	UBeyondEquipmentComponent* Equipment = GetShownEquipment();
	FBeyondItemInstance Item;
	if (!Equipment || !Equipment->GetEquipped(ItemSlot, Item) || !Equipment->Unequip(ItemSlot))
	{
		return false;
	}
	ShowMessage(FText::Format(LOCTEXT("Unequipped", "{0} is back in the bag"), UBeyondItemLibrary::GetItemName(Item)));
	PlayUISound(EquipSound);
	return true;
}

bool UBeyondInventoryWidget::DiscardBagItem(FGuid ItemId)
{
	UBeyondInventoryComponent* Inventory = GetInventory();
	FBeyondItemInstance Item;
	if (!Inventory || !Inventory->RemoveItem(ItemId, Item))
	{
		return false;
	}
	DiscardArmedItem.Invalidate();
	DiscardArmedAge = -1.0f;
	if (Hovered.BagItem == ItemId)
	{
		Hovered = FInventoryHit();
	}
	ShowMessage(FText::Format(LOCTEXT("Discarded", "{0} discarded"), UBeyondItemLibrary::GetItemName(Item)));
	return true;
}

void UBeyondInventoryWidget::HandleDiscardKey()
{
	if (Hovered.bSlot)
	{
		ShowMessage(LOCTEXT("TakeOffFirst", "Take it off first to discard it"), true);
		return;
	}
	if (!Hovered.BagItem.IsValid())
	{
		ShowMessage(LOCTEXT("HoverToDiscard", "Point at a bag item and press X to discard it"), true);
		return;
	}
	if (DiscardArmedItem == Hovered.BagItem && DiscardArmedAge >= 0.0f && DiscardArmedAge < InventoryDiscardWindow)
	{
		DiscardBagItem(Hovered.BagItem);
		return;
	}

	FBeyondItemInstance Item;
	const UBeyondInventoryComponent* Inventory = GetInventory();
	if (Inventory && Inventory->FindItem(Hovered.BagItem, Item))
	{
		DiscardArmedItem = Hovered.BagItem;
		DiscardArmedAge = 0.0f;
		ShowMessage(FText::Format(LOCTEXT("ConfirmDiscard", "Press X again to discard {0} for good"), UBeyondItemLibrary::GetItemName(Item)), true);
	}
}

void UBeyondInventoryWidget::CycleTab(int32 Direction)
{
	if (Tabs.Num() > 1)
	{
		SelectTab((ActiveTab + Direction + Tabs.Num()) % Tabs.Num());
	}
}

void UBeyondInventoryWidget::Close()
{
	if (ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetOwningPlayer()))
	{
		PC->CloseInventory();
	}
	else
	{
		RemoveFromParent();
	}
}

void UBeyondInventoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	LastLocalSize = FVector2f(MyGeometry.GetLocalSize());
	AdvanceAnimation(InDeltaTime);
}

void UBeyondInventoryWidget::AdvanceAnimation(float DeltaSeconds)
{
	AnimTime += DeltaSeconds;
	if (MessageAge >= 0.0f)
	{
		MessageAge += DeltaSeconds;
	}
	if (DiscardArmedAge >= 0.0f)
	{
		DiscardArmedAge += DeltaSeconds;
		if (DiscardArmedAge > InventoryDiscardWindow)
		{
			DiscardArmedAge = -1.0f;
			DiscardArmedItem.Invalidate();
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
}

FReply UBeyondInventoryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	LastLocalSize = FVector2f(InGeometry.GetLocalSize());
	HoverPosition = FVector2f(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
	Hovered = HitTest(HoverPosition, LastLocalSize);
	return FReply::Handled();
}

FReply UBeyondInventoryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	LastLocalSize = FVector2f(InGeometry.GetLocalSize());
	HoverPosition = FVector2f(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const FInventoryHit Hit = HitTest(HoverPosition, LastLocalSize);
	if (Hit.Tab != INDEX_NONE)
	{
		SelectTab(Hit.Tab);
	}
	else if (Hit.bSlot)
	{
		UnequipSlot(Hit.ItemSlot);
	}
	else if (Hit.BagItem.IsValid())
	{
		EquipBagItem(Hit.BagItem);
	}
	// The grid shifts after a change; hover what is under the mouse now
	Hovered = HitTest(HoverPosition, LastLocalSize);
	return FReply::Handled();
}

FReply UBeyondInventoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::I || Key == EKeys::Gamepad_Special_Right)
	{
		Close();
		return FReply::Handled();
	}
	if (Key == EKeys::K)
	{
		// Straight over to the skill tree
		if (ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetOwningPlayer()))
		{
			PC->OpenSkillTree();
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
	if (Key == EKeys::X)
	{
		HandleDiscardKey();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

//~ Tooltip text

TArray<TPair<FString, FLinearColor>> UBeyondInventoryWidget::DescribeItem(const FBeyondItemInstance& Item, bool bWorn) const
{
	TArray<TPair<FString, FLinearColor>> Lines;
	const UBeyondItemDefinition* Definition = Item.GetDefinition();
	if (!Definition)
	{
		return Lines;
	}
	const FLinearColor Dim = InventoryWithAlpha(TextColor, 0.65f);
	Lines.Emplace(FString::Printf(TEXT("%s %s  \u00B7  item level %d"), *UBeyondItemLibrary::GetTierName(Item.Tier).ToString(),
		*UBeyondItemLibrary::GetSlotName(Definition->Slot).ToString(), Item.ItemLevel), Dim);

	// Stats, with the difference to what the shown demigod wears in that slot
	const UBeyondEquipmentComponent* Equipment = GetShownEquipment();
	FBeyondItemInstance Worn;
	const bool bCompare = !bWorn && Equipment && Equipment->GetEquipped(Definition->Slot, Worn);
	const TArray<FBeyondItemStat> Stats = UBeyondItemLibrary::GetItemStats(Item);
	for (const FBeyondItemStat& Stat : Stats)
	{
		FString Line = UBeyondItemLibrary::FormatStat(Stat).ToString();
		FLinearColor Color = TextColor;
		if (bCompare)
		{
			const float Delta = Stat.Value - UBeyondItemLibrary::GetItemStat(Worn, Stat.Stat);
			if (!FMath::IsNearlyZero(Delta))
			{
				Line += FString::Printf(TEXT("   (%s%.0f)"), Delta > 0.0f ? TEXT("+") : TEXT(""), Delta);
				Color = Delta > 0.0f ? BetterColor : WarningColor;
			}
		}
		Lines.Emplace(Line, Color);
	}
	if (bCompare)
	{
		for (const FBeyondItemStat& Lost : UBeyondItemLibrary::GetItemStats(Worn))
		{
			if (!Stats.ContainsByPredicate([&Lost](const FBeyondItemStat& Stat) { return Stat.Stat == Lost.Stat; }))
			{
				Lines.Emplace(FString::Printf(TEXT("-%.0f %s   (worn now)"), Lost.Value, *UBeyondSkillTreeAsset::GetStatName(Lost.Stat).ToString()), WarningColor);
			}
		}
	}

	// Set bonuses (lit when the shown demigod has them)
	if (const UBeyondArmorSet* Set = Definition->Set)
	{
		const int32 Pieces = Equipment ? Equipment->GetSetPieceCount(Set) : 0;
		Lines.Emplace(FString::Printf(TEXT("%s set  (%d / %d worn)"), *Set->SetName.ToString(), Pieces, UBeyondArmorSet::FullSetPieces), Set->Color);
		TArray<FString> TwoPiece;
		for (const FBeyondItemStat& Stat : Set->TwoPieceStats)
		{
			TwoPiece.Add(UBeyondItemLibrary::FormatStat(Stat).ToString());
		}
		Lines.Emplace(FString::Printf(TEXT("2: %s"), *FString::Join(TwoPiece, TEXT(", "))),
			Pieces >= UBeyondArmorSet::PartialSetPieces ? TextColor : InventoryWithAlpha(TextColor, 0.45f));
		if (!Set->FullSetDescription.IsEmpty())
		{
			Lines.Emplace(FString::Printf(TEXT("4: %s"), *Set->FullSetDescription.ToString()),
				Pieces >= UBeyondArmorSet::FullSetPieces ? TextColor : InventoryWithAlpha(TextColor, 0.45f));
		}
	}

	if (!Definition->Description.IsEmpty())
	{
		Lines.Emplace(Definition->Description.ToString(), Dim);
	}

	// What a click does
	if (bWorn)
	{
		Lines.Emplace(TEXT("Click: take it off"), GoldColor);
	}
	else
	{
		FText Reason;
		const ABeyondCharacterBase* Character = GetShownCharacter();
		if (Equipment && Equipment->CanEquip(Item, Reason))
		{
			Lines.Emplace(FString::Printf(TEXT("Click: equip on %s"), Character ? *Character->GetCharacterDisplayName().ToString() : TEXT("")), GoldColor);
		}
		else
		{
			Lines.Emplace(Reason.ToString(), WarningColor);
		}
		Lines.Emplace(TEXT("X twice: discard"), Dim);
	}
	return Lines;
}

//~ Painting

int32 UBeyondInventoryWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 BackLayer = BaseLayer + 1;
	const int32 PanelLayer = BaseLayer + 2;
	const int32 SlotLayer = BaseLayer + 3;
	const int32 IconLayer = BaseLayer + 5;
	const int32 TextLayer = BaseLayer + 7;
	const int32 TooltipLayer = BaseLayer + 10;

	const FVector2f LocalSize(AllottedGeometry.GetLocalSize());
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
	const FSlateBrush* FrameBrush = GetTextureBrush(SlotFrame);
	const FSlateBrush* GlowBrush = GetTextureBrush(Glow);

	auto DrawBrush = [&](const FSlateBrush* ShapeBrush, const FVector2f& ShapeTopLeft, const FVector2f& ShapeSize, const FLinearColor& ShapeTint, int32 ShapeLayer)
	{
		if (ShapeBrush)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, ShapeLayer, AllottedGeometry.ToPaintGeometry(ShapeSize, FSlateLayoutTransform(ShapeTopLeft)),
				ShapeBrush, ESlateDrawEffect::None, ShapeTint);
		}
	};
	auto DrawLabel = [&](const FString& Label, const FSlateFontInfo& LabelFont, const FVector2f& LabelPosition, const FLinearColor& LabelTint, float Align, int32 LabelLayer)
	{
		const FVector2f LabelSize = MeasureInventoryText(Label, LabelFont);
		const FVector2f LabelTopLeft(LabelPosition.X - LabelSize.X * Align, LabelPosition.Y);
		FSlateDrawElement::MakeText(OutDrawElements, LabelLayer, AllottedGeometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(LabelTopLeft + FVector2f(1.5f, 1.5f))),
			Label, LabelFont, ESlateDrawEffect::None, InventoryWithAlpha(FLinearColor::Black, 0.7f * LabelTint.A));
		FSlateDrawElement::MakeText(OutDrawElements, LabelLayer + 1, AllottedGeometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(LabelTopLeft)),
			Label, LabelFont, ESlateDrawEffect::None, LabelTint);
		return LabelSize;
	};
	auto DrawGlyph = [&](EBeyondItemSlot ItemSlot, bool bStaff, const FVector2f& Centre, float Size, const FLinearColor& GlyphTint, int32 GlyphLayer)
	{
		for (const TArray<FVector2f>& Line : GetInventoryGlyph(ItemSlot, bStaff))
		{
			TArray<FVector2f> Points;
			Points.Reserve(Line.Num());
			for (const FVector2f& Point : Line)
			{
				Points.Add(Centre + Point * Size);
			}
			FSlateDrawElement::MakeLines(OutDrawElements, GlyphLayer, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None,
				GlyphTint, true, FMath::Max(1.5f, Size * 0.035f));
		}
	};
	// A framed item square: frame tinted by tier, the item's icon or the slot drawing
	auto DrawItemBox = [&](const FBeyondItemInstance* Item, EBeyondItemSlot EmptySlot, bool bEmptyStaff, const FVector2f& Centre, float Size, bool bHover)
	{
		const UBeyondItemDefinition* Definition = Item ? Item->GetDefinition() : nullptr;
		const FLinearColor TierColor = Definition ? UBeyondItemLibrary::GetTierColor(Item->Tier) : FLinearColor(0.32f, 0.33f, 0.38f, 0.9f);
		const FVector2f BoxSize(Size, Size);
		if (Definition && GlowBrush && Item->Tier >= EBeyondItemTier::Rare)
		{
			DrawBrush(GlowBrush, Centre - BoxSize * 0.9f, BoxSize * 1.8f, InventoryWithAlpha(TierColor, 0.18f), PanelLayer);
		}
		if (FrameBrush)
		{
			DrawBrush(FrameBrush, Centre - BoxSize * 0.5f - FVector2f(3.0f, 3.0f), BoxSize + FVector2f(6.0f, 6.0f), InventoryWithAlpha(TierColor, bHover ? 1.0f : 0.85f), SlotLayer);
		}
		else
		{
			DrawBrush(&PanelBrush, Centre - BoxSize * 0.5f - FVector2f(3.0f, 3.0f), BoxSize + FVector2f(6.0f, 6.0f), InventoryWithAlpha(TierColor, bHover ? 1.0f : 0.85f), SlotLayer);
		}
		DrawBrush(&PanelBrush, Centre - BoxSize * 0.5f + FVector2f(3.0f, 3.0f), BoxSize - FVector2f(6.0f, 6.0f), FLinearColor(PanelColor.R, PanelColor.G, PanelColor.B, 1.0f), SlotLayer + 1);

		if (Definition && Definition->Icon)
		{
			DrawBrush(GetTextureBrush(Definition->Icon), Centre - BoxSize * 0.32f, BoxSize * 0.64f, FLinearColor::White, IconLayer);
		}
		else
		{
			const EBeyondItemSlot ItemSlot = Definition ? Definition->Slot : EmptySlot;
			const bool bStaff = Definition ? IsStaffTag(Definition->WeaponTag) : bEmptyStaff;
			DrawGlyph(ItemSlot, bStaff, Centre, Size * 0.8f, Definition ? TierColor : InventoryWithAlpha(TextColor, 0.25f), IconLayer);
		}
		if (Definition && Definition->Set)
		{
			// Set pieces carry a dot in the set's colour
			DrawBrush(&PillBrush, Centre + FVector2f(-Size * 0.5f + 6.0f, -Size * 0.5f + 6.0f), FVector2f(10.0f, 10.0f), Definition->Set->Color, IconLayer + 1);
		}
		if (bHover)
		{
			DrawBrush(&PanelBrush, Centre - BoxSize * 0.5f - FVector2f(5.0f, 5.0f), BoxSize + FVector2f(10.0f, 10.0f), InventoryWithAlpha(FLinearColor::White, 0.18f), PanelLayer);
		}
	};

	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 22);
	const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 15);
	const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);

	//~ Backdrop
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
		const FVector2f TabSize(InventoryTabWidth, InventoryTabHeight);
		DrawBrush(&PillBrush, Centre - TabSize * 0.5f - FVector2f(2.0f, 2.0f), TabSize + FVector2f(4.0f, 4.0f), InventoryWithAlpha(GoldColor, bActive ? 0.9f : 0.3f), BackLayer);
		DrawBrush(&PillBrush, Centre - TabSize * 0.5f, TabSize, bActive ? AccentColor : FLinearColor(0.12f, 0.13f, 0.18f, 0.95f), PanelLayer);
		const FString TabTitle = Tabs[Index].Title.ToString().ToUpper();
		DrawLabel(TabTitle, HeaderFont, Centre - FVector2f(0.0f, MeasureInventoryText(TabTitle, HeaderFont).Y * 0.5f), TextColor, 0.5f, TextLayer);
	}
	if (Tabs.Num() > 1)
	{
		DrawLabel(TEXT("Q"), SmallFont, GetTabCenter(0, LocalSize) - FVector2f(InventoryTabWidth * 0.5f + 22.0f, 7.0f), InventoryWithAlpha(TextColor, 0.6f), 0.5f, TextLayer);
		DrawLabel(TEXT("E"), SmallFont, GetTabCenter(Tabs.Num() - 1, LocalSize) + FVector2f(InventoryTabWidth * 0.5f + 22.0f, -7.0f), InventoryWithAlpha(TextColor, 0.6f), 0.5f, TextLayer);
	}

	const ABeyondCharacterBase* Character = GetShownCharacter();
	const UBeyondEquipmentComponent* Equipment = GetShownEquipment();
	const UBeyondInventoryComponent* Inventory = GetInventory();

	//~ Paper doll
	const FVector2f DollCentre = GetDollCenter(LocalSize);
	DrawLabel(TEXT("EQUIPMENT"), TitleFont, FVector2f(DollCentre.X, 130.0f), GoldColor, 0.5f, TextLayer);
	if (Character)
	{
		DrawLabel(FString::Printf(TEXT("%s  \u00B7  Level %d"), *Character->GetCharacterDisplayName().ToString(), Character->GetCharacterLevel()), BodyFont,
			FVector2f(DollCentre.X, 166.0f), InventoryWithAlpha(TextColor, 0.75f), 0.5f, TextLayer);
	}
	const bool bWieldsStaff = Character && IsStaffTag(Character->GetDefaultWeaponTag());
	for (const EBeyondItemSlot ItemSlot : InventorySlotOrder)
	{
		const FVector2f Centre = GetSlotCenter(ItemSlot, LocalSize);
		FBeyondItemInstance Worn;
		const bool bHasItem = Equipment && Equipment->GetEquipped(ItemSlot, Worn);
		const bool bHover = Hovered.bSlot && Hovered.ItemSlot == ItemSlot;
		DrawItemBox(bHasItem ? &Worn : nullptr, ItemSlot, bWieldsStaff, Centre, SlotSize, bHover);
		DrawLabel(UBeyondItemLibrary::GetSlotName(ItemSlot).ToString().ToUpper(), SmallFont, FVector2f(Centre.X, Centre.Y + SlotSize * 0.5f + 6.0f),
			InventoryWithAlpha(TextColor, bHasItem ? 0.85f : 0.45f), 0.5f, TextLayer);
		if (bFlashSlot && FlashSlot == ItemSlot && FlashAge >= 0.0f)
		{
			const float Alpha = FlashAge / 0.6f;
			const float Grow = SlotSize + 10.0f + 50.0f * Alpha;
			DrawBrush(&PanelBrush, Centre - FVector2f(Grow, Grow) * 0.5f, FVector2f(Grow, Grow), InventoryWithAlpha(GoldColor, 0.5f * (1.0f - Alpha)), PanelLayer);
		}
	}

	//~ Stats (right of the doll): total and the equipment's share
	const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	const FVector2f StatsTopLeft(DollCentre.X + SlotSize * 1.45f + SlotSize * 0.5f + 46.0f, 230.0f);
	DrawLabel(TEXT("STATS"), HeaderFont, StatsTopLeft, GoldColor, 0.0f, TextLayer);
	float StatTop = StatsTopLeft.Y + 32.0f;
	for (const EBeyondSkillStat Stat : InventoryStatOrder)
	{
		const float Total = ASC ? ASC->GetNumericAttribute(InventoryStatAttribute(Stat)) : 0.0f;
		const float FromGear = Equipment ? Equipment->GetStatBonus(Stat) : 0.0f;
		DrawLabel(UBeyondSkillTreeAsset::GetStatName(Stat).ToString(), BodyFont, FVector2f(StatsTopLeft.X, StatTop), InventoryWithAlpha(TextColor, 0.8f), 0.0f, TextLayer);
		DrawLabel(FString::Printf(TEXT("%.0f"), Total), HeaderFont, FVector2f(StatsTopLeft.X + 190.0f, StatTop - 3.0f), TextColor, 1.0f, TextLayer);
		if (FromGear > 0.0f)
		{
			DrawLabel(FString::Printf(TEXT("+%.0f"), FromGear), BodyFont, FVector2f(StatsTopLeft.X + 204.0f, StatTop), BetterColor, 0.0f, TextLayer);
		}
		StatTop += 28.0f;
	}
	DrawLabel(TEXT("green: from equipment"), SmallFont, FVector2f(StatsTopLeft.X, StatTop + 4.0f), InventoryWithAlpha(BetterColor, 0.6f), 0.0f, TextLayer);

	//~ Armor sets worn
	float SetTop = DollCentre.Y + SlotSize * 1.45f + SlotSize * 0.5f + 48.0f;
	const float SetLeft = DollCentre.X - SlotSize * 1.45f - SlotSize * 0.5f;
	const float SetWidth = StatsTopLeft.X + 260.0f - SetLeft;
	DrawLabel(TEXT("ARMOR SETS"), HeaderFont, FVector2f(SetLeft, SetTop), GoldColor, 0.0f, TextLayer);
	SetTop += 30.0f;
	const TArray<const UBeyondArmorSet*> Sets = Equipment ? Equipment->GetWornSets() : TArray<const UBeyondArmorSet*>();
	if (Sets.IsEmpty())
	{
		DrawLabel(TEXT("Wear 2 pieces of a set for its bonus, all 4 for its full-set effect."), BodyFont, FVector2f(SetLeft, SetTop),
			InventoryWithAlpha(TextColor, 0.55f), 0.0f, TextLayer);
	}
	for (const UBeyondArmorSet* Set : Sets)
	{
		const int32 Pieces = Equipment->GetSetPieceCount(Set);
		FString Title = FString::Printf(TEXT("%s  %d / %d"), *Set->SetName.ToString(), Pieces, UBeyondArmorSet::FullSetPieces);
		if (Set->FullSetEffect == EBeyondSetEffect::VenomInfusion && Equipment->IsInfused())
		{
			Title += TEXT("   INFUSED");
		}
		DrawLabel(Title, HeaderFont, FVector2f(SetLeft, SetTop), Set->Color, 0.0f, TextLayer);
		SetTop += 24.0f;

		TArray<FString> TwoPiece;
		for (const FBeyondItemStat& Stat : Set->TwoPieceStats)
		{
			TwoPiece.Add(UBeyondItemLibrary::FormatStat(Stat).ToString());
		}
		const bool bTwo = Pieces >= UBeyondArmorSet::PartialSetPieces;
		DrawLabel(FString::Printf(TEXT("2 pieces: %s"), *FString::Join(TwoPiece, TEXT(", "))), BodyFont, FVector2f(SetLeft + 14.0f, SetTop),
			bTwo ? TextColor : InventoryWithAlpha(TextColor, 0.4f), 0.0f, TextLayer);
		SetTop += 20.0f;
		const bool bFull = Pieces >= UBeyondArmorSet::FullSetPieces;
		for (const FString& Line : WrapInventoryText(FString::Printf(TEXT("4 pieces: %s"), *Set->FullSetDescription.ToString()), BodyFont, SetWidth - 14.0f))
		{
			DrawLabel(Line, BodyFont, FVector2f(SetLeft + 14.0f, SetTop), bFull ? TextColor : InventoryWithAlpha(TextColor, 0.4f), 0.0f, TextLayer);
			SetTop += 18.0f;
		}
		SetTop += 10.0f;
	}

	//~ The bag
	const TArray<FBeyondItemInstance> BagItems = GetSortedBagItems();
	const int32 Capacity = Inventory ? Inventory->Capacity : 0;
	const FVector2f GridTopLeft = GetGridTopLeft(LocalSize);
	DrawLabel(TEXT("BAG"), TitleFont, FVector2f(GridTopLeft.X, 130.0f), GoldColor, 0.0f, TextLayer);
	DrawLabel(FString::Printf(TEXT("%d / %d"), BagItems.Num(), Capacity), HeaderFont, FVector2f(GridTopLeft.X + GridColumns * (CellSize + InventoryCellGap) - InventoryCellGap, 138.0f),
		BagItems.Num() >= Capacity ? WarningColor : InventoryWithAlpha(TextColor, 0.8f), 1.0f, TextLayer);
	for (int32 Index = 0; Index < FMath::Max(Capacity, BagItems.Num()); ++Index)
	{
		const FVector2f TopLeft = GetCellTopLeft(Index, LocalSize);
		const FVector2f Centre = TopLeft + FVector2f(CellSize, CellSize) * 0.5f;
		if (!BagItems.IsValidIndex(Index))
		{
			DrawBrush(&PanelBrush, TopLeft, FVector2f(CellSize, CellSize), FLinearColor(0.1f, 0.11f, 0.16f, 0.55f), PanelLayer);
			continue;
		}

		const FBeyondItemInstance& Item = BagItems[Index];
		const bool bHover = Hovered.BagItem == Item.Id;
		DrawItemBox(&Item, EBeyondItemSlot::Helm, false, Centre, CellSize, bHover);
		DrawLabel(FString::FromInt(Item.ItemLevel), SmallFont, TopLeft + FVector2f(CellSize - 6.0f, CellSize - 18.0f), InventoryWithAlpha(TextColor, 0.75f), 1.0f, TextLayer);

		// Green arrow: better than what the shown demigod wears there; red cross: they can't use it
		const UBeyondItemDefinition* Definition = Item.GetDefinition();
		FText Reason;
		if (Equipment && Definition)
		{
			if (!Equipment->CanEquip(Item, Reason))
			{
				const FVector2f Mark = TopLeft + FVector2f(CellSize - 14.0f, 12.0f);
				TArray<FVector2f> CrossA = { Mark - FVector2f(5.0f, 5.0f), Mark + FVector2f(5.0f, 5.0f) };
				TArray<FVector2f> CrossB = { Mark + FVector2f(-5.0f, 5.0f), Mark + FVector2f(5.0f, -5.0f) };
				FSlateDrawElement::MakeLines(OutDrawElements, IconLayer + 1, AllottedGeometry.ToPaintGeometry(), MoveTemp(CrossA), ESlateDrawEffect::None, WarningColor, true, 2.0f);
				FSlateDrawElement::MakeLines(OutDrawElements, IconLayer + 1, AllottedGeometry.ToPaintGeometry(), MoveTemp(CrossB), ESlateDrawEffect::None, WarningColor, true, 2.0f);
			}
			else
			{
				FBeyondItemInstance Worn;
				const bool bHasWorn = Equipment->GetEquipped(Definition->Slot, Worn);
				if (!bHasWorn || InventoryItemScore(Item) > InventoryItemScore(Worn) + 0.5f)
				{
					const FVector2f Mark = TopLeft + FVector2f(CellSize - 14.0f, 14.0f);
					TArray<FVector2f> Arrow = { Mark + FVector2f(-6.0f, 3.0f), Mark + FVector2f(0.0f, -4.0f), Mark + FVector2f(6.0f, 3.0f) };
					FSlateDrawElement::MakeLines(OutDrawElements, IconLayer + 1, AllottedGeometry.ToPaintGeometry(), MoveTemp(Arrow), ESlateDrawEffect::None, BetterColor, true, 2.5f);
				}
			}
		}
		if (DiscardArmedItem == Item.Id && DiscardArmedAge >= 0.0f)
		{
			const float Pulse = 0.5f + 0.5f * FMath::Sin(AnimTime * 10.0f);
			DrawBrush(&PanelBrush, TopLeft - FVector2f(4.0f, 4.0f), FVector2f(CellSize + 8.0f, CellSize + 8.0f), InventoryWithAlpha(WarningColor, 0.3f + 0.4f * Pulse), PanelLayer);
		}
	}

	//~ Tooltip
	const FBeyondItemInstance* TooltipItem = nullptr;
	FBeyondItemInstance WornForTooltip;
	if (Hovered.bSlot && Equipment && Equipment->GetEquipped(Hovered.ItemSlot, WornForTooltip))
	{
		TooltipItem = &WornForTooltip;
	}
	else if (Hovered.BagItem.IsValid())
	{
		TooltipItem = BagItems.FindByPredicate([this](const FBeyondItemInstance& Item) { return Item.Id == Hovered.BagItem; });
	}
	if (TooltipItem)
	{
		const TArray<TPair<FString, FLinearColor>> Described = DescribeItem(*TooltipItem, Hovered.bSlot);
		TArray<TPair<FString, FLinearColor>> Lines;
		for (const TPair<FString, FLinearColor>& Line : Described)
		{
			for (const FString& Wrapped : WrapInventoryText(Line.Key, BodyFont, InventoryTooltipWidth - 32.0f))
			{
				Lines.Emplace(Wrapped, Line.Value);
			}
		}

		const float LineHeight = 18.0f;
		const float Height = 46.0f + Lines.Num() * LineHeight + 12.0f;
		float Left = HoverPosition.X + 24.0f;
		if (Left + InventoryTooltipWidth > LocalSize.X - 20.0f)
		{
			Left = HoverPosition.X - 24.0f - InventoryTooltipWidth;
		}
		const float Top = FMath::Clamp(HoverPosition.Y - 30.0f, 20.0f, LocalSize.Y - Height - 20.0f);
		const FLinearColor TierColor = UBeyondItemLibrary::GetTierColor(TooltipItem->Tier);
		DrawBrush(&PanelBrush, FVector2f(Left - 2.0f, Top - 2.0f), FVector2f(InventoryTooltipWidth + 4.0f, Height + 4.0f), InventoryWithAlpha(TierColor, 0.9f), TooltipLayer);
		DrawBrush(&PanelBrush, FVector2f(Left, Top), FVector2f(InventoryTooltipWidth, Height), PanelColor, TooltipLayer + 1);
		DrawLabel(UBeyondItemLibrary::GetItemName(*TooltipItem).ToString(), HeaderFont, FVector2f(Left + 16.0f, Top + 12.0f), TierColor, 0.0f, TooltipLayer + 2);
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
			InventoryWithAlpha(bMessageWarning ? WarningColor : GoldColor, Alpha), 0.5f, TooltipLayer);
	}
	DrawLabel(TEXT("LMB: equip / take off    Q / E: switch demigod    X twice: discard    K: skill tree    I / Esc: close"), BodyFont,
		FVector2f(LocalSize.X * 0.5f, LocalSize.Y - 46.0f), InventoryWithAlpha(TextColor, 0.65f), 0.5f, TextLayer);

	return TooltipLayer + 3;
}

#undef LOCTEXT_NAMESPACE
