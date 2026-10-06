// Fill out your copyright notice in the Description page of Project Settings.

#include "Progression/BeyondSkillTreeComponent.h"
#include "AbilitySystem/Abilities/BeyondGA_DuoStrike.h"
#include "AbilitySystem/BeyondGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "BeyondGameplayTags.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "GameFramework/Controller.h"
#include "Player/BeyondPartyComponent.h"
#include "WorldBeyond.h"

#define LOCTEXT_NAMESPACE "BeyondSkillTree"

namespace
{
	FGameplayTag SkillStatTag(EBeyondSkillStat Stat)
	{
		switch (Stat)
		{
		case EBeyondSkillStat::MaxHealth: return BeyondTags::SetByCaller_MaxHealth;
		case EBeyondSkillStat::MaxStamina: return BeyondTags::SetByCaller_MaxStamina;
		case EBeyondSkillStat::Strength: return BeyondTags::SetByCaller_Strength;
		case EBeyondSkillStat::Arcana: return BeyondTags::SetByCaller_Arcana;
		case EBeyondSkillStat::Defense: return BeyondTags::SetByCaller_Defense;
		}
		return FGameplayTag();
	}
}

UBeyondSkillTreeComponent::UBeyondSkillTreeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ABeyondCharacterBase* UBeyondSkillTreeComponent::GetCharacter() const
{
	return Cast<ABeyondCharacterBase>(GetOwner());
}

void UBeyondSkillTreeComponent::SetTree(UBeyondSkillTreeAsset* NewTree)
{
	if (Tree == NewTree)
	{
		return;
	}
	Tree = NewTree;
	// Ranks of nodes the new tree doesn't have are dropped (their points are lost; trees are swapped in the editor only)
	RestoreRanks(TMap<FName, int32>(Ranks));
}

int32 UBeyondSkillTreeComponent::GetRank(FName NodeId) const
{
	const int32* Rank = Ranks.Find(NodeId);
	return Rank ? *Rank : 0;
}

bool UBeyondSkillTreeComponent::MeetsRequirements(const FBeyondSkillNode& Node) const
{
	if (GetTreeLevel() < Node.RequiredLevel)
	{
		return false;
	}
	for (const FName& Required : Node.Requires)
	{
		if (GetRank(Required) <= 0)
		{
			return false;
		}
	}
	return true;
}

EBeyondSkillNodeState UBeyondSkillTreeComponent::GetNodeState(FName NodeId) const
{
	const FBeyondSkillNode* Node = Tree ? Tree->FindNode(NodeId) : nullptr;
	if (!Node)
	{
		return EBeyondSkillNodeState::Locked;
	}
	const int32 Rank = GetRank(NodeId);
	if (Rank >= Node->MaxRank)
	{
		return EBeyondSkillNodeState::Maxed;
	}
	if (Rank > 0)
	{
		return EBeyondSkillNodeState::Acquired;
	}
	return MeetsRequirements(*Node) ? EBeyondSkillNodeState::Available : EBeyondSkillNodeState::Locked;
}

FText UBeyondSkillTreeComponent::GetCurrencyName() const
{
	return Tree && Tree->Currency == EBeyondSkillCurrency::BondPoints ? LOCTEXT("BondPoints", "Bond Points") : LOCTEXT("SkillPoints", "Skill Points");
}

bool UBeyondSkillTreeComponent::CanUnlock(FName NodeId, FText& OutReason) const
{
	const FBeyondSkillNode* Node = Tree ? Tree->FindNode(NodeId) : nullptr;
	if (!Node)
	{
		OutReason = LOCTEXT("Unknown", "Unknown skill");
		return false;
	}
	if (GetRank(NodeId) >= Node->MaxRank)
	{
		OutReason = LOCTEXT("Maxed", "Mastered");
		return false;
	}
	if (GetTreeLevel() < Node->RequiredLevel)
	{
		OutReason = FText::Format(LOCTEXT("NeedsLevel", "Requires level {0}"), FText::AsNumber(Node->RequiredLevel));
		return false;
	}
	for (const FName& Required : Node->Requires)
	{
		if (GetRank(Required) <= 0)
		{
			const FBeyondSkillNode* RequiredNode = Tree->FindNode(Required);
			OutReason = FText::Format(LOCTEXT("NeedsSkill", "Requires {0}"), RequiredNode ? RequiredNode->DisplayName : FText::FromName(Required));
			return false;
		}
	}
	if (GetAvailablePoints() < Node->Cost)
	{
		OutReason = FText::Format(LOCTEXT("NeedsPoints", "Needs {0} {1}"), FText::AsNumber(Node->Cost), GetCurrencyName());
		return false;
	}
	OutReason = FText::GetEmpty();
	return true;
}

