// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystem/BeyondFX.h"
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
 * see UBeyondPartyComponent::IsBossDefeated), wakes it and seals the arena when a demigod comes within Engage Radius,
 * gives hazards their centre and radius, darkens for a Darkness twist, and resets everything when the party wipes
 * (boss back at full health in its first phase, adds and hazards gone, unsealed). Place a checkpoint at the entrance.
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	TObjectPtr<USceneComponent> BossSpawnPoint;

	UFUNCTION(BlueprintPure, Category = "Arena")
	ABeyondBossCharacter* GetBoss() const { return SpawnedBoss.Get(); }

	UFUNCTION(BlueprintPure, Category = "Arena")
	EBeyondArenaState GetArenaState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Arena")
	bool IsSealed() const { return bSealed; }

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
	void HandlePartyWiped();
	bool IsBeaten() const;

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
	float Darkness = 0.0f;
	FTimerHandle EngageTimer;
	FTimerHandle RespawnTimer;
	FDelegateHandle PartyWipedHandle;
};
