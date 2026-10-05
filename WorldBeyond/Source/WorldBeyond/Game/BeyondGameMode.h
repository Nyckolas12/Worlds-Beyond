// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BeyondGameMode.generated.h"

class APlayerController;

/**
 * Checkpoints and party-wipe respawns. Until a checkpoint is reached, the party respawns at the player start.
 */
UCLASS()
class WORLDBEYOND_API ABeyondGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0"))
	float RespawnDelay = 3.0f;

	UFUNCTION(BlueprintCallable, Category = "Respawn")
	void SetCheckpoint(const FTransform& SpawnTransform);

	void HandlePartyWiped(APlayerController* PlayerController);

protected:
	// Show a "you fell" screen, fade out, etc.
	UFUNCTION(BlueprintImplementableEvent, Category = "Respawn")
	void OnPartyWiped();

	UFUNCTION(BlueprintImplementableEvent, Category = "Respawn")
	void OnPartyRespawned();

private:
	void RespawnParty(TWeakObjectPtr<APlayerController> PlayerController);

	FTransform CheckpointTransform;
	bool bHasCheckpoint = false;
	FTimerHandle RespawnTimer;
};
