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
	AfterDelay
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "1"))
	int32 Level = 1;
};

/**
 * Places a group of roster enemies (an enemy camp, a pack of wolves). They spawn around it when play starts (or when
 * the party first comes within Activation Radius), have a chance to be elites, and come back according to the
 * respawn rule. Drop it in a level and fill Entries.
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

	// 0 spawns when play starts; otherwise when a demigod first comes this close
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner", meta = (ClampMin = "0"))
	float ActivationRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner")
	EBeyondRespawnRule Respawn = EBeyondRespawnRule::OnPartyWipe;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner", meta = (ClampMin = "1", EditCondition = "Respawn == EBeyondRespawnRule::AfterDelay"))
	float RespawnDelay = 90.0f;

	// Spawn (again) everything that isn't alive
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void SpawnMissing();

	UFUNCTION(BlueprintPure, Category = "Spawner")
	TArray<ABeyondEnemyCharacter*> GetSpawnedEnemies() const;

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
	};

	void BuildSlots();
	void SpawnSlot(FSlot& Slot);
	void CheckActivation();
	void HandlePartyWiped();

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
