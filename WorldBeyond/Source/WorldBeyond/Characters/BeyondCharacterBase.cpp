// Fill out your copyright notice in the Description page of Project Settings.


#include "BeyondCharacterBase.h"
#include "BeyondCharacterBase.h"

// Sets default values
ABeyondCharacterBase::ABeyondCharacterBase()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Add the ability system component
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(ASCReplicationMode);


	
}

// Called when the game starts or when spawned
void ABeyondCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABeyondCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABeyondCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void ABeyondCharacterBase::PossessedBy(AController* NewControl)
{
	Super::PossessedBy(NewControl);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this,this);
	}
}

void ABeyondCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this,this);
	}
}

UAbilitySystemComponent* ABeyondCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}