bool UBeyondSkillTreeComponent::Unlock(FName NodeId)
{
	FText Reason;
	const FBeyondSkillNode* Node = Tree ? Tree->FindNode(NodeId) : nullptr;
	if (!Node || !CanUnlock(NodeId, Reason) || !PayPoints(Node->Cost))
	{
		return false;
	}

	const int32 NewRank = GetRank(NodeId) + 1;
	Ranks.Add(NodeId, NewRank);
	ApplyEffects();

	UE_LOG(LogBeyond, Log, TEXT("Skill tree %s: %s rank %d (%d %s left)"), *GetNameSafe(Tree), *NodeId.ToString(), NewRank,
		GetAvailablePoints(), *GetCurrencyName().ToString());
	OnSkillUnlocked.Broadcast(this, NodeId, NewRank);
	BroadcastChanged();
	return true;
}

int32 UBeyondSkillTreeComponent::ResetTree()
{
	const int32 Refund = GetSpentPoints();
	Ranks.Reset();
	RefundPoints(Refund);
	ApplyEffects();
	BroadcastChanged();
	return Refund;
}

int32 UBeyondSkillTreeComponent::GetSpentPoints() const
{
	int32 Spent = 0;
	for (const TPair<FName, int32>& Pair : Ranks)
	{
		if (const FBeyondSkillNode* Node = Tree ? Tree->FindNode(Pair.Key) : nullptr)
		{
			Spent += Node->Cost * Pair.Value;
		}
	}
	return Spent;
}

void UBeyondSkillTreeComponent::RestoreRanks(const TMap<FName, int32>& SavedRanks)
{
	Ranks.Reset();
	for (const TPair<FName, int32>& Pair : SavedRanks)
	{
		const FBeyondSkillNode* Node = Tree ? Tree->FindNode(Pair.Key) : nullptr;
		if (!Tree || (Node && Pair.Value > 0))
		{
			Ranks.Add(Pair.Key, Node ? FMath::Clamp(Pair.Value, 0, Node->MaxRank) : Pair.Value);
		}
	}
	ApplyEffects();
	BroadcastChanged();
}

void UBeyondSkillTreeComponent::BroadcastChanged()
{
	OnSkillTreeChanged.Broadcast(this);
}

float UBeyondSkillTreeComponent::GetStatBonus(EBeyondSkillStat Stat) const
{
	float Total = 0.0f;
	for (const TPair<FName, int32>& Pair : Ranks)
	{
		if (const FBeyondSkillNode* Node = Tree ? Tree->FindNode(Pair.Key) : nullptr)
		{
			for (const FBeyondSkillEffect& Effect : Node->Effects)
			{
				if (Effect.Type == EBeyondSkillEffectType::Stat && Effect.Stat == Stat)
				{
					Total += Effect.Value * Pair.Value;
				}
			}
		}
	}
	return Total;
}

float UBeyondSkillTreeComponent::GetEffectTotal(EBeyondSkillEffectType Type) const
{
	float Total = 0.0f;
	for (const TPair<FName, int32>& Pair : Ranks)
	{
		if (const FBeyondSkillNode* Node = Tree ? Tree->FindNode(Pair.Key) : nullptr)
		{
			for (const FBeyondSkillEffect& Effect : Node->Effects)
			{
				if (Effect.Type == Type)
				{
					Total += Effect.Value * Pair.Value;
				}
			}
		}
	}
	return Total;
}

int32 UBeyondSkillTreeComponent::GetAbilityRankBonus(const UClass* AbilityClass) const
{
	int32 Bonus = 0;
	for (const TPair<FName, int32>& Pair : Ranks)
	{
		if (const FBeyondSkillNode* Node = Tree ? Tree->FindNode(Pair.Key) : nullptr)
		{
			for (const FBeyondSkillEffect& Effect : Node->Effects)
			{
				if (Effect.Type == EBeyondSkillEffectType::AbilityRank && Effect.Ability && AbilityClass && AbilityClass->IsChildOf(Effect.Ability))
				{
					Bonus += Pair.Value;
				}
			}
		}
	}
	return Bonus;
}

bool UBeyondSkillTreeComponent::TargetsAbility(const UClass* AbilityClass) const
{
	if (!Tree || !AbilityClass)
	{
		return false;
	}
	for (const FBeyondSkillNode& Node : Tree->Nodes)
	{
		for (const FBeyondSkillEffect& Effect : Node.Effects)
		{
			if (Effect.Type == EBeyondSkillEffectType::AbilityRank && Effect.Ability && AbilityClass->IsChildOf(Effect.Ability))
			{
				return true;
			}
		}
	}
	return false;
}

