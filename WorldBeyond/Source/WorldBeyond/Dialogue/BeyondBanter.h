// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "BeyondBanter.generated.h"

class ABeyondCharacterBase;
class UBoxComponent;

// What starts a banter
UENUM(BlueprintType)
enum class EBeyondBanterTrigger : uint8
{
	// The party walks into a banter volume (Plan 5: region volumes); Context = the region id
	RegionEntered,
	// The leader drops under Low Health Fraction in a fight
	LowHealth,
	// A boss fell; Context = its boss id
	BossDefeated,
	// Nobody moved or fought for Idle Seconds
	Idle,
	// A demigod levelled up
	LevelUp,
	// A downed demigod was revived
	Revived,
	// Only by name (Blueprints, console Beyond.Banter)
	Scripted
};

/** One banter: a free-movement DT_Dialogue chain the demigods say while you play */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondBanterLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banter")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banter")
	EBeyondBanterTrigger Trigger = EBeyondBanterTrigger::Idle;

	// Region or boss id it is about; None fits any (lines for the exact context win)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banter")
	FName Context;

	// First DT_Dialogue row (rows of type "free movement dialogue")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banter")
	FName StartRow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banter")
	TArray<FName> RequiredFlags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banter")
	TArray<FName> BlockedByFlags;

	// Said only once (remembered as the flag Banter.<Id>)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banter")
	bool bOnce = false;

	// Seconds before this line can come again
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banter", meta = (ClampMin = "0"))
	float Cooldown = 300.0f;

	// Chance against the other fitting lines
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banter", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;

	// Only while this demigod leads (empty: either)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banter")
	TSoftClassPtr<ABeyondCharacterBase> RequiredLeader;
};

/** Angel and Ji-Woong's banter (DA_Banter, made by migrate_pass13.py; Project Settings -> Worlds Beyond Dialogue) */
UCLASS(BlueprintType)
class WORLDBEYOND_API UBeyondBanterSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banter", meta = (TitleProperty = "Id"))
	TArray<FBeyondBanterLine> Lines;

	const FBeyondBanterLine* FindLine(FName Id) const;
};

/**
 * Starts banter when the party leader walks in: a region's first look until Plan 5 brings region volumes (which
 * call UBeyondBanterComponent::TriggerBanter the same way).
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API ABeyondBanterVolume : public AActor
{
	GENERATED_BODY()

public:
	ABeyondBanterVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Banter")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banter")
	EBeyondBanterTrigger Trigger = EBeyondBanterTrigger::RegionEntered;

	// e.g. region_forest
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Banter")
	FName Context;

protected:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
