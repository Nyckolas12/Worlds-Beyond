// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/BeyondEquipmentComponent.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "AbilitySystem/BeyondGameplayEffects.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "BeyondGameplayTags.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Game/BeyondCombatSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Items/BeyondInventoryComponent.h"
#include "Items/BeyondItemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Progression/BeyondStatEffects.h"
#include "TimerManager.h"
#include "WorldBeyond.h"

UBeyondEquipmentComponent::UBeyondEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBeyondEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		HitLandedHandle = Combat->OnHitLanded.AddUObject(this, &ThisClass::HandleHitLanded);
	}
	if (const ABeyondCharacterBase* Character = GetCharacter())
	{
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		{
			AbilityActivatedHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &ThisClass::HandleAbilityActivated);
		}
	}
}

void UBeyondEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		Combat->OnHitLanded.Remove(HitLandedHandle);
	}
	if (const ABeyondCharacterBase* Character = GetCharacter())
	{
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		{
			ASC->AbilityActivatedCallbacks.Remove(AbilityActivatedHandle);
		}
	}
	EndInfusion();
	Super::EndPlay(EndPlayReason);
}

ABeyondCharacterBase* UBeyondEquipmentComponent::GetCharacter() const
{
	return Cast<ABeyondCharacterBase>(GetOwner());
}

UBeyondInventoryComponent* UBeyondEquipmentComponent::GetInventory() const
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	return PC ? PC->FindComponentByClass<UBeyondInventoryComponent>() : nullptr;
}

bool UBeyondEquipmentComponent::GetEquipped(EBeyondItemSlot Slot, FBeyondItemInstance& OutItem) const
{
	if (const FBeyondItemInstance* Item = Equipped.Find(Slot))
	{
		OutItem = *Item;
		return true;
	}
	return false;
}

bool UBeyondEquipmentComponent::CanEquip(const FBeyondItemInstance& Item, FText& OutReason) const
{
	return UBeyondItemLibrary::CanCharacterUse(Item, GetCharacter(), OutReason);
}

bool UBeyondEquipmentComponent::EquipItem(const FBeyondItemInstance& Item)
{
	FText Reason;
	const UBeyondItemDefinition* Definition = Item.GetDefinition();
	if (!Definition || !CanEquip(Item, Reason))
	{
		return false;
	}

	FBeyondItemInstance Previous;
	if (Equipped.RemoveAndCopyValue(Definition->Slot, Previous))
	{
		if (UBeyondInventoryComponent* Inventory = GetInventory())
		{
			Inventory->ReturnItem(Previous);
		}
	}
	Equipped.Add(Definition->Slot, Item);
	ApplyEffects();
	UE_LOG(LogBeyond, Log, TEXT("%s equips %s (%s)"), *GetNameSafe(GetOwner()), *UBeyondItemLibrary::GetItemName(Item).ToString(),
		*UBeyondItemLibrary::GetTierName(Item.Tier).ToString());
	OnEquipmentChanged.Broadcast(this);
	return true;
}

bool UBeyondEquipmentComponent::EquipFromInventory(FGuid ItemId)
{
	UBeyondInventoryComponent* Inventory = GetInventory();
	FBeyondItemInstance Item;
	FText Reason;
	if (!Inventory || !Inventory->FindItem(ItemId, Item) || !CanEquip(Item, Reason))
	{
		return false;
	}
	Inventory->RemoveItem(ItemId, Item);
	return EquipItem(Item);
}

bool UBeyondEquipmentComponent::Unequip(EBeyondItemSlot Slot)
{
	FBeyondItemInstance Item;
	if (!Equipped.RemoveAndCopyValue(Slot, Item))
	{
		return false;
	}
	if (UBeyondInventoryComponent* Inventory = GetInventory())
	{
		Inventory->ReturnItem(Item);
	}
	ApplyEffects();
	OnEquipmentChanged.Broadcast(this);
	return true;
}