TArray<TSubclassOf<UGameplayAbility>> UBeyondSkillTreeComponent::GetUnlockedAbilities(EBeyondSkillEffectType Type) const
{
	TArray<TSubclassOf<UGameplayAbility>> Result;
	if (!Tree)
	{
		return Result;
	}
	// In tree order, so the duo loadout cycles predictably
	for (const FBeyondSkillNode& Node : Tree->Nodes)
	{
		if (GetRank(Node.Id) <= 0)
		{
			continue;
		}
		for (const FBeyondSkillEffect& Effect : Node.Effects)
		{
			if (Effect.Type == Type && Effect.Ability)
			{
				Result.AddUnique(Effect.Ability);
			}
		}
	}
	return Result;
}

int32 UBeyondSkillTreeComponent::GetAvailablePoints() const
{
	const ABeyondCharacterBase* Character = GetCharacter();
	return Character ? Character->GetSkillPoints() : 0;
}

int32 UBeyondSkillTreeComponent::GetTreeLevel() const
{
	const ABeyondCharacterBase* Character = GetCharacter();
	return Character ? Character->GetCharacterLevel() : 1;
}

bool UBeyondSkillTreeComponent::PayPoints(int32 Amount)
{
	ABeyondCharacterBase* Character = GetCharacter();
	return Character && Character->SpendSkillPoints(Amount);
}

void UBeyondSkillTreeComponent::RefundPoints(int32 Amount)
{
	if (ABeyondCharacterBase* Character = GetCharacter())
	{
		Character->AddSkillPoints(Amount);
	}
}

void UBeyondSkillTreeComponent::ApplyEffects()
{
	ABeyondCharacterBase* Character = GetCharacter();
	if (!Character || !Character->HasAuthority())
	{
		return;
	}
	ApplyStats(Character);
	const bool bAbilitiesChanged = ApplyGrantedAbilities(Character);
	ApplyAbilityRanks(Character);
	if (bAbilitiesChanged)
	{
		Character->SendAbilitiesChangedEvent();
	}
}

void UBeyondSkillTreeComponent::ApplyStats(ABeyondCharacterBase* Character)
{
	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	const float OldMaxHealth = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());
	const float OldMaxStamina = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxStaminaAttribute());

	// The new effect goes on before the old one comes off, so a max never dips and cuts the current value
	const FActiveGameplayEffectHandle OldHandle = StatsHandle;
	StatsHandle.Invalidate();

	static const EBeyondSkillStat AllStats[] = { EBeyondSkillStat::MaxHealth, EBeyondSkillStat::MaxStamina, EBeyondSkillStat::Strength,
		EBeyondSkillStat::Arcana, EBeyondSkillStat::Defense };
	bool bAnyStat = false;
	for (const EBeyondSkillStat Stat : AllStats)
	{
		bAnyStat |= !FMath::IsNearlyZero(GetStatBonus(Stat));
	}
	if (bAnyStat)
	{
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UBeyondGE_SkillStats::StaticClass(), 1.0f, ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			for (const EBeyondSkillStat Stat : AllStats)
			{
				Spec.Data->SetSetByCallerMagnitude(SkillStatTag(Stat), GetStatBonus(Stat));
			}
			StatsHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}
	if (OldHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(OldHandle);
	}

	// A bigger maximum comes with the extra health / stamina (taking it back is the clamp's job)
	if (!ASC->HasMatchingGameplayTag(BeyondTags::State_Dead))
	{
		const float HealthGain = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute()) - OldMaxHealth;
		if (HealthGain > 0.0f)
		{
			ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(),
				ASC->GetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute()) + HealthGain);
		}
		const float StaminaGain = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxStaminaAttribute()) - OldMaxStamina;
		if (StaminaGain > 0.0f)
		{
			ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentStaminaAttribute(),
				ASC->GetNumericAttributeBase(UCharacterAttributeSet::GetCurrentStaminaAttribute()) + StaminaGain);
		}
	}
}

