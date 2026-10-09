// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondAbilitySet.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "BeyondBossCharacter.generated.h"

class ABeyondAreaStrike;
class ABeyondBossArena;
class UBeyondBossDefinition;
class UFXSystemComponent;
struct FBeyondBossTwist;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondBossPhaseSignature, ABeyondBossCharacter*, Boss, int32, PhaseIndex);

/**
 * A mini-boss or main boss (Plan 3B), driven by a UBeyondBossDefinition:
 * - phases by health threshold: a hit can't skip one (the next threshold is a health floor); at each threshold the boss
 *   stops, can't be hurt for its transition, plays it, gets the phase's abilities and Boss.Phase.N tag and runs the
 *   phase's twists (hazards, summons, enrage, shadow clones, darkness);
 * - fights inside an ABeyondBossArena when it has one (centre / radius for hazards, darkness, wipe reset).
 * Shadow clones are ABeyondBossCharacters too, but with no phases.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API ABeyondBossCharacter : public ABeyondEnemyCharacter
{
	GENERATED_BODY()

public:
	ABeyondBossCharacter();

	UFUNCTION(BlueprintPure, Category = "Boss")
	UBeyondBossDefinition* GetBossDefinition() const;

	// 0 = the first phase
	UFUNCTION(BlueprintPure, Category = "Boss")
	int32 GetPhaseIndex() const { return PhaseIndex; }

	UFUNCTION(BlueprintPure, Category = "Boss")
	int32 GetPhaseCount() const;

	// Health shares where the later phases start (the boss bar's notches)
	UFUNCTION(BlueprintPure, Category = "Boss")
	TArray<float> GetPhaseThresholds() const;

	UFUNCTION(BlueprintPure, Category = "Boss")
	bool IsTransitioning() const { return bTransitioning; }

	UFUNCTION(BlueprintPure, Category = "Boss")
	bool IsEnraged() const { return bEnraged; }

	UFUNCTION(BlueprintPure, Category = "Boss")
	FText GetBossTitle() const;

	// The latest twist announcement and how long ago it was made (boss bar banner); false if none
	bool GetAnnouncement(FText& OutText, float& OutAge) const;

	void SetArena(ABeyondBossArena* InArena);

	UFUNCTION(BlueprintPure, Category = "Boss")
	ABeyondBossArena* GetArena() const { return Arena.Get(); }

	// Its living summons and shadow clones
	UFUNCTION(BlueprintPure, Category = "Boss")
	TArray<ABeyondEnemyCharacter*> GetMinions() const;

	// Hazards it put down that are still there
	UFUNCTION(BlueprintPure, Category = "Boss")
	TArray<ABeyondAreaStrike*> GetHazards() const;

	// Skip to a phase (console Beyond.BossPhase, tests); runs its transition and twists
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void ForcePhase(int32 NewPhaseIndex);

	UPROPERTY(BlueprintAssignable, Category = "Boss")
	FBeyondBossPhaseSignature OnPhaseChanged;

	//~ ABeyondCharacterBase
	virtual float GetHealthFloor() const override;
	virtual float ModifyDamageTaken(float Damage, AActor* DamageInstigator, const FGameplayTagContainer& DamageTags) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleDeath_Implementation() override;

private:
	bool RunsPhases() const;
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void EnterPhase(int32 NewPhaseIndex, bool bTransition);
	void EndTransition();
	void RunTwist(const FBeyondBossTwist& Twist);
	void SpawnHazards(const FBeyondBossTwist& Twist);
	void SpawnSummons(const FBeyondBossTwist& Twist);
	void SpawnClones(const FBeyondBossTwist& Twist);
	void Enrage(const FBeyondBossTwist& Twist);
	void ClearFight(bool bKillMinions);
	AActor* FindFightTarget() const;
	FVector GetFightCentre() const;
	float GetFightRadius() const;
	int32 CountLiveSummons() const;

	int32 PhaseIndex = 0;
	bool bTransitioning = false;
	bool bPhasePending = false;
	bool bEnraged = false;
	bool bPhasesStarted = false;
	// Damage taken x this while any summon lives (pack shield)
	float SummonShield = 1.0f;
	float AnnouncementTime = -1000.0f;
	FText Announcement;

	TWeakObjectPtr<ABeyondBossArena> Arena;
	TArray<TWeakObjectPtr<ABeyondEnemyCharacter>> Summons;
	TArray<TWeakObjectPtr<ABeyondEnemyCharacter>> Clones;
	TArray<TWeakObjectPtr<ABeyondAreaStrike>> Hazards;
	TArray<FTimerHandle> TwistTimers;
	FTimerHandle TransitionTimer;
	FDelegateHandle HealthChangedHandle;
	FBeyondAbilitySetHandles PhaseAbilityHandles;
	TWeakObjectPtr<UFXSystemComponent> EnrageAura;
};
