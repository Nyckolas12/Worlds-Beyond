// Fill out your copyright notice in the Description page of Project Settings.


#include "BeyondCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffectExtension.h"

// Sets default values
ABeyondCharacterBase::ABeyondCharacterBase()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Add the ability system component
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(ASCReplicationMode);

	AttributeSet = CreateDefaultSubobject<UCharacterAttributeSet>(TEXT("BasicAttributeSet"));

	
}




// Called when the game starts or when spawned
void ABeyondCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	InitializeAttributeSet();
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
		GrantAbilities(StartingAbilities);
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

void ABeyondCharacterBase::OnDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (NewCount > 0)
	{
		HandleDeath();
	}
}

void ABeyondCharacterBase::HandleDeath_Implementation()
{
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();

	FVector Impulse = GetActorForwardVector() * -20000;
	Impulse.Z = 15000;
	GetMesh()->AddImpulseAtLocation(Impulse, GetActorLocation());
}

void ABeyondCharacterBase::InitializeAttributeSet()
{
	if (AbilitySystemComponent && AttributeSet)
	{
		
	}
}

UAbilitySystemComponent* ABeyondCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

TArray<FGameplayAbilitySpecHandle> ABeyondCharacterBase::GrantAbilities(
	TArray<TSubclassOf<UGameplayAbility>> AbilitiesToGrant)
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return TArray<FGameplayAbilitySpecHandle>();
	}

	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	for (TSubclassOf<UGameplayAbility> Ability : AbilitiesToGrant)
	{
		FGameplayAbilitySpecHandle SpecHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(
			Ability, 1, -1, this
		));
		AbilityHandles.Add(SpecHandle);
	}

	SendAbilitiesChangedEvent();
	return AbilityHandles;
}

void ABeyondCharacterBase::RemoveAbilities(TArray<FGameplayAbilitySpecHandle> AbilityHandlesToRemove)
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return;
	}

	for (FGameplayAbilitySpecHandle AbilityHandle : AbilityHandlesToRemove)
	{
		AbilitySystemComponent->ClearAbility(AbilityHandle);
	}

	SendAbilitiesChangedEvent();
}

void ABeyondCharacterBase::SendAbilitiesChangedEvent()
{
	FGameplayEventData EventData;
	EventData.EventTag = FGameplayTag::RequestGameplayTag(FName("Event.Abilities.Changed"));
	EventData.Instigator = this;
	EventData.Target = this;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, EventData.EventTag, EventData);
}

void ABeyondCharacterBase::ServerSendGameplayEventToSelf_Implementation(FGameplayEventData EventData)
{
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, EventData.EventTag, EventData);
}


