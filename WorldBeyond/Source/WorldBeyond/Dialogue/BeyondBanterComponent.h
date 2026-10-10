// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Dialogue/BeyondBanter.h"
#include "BeyondBanterComponent.generated.h"

class ABeyondCharacterBase;
class UBeyondPartyComponent;

/**
 * Angel and Ji-Woong talking while you play (Plan 4), FF7 Rebirth style: free-movement DT_Dialogue rows at the bottom
 * of the screen on the leader's BP_AC_Dialogue, auto-advancing. Lives on ABeyondPlayerController.
 * Triggers: banter volumes (regions), the leader low in a fight, a boss falling, standing idle, level-ups, revives.
 * Never while talking to someone, in a menu or a cutscene, or with a demigod down; at most one per Banter Cooldown.
 * Story moments (a new region, a boss victory) don't wait for it and cut a running quip short. Each line has its own
 * cooldown; once-only lines are saved (Banter.<Id>).
 */
UCLASS(ClassGroup = (Beyond), meta = (BlueprintSpawnableComponent))
class WORLDBEYOND_API UBeyondBanterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBeyondBanterComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banter")
	bool bBanterEnabled = true;

	// Leave empty to use the one in Project Settings -> Worlds Beyond Dialogue
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banter")
	TObjectPtr<UBeyondBanterSet> BanterSet;

	// Plays a fitting line (lines for the exact Context win over general ones); false if none could play now
	UFUNCTION(BlueprintCallable, Category = "Banter")
	bool TriggerBanter(EBeyondBanterTrigger Trigger, FName Context = NAME_None);

	// Plays one line by id (bForce: ignore cooldowns and flags, still not during a conversation)
	UFUNCTION(BlueprintCallable, Category = "Banter")
	bool PlayBanter(FName Id, bool bForce = false);

	UFUNCTION(BlueprintPure, Category = "Banter")
	UBeyondBanterSet* GetBanterSet() const;

	// The last line that played
	UFUNCTION(BlueprintPure, Category = "Banter")
	FName GetLastBanterId() const { return LastBanterId; }

	// Whether Trigger could play a line now; WhyNot says what stops it
	bool CanBanterNow(EBeyondBanterTrigger Trigger, FString& WhyNot) const;

	// Forget every cooldown (tests, console)
	UFUNCTION(BlueprintCallable, Category = "Banter")
	void ResetCooldowns();

	// The leader moved / fought: restarts the idle clock
	void NoteActivity();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleLeaderChanged(ABeyondCharacterBase* NewLeader, ABeyondCharacterBase* OldLeader);

	UFUNCTION()
	void HandleLevelUp(ABeyondCharacterBase* Member, int32 NewLevel);

	UFUNCTION()
	void HandleReviveProgress(ABeyondCharacterBase* Member, float Progress);

	UFUNCTION()
	void HandleBossDefeated(FName BossId);

	UFUNCTION()
	void HandleDamageDealt(AActor* DamageInstigator, AActor* Target, float Damage);

private:
	UBeyondPartyComponent* GetParty() const;
	ABeyondCharacterBase* GetLeader() const;
	bool IsInFight() const;
	bool IsRowPlayable(FName Row) const;
	bool Play(const FBeyondBanterLine& Line);
	void Check();
	double Now() const;

	TMap<FName, double> LastPlayed;
	double LastBanterTime = -1.0e9;
	double LastActivityTime = 0.0;
	double LastCombatTime = -1.0e9;
	bool bLeaderWasLow = false;
	FName LastBanterId;
	// What started the banter on screen (a story moment may cut a quip short, not another story moment)
	EBeyondBanterTrigger CurrentTrigger = EBeyondBanterTrigger::Idle;
	FName PendingBoss;
	FTimerHandle CheckTimer;
	FTimerHandle BossTimer;
};
