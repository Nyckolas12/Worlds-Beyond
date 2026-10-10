// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BeyondEnemySubsystem.generated.h"

class ABeyondEnemyCharacter;
class UBeyondAffixDefinition;
class UBeyondEnemyDefinition;

/** How one enemy comes into the world */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondEnemySpawnParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "1"))
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bElite = false;

	// Elites without affixes roll theirs (Project Settings -> Worlds Beyond Enemies)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TArray<TObjectPtr<UBeyondAffixDefinition>> Affixes;

	// Summoned / split / cloned: no loot, no EXP
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bSummoned = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0.01"))
	float HealthScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0.1"))
	float SizeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
	float OutgoingDamageScale = 1.0f;

	// A boss's shadow copy (no phases, no boss bar)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bClone = false;
};

/**
 * Enemies in a world (Plan 3): spawns roster entries (with their definition applied before they set up), keeps the
 * list of living enemies (plates, tests, the console), passes pack alerts on and sends everyone home on a party wipe.
 * Console: Beyond.Spawn <id> [level] [elite | <affix id>...] [x<count>], Beyond.KillEnemies, Beyond.ListEnemies.
 */
UCLASS()
class WORLDBEYOND_API UBeyondEnemySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBeyondEnemySubsystem* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// Spawns Definition at Transform (snapped to the ground / navmesh), set up as an enemy of Params' level
	UFUNCTION(BlueprintCallable, Category = "Beyond|Enemies")
	ABeyondEnemyCharacter* SpawnEnemy(UBeyondEnemyDefinition* Definition, const FTransform& Transform, const FBeyondEnemySpawnParams& Params);

	// Count of them in a small ring around Centre
	UFUNCTION(BlueprintCallable, Category = "Beyond|Enemies")
	TArray<ABeyondEnemyCharacter*> SpawnPack(UBeyondEnemyDefinition* Definition, FVector Centre, float Yaw, int32 Count, const FBeyondEnemySpawnParams& Params);

	UFUNCTION(BlueprintPure, Category = "Beyond|Enemies")
	UBeyondEnemyDefinition* FindDefinition(FName EnemyId) const;

	UFUNCTION(BlueprintPure, Category = "Beyond|Enemies")
	UBeyondAffixDefinition* FindAffix(FName AffixId) const;

	// Random affixes for an elite of Definition at Level
	UFUNCTION(BlueprintCallable, Category = "Beyond|Enemies")
	TArray<UBeyondAffixDefinition*> RollAffixes(const UBeyondEnemyDefinition* Definition, int32 Level) const;

	UFUNCTION(BlueprintPure, Category = "Beyond|Enemies")
	TArray<ABeyondEnemyCharacter*> GetLiveEnemies() const;

	// Idle enemies within Radius of Source join its fight against Target
	void AlertNearby(const ABeyondEnemyCharacter* Source, AActor* Target, float Radius);

	// One enemy starts fighting Target (or whoever fired it: projectiles resolve to their instigator)
	void AlertEnemy(ABeyondEnemyCharacter* Enemy, AActor* Target);

	// The party went down: summoned enemies vanish, the rest go home refilled
	void ResetAllEnemies();

	void RegisterEnemy(ABeyondEnemyCharacter* Enemy);
	void UnregisterEnemy(ABeyondEnemyCharacter* Enemy);

	// Ground (and navmesh, when there is one) under Desired; false if neither was found
	static bool FindGroundPoint(const UWorld* World, const FVector& Desired, FVector& OutPoint);

private:
	TArray<TWeakObjectPtr<ABeyondEnemyCharacter>> LiveEnemies;
	FDelegateHandle PartyWipedHandle;
};
