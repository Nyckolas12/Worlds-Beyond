// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/BeyondCheckpoint.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Game/BeyondGameMode.h"
#include "GameFramework/Pawn.h"

ABeyondCheckpoint::ABeyondCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(200.0f, 200.0f, 150.0f));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	RootComponent = Trigger;

	SpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(Trigger);
	SpawnPoint->SetRelativeLocation(FVector(0.0f, 0.0f, -50.0f));
}

void ABeyondCheckpoint::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleOverlap);
}

void ABeyondCheckpoint::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (bActivated || !Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	if (ABeyondGameMode* GameMode = GetWorld()->GetAuthGameMode<ABeyondGameMode>())
	{
		bActivated = true;
		GameMode->SetCheckpoint(SpawnPoint->GetComponentTransform());
		OnActivated();
	}
}