void UBeyondEquipmentComponent::RestoreEquipped(const TMap<EBeyondItemSlot, FBeyondItemInstance>& SavedItems)
{
	Equipped.Reset();
	for (const TPair<EBeyondItemSlot, FBeyondItemInstance>& Pair : SavedItems)
	{
		const UBeyondItemDefinition* Definition = Pair.Value.IsValid() ? Pair.Value.GetDefinition() : nullptr;
		if (Definition && Definition->Slot == Pair.Key)
		{
			Equipped.Add(Pair.Key, Pair.Value);
		}
	}
	ApplyEffects();
	OnEquipmentChanged.Broadcast(this);
}

int32 UBeyondEquipmentComponent::GetSetPieceCount(const UBeyondArmorSet* Set) const
{
	int32 Count = 0;
	for (const TPair<EBeyondItemSlot, FBeyondItemInstance>& Pair : Equipped)
	{
		const UBeyondItemDefinition* Definition = Pair.Value.GetDefinition();
		if (Set && Definition && Definition->Set == Set)
		{
			++Count;
		}
	}
	return Count;
}

TArray<const UBeyondArmorSet*> UBeyondEquipmentComponent::GetWornSets() const
{
	TArray<const UBeyondArmorSet*> Sets;
	for (const TPair<EBeyondItemSlot, FBeyondItemInstance>& Pair : Equipped)
	{
		const UBeyondItemDefinition* Definition = Pair.Value.GetDefinition();
		if (Definition && Definition->Set)
		{
			Sets.AddUnique(Definition->Set);
		}
	}
	return Sets;
}

const UBeyondArmorSet* UBeyondEquipmentComponent::FindFullSet(EBeyondSetEffect Effect) const
{
	for (const UBeyondArmorSet* Set : GetWornSets())
	{
		if (Set->FullSetEffect == Effect && GetSetPieceCount(Set) >= UBeyondArmorSet::FullSetPieces)
		{
			return Set;
		}
	}
	return nullptr;
}

float UBeyondEquipmentComponent::GetStatBonus(EBeyondSkillStat Stat) const
{
	float Total = 0.0f;
	for (const TPair<EBeyondItemSlot, FBeyondItemInstance>& Pair : Equipped)
	{
		Total += UBeyondItemLibrary::GetItemStat(Pair.Value, Stat);
	}
	for (const UBeyondArmorSet* Set : GetWornSets())
	{
		if (GetSetPieceCount(Set) >= UBeyondArmorSet::PartialSetPieces)
		{
			for (const FBeyondItemStat& Bonus : Set->TwoPieceStats)
			{
				if (Bonus.Stat == Stat)
				{
					Total += Bonus.Value;
				}
			}
		}
	}
	return Total;
}

void UBeyondEquipmentComponent::ApplyEffects()
{
	ABeyondCharacterBase* Character = GetCharacter();
	if (!Character || !Character->HasAuthority())
	{
		return;
	}

	TMap<EBeyondSkillStat, float> Bonuses;
	for (const EBeyondSkillStat Stat : { EBeyondSkillStat::MaxHealth, EBeyondSkillStat::MaxStamina, EBeyondSkillStat::Strength,
		EBeyondSkillStat::Arcana, EBeyondSkillStat::Defense })
	{
		Bonuses.Add(Stat, GetStatBonus(Stat));
	}
	StatsHandle = BeyondStats::ApplyStatBonuses(Character->GetAbilitySystemComponent(), UBeyondGE_EquipmentStats::StaticClass(), Bonuses, StatsHandle);

	// Took a Venomweave piece off mid-infusion
	if (IsInfused() && !HasFullSet(EBeyondSetEffect::VenomInfusion))
	{
		EndInfusion();
	}
}

float UBeyondEquipmentComponent::GetDamageTakenMultiplier() const
{
	const UBeyondArmorSet* Sun = FindFullSet(EBeyondSetEffect::Sunfire);
	const ABeyondCharacterBase* Character = GetCharacter();
	if (!Sun || !Character || Sun->Effect.DamageTakenReduction <= 0.0f)
	{
		return 1.0f;
	}
	const float MaxHealth = UBeyondCombatLibrary::GetActorMaxHealth(Character);
	const float Fraction = MaxHealth > 0.0f ? UBeyondCombatLibrary::GetActorHealth(Character) / MaxHealth : 0.0f;
	return Fraction > Sun->Effect.ReductionHealthThreshold ? 1.0f - Sun->Effect.DamageTakenReduction : 1.0f;
}

