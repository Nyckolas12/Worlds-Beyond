// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystem/BeyondFX.h"
#include "Characters/BeyondRushComponent.h"
#include "BeyondBossArena.generated.h"

class ABeyondBossCharacter;
class ABeyondCharacterBase;
class UBeyondBossDefinition;
class UBillboardComponent;
class UBoxComponent;
class UFXSystemComponent;
class UPostProcessComponent;

UENUM(BlueprintType)
enum class EBeyondArenaState : uint8
{
	// Waiting for the party (the boss stands at its spawn point)
	Dormant,
	// Sealed in with the boss
	Fighting,
	// The boss fell
	Defeated
};

/**
 * Where a boss fight happens (Plan 3B): spawns the boss at Boss Spawn Point (unless a story boss was already beaten,
 * see UBeyondPartyComponent::IsBossDefeated; with Remember Defeat any boss whose Boss.<id> flag is set), wakes it and
 * seals the arena when a demigod comes within Engage Radius,
 * gives hazards their centre and radius, darkens for a Darkness twist, and resets everything when the party wipes
 * (boss back at full health in its first phase, adds and hazards gone, unsealed). Place a checkpoint at the entrance.
 * When the fight starts, every demigod still outside the ring dashes in (blinks if far or blocked; a fallen one is
 * carried in) and the walls rise once the whole party is inside, whichever demigod the player controls.
 * In the open world (Plan 5) the boss only exists while the party is within Boss Spawn Radius (and there is ground
 * under it), and goes away again past Boss Despawn Radius while the fight hasn't started. An arena without a boss is a
 * marked site for a later one. Always loaded in a World Partition map; the world map shows it.
 */
UCLASS()
class WORLDBEYOND_API ABeyondBossArena : public AActor
{
	GENERATED_BODY()

public:
	ABeyondBossArena();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena")
	TObjectPtr<UBeyondBossDefinition> Boss;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "1"))
	int32 BossLevel = 10;

	// The map marker's id (empty: the boss id)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena")
	FName ArenaId;

	// Also a mini-boss stays beaten once its Boss.<id> story flag is set (every boss kill sets it)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena")
	bool bRememberDefeat = false;

	// 0: the boss spawns when play starts; otherwise when a demigod comes this close
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "0"))
	float BossSpawnRadius = 0.0f;

	// With a Boss Spawn Radius: a boss that hasn't been fought goes away once every demigod is this far (0: never)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "0", EditCondition = "BossSpawnRadius > 0"))
	float BossDespawnRadius = 0.0f;

	// The fighting ground (hazard placement, the seal ring)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "500"))
	float ArenaRadius = 2400.0f;

	// A demigod this close to the centre starts the fight
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "100"))
	float EngageRadius = 1800.0f;

	// Walls go up around the arena during the fight
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena")
	bool bSealDuringFight = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "4", ClampMax = "64"))
	int32 SealSegments = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "100"))
	float SealHeight = 800.0f;

	// Looping on every seal segment while sealed (fire / ice wall); Max Lifetime 0
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena")
	FBeyondFX SealFX;

	// After a party wipe, the boss comes back this long after it was removed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "0"))
	float RespawnDelay = 3.0f;

	// When the fight starts, demigods outside the ring are brought in before the walls rise
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Party")
	bool bPullPartyInside = true;

	// How far inside the wall they end up
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Party", meta = (ClampMin = "0", EditCondition = "bPullPartyInside"))
	float PullMargin = 150.0f;

	// The walls rise after this long even if someone hasn't arrived (they blink the rest of the way)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Party", meta = (ClampMin = "0.1", EditCondition = "bPullPartyInside"))
	float PullTimeout = 1.2f;

	// The dash in (speed, animation, trail) and the blink used beyond Max Rush Distance
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Party", meta = (EditCondition = "bPullPartyInside"))
	FBeyondRushSettings PartyRush;

	// Location (2D) is inside the ring, Margin within the wall
	UFUNCTION(BlueprintPure, Category = "Arena")
	bool IsInside(const FVector& Location, float Margin = 0.0f) const;

	// Point moved (2D) to within the ring, Margin within the wall
	UFUNCTION(BlueprintPure, Category = "Arena")
	FVector ClampInside(const FVector& Point, float Margin) const;

	// The sealed arena Location is in, if any
	static ABeyondBossArena* FindSealedArenaAt(const UWorld* World, const FVector& Location);

	// Party members still on their way in (the walls are up once it's empty)
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetMembersComingIn() const { return Incoming.Num(); }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	TObjectPtr<USceneComponent> BossSpawnPoint;

	UFUNCTION(BlueprintPure, Category = "Arena")
	ABeyondBossCharacter* GetBoss() const { return SpawnedBoss.Get(); }

	UFUNCTION(BlueprintPure, Category = "Arena")
	EBeyondArenaState GetArenaState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Arena")
	bool IsSealed() const { return bSealed; }

	// The boss fell here (or was already beaten when the party arrived)
	UFUNCTION(BlueprintPure, Category = "Arena")
	bool IsDefeated() const;

	// 0 normal .. 1 very dark (a Darkness twist)
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void SetDarkness(float Amount);

	UFUNCTION(BlueprintPure, Category = "Arena")
	float GetDarkness() const { return Darkness; }

	// Start the fight now (tests, scripted encounters)
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void Engage(AActor* Target = nullptr);

	// As after a party wipe: boss removed and brought back fresh, arena reopened
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void ResetArena();

	// Spawn the boss if it isn't there (and isn't beaten)
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void SpawnBoss();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BuildSeal();
	void SetSealed(bool bNewSealed);
	void CheckEngage();
	// Boss Spawn Radius: the boss comes when the party is near and goes when it is far
	void UpdateBossPresence(const TArray<ABeyondCharacterBase*>& Party);
	void HandlePartyWiped();
	bool IsBeaten() const;

	TArray<ABeyondCharacterBase*> GetParty() const;
	// Starts bringing everyone outside the ring in; false if nobody has to travel (seal right away)
	bool PullPartyInside(AActor* Trigger);
	FVector GetEntryPoint(const ABeyondCharacterBase* Member, const ABeyondCharacterBase* Anchor, int32 Index) const;
	void HandleMemberArrived(ABeyondCharacterBase* Member);
	// Anyone still on the way blinks in; then the walls rise
	void FinishPull();
	void CancelPull();

	UFUNCTION()
	void HandleBossKilled(ABeyondCharacterBase* Character, AActor* Killer);

	UPROPERTY(VisibleAnywhere, Category = "Arena")
	TObjectPtr<UPostProcessComponent> DarknessVolume;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> SealWalls;

	TArray<TWeakObjectPtr<UFXSystemComponent>> SealEffects;
	TWeakObjectPtr<ABeyondBossCharacter> SpawnedBoss;
	EBeyondArenaState State = EBeyondArenaState::Dormant;
	bool bSealed = false;
	// Inside PullPartyInside: arrivals don't raise the walls yet
	bool bStartingPull = false;
	float Darkness = 0.0f;
	FTimerHandle EngageTimer;
	FTimerHandle RespawnTimer;
	FTimerHandle PullTimer;
	FDelegateHandle PartyWipedHandle;

	// Members dashing in, and where to
	TMap<TWeakObjectPtr<ABeyondCharacterBase>, FVector> Incoming;
};