bool UBeyondSkillTreeComponent::ApplyGrantedAbilities(ABeyondCharacterBase* Character)
{
	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (!ASC)
	{
		return false;
	}

	// What the taken nodes grant, and on which slot
	TMap<UClass*, FGameplayTag> Wanted;
	if (Tree)
	{
		for (const FBeyondSkillNode& Node : Tree->Nodes)
		{
			if (GetRank(Node.Id) <= 0)
			{
				continue;
			}
			for (const FBeyondSkillEffect& Effect : Node.Effects)
			{
				if (Effect.Type == EBeyondSkillEffectType::GrantAbility && Effect.Ability)
				{
					Wanted.Add(Effect.Ability.Get(), Effect.InputTag);
				}
			}
		}
	}

	bool bChanged = false;
	for (auto It = GrantedAbilities.CreateIterator(); It; ++It)
	{
		if (!Wanted.Contains(It->Key))
		{
			ASC->ClearAbility(It->Value);
			It.RemoveCurrent();
			bChanged = true;
		}
	}
	for (const TPair<UClass*, FGameplayTag>& Pair : Wanted)
	{
		if (GrantedAbilities.Contains(Pair.Key) || ASC->FindAbilitySpecFromClass(Pair.Key))
		{
			continue;
		}
		FGameplayAbilitySpec Spec(Pair.Key, 1, INDEX_NONE, Character);
		if (Pair.Value.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(Pair.Value);
		}
		GrantedAbilities.Add(Pair.Key, ASC->GiveAbility(Spec));
		bChanged = true;
	}
	return bChanged;
}

void UBeyondSkillTreeComponent::ApplyAbilityRanks(ABeyondCharacterBase* Character) const
{
	UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}
	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability || !TargetsAbility(Spec.Ability->GetClass()))
		{
			continue;
		}
		const int32 Wanted = 1 + GetAbilityRankBonus(Spec.Ability->GetClass());
		if (Spec.Level != Wanted)
		{
			Spec.Level = Wanted;
			ASC->MarkAbilitySpecDirty(Spec);
		}
	}
}

//~ Duo tree

UBeyondPartyComponent* UBeyondDuoSkillTreeComponent::GetParty() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UBeyondPartyComponent>() : nullptr;
}

void UBeyondDuoSkillTreeComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UBeyondPartyComponent* Party = GetParty())
	{
		Party->OnLeaderChanged.AddUniqueDynamic(this, &ThisClass::HandleLeaderChanged);
	}
}

void UBeyondDuoSkillTreeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBeyondPartyComponent* Party = GetParty())
	{
		Party->OnLeaderChanged.RemoveDynamic(this, &ThisClass::HandleLeaderChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void UBeyondDuoSkillTreeComponent::HandleLeaderChanged(ABeyondCharacterBase* NewLeader, ABeyondCharacterBase* OldLeader)
{
	// The party has just formed (or swapped): make sure both demigods carry the unlocked duo powers and the loadout
	ApplyEffects();
}

int32 UBeyondDuoSkillTreeComponent::GetAvailablePoints() const
{
	const UBeyondPartyComponent* Party = GetParty();
	return Party ? Party->GetBondPoints() : 0;
}

int32 UBeyondDuoSkillTreeComponent::GetTreeLevel() const
{
	const UBeyondPartyComponent* Party = GetParty();
	return Party ? Party->GetPartyLevel() : 1;
}

bool UBeyondDuoSkillTreeComponent::PayPoints(int32 Amount)
{
	UBeyondPartyComponent* Party = GetParty();
	return Party && Party->SpendBondPoints(Amount);
}

void UBeyondDuoSkillTreeComponent::RefundPoints(int32 Amount)
{
	if (UBeyondPartyComponent* Party = GetParty())
	{
		Party->AddBondPoints(Amount);
	}
}

TSubclassOf<UGameplayAbility> UBeyondDuoSkillTreeComponent::GetStarterDuoPower() const
{
	if (Tree && Tree->StarterDuoPower)
	{
		return Tree->StarterDuoPower;
	}
	// No starter set on the tree: whatever duo move the party started with
	if (const UBeyondPartyComponent* Party = GetParty())
	{
		for (const ABeyondCharacterBase* Member : Party->GetMembers())
		{
			const UAbilitySystemComponent* ASC = Member ? Member->GetAbilitySystemComponent() : nullptr;
			if (!ASC)
			{
				continue;
			}
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->IsA<UBeyondGA_DuoStrike>() && Spec.GetDynamicSpecSourceTags().HasTagExact(BeyondTags::Ability_Input_Duo))
				{
					return Spec.Ability->GetClass();
				}
			}
		}
	}
	return nullptr;
}

TArray<TSubclassOf<UGameplayAbility>> UBeyondDuoSkillTreeComponent::GetAvailableDuoPowers() const
{
	TArray<TSubclassOf<UGameplayAbility>> Result;
	if (const TSubclassOf<UGameplayAbility> Starter = GetStarterDuoPower())
	{
		Result.Add(Starter);
	}
	for (const TSubclassOf<UGameplayAbility>& Power : GetUnlockedAbilities(EBeyondSkillEffectType::DuoPower))
	{
		Result.AddUnique(Power);
	}
	return Result;
}

