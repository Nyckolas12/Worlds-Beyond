// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/BeyondGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Player/BeyondPlayerController.h"
#include "Player/BeyondPartyComponent.h"
#include "TimerManager.h"
#include "World/BeyondWorldSubsystem.h"

void ABeyondGameMode::SetCheckpoint(const FTransform& SpawnTransform)
{
	CheckpointTransform = SpawnTransform;
	bHasCheckpoint = true;
}

void ABeyondGameMode::HandlePartyWiped(APlayerController* PlayerController)
{
	OnPartyWiped();

	FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &ThisClass::RespawnParty, TWeakObjectPtr<APlayerController>(PlayerController));
	GetWorldTimerManager().SetTimer(RespawnTimer, Delegate, FMath::Max(RespawnDelay, 0.01f), false);
}

void ABeyondGameMode::RespawnParty(TWeakObjectPtr<APlayerController> PlayerController)
{
	const ABeyondPlayerController* BeyondPC = Cast<ABeyondPlayerController>(PlayerController.Get());
	if (!BeyondPC)
	{
		return;
	}

	FTransform SpawnTransform = CheckpointTransform;
	if (!bHasCheckpoint)
	{
		if (const AActor* Start = FindPlayerStart(PlayerController.Get()))
		{
			SpawnTransform = Start->GetActorTransform();
		}
	}

	// The open world waits for the ground at the respawn point first (it may have streamed out)
	UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this);
	if (!WorldSubsystem || !WorldSubsystem->ShouldHoldOnTravel() || !WorldSubsystem->TravelPartyTo(SpawnTransform, EBeyondTravelReason::Respawn, true))
	{
		BeyondPC->PartyComponent->RespawnPartyAt(SpawnTransform);
	}
	OnPartyRespawned();
}
