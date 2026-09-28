// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GenericTeamAgentInterface.h"
#include "AbilitySystem/BeyondAbilitySet.h"
#include "WorldBeyond/CharacterAttributeSet.h"
#include "BeyondCharacterBase.generated.h"

class UBeyondAbilitySet;

UENUM(BlueprintType)
enum class EBeyondTeam : uint8
{
	// No team: never hostile, never friendly (maps to FGenericTeamId::NoTeam)
	Neutral = 0,
	Player = 1,
	Enemy = 2
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FBeyondHitTakenSignature, ABeyondCharacterBase*, Character, AActor*, DamageInstigator, float, Damage, FGameplayTag, HitResponse);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondKilledSignature, ABeyondCharacterBase*, Character, AActor*, Killer);

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

	void InitializeAttributeSet();

protected:
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = "AbilitySystem")
	EGameplayEffectReplicationMode ASCReplicationMode = EGameplayEffectReplicationMode::Mixed;

	// Abilities, input slots and startup effects (stamina regen etc.)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem")
	TObjectPtr<UBeyondAbilitySet> AbilitySet;

	// Legacy list, granted in addition to AbilitySet. Prefer AbilitySet for new work.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilitySystem")
	TArray<TSubclassOf<UGameplayAbility>> StartingAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem|Attributes", meta = (ClampMin = "1"))
	float DefaultMaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem|Attributes", meta = (ClampMin = "1"))
	float DefaultMaxStamina = 100.0f;

	// Optional: an instant effect that sets starting attributes instead of the defaults above
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem|Attributes")
	TSubclassOf<UGameplayEffect> DefaultAttributesEffect;

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

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void PossessedBy(AController* NewControl) override;

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
	void HandleAttributeHitTaken(AActor* DamageInstigator, AActor* Causer, float Damage, FGameplayTag HitResponse);
	void HandleOutOfHealth(AActor* DamageInstigator, AActor* Causer, float Damage, FGameplayTag HitResponse);
	void Die();

	void Input_AbilityPressed(FGameplayTag InputTag);
	void Input_AbilityReleased(FGameplayTag InputTag);
	void CollectSpecsWithInputTag(const FGameplayTag& InputTag, TArray<FGameplayAbilitySpecHandle>& OutHandles) const;

	bool bAbilitySystemBound = false;
	bool bStartupGiven = false;

	FBeyondAbilitySetHandles AbilitySetHandles;

	TWeakObjectPtr<AActor> LastDamageInstigator;

	// Pre-ragdoll setup, restored by Revive
	FTransform MeshRelativeTransform;
	ECollisionEnabled::Type MeshCollision = ECollisionEnabled::QueryOnly;
	ECollisionEnabled::Type CapsuleCollision = ECollisionEnabled::QueryAndPhysics;
};