TSubclassOf<UGameplayAbility> UBeyondDuoSkillTreeComponent::GetDuoLoadout() const
{
	return Loadout && GetAvailableDuoPowers().Contains(Loadout) ? Loadout : GetStarterDuoPower();
}

bool UBeyondDuoSkillTreeComponent::SetDuoLoadout(TSubclassOf<UGameplayAbility> DuoPower)
{
	if (!DuoPower || !GetAvailableDuoPowers().Contains(DuoPower))
	{
		return false;
	}
	Loadout = DuoPower;
	ApplyEffects();
	BroadcastChanged();
	UE_LOG(LogBeyond, Log, TEXT("Duo loadout: %s on the duo slot"), *GetNameSafe(DuoPower.Get()));
	return true;
}

void UBeyondDuoSkillTreeComponent::ApplyEffects()
{
	UBeyondPartyComponent* Party = GetParty();
	if (!Party)
	{
		return;
	}

	Party->SetBondModifiers(1.0f + GetEffectTotal(EBeyondSkillEffectType::BondGain), GetEffectTotal(EBeyondSkillEffectType::BondEcho));
	for (ABeyondCharacterBase* Member : Party->GetMembers())
	{
		if (Member && Member->HasAuthority())
		{
			ApplyDuoPowers(Member);
			ApplyAbilityRanks(Member);
			ApplyLoadout(Member);
		}
	}
}

void UBeyondDuoSkillTreeComponent::ApplyDuoPowers(ABeyondCharacterBase* Member)
{
	UAbilitySystemComponent* ASC = Member->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	const TArray<TSubclassOf<UGameplayAbility>> Unlocked = GetUnlockedAbilities(EBeyondSkillEffectType::DuoPower);
	TSet<UClass*> UnlockedClasses;
	for (const TSubclassOf<UGameplayAbility>& Power : Unlocked)
	{
		UnlockedClasses.Add(Power.Get());
	}
	TMap<UClass*, FGameplayAbilitySpecHandle>& Granted = GrantedPowers.FindOrAdd(Member);

	bool bChanged = false;
	for (auto It = Granted.CreateIterator(); It; ++It)
	{
		if (!UnlockedClasses.Contains(It->Key))
		{
			ASC->ClearAbility(It->Value);
			It.RemoveCurrent();
			bChanged = true;
		}
	}
	for (const TSubclassOf<UGameplayAbility>& Power : Unlocked)
	{
		if (Granted.Contains(Power.Get()) || ASC->FindAbilitySpecFromClass(Power))
		{
			continue;
		}
		// Off any key until the loadout puts it on the duo slot
		FGameplayAbilitySpec Spec(Power, 1, INDEX_NONE, Member);
		Spec.GetDynamicSpecSourceTags().AddTag(BeyondTags::Ability_Input_Unbound);
		Granted.Add(Power.Get(), ASC->GiveAbility(Spec));
		bChanged = true;
	}
	if (bChanged)
	{
		Member->SendAbilitiesChangedEvent();
	}
}

void UBeyondDuoSkillTreeComponent::ApplyLoadout(ABeyondCharacterBase* Member) const
{
	UAbilitySystemComponent* ASC = Member->GetAbilitySystemComponent();
	const TSubclassOf<UGameplayAbility> Selected = GetDuoLoadout();
	TSet<UClass*> Powers;
	for (const TSubclassOf<UGameplayAbility>& Power : GetAvailableDuoPowers())
	{
		Powers.Add(Power.Get());
	}
	if (!ASC || !Selected)
	{
		return;
	}

	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability || !Powers.Contains(Spec.Ability->GetClass()))
		{
			continue;
		}
		FGameplayTagContainer& SourceTags = Spec.GetDynamicSpecSourceTags();
		const bool bOnSlot = Spec.Ability->GetClass() == Selected.Get();
		const FGameplayTag Add = bOnSlot ? BeyondTags::Ability_Input_Duo : BeyondTags::Ability_Input_Unbound;
		const FGameplayTag Remove = bOnSlot ? BeyondTags::Ability_Input_Unbound : BeyondTags::Ability_Input_Duo;
		if (!SourceTags.HasTagExact(Add) || SourceTags.HasTagExact(Remove))
		{
			SourceTags.RemoveTag(Remove);
			SourceTags.AddTag(Add);
			ASC->MarkAbilitySpecDirty(Spec);
		}
	}
}

#undef LOCTEXT_NAMESPACE
