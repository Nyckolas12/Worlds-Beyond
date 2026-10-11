// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GenericTeamAgentInterface.h"
#include "InputCoreTypes.h"
#include "AbilitySystem/BeyondAbilitySet.h"
#include "AbilitySystem/BeyondFX.h"
#include "Characters/BeyondAimComponent.h"
#include "Characters/BeyondLegacyDamageBridge.h"
#include "Progression/BeyondProgressionAttributeSet.h"
#include "Progression/BeyondProgressionSettings.h"
#include "WorldBeyond/CharacterAttributeSet.h"
#include "BeyondCharacterBase.generated.h"

class UBeyondAbilitySet;
class UBeyondEquipmentComponent;
class UBeyondItemDefinition;
class UBeyondSkillTreeAsset;
class UBeyondSkillTreeComponent;
class UInputAction;
class UAnimMontage;
class USkeletalMeshComponent;
class UUserWidget;

UENUM(BlueprintType)
enum class EBeyondTeam : uint8
{
	// No team: never hostile, never friendly (maps to FGenericTeamId::NoTeam)
	Neutral = 0,
	Player = 1,
	Enemy = 2
};

// Part a demigod plays in the duo super move
UENUM(BlueprintType)
enum class EBeyondDuoRole : uint8
{
	None,
	// Channels raw power into the partner (Angel's lightning)
	Conduit,
	// Absorbs the conduit's power and releases it (Ji-Woong's shockwave)
	Striker
};

USTRUCT(BlueprintType)
struct FBeyondInputBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<const UInputAction> InputAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Ability.Input"))
	FGameplayTag InputTag;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FBeyondHitTakenSignature, ABeyondCharacterBase*, Character, AActor*, DamageInstigator, float, Damage, FGameplayTag, HitResponse);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondKilledSignature, ABeyondCharacterBase*, Character, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondLevelUpSignature, ABeyondCharacterBase*, Character, int32, NewLevel);

