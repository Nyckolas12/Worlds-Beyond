// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondFX.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Progression/BeyondSkillTree.h"
#include "BeyondItemTypes.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EBeyondItemSlot : uint8
{
	Helm,
	Chest,
	Gauntlets,
	Boots,
	Weapon
};

UENUM(BlueprintType)
enum class EBeyondItemTier : uint8
{
	Common,
	Uncommon,
	Rare,
	Epic,
	Legendary
};

/** What a full armor set (all four pieces) does */
UENUM(BlueprintType)
enum class EBeyondSetEffect : uint8
{
	None,
	// Using Q / E / R coats weapon and spells in venom for a while: hits poison
	VenomInfusion,
	// Hits may arc lightning to nearby enemies
	ChainLightning,
	// Hits burn; above a health threshold the wearer takes less damage
	Sunfire
};

USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondItemStat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Item")
	EBeyondSkillStat Stat = EBeyondSkillStat::MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Item")
	float Value = 0.0f;
};

/** Numbers and effects of a full-set bonus (which ones matter depends on the effect) */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondSetEffectSettings
{
	GENERATED_BODY()

	// Venom Infusion: how long a Q / E / R use keeps weapon and spells coated, and how often it can start
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set Effect", meta = (ClampMin = "0"))
	float InfusionDuration = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set Effect", meta = (ClampMin = "0"))
	float InfusionCooldown = 12.0f;

	// Chance per hit (1 = every hit) and the shortest time between two procs
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set Effect", meta = (ClampMin = "0", ClampMax = "1"))
	float ProcChance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set Effect", meta = (ClampMin = "0"))
	float ProcCooldown = 0.0f;

	// Poison / burn: damage per second and seconds (a new hit refreshes it)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set Effect", meta = (ClampMin = "0"))
	float DamagePerSecond = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set Effect", meta = (ClampMin = "0"))
	float DotDuration = 4.0f;

	// Chain lightning: damage at each enemy, how far it jumps, how many enemies
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set Effect", meta = (ClampMin = "0"))
	float ProcDamage = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set Effect", meta = (ClampMin = "0"))
	float Radius = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set Effect", meta = (ClampMin = "0"))
	int32 MaxTargets = 3;

	// Sunfire: damage taken x (1 - this) while health is above Reduction Health Threshold (fraction of max)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set Effect", meta = (ClampMin = "0", ClampMax = "0.9"))
	float DamageTakenReduction = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set Effect", meta = (ClampMin = "0", ClampMax = "1"))
	float ReductionHealthThreshold = 0.5f;

	// On the wearer while infused (its Max Lifetime should match Infusion Duration)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set Effect")
	FBeyondFX AuraFX;

	// On a poisoned / burning enemy (Max Lifetime about the DoT's duration)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set Effect")
	FBeyondFX TargetFX;

	// At each enemy the chain lightning reaches
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set Effect")
	FBeyondFX ProcFX;
};

/** An armor set: a bonus at 2 pieces and a full-set effect at 4 */
UCLASS(BlueprintType)
class WORLDBEYOND_API UBeyondArmorSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static constexpr int32 FullSetPieces = 4;
	static constexpr int32 PartialSetPieces = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set")
	FText SetName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set")
	FLinearColor Color = FLinearColor(0.3f, 1.0f, 0.4f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set")
	TArray<FBeyondItemStat> TwoPieceStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set")
	EBeyondSetEffect FullSetEffect = EBeyondSetEffect::None;

	// What the full set does, for tooltips
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set", meta = (MultiLine = "true"))
	FText FullSetDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set")
	FBeyondSetEffectSettings Effect;
};

/** One kind of item (Venomweave Helm, Gilded Saber); drops are instances of it with a tier and a level */
UCLASS(BlueprintType)
class WORLDBEYOND_API UBeyondItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true"))
	FText Description;

	// Empty: the screens draw the slot's shape in the tier colour
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EBeyondItemSlot Slot = EBeyondItemSlot::Helm;

	// Weapons: who can use it, matched against the demigod's Default Weapon Tag (Weapon.Melee.Sword: Ji-Woong,
	// Weapon.Ranged.Staff: Angel)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (Categories = "Weapon", EditCondition = "Slot == EBeyondItemSlot::Weapon"))
	FGameplayTag WeaponTag;

	// Stats at item level 1 and Common tier; tier and level scale them (Project Settings -> Worlds Beyond Loot)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TArray<FBeyondItemStat> BaseStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UBeyondArmorSet> Set;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Drops")
	EBeyondItemTier MinTier = EBeyondItemTier::Common;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Drops")
	EBeyondItemTier MaxTier = EBeyondItemTier::Legendary;

	// Relative chance to be picked from the database (0: never drops randomly)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Drops", meta = (ClampMin = "0"))
	float DropWeight = 1.0f;

	// Not used yet: armor / weapon art for when the items should change how the demigods look
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Visual")
	TSoftObjectPtr<UObject> VisualMesh;
};

/** Every item that can drop (the loot pool) */
UCLASS(BlueprintType)
class WORLDBEYOND_API UBeyondItemDatabase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	TArray<TObjectPtr<UBeyondItemDefinition>> Items;
};

/** One item the party owns: a definition rolled at a tier and level, with any bonus stats */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondItemInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Item")
	FGuid Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Item")
	TSoftObjectPtr<UBeyondItemDefinition> Definition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Item")
	EBeyondItemTier Tier = EBeyondItemTier::Common;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Item")
	int32 ItemLevel = 1;

	// Extra stats Epic and Legendary drops roll
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Item")
	TArray<FBeyondItemStat> BonusStats;

	bool IsValid() const { return Id.IsValid() && !Definition.IsNull(); }
	const UBeyondItemDefinition* GetDefinition() const { return Definition.LoadSynchronous(); }
};
