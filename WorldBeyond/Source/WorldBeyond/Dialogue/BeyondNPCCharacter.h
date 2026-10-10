// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BeyondNPCCharacter.generated.h"

class UAnimInstance;
class UMaterialInterface;
class USkeletalMesh;

/** A conversation an NPC can start (DT_Dialogue), picked by story flags */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondNPCConversation
{
	GENERATED_BODY()

	// First DT_Dialogue row
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conversation")
	FName StartRow;

	// Story flags that must all be set (e.g. Quest_Gorehide, Boss.boss_gorehide)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conversation")
	TArray<FName> RequiredFlags;

	// Skipped once any of these is set
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conversation")
	TArray<FName> BlockedByFlags;

	// Said only once (remembered as the flag Talked.<NPC Id>.<Start Row>)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conversation")
	bool bOnce = false;
};

/** Over-head chatter an NPC starts with its Chatter Partners (DT_TextOverHead) */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondNPCChatter
{
	GENERATED_BODY()

	// First DT_TextOverHead row; rows with Dialogue Actor Index i are said by Chatter Partners[i]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chatter")
	FName StartRow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chatter")
	TArray<FName> RequiredFlags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chatter")
	TArray<FName> BlockedByFlags;

	// Only when every Chatter Partner is there and free
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chatter")
	bool bNeedsPartners = true;
};

/**
 * A villager (Plan 4). BP_NPC_Base is the pack's BP_ExampleCharacter reparented to this class, so it keeps the pack's
 * dialogue interface (BP_I_Dialogue: Row Name, Is in Dialogue), its over-head text and focus camera.
 * - F talks: the first Conversation whose flags pass, face to face on the leader's BP_AC_Dialogue
 * - Chatter: over-head lines with its Chatter Partners while the leader is near (Project Settings -> Worlds Beyond
 *   Dialogue); Greet Rows: one line when the party first comes close
 * - optional wandering around where it was placed
 * Neutral: no team (enemies and the buddy ignore it) and no ability system (it can't be hurt).
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API ABeyondNPCCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABeyondNPCCharacter();

	// Used in flags (Talked.<NPC Id>.<Row>) and the log
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FName NPCId;

	// Shown on the talk prompt
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FText DisplayName;

	//~ Appearance (applied to CharacterMesh0 when set)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Appearance")
	TObjectPtr<USkeletalMesh> NPCMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Appearance")
	TSubclassOf<UAnimInstance> NPCAnimClass;

	// Per material slot; empty entries keep the mesh's own
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Appearance")
	TArray<TObjectPtr<UMaterialInterface>> NPCMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Appearance", meta = (ClampMin = "0.5", ClampMax = "2"))
	float NPCScale = 1.0f;

	// Mannequins face +Y: turn the mesh this much so the NPC faces where the actor does
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Appearance")
	float MeshYaw = -90.0f;

	//~ Talking

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Talk")
	bool bCanTalk = true;

	// In order: the first one whose flags pass is used
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Talk", meta = (TitleProperty = "StartRow"))
	TArray<FBeyondNPCConversation> Conversations;

	//~ Chatter

	// Taken in turn (skipping ones whose flags don't pass)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Chatter", meta = (TitleProperty = "StartRow"))
	TArray<FBeyondNPCChatter> Chatter;

	// Who else speaks in its chatter (placed NPCs; index = the row's Dialogue Actor Index)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "NPC|Chatter")
	TArray<TObjectPtr<AActor>> ChatterPartners;

	// One DT_TextOverHead line said when the party comes close (one at random)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Chatter")
	TArray<FName> GreetRows;

	//~ Life

	// Strolls around where it was placed; 0 stands still
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Life", meta = (ClampMin = "0"))
	float WanderRadius = 0.0f;

	// Seconds it waits between strolls
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Life")
	FVector2D WanderPause = FVector2D(5.0f, 10.0f);

	UFUNCTION(BlueprintPure, Category = "NPC")
	FText GetNPCName() const;

	// The conversation F would start now (None if it has nothing to say)
	UFUNCTION(BlueprintPure, Category = "NPC|Talk")
	FName PickConversation() const;

	UFUNCTION(BlueprintPure, Category = "NPC|Talk")
	bool CanTalkNow() const;

	// Starts the picked conversation with Speaker (the leader)
	UFUNCTION(BlueprintCallable, Category = "NPC|Talk")
	bool TalkTo(ACharacter* Speaker);

	// In a face-to-face talk or chatting over heads
	UFUNCTION(BlueprintPure, Category = "NPC")
	bool IsBusy() const;

	// The chatter it would start next (None if none passes)
	UFUNCTION(BlueprintPure, Category = "NPC|Chatter")
	FName PickChatter() const;

	// Starts Row (None: the next chatter) over its head with its partners
	UFUNCTION(BlueprintCallable, Category = "NPC|Chatter")
	bool StartChatter(FName Row = NAME_None);

	// Says one of its Greet Rows
	UFUNCTION(BlueprintCallable, Category = "NPC|Chatter")
	bool Greet();

	// Chatter starts over from its first entry, and the greeting and the timer are forgotten
	UFUNCTION(BlueprintCallable, Category = "NPC|Chatter")
	void ResetChatter();

	FName GetTalkedFlag(FName Row) const;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyAppearance();
	void Think();
	void Wander();
	bool ArePartnersFree() const;
	const FBeyondNPCConversation* FindConversation() const;

	FTimerHandle ThinkTimer;
	FVector Home = FVector::ZeroVector;
	double NextChatterTime = 0.0;
	double LastGreetTime = -1.0e9;
	double NextWanderTime = 0.0;
	int32 NextChatterIndex = 0;
	bool bLeaderClose = false;
};
