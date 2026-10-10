// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/BeyondFX.h"
#include "Progression/BeyondProgressionSettings.h"
#include "BeyondEnemyDefinition.generated.h"

class ABeyondEnemyCharacter;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UBeyondAbilitySet;
class UBeyondAffixDefinition;
class UBeyondItemDefinition;
class UBlendSpace;
class UMaterialInterface;
class USkeletalMesh;
class USoundBase;

/** How an enemy's brain (ABeyondEnemyController) behaves */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondEnemyAIConfig
{
	GENERATED_BODY()

	// Seconds between decisions
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0.05"))
	float ThinkInterval = 0.25f;

	// How far it sees a demigod (needs line of sight)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Senses", meta = (ClampMin = "0"))
	float SightRadius = 1800.0f;

	// Half-angle of its view cone, in degrees
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Senses", meta = (ClampMin = "0", ClampMax = "180"))
	float SightHalfAngle = 70.0f;

	// Closer than this it notices a demigod whatever way it faces
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Senses", meta = (ClampMin = "0"))
	float CloseSenseRadius = 450.0f;

	// When it starts a fight, idle enemies this close join in (packs)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Senses", meta = (ClampMin = "0"))
	float AlertRadius = 1200.0f;

	// Further than this from home it gives up and walks back (invulnerable, refilled on arrival)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Home", meta = (ClampMin = "0"))
	float LeashRadius = 3200.0f;

	// Idle wandering around home (0 stands still)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Home", meta = (ClampMin = "0"))
	float WanderRadius = 350.0f;

	// Walk speed while wandering, as a share of Walk Speed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Home", meta = (ClampMin = "0.05", ClampMax = "1"))
	float WanderSpeedScale = 0.4f;

	// Distance it fights from; 0 uses its abilities' AI ranges
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "0"))
	float PreferredRange = 0.0f;

	// Ranged / casters: backs away when a demigod comes closer than this (0 never)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "0"))
	float KeepAwayDistance = 0.0f;

	// Takes attack tokens from its target for melee attacks (only a few enemies swing at once; the rest circle)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
	bool bUsesAttackTokens = true;

	// Circling distance while it waits for a token
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "100"))
	float StrafeRadius = 380.0f;

	// Pause after an attack before the next decision
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "0"))
	float AttackRecovery = 0.4f;
};

/**
 * One enemy of the roster (Plan 3): looks, stats, abilities, reactions, brain and elite options. Applied to an
 * ABeyondEnemyCharacter before its abilities and stats are set up, so every roster entry runs on the same C++ class
 * (a Character Class Blueprint is only needed for enemies with their own graphs, like the bosses).
 * Swap Mesh / Anim / Locomotion here to replace a stand-in with real art.
 */
UCLASS(BlueprintType)
class WORLDBEYOND_API UBeyondEnemyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Used by the console (Beyond.Spawn <id>), spawners and summons
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	FName EnemyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (Categories = "Region"))
	FGameplayTag Region;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	EBeyondEnemyRank Rank = EBeyondEnemyRank::Regular;

	// How many spawn together from the console and spawners that don't set a count (packs)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "1"))
	int32 PackSize = 1;

	// Spawned class; empty uses ABeyondEnemyCharacter
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	TSoftClassPtr<ABeyondEnemyCharacter> CharacterClass;

	//~ Looks

	// Empty keeps the character class's own mesh (bosses)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looks")
	TObjectPtr<USkeletalMesh> Mesh;

	// Replaces the mesh's materials by index (empty entries keep the original)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looks")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	// Empty uses UBeyondEnemyAnimInstance (Locomotion + montages, no anim Blueprint needed)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looks")
	TSubclassOf<UAnimInstance> AnimClass;

	// Walk / run blendspace (1D speed, or 2D direction + speed) for UBeyondEnemyAnimInstance
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looks")
	TObjectPtr<UBlendSpace> Locomotion;

	// Played instead when there is no Locomotion
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looks")
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	// Region tint over the stand-in mesh (fresnel overlay; alpha = strength, 0 = none)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looks")
	FLinearColor Tint = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	// Uniform actor scale (capsule and mesh)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looks", meta = (ClampMin = "0.1"))
	float Scale = 1.0f;

	// Unscaled capsule; 0 keeps the class default
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looks", meta = (ClampMin = "0"))
	float CapsuleRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looks", meta = (ClampMin = "0"))
	float CapsuleHalfHeight = 0.0f;

	// Added to the mesh's place at the capsule's bottom (feet on the ground)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looks")
	FVector MeshOffset = FVector::ZeroVector;

	// Paragon meshes face +Y: -90 turns them to the capsule's forward
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looks")
	float MeshYaw = -90.0f;

	//~ Stats (level 1)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1"))
	float MaxHealth = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
	float Strength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
	float Arcana = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
	float Defense = 0.0f;

	// Added per level above 1
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	FBeyondStatGrowth Growth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
	float WalkSpeed = 380.0f;

	// Below 0 uses the rank's EXP (Project Settings -> Worlds Beyond Progression)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float ExperienceReward = -1.0f;

	//~ Combat

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UBeyondAbilitySet> AbilitySet;

	// Hit reactions by response (Event.Hit.Light / Stagger / Stun / KnockBack); a missing one uses Light
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (Categories = "Event.Hit"))
	TMap<FGameplayTag, TObjectPtr<UAnimMontage>> HitReactions;

	// Seconds between two light flinches (so a combo doesn't stun-lock it); stagger and stronger always react
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0"))
	float LightHitReactCooldown = 1.2f;

	// Never reacts to hits (big brutes, bosses)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bUninterruptible = false;

	// One is picked at random; without any the body ragdolls (needs a physics asset) or just stops
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TArray<TObjectPtr<UAnimMontage>> DeathMontages;

	// Played when it appears
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UAnimMontage> SpawnMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	FBeyondFX SpawnFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	FBeyondFX DeathFX;

	// Played once when it spots a demigod
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USoundBase> AggroSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	FBeyondEnemyAIConfig AI;

	//~ Elites and loot

	// Can roll as an elite (spawners' Elite Chance, Beyond.Spawn <id> <level> elite)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elite")
	bool bCanBeElite = true;

	// Affixes it may roll; empty allows every affix in the roster
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elite")
	TArray<TObjectPtr<UBeyondAffixDefinition>> AllowedAffixes;

	// Always dropped on a party kill, on top of the rank's roll
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TArray<TSoftObjectPtr<UBeyondItemDefinition>> GuaranteedLoot;
};

/** Every enemy and affix the game knows (Project Settings -> Worlds Beyond Enemies -> Roster) */
UCLASS(BlueprintType)
class WORLDBEYOND_API UBeyondEnemyRoster : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roster")
	TArray<TObjectPtr<UBeyondEnemyDefinition>> Enemies;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roster")
	TArray<TObjectPtr<UBeyondAffixDefinition>> Affixes;

	UFUNCTION(BlueprintPure, Category = "Roster")
	UBeyondEnemyDefinition* FindEnemy(FName EnemyId) const;

	UFUNCTION(BlueprintPure, Category = "Roster")
	UBeyondAffixDefinition* FindAffix(FName AffixId) const;
};
