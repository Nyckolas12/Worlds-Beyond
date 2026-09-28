// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BeyondCheckpoint.generated.h"

class UArrowComponent;
class UBoxComponent;

/** Walk the controlled demigod into the box to set the respawn point (the arrow). */
UCLASS()
class WORLDBEYOND_API ABeyondCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	ABeyondCheckpoint();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	TObjectPtr<UBoxComponent> Trigger;

	// Where and which way the party respawns
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	TObjectPtr<UArrowComponent> SpawnPoint;

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Checkpoint")
	void OnActivated();

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	bool bActivated = false;
};