float UBeyondEquipmentComponent::GetProcScale() const
{
	const ABeyondCharacterBase* Character = GetCharacter();
	return 1.0f + 0.1f * FMath::Max((Character ? Character->GetCharacterLevel() : 1) - 1, 0);
}

bool UBeyondEquipmentComponent::IsInfused() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < InfusedUntil;
}

bool UBeyondEquipmentComponent::StartInfusion()
{
	const UBeyondArmorSet* Venom = FindFullSet(EBeyondSetEffect::VenomInfusion);
	ABeyondCharacterBase* Character = GetCharacter();
	UWorld* World = GetWorld();
	if (!Venom || !Character || !World || World->GetTimeSeconds() < NextInfusionTime)
	{
		return false;
	}

	EndInfusion();
	const float Now = World->GetTimeSeconds();
	InfusedUntil = Now + Venom->Effect.InfusionDuration;
	NextInfusionTime = Now + Venom->Effect.InfusionCooldown;
	USceneComponent* AttachTo = Character->GetCombatMesh() ? static_cast<USceneComponent*>(Character->GetCombatMesh()) : Character->GetRootComponent();
	InfusionAura = BeyondFX::SpawnAttached(Venom->Effect.AuraFX, AttachTo);
	World->GetTimerManager().SetTimer(InfusionTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		EndInfusion();
	}), FMath::Max(Venom->Effect.InfusionDuration, 0.05f), false);
	UE_LOG(LogBeyond, Log, TEXT("%s: Venom Infusion for %.0f s"), *GetNameSafe(Character), Venom->Effect.InfusionDuration);
	return true;
}

void UBeyondEquipmentComponent::EndInfusion()
{
	if (UFXSystemComponent* Aura = InfusionAura.Get())
	{
		Aura->DestroyComponent();
	}
	InfusionAura.Reset();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InfusionTimer);
		InfusedUntil = FMath::Min(InfusedUntil, World->GetTimeSeconds());
	}
}

void UBeyondEquipmentComponent::HandleAbilityActivated(UGameplayAbility* Ability)
{
	if (!Ability || !HasFullSet(EBeyondSetEffect::VenomInfusion))
	{
		return;
	}

	// Only the ability slots (Q / E / R) coat the weapon: not the combo, dodges or the duo move
	static const FGameplayTag InputRoot = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input"));
	const FGameplayTag Slots[] = { BeyondTags::Ability_Input_Q, BeyondTags::Ability_Input_E, BeyondTags::Ability_Input_R };
	const FGameplayAbilitySpec* Spec = Ability->GetCurrentAbilitySpec();
	bool bOnSlot = false;
	if (Spec && Spec->GetDynamicSpecSourceTags().HasTag(InputRoot))
	{
		for (const FGameplayTag& Slot : Slots)
		{
			bOnSlot |= Spec->GetDynamicSpecSourceTags().HasTagExact(Slot);
		}
	}
	else if (const UBeyondGameplayAbility* BeyondAbility = Cast<UBeyondGameplayAbility>(Ability))
	{
		for (const FGameplayTag& Slot : Slots)
		{
			bOnSlot |= BeyondAbility->InputTag == Slot;
		}
	}
	if (bOnSlot)
	{
		StartInfusion();
	}
}

bool UBeyondEquipmentComponent::IsFromWearer(const AActor* DamageInstigator) const
{
	const AActor* Wearer = GetOwner();
	// Projectiles and weapons report themselves; walk up to whoever fired them
	for (int32 Depth = 0; DamageInstigator && Depth < 4; ++Depth)
	{
		if (DamageInstigator == Wearer)
		{
			return true;
		}
		DamageInstigator = DamageInstigator->GetInstigator() && DamageInstigator->GetInstigator() != DamageInstigator
			? DamageInstigator->GetInstigator() : DamageInstigator->GetOwner();
	}
	return false;
}

