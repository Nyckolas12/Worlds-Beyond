// Fill out your copyright notice in the Description page of Project Settings.

#include "Dialogue/BeyondBanter.h"
#include "Components/BoxComponent.h"
#include "Dialogue/BeyondBanterComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

const FBeyondBanterLine* UBeyondBanterSet::FindLine(FName Id) const
{
	return Lines.FindByPredicate([Id](const FBeyondBanterLine& Line) { return Line.Id == Id; });
}

ABeyondBanterVolume::ABeyondBanterVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(600.0f, 600.0f, 300.0f));
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetGenerateOverlapEvents(true);
	Box->ShapeColor = FColor(120, 140, 255);
	RootComponent = Box;
	Box->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleOverlap);
}

void ABeyondBanterVolume::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Only the player's demigod (the buddy walking in after the leader doesn't count twice)
	const APawn* Pawn = Cast<APawn>(OtherActor);
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (UBeyondBanterComponent* Banter = PC ? PC->FindComponentByClass<UBeyondBanterComponent>() : nullptr)
	{
		Banter->TriggerBanter(Trigger, Context);
	}
}
