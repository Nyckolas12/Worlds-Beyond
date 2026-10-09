// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "BeyondEnemyController.generated.h"

class ABeyondCharacterBase;
class ABeyondEnemyCharacter;
struct FAbilityEndedData;
struct FBeyondEnemyAIConfig;

UENUM(BlueprintType)
enum class EBeyondEnemyAIState : uint8
{
	// Standing / wandering around home
	Idle,
	// Fighting a demigod
	Combat,
	// Gave up (too far from home, no target): walking back, invulnerable, refilled on arrival
	Returning
};

/**
 * Brain of a Plan 3 enemy (ABeyondEnemyCharacter), a C++ think loop like the buddy's (no behavior tree):
 * - Idle: wanders around home and watches its view cone (plus a small all-round radius) for demigods;
 * - Combat: picks abilities by their AI hints (range, weight, cooldown), takes attack tokens from the target for melee
 *   swings (only a few enemies swing at one demigod at a time; the rest circle at Strafe Radius), casters keep their
 *   distance; being hit or a packmate's alert also starts a fight;
 * - Returning: past its Leash Radius (or without a target) it walks home invulnerable and refills.
 * Tuning comes from the enemy definition's AI settings.
 */
UCLASS()
class WORLDBEYOND_API ABeyondEnemyController : public AAIController
{
	GENERATED_BODY()

public:
	ABeyondEnemyController();

	UFUNCTION(BlueprintPure, Category = "Enemy AI")
	EBeyondEnemyAIState GetAIState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Enemy AI")
	AActor* GetCombatTarget() const { return CombatTarget.Get(); }

	// Start (or switch to) fighting Target; bAlertOthers calls idle enemies nearby in too
	UFUNCTION(BlueprintCallable, Category = "Enemy AI")
	void EngageTarget(AActor* Target, bool bAlertOthers = true);

	// Give up and go home (bTeleport: be there at once, e.g. after a party wipe)
	UFUNCTION(BlueprintCallable, Category = "Enemy AI")
	void ReturnHome(bool bTeleport = false);

	// Off: the enemy stands still and does nothing (tests, cutscenes)
	UFUNCTION(BlueprintCallable, Category = "Enemy AI")
	void SetBrainEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Enemy AI")
	bool IsBrainEnabled() const { return bBrainEnabled; }

	// Attack tokens it holds right now (on its target)
	UFUNCTION(BlueprintPure, Category = "Enemy AI")
	int32 GetHeldTokens() const { return TokensHeld; }

	// Waiting for an attack token (circling)
	UFUNCTION(BlueprintPure, Category = "Enemy AI")
	bool IsWaitingForToken() const { return bWaitingForToken; }

	// Run one decision now (tests)
	void ThinkNow() { Think(); }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	void Think();

private:
	ABeyondEnemyCharacter* GetEnemy() const;
	const FBeyondEnemyAIConfig& GetConfig() const;
	float GetCombatSpeed() const;

	AActor* LookForTarget() const;
	bool IsValidTarget(const AActor* Target) const;
	void SetState(EBeyondEnemyAIState NewState);

	void ThinkIdle();
	void ThinkCombat();
	void ThinkReturning();
	void FinishReturn();

	bool TryAttack(AActor* Target);
	void Reposition(AActor* Target);
	void IssueMoveTo(const FVector& Location, float AcceptanceRadius);
	void ReleaseTokens();
	void FaceActor(const AActor* Target) const;

	void HandleAbilityEnded(const FAbilityEndedData& Data);

	UFUNCTION()
	void HandleHitTaken(ABeyondCharacterBase* HitCharacter, AActor* DamageInstigator, float Damage, FGameplayTag HitResponse);

	UFUNCTION()
	void HandleKilled(ABeyondCharacterBase* KilledCharacter, AActor* Killer);

	EBeyondEnemyAIState State = EBeyondEnemyAIState::Idle;
	TWeakObjectPtr<AActor> CombatTarget;
	TWeakObjectPtr<ABeyondCharacterBase> TokenTarget;
	FGameplayAbilitySpecHandle TokenAbility;
	int32 TokensHeld = 0;
	bool bWaitingForToken = false;
	bool bBrainEnabled = true;

	float StateStartTime = 0.0f;
	float NextWanderTime = 0.0f;
	float NextActionTime = 0.0f;
	float NextStrafeFlipTime = 0.0f;
	float LastAggroSoundTime = -1000.0f;
	float StrafeSign = 1.0f;
	float CombatSpeed = 0.0f;

	FTimerHandle ThinkTimer;
	FDelegateHandle AbilityEndedHandle;
};
