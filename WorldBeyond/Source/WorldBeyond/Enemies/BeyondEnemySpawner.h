// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BeyondEnemySpawner.generated.h"

class ABeyondCharacterBase;
class ABeyondEnemyCharacter;
class UBeyondEnemyDefinition;
class UBillboardComponent;

UENUM(BlueprintType)
enum class EBeyondRespawnRule : uint8
{
	// Dead stays dead
	Never,
	// The fallen come back when the party wipes
	OnPartyWipe,
	// Each one comes back Respawn Delay seconds after it fell
	AfterDelay,
	// The fallen come back when the party rests at a waystone (or wipes)
	OnRest
};

USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TObjectPtr<UBeyondEnemyDefinition> Enemy;

	// 0 uses the enemy's Pack Size
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0"))
	int32 Count = 0;

	// 0: the level band of the region the spawner is in (plus the spawner's Level Offset)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0"))
	int32 Level = 1;
};

/**
 * Places a group of roster enemies (an enemy camp, a pack of wolves). They spawn around it when play starts (or when
 * the party comes within Activation Radius), have a chance to be elites, and come back according to the respawn rule.
 * Entries with Level 0 take the level band of the region they stand in (Plan 5). With a Deactivation Radius, enemies
 * that aren't fighting are put away once the party is that far and come back (the fallen per the respawn rule) when it
 * returns, so an open world only has the camps near the party alive. Always loaded in a World Partition map; enemies
 * only spawn once there is ground under them. Drop it in a level and fill Entries.
 */
UCLASS()
class WORLDBEYOND_API ABeyondEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	ABeyondEnemySpawner();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner", meta = (TitleProperty = "Enemy"))
	TArray<FBeyondSpawnEntry> Entries;

	// Enemies stand within this distance of the spawner
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner", meta = (ClampMin = "0"))
	float SpawnRadius = 450.0f;

	// Chance that each enemy (that can be) is an elite with rolled affixes
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner", meta = (ClampMin = "0", ClampMax = "1"))
	float EliteChance = 0.1f;

	// 0 spawns when play starts; otherwise when a demigod comes this close
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner", meta = (ClampMin = "0"))
	float ActivationRadius = 0.0f;

	// With an Activation Radius: enemies not in a fight are put away once every demigod is this far (0: never)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner", meta = (ClampMin = "0", EditCondition = "ActivationRadius > 0"))
	float DeactivationRadius = 0.0f;

	// Added to the region's lowest level for Level 0 entries (kept inside the band)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner")
	int32 LevelOffset = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner")
	EBeyondRespawnRule Respawn = EBeyondRespawnRule::OnPartyWipe;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner", meta = (ClampMin = "1", EditCondition = "Respawn == EBeyondRespawnRule::AfterDelay"))
	float RespawnDelay = 90.0f;

	// Spawn (again) everything that isn't alive
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void SpawnMissing();

	UFUNCTION(BlueprintPure, Category = "Spawner")
	TArray<ABeyondEnemyCharacter*> GetSpawnedEnemies() const;

	// The party is near (its enemies are out)
	UFUNCTION(BlueprintPure, Category = "Spawner")
	bool IsActive() const { return bActivated; }

	// The level an entry spawns at here (Level 0: the region band plus Level Offset)
	UFUNCTION(BlueprintPure, Category = "Spawner")
	int32 GetSpawnLevel(int32 EntryIndex) const;

	// Fallen enemies waiting for the respawn rule
	UFUNCTION(BlueprintPure, Category = "Spawner")
	int32 GetDefeatedCount() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FSlot
	{
		int32 EntryIndex = 0;
		TWeakObjectPtr<ABeyondEnemyCharacter> Enemy;
		FVector Location = FVector::ZeroVector;
		bool bEverSpawned = false;
		// Fell and waits for the respawn rule
		bool bDefeated = false;
		double DefeatedTime = 0.0;
	};

	void BuildSlots();
	void SpawnSlot(FSlot& Slot);
	// Spawns every slot that has no living enemy and isn't waiting to respawn
	void SpawnWaiting();
	void CheckActivation();
	void Activate();
	void Deactivate();
	// Puts away every enemy not in a fight (they stay alive in their slot)
	void PutAwayIdleEnemies();
	bool IsPartyWithin(float Radius) const;
	bool IsAnyEnemyFighting() const;
	void ClearDefeated();
	void HandlePartyWiped();

	UFUNCTION()
	void HandlePartyRested();

	UFUNCTION()
	void HandleEnemyKilled(ABeyondCharacterBase* Character, AActor* Killer);

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif

	TArray<FSlot> Slots;
	bool bActivated = false;
	FTimerHandle ActivationTimer;
	FDelegateHandle PartyWipedHandle;
};