void UBeyondEquipmentComponent::HandleHitLanded(AActor* DamageInstigator, AActor* Target, float Damage, const FGameplayTagContainer& DamageTags)
{
	ABeyondCharacterBase* Character = GetCharacter();
	if (Damage <= 0.0f || !Target || !Character || DamageTags.HasTag(BeyondTags::DamageType_Proc) || !IsFromWearer(DamageInstigator)
		|| UBeyondCombatLibrary::IsActorDead(Character) || !UBeyondCombatLibrary::AreHostile(Character, Target))
	{
		return;
	}

	if (const UBeyondArmorSet* Venom = FindFullSet(EBeyondSetEffect::VenomInfusion); Venom && IsInfused())
	{
		ApplyDamageOverTime(Target, Venom->Effect, BeyondTags::DamageType_Proc_Poison, PoisonEffects);
	}
	if (const UBeyondArmorSet* Sun = FindFullSet(EBeyondSetEffect::Sunfire))
	{
		ApplyDamageOverTime(Target, Sun->Effect, BeyondTags::DamageType_Proc_Burn, BurnEffects);
	}
	if (const UBeyondArmorSet* Storm = FindFullSet(EBeyondSetEffect::ChainLightning))
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextChainTime && FMath::FRand() < Storm->Effect.ProcChance)
		{
			NextChainTime = Now + Storm->Effect.ProcCooldown;
			ChainLightning(Target, Storm->Effect);
		}
	}
}

void UBeyondEquipmentComponent::ApplyDamageOverTime(AActor* Target, const FBeyondSetEffectSettings& Settings, const FGameplayTag& DamageTag,
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle>& Active)
{
	ABeyondCharacterBase* Character = GetCharacter();
	UAbilitySystemComponent* SourceASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!SourceASC || !TargetASC || UBeyondCombatLibrary::IsActorDead(Target) || Settings.DamagePerSecond <= 0.0f || Settings.DotDuration <= 0.0f)
	{
		return;
	}

	// A new hit refreshes the DoT instead of stacking another one
	bool bWasActive = false;
	if (const FActiveGameplayEffectHandle* Existing = Active.Find(TargetASC); Existing && TargetASC->GetActiveGameplayEffect(*Existing))
	{
		bWasActive = true;
		TargetASC->RemoveActiveGameplayEffect(*Existing);
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(Character, Character);
	const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UBeyondGE_DamageOverTime::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Damage, Settings.DamagePerSecond * GetProcScale());
	Spec.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Duration, Settings.DotDuration);
	Spec.Data->AddDynamicAssetTag(DamageTag);
	Active.Add(TargetASC, SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC));

	if (!bWasActive)
	{
		BeyondFX::SpawnAttached(Settings.TargetFX, Target->GetRootComponent());
	}
}

void UBeyondEquipmentComponent::ChainLightning(AActor* Target, const FBeyondSetEffectSettings& Settings)
{
	ABeyondCharacterBase* Character = GetCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World || Settings.MaxTargets <= 0)
	{
		return;
	}

	const FVector Origin = Target->GetActorLocation();
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondChainLightning), false, Character);
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Settings.Radius), Params);

	TArray<AActor*> Targets;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor && Actor != Target && !Targets.Contains(Actor) && UBeyondCombatLibrary::AreHostile(Character, Actor) && !UBeyondCombatLibrary::IsActorDead(Actor))
		{
			Targets.Add(Actor);
		}
	}
	Targets.Sort([&Origin](const AActor& A, const AActor& B)
	{
		return FVector::DistSquared(Origin, A.GetActorLocation()) < FVector::DistSquared(Origin, B.GetActorLocation());
	});
	if (Targets.Num() > Settings.MaxTargets)
	{
		Targets.SetNum(Settings.MaxTargets);
	}

	BeyondFX::SpawnAtLocation(this, Settings.ProcFX, Origin);
	for (AActor* Struck : Targets)
	{
		BeyondFX::SpawnAtLocation(this, Settings.ProcFX, Struck->GetActorLocation());
		UBeyondCombatLibrary::ApplyDamage(Character, Struck, Settings.ProcDamage * GetProcScale(), BeyondTags::DamageType_Proc_Lightning,
			BeyondTags::Event_Hit_Light, true);
	}
}
