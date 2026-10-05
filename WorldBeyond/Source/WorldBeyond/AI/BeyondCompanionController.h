// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "BeyondCompanionController.generated.h"

class ABeyondCharacterBase;

/**
 * Drives the demigod the player isn't controlling: follows the leader, fights whatever attacks
 * the leader (or the nearest hostile), and uses its GAS abilities via their AI hints.
 * Tune the numbers in a Blueprint child.
 */
UCLASS()
class WORLDBEYOND_API ABeyondCompanionController : public AAIController
{
	GENERATED_BODY()

public:
	ABeyondCompanionController();

	void SetLeader(ABeyondCharacterBase* NewLeader);

	UFUNCTION(BlueprintPure, Category = "Companion")
	ABeyondCharacterBase* GetLeader() const { return Leader.Get(); }

	UFUNCTION(BlueprintPure, Category = "Companion")
	AActor* GetCombatTarget() const { return CombatTarget.Get(); }

	// Standing still because a cutscene (level sequence) has it or its leader
	UFUNCTION(BlueprintPure, Category = "Companion")
	bool IsHoldingForCutscene() const { return bHoldingForCutscene; }

	// Seconds between decisions
	UPROPERTY(EditDefaultsOnly, Category = "Companion", meta = (ClampMin = "0.05"))
	float ThinkInterval = 0.2f;

	// How far behind/beside the leader to stay when not fighting
	UPROPERTY(EditDefaultsOnly, Category = "Companion|Follow")
	float FollowDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Companion|Follow")
	float FollowAcceptanceRadius = 120.0f;

	// Teleport next to the leader when further than this (stuck, fell behind)
	UPROPERTY(EditDefaultsOnly, Category = "Companion|Follow")
	float RegroupDistance = 2500.0f;

	// Enemies within this distance of the leader get engaged
	UPROPERTY(EditDefaultsOnly, Category = "Companion|Combat")
	float EngageRadius = 1500.0f;

	// Give up on a target once it's this far from the leader
	UPROPERTY(EditDefaultsOnly, Category = "Companion|Combat")
	float DisengageRadius = 2200.0f;

	// Approach distance when no ability reports a range
	UPROPERTY(EditDefaultsOnly, Category = "Companion|Combat")
	float FallbackAttackRange = 200.0f;

	// Only moves issued by this controller are accepted; old Blueprint "follow" logic on the characters is ignored
	virtual FPathFollowingRequestResult MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath = nullptr) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	void Think();
	bool IsValidTarget(const AActor* Target) const;
	AActor* PickTarget() const;
	void FollowLeader();
	void FaceActor(const AActor* Target) const;

	UFUNCTION()
	void HandleLeaderHitTaken(ABeyondCharacterBase* HitCharacter, AActor* DamageInstigator, float Damage, FGameplayTag HitResponse);

	UFUNCTION()
	void HandleSelfHitTaken(ABeyondCharacterBase* HitCharacter, AActor* DamageInstigator, float Damage, FGameplayTag HitResponse);

private:
	TWeakObjectPtr<ABeyondCharacterBase> Leader;
	TWeakObjectPtr<AActor> CombatTarget;
	TWeakObjectPtr<AActor> LeaderAttacker;
	TWeakObjectPtr<AActor> SelfAttacker;
	FTimerHandle ThinkTimer;

	// Alternates which side of the leader to follow on
	float FollowSide = 1.0f;

	bool bIssuingOwnMove = false;
	bool bHoldingForCutscene = false;
};