UCLASS()
class WORLDBEYOND_API ABeyondCharacterBase : public ACharacter, public IAbilitySystemInterface, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABeyondCharacterBase();

	//Ability System Component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	class UAbilitySystemComponent* AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (AllowPrivateAccess = "true"))
	UCharacterAttributeSet* AttributeSet;

	// Who this character fights for. AI perception and UBeyondCombatLibrary::AreHostile use it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Team")
	EBeyondTeam TeamAffiliation = EBeyondTeam::Player;

	// Fired after every hit, including blocked / parried ones (Damage 0)
	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FBeyondHitTakenSignature OnCharacterHitTaken;

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FBeyondKilledSignature OnCharacterKilled;

	// Fired when EXP raised this character's level (not when a save is restored)
	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FBeyondLevelUpSignature OnCharacterLevelUp;

	void InitializeAttributeSet();

	// Leaves the ability system exactly one UCharacterAttributeSet and points AttributeSet at it (see the .cpp)
	void ResolveAttributeSet();

	// Name shown on the HUD (level-up banner); empty uses the class name without "BP_"
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	FText DisplayName;

	UFUNCTION(BlueprintPure, Category = "Character")
	FText GetCharacterDisplayName() const;

	//~ Progression (EXP and skill points only exist on Player-team characters)

	// How strong this character is as an enemy: decides its EXP (Project Settings -> Game -> Worlds Beyond Progression)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	EBeyondEnemyRank Rank = EBeyondEnemyRank::Regular;

	// EXP the party gets for killing this character; below 0 uses the rank's amount scaled by level
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	float ExperienceReward = -1.0f;

	// Level on spawn (enemies get Stat Growth for every level above 1; the demigods' saved level replaces it)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression", meta = (ClampMin = "1"))
	int32 StartingLevel = 1;

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetCharacterLevel() const;

	// EXP into the current level
	UFUNCTION(BlueprintPure, Category = "Progression")
	float GetExperience() const;

	// EXP the current level needs in total (0 at the max level)
	UFUNCTION(BlueprintPure, Category = "Progression")
	float GetExperienceToNextLevel() const;

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetSkillPoints() const;

	// What killing this character is worth
	UFUNCTION(BlueprintPure, Category = "Progression")
	float GetExperienceRewardValue() const;

	// Whether this character earns EXP (Player team)
	UFUNCTION(BlueprintPure, Category = "Progression")
	bool CanGainExperience() const { return ProgressionSet != nullptr; }

	// Adds EXP through UBeyondGE_GrantExperience; levels up as thresholds are crossed
	UFUNCTION(BlueprintCallable, Category = "Progression")
	void GrantExperience(float Amount);

	// Puts back a saved level / EXP / skill points (no banner or level-up event); refills health and stamina
	UFUNCTION(BlueprintCallable, Category = "Progression")
	void RestoreProgress(int32 NewLevel, float NewExperience, int32 NewSkillPoints);

	UBeyondProgressionAttributeSet* GetProgressionSet() const { return ProgressionSet; }

	// Takes Amount skill points; false (and nothing taken) if there aren't enough
	UFUNCTION(BlueprintCallable, Category = "Progression")
	bool SpendSkillPoints(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Progression")
	void AddSkillPoints(int32 Amount);

	// This demigod's skill tree (Player team only; created from Skill Tree)
	UFUNCTION(BlueprintPure, Category = "Progression")
	UBeyondSkillTreeComponent* GetSkillTreeComponent() const { return SkillTreeComponent; }

	// What this demigod wears (Player team only)
	UFUNCTION(BlueprintPure, Category = "Equipment")
	UBeyondEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	// Always dropped when this enemy is defeated by the party, on top of its rank's random loot (boss drops)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TArray<TSoftObjectPtr<UBeyondItemDefinition>> GuaranteedLoot;

	// Whether a party kill drops loot (summoned adds and boss clones don't)
	virtual bool CanDropLoot() const { return true; }

	/**
	 * Damage this character takes after Defense, before it lands (elite affixes: Warded, Juggernaut...).
	 * DamageTags are the damage spec's tags (DamageType.*, Event.Hit.*).
	 */
	virtual float ModifyDamageTaken(float Damage, AActor* DamageInstigator, const FGameplayTagContainer& DamageTags) const { return Damage; }

	// Health a single hit can't take this character below (bosses: the next phase threshold); 0 for none
	virtual float GetHealthFloor() const { return 0.0f; }

	// Damage this character deals x this (boss clones hit softer)
	virtual float GetOutgoingDamageScale() const { return 1.0f; }

	// The ability a key slot fires (same rule as player input); null if none
	const UGameplayAbility* FindAbilityOnInput(const FGameplayTag& InputTag) const;

	// What this character gains per level above 1
	UFUNCTION(BlueprintPure, Category = "Progression")
	FBeyondStatGrowth GetStatGrowth() const { return StatGrowth; }

	/**
	 * The skeletal mesh that actually animates. MetaHumans keep CharacterMesh0 empty and animate a "Body"
	 * child, so abilities play montages and attach weapons here instead of on GetMesh().
	 */
	UFUNCTION(BlueprintPure, Category = "Character")
	USkeletalMeshComponent* GetCombatMesh() const;

	/**
	 * The engine version stops montages on CharacterMesh0, which is empty on MetaHumans; this stops them on the combat mesh.
	 * None stops everything playing there (an attack can be two montages at once: swing + footwork).
	 */
	virtual void StopAnimMontage(UAnimMontage* AnimMontage = nullptr) override;

	// Same for playing: the engine version plays on the empty CharacterMesh0 of the MetaHumans (the dialogue pack's
	// DialogueAnim rows use it)
	virtual float PlayAnimMontage(UAnimMontage* AnimMontage, float InPlayRate = 1.0f, FName StartSectionName = NAME_None) override;

	/**
	 * Montage this character plays when one of these (Blueprint) abilities activates, e.g. Angel: GA_HealSpell -> AM_Heal.
	 * Lets a shared ability look and sound different per demigod; abilities with their own montage ignore it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem")
	TMap<TSubclassOf<UGameplayAbility>, TObjectPtr<UAnimMontage>> AbilityMontages;

	// Part this demigod plays in the duo super move
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party")
	EBeyondDuoRole DuoRole = EBeyondDuoRole::None;

	// Show a boss health bar (e.g. W_BossHealthBar, which needs an UpdateHealthPercentage(Health, MaxHealth) function) while the player is near
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Boss")
	TSubclassOf<UUserWidget> BossBarWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Boss", meta = (ClampMin = "0", EditCondition = "BossBarWidgetClass != nullptr"))
	float BossBarShowRadius = 2500.0f;

	// Weapon to equip on spawn, sent to the equip ability as the event's target tag (e.g. Weapon.Melee.Sword)
	const FGameplayTag& GetDefaultWeaponTag() const { return DefaultWeaponTag; }

	const TArray<FBeyondInputBinding>& GetAbilityInputBindings() const { return AbilityInputBindings; }

	bool IsLegacyKeyInputDisabled() const { return bDisableLegacyKeyInput; }

	const TArray<FKey>& GetLegacyKeysToDisable() const { return LegacyKeysToDisable; }

	const FBeyondAimSettings& GetAimSettings() const { return AimSettings; }

	// Crosshair, shoulder camera and aim input; only exists when Aim Settings are enabled (Angel)
	UFUNCTION(BlueprintPure, Category = "Aim")
	UBeyondAimComponent* GetAimComponent() const { return AimComponent; }

protected:
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = "AbilitySystem")
	EGameplayEffectReplicationMode ASCReplicationMode = EGameplayEffectReplicationMode::Mixed;

	// Abilities, input slots and startup effects (stamina regen etc.)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem")
	TObjectPtr<UBeyondAbilitySet> AbilitySet;

	// Legacy list, granted in addition to AbilitySet. Prefer AbilitySet for new work.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilitySystem")
	TArray<TSubclassOf<UGameplayAbility>> StartingAbilities;

	// Never granted to this character, even when Blueprint BeginPlay gives them (abilities replaced by newer ones)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem")
	TArray<TSubclassOf<UGameplayAbility>> SuppressedAbilities;

	// Name of the component that animates when CharacterMesh0 has no mesh (MetaHumans)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	FName CombatMeshName = TEXT("Body");

	// AI controllers of this character also perceive hostile teams (the old sense configs only detected neutrals/friendlies)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team")
	bool bAIPerceivesHostileTeams = true;

	// Input actions that press an ability slot. Abilities granted anywhere (ability set or Blueprint) are matched by their Input Tag.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FBeyondInputBinding> AbilityInputBindings;

	// Confirms / cancels targeting (ground-targeted abilities such as GA_AOEAttack)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputAction> ConfirmTargetAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputAction> CancelTargetAction;

	// Drop the old raw key events (Blueprint "Keyboard F / Left Mouse Button" nodes). Turn on once those attacks are GAS abilities.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	bool bDisableLegacyKeyInput = false;

	/**
	 * Only these keys' raw events are dropped; empty drops every one. Ji-Woong: Left Mouse Button (his LMB is the GAS
	 * sword combo now), so his "1" draw / sheathe and other Blueprint keys keep working.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (EditCondition = "bDisableLegacyKeyInput"))
	TArray<FKey> LegacyKeysToDisable;

	// Third-person aiming for casters: crosshair, over-the-shoulder camera, hold-to-aim (Angel)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim")
	FBeyondAimSettings AimSettings;

	// Releasing an ability's key confirms its targeting (hold to aim, release to cast)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	bool bConfirmTargetingOnRelease = true;

	// Weapon to equip on spawn (instantly), sent to the equip ability as the event's target tag (e.g. Weapon.Melee.Sword)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (Categories = "Weapon"))
	FGameplayTag DefaultWeaponTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FGameplayTag EquipWeaponEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem|Attributes", meta = (ClampMin = "1"))
	float DefaultMaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem|Attributes", meta = (ClampMin = "1"))
	float DefaultMaxStamina = 100.0f;

	// Optional: an instant effect that sets starting attributes instead of the defaults above
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem|Attributes")
	TSubclassOf<UGameplayEffect> DefaultAttributesEffect;

	// Use the Max Health set on the old BPC_DamageSystem component (keeps each enemy's tuning)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem|Attributes")
	bool bUseLegacyMaxHealth = true;

	// Melee damage +1 % per point
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem|Attributes", meta = (ClampMin = "0"))
	float BaseStrength = 0.0f;

	// Ability / projectile damage +1 % per point
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem|Attributes", meta = (ClampMin = "0"))
	float BaseArcana = 0.0f;

	// Damage taken x 100 / (100 + Defense)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem|Attributes", meta = (ClampMin = "0"))
	float BaseDefense = 0.0f;

	// Added for every level above 1
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression")
	FBeyondStatGrowth StatGrowth;

	// Burst on the character when it levels up (attached to its root)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression")
	FBeyondFX LevelUpFX;

	// The demigod's skill tree (spent with skill points); see UBeyondSkillTreeComponent
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression")
	TObjectPtr<UBeyondSkillTreeAsset> SkillTree;

	UFUNCTION(BlueprintImplementableEvent, Category = "Progression")
	void OnLevelUp(int32 NewLevel);

	// Seconds before a dead character is destroyed (0 keeps the body, e.g. for revivable demigods)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0"))
	float DestroyDelayAfterDeath = 0.0f;

public:
	// Played when this demigod becomes the player-controlled party leader
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party")
	TObjectPtr<USoundBase> SwapInSound;

	// Bring a dead character back at a fraction of max health (checkpoint respawn, buddy revive)
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void Revive(float HealthFraction = 1.0f);

	// How many enemies may attack this character at the same time (enemy AI attack tokens)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|AI", meta = (ClampMin = "0"))
	int32 MaxAttackTokens = 2;

	UFUNCTION(BlueprintCallable, Category = "Combat|AI")
	bool TryReserveAttackTokens(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Combat|AI")
	void ReleaseAttackTokens(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Combat|AI")
	int32 GetAvailableAttackTokens() const { return AvailableAttackTokens; }

	// Copy the old BPC_DamageSystem blocking / invincible / interruptible flags onto GAS tags
	void SyncLegacyDamageState();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void PossessedBy(AController* NewControl) override;
	virtual void PawnClientRestart() override;

	virtual void OnRep_PlayerState() override;
	virtual void OnDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damage")
	void HandleDeath();

	// Blueprint hooks for hit reactions / death visuals and sounds
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat")
	void OnHitTaken(AActor* DamageInstigator, float Damage, FGameplayTag HitResponse);

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat")
	void OnKilled(AActor* Killer);

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat")
	void OnRevived();

public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	//~ IGenericTeamAgentInterface
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;

	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	TArray<FGameplayAbilitySpecHandle> GrantAbilities(TArray<TSubclassOf<UGameplayAbility>> AbilitiesToGrant);

	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	void RemoveAbilities(TArray<FGameplayAbilitySpecHandle> AbilityHandlesToRemove);

	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	void SendAbilitiesChangedEvent();

	UFUNCTION(BlueprintPure, Category = "AbilitySystem")
	bool IsAbilitySuppressed(TSubclassOf<UGameplayAbility> AbilityClass) const;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "AbilitySystem")
	void ServerSendGameplayEventToSelf(FGameplayEventData EventData);

	// Activate the ability bound to an input slot (used by player input, the companion AI, and legacy key events)
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	bool TryActivateAbilityByInputTag(UPARAM(meta = (Categories = "Ability.Input")) FGameplayTag InputTag);

	UFUNCTION(BlueprintPure, Category = "AbilitySystem")
	const UBeyondAbilitySet* GetAbilitySet() const { return AbilitySet; }

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	void InitAbilitySystem();
	void UseCombatMeshForAbilities();
	void RemoveSuppressedAbilities();
	void ConfigureAIPerception(AController* NewController) const;
	void HandleAttributeHitTaken(AActor* DamageInstigator, AActor* Causer, float Damage, FGameplayTag HitResponse);
	void HandleOutOfHealth(AActor* DamageInstigator, AActor* Causer, float Damage, FGameplayTag HitResponse);
	void Die();

	void HandleHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void SyncLegacyHealth();

	void CreateProgressionSet();
	// (Re)applies Stat Growth x (Level - 1)
	void ApplyLevelStats();
	void RefillVitals();
	void HandleLevelUp(int32 OldLevel, int32 NewLevel);

	void EquipDefaultWeapon();
	void BindAbilityInput(class UEnhancedInputComponent* EnhancedInput, const UInputAction* Action, const FGameplayTag& InputTag);
	void Input_ConfirmTarget();
	void Input_CancelTarget();
	void Input_AimStarted();
	void Input_AimStopped();
	void CreateAimComponent();

	void Input_AbilityPressed(FGameplayTag InputTag);
	void Input_AbilityReleased(FGameplayTag InputTag);
	void CollectSpecsWithInputTag(const FGameplayTag& InputTag, TArray<FGameplayAbilitySpecHandle>& OutHandles) const;

	UPROPERTY(Transient)
	TObjectPtr<UBeyondAimComponent> AimComponent;

	UPROPERTY(Transient)
	TObjectPtr<UBeyondProgressionAttributeSet> ProgressionSet;

	UPROPERTY(Transient)
	TObjectPtr<UBeyondSkillTreeComponent> SkillTreeComponent;

	UPROPERTY(Transient)
	TObjectPtr<UBeyondEquipmentComponent> EquipmentComponent;

	FActiveGameplayEffectHandle LevelStatsHandle;

	bool bAbilitySystemBound = false;
	bool bStartupGiven = false;

	FBeyondAbilitySetHandles AbilitySetHandles;

	TWeakObjectPtr<AActor> LastDamageInstigator;

	int32 AvailableAttackTokens = 0;

	mutable TWeakObjectPtr<USkeletalMeshComponent> CachedCombatMesh;

	// The old Blueprint BPC_DamageSystem component, kept in sync with GAS
	TWeakObjectPtr<UActorComponent> LegacyDamageComponent;
	BeyondLegacyDamage::FStateMirror LegacyStateMirror;

	// Pre-ragdoll setup, restored by Revive
	FTransform MeshRelativeTransform;
	ECollisionEnabled::Type MeshCollision = ECollisionEnabled::QueryOnly;
	ECollisionEnabled::Type CapsuleCollision = ECollisionEnabled::QueryAndPhysics;
};
