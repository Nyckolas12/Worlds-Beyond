// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BeyondPartyComponent.generated.h"

class ABeyondCharacterBase;
class ABeyondCompanionController;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondLeaderChangedSignature, ABeyondCharacterBase*, NewLeader, ABeyondCharacterBase*, OldLeader);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondReviveProgressSignature, ABeyondCharacterBase*, DownedMember, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBeyondPartyWipedSignature);

/**
 * The two demigods: the player controls the leader, the other is driven by a companion controller.
 * Handles swapping, auto-swap when the leader falls, proximity revives and party wipes.
 * Lives on ABeyondPlayerController.
 */
UCLASS(ClassGroup = (Beyond), meta = (BlueprintSpawnableComponent))
class WORLDBEYOND_API UBeyondPartyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBeyondPartyComponent();

	/**
	 * Party members. Instances already placed in the level are reused, missing ones are spawned
	 * next to the leader. Leave empty to use every Player-team Beyond character in the level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party")
	TArray<TSubclassOf<ABeyondCharacterBase>> PartyClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party")
	TSubclassOf<ABeyondCompanionController> CompanionControllerClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Swap", meta = (ClampMin = "0"))
	float SwapCooldown = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Swap", meta = (ClampMin = "0"))
	float CameraBlendTime = 0.35f;

	// Swap to the other demigod this long after the leader dies
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Swap", meta = (ClampMin = "0"))
	float AutoSwapDelayOnDeath = 1.5f;

	// Stand this close to a downed demigod to revive it
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Revive", meta = (ClampMin = "0"))
	float ReviveRadius = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Revive", meta = (ClampMin = "0.1"))
	float ReviveTime = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Revive", meta = (ClampMin = "0.05", ClampMax = "1"))
	float ReviveHealthFraction = 0.3f;

	UPROPERTY(BlueprintAssignable, Category = "Party")
	FBeyondLeaderChangedSignature OnLeaderChanged;

	UPROPERTY(BlueprintAssignable, Category = "Party")
	FBeyondReviveProgressSignature OnReviveProgress;

	UPROPERTY(BlueprintAssignable, Category = "Party")
	FBeyondPartyWipedSignature OnPartyWiped;

	// Called by the player controller once it possesses its first pawn
	void InitializeParty(APawn* InitialLeader);

	UFUNCTION(BlueprintCallable, Category = "Party")
	bool SwapLeader();

	UFUNCTION(BlueprintPure, Category = "Party")
	ABeyondCharacterBase* GetLeader() const;

	// The member the player isn't controlling (first one, if more than two)
	UFUNCTION(BlueprintPure, Category = "Party")
	ABeyondCharacterBase* GetCompanion() const;

	UFUNCTION(BlueprintPure, Category = "Party")
	TArray<ABeyondCharacterBase*> GetMembers() const;

	// Revive / heal everyone and move them to Transform (checkpoint respawn)
	UFUNCTION(BlueprintCallable, Category = "Party")
	void RespawnPartyAt(const FTransform& Transform);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UFUNCTION()
	void HandleMemberKilled(ABeyondCharacterBase* Member, AActor* Killer);

private:
	APlayerController* GetPlayerController() const;
	void AddMember(ABeyondCharacterBase* Member);
	void GiveToCompanionController(ABeyondCharacterBase* Member, ABeyondCharacterBase* NewLeader);
	ABeyondCharacterBase* FindNextAliveMember(const ABeyondCharacterBase* After) const;
	bool SwapTo(ABeyondCharacterBase* NewLeader, bool bIgnoreCooldown);
	void AutoSwapAfterDeath();
	void UpdateRevive(float DeltaTime);

	UPROPERTY(Transient)
	TArray<TObjectPtr<ABeyondCharacterBase>> Members;

	TWeakObjectPtr<ABeyondCharacterBase> ReviveTarget;
	float ReviveProgress = 0.0f;
	float LastSwapTime = -1000.0f;
	bool bInitialized = false;
	FTimerHandle AutoSwapTimer;
};
