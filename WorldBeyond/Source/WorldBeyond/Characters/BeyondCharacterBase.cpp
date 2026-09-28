// Fill out your copyright notice in the Description page of Project Settings.


#include "BeyondCharacterBase.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/BeyondAbilitySet.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGameplayTags.h"
#include "Perception/AISense_Damage.h"

// Sets default values
ABeyondCharacterBase::ABeyondCharacterBase()
{
	// Blueprints that implement Event Tick turn ticking back on automatically
	PrimaryActorTick.bCanEverTick = false;

	// Add the ability system component
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(ASCReplicationMode);

	AttributeSet = CreateDefaultSubobject<UCharacterAttributeSet>(TEXT("BasicAttributeSet"));


}




// Called when the game starts or when spawned
void ABeyondCharacterBase::BeginPlay()
{
	MeshRelativeTransform = GetMesh()->GetRelativeTransform();
	MeshCollision = GetMesh()->GetCollisionEnabled();
	CapsuleCollision = GetCapsuleComponent()->GetCollisionEnabled();

	// Before Super so Blueprint BeginPlay already sees attributes and abilities
	InitAbilitySystem();
	Super::BeginPlay();
}

// Called to bind functionality to input
void ABeyondCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput || !AbilitySet)
	{
		return;
	}

	for (const FBeyondAbilitySet_Ability& Entry : AbilitySet->Abilities)
	{
		if (!Entry.InputAction || !Entry.Ability)
		{
			continue;
		}

		FGameplayTag InputTag = Entry.InputTag;
		if (!InputTag.IsValid())
		{
			if (const UBeyondGameplayAbility* BeyondCDO = Cast<UBeyondGameplayAbility>(Entry.Ability->GetDefaultObject()))
			{
				InputTag = BeyondCDO->InputTag;
			}
		}
		if (!InputTag.IsValid())
		{
			continue;
		}

		EnhancedInput->BindAction(Entry.InputAction.Get(), ETriggerEvent::Started, this, &ThisClass::Input_AbilityPressed, InputTag);
		EnhancedInput->BindAction(Entry.InputAction.Get(), ETriggerEvent::Completed, this, &ThisClass::Input_AbilityReleased, InputTag);
	}
}

void ABeyondCharacterBase::PossessedBy(AController* NewControl)
{
	Super::PossessedBy(NewControl);

	InitAbilitySystem();

	// AI controllers start with NoTeam; give them ours so perception affiliation works
	if (IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(NewControl))
	{
		TeamAgent->SetGenericTeamId(GetGenericTeamId());
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

void ABeyondCharacterBase::InitAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (!bAbilitySystemBound)
	{
		bAbilitySystemBound = true;

		if (AttributeSet)
		{
			AttributeSet->OnHitTaken.AddUObject(this, &ThisClass::HandleAttributeHitTaken);
			AttributeSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);
		}

		AbilitySystemComponent->RegisterGameplayTagEvent(BeyondTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnDeadTagChanged);
	}

	// Grant everything exactly once, no matter how often the character is possessed (swapping)
	if (HasAuthority() && !bStartupGiven)
	{
		bStartupGiven = true;

		InitializeAttributeSet();

		if (AbilitySet)
		{
			AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, this, &AbilitySetHandles);
		}

		GrantAbilities(StartingAbilities);
	}
}

void ABeyondCharacterBase::InitializeAttributeSet()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	if (DefaultAttributesEffect)
	{
		AbilitySystemComponent->ApplyGameplayEffectToSelf(DefaultAttributesEffect->GetDefaultObject<UGameplayEffect>(), 1.0f, AbilitySystemComponent->MakeEffectContext());
		return;
	}

	// Max before current so the clamp in the attribute set doesn't cut the current value
	AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), DefaultMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), DefaultMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), DefaultMaxStamina);
	AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentStaminaAttribute(), DefaultMaxStamina);
}

void ABeyondCharacterBase::HandleAttributeHitTaken(AActor* DamageInstigator, AActor* Causer, float Damage, FGameplayTag HitResponse)
{
	if (Damage > 0.0f)
	{
		LastDamageInstigator = DamageInstigator;

		// Keeps AI damage-sense reactions (turn to / chase the attacker) working with GAS damage
		if (DamageInstigator)
		{
			UAISense_Damage::ReportDamageEvent(this, this, DamageInstigator, Damage, DamageInstigator->GetActorLocation(), GetActorLocation());
		}
	}

	OnHitTaken(DamageInstigator, Damage, HitResponse);
	OnCharacterHitTaken.Broadcast(this, DamageInstigator, Damage, HitResponse);
}

void ABeyondCharacterBase::HandleOutOfHealth(AActor* DamageInstigator, AActor* Causer, float Damage, FGameplayTag HitResponse)
{
	if (DamageInstigator)
	{
		LastDamageInstigator = DamageInstigator;
	}

	// The tag change drives the rest of the death flow (OnDeadTagChanged)
	if (AbilitySystemComponent && !AbilitySystemComponent->HasMatchingGameplayTag(BeyondTags::State_Dead))
	{
		AbilitySystemComponent->AddLooseGameplayTag(BeyondTags::State_Dead);
	}
}

void ABeyondCharacterBase::OnDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (NewCount > 0)
	{
		Die();
	}
	else
	{
		OnRevived();
	}
}

void ABeyondCharacterBase::Die()
{
	AActor* Killer = LastDamageInstigator.Get();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();

		// Lets a GA_Death with an Event.Death trigger play
		FGameplayEventData Payload;
		Payload.EventTag = BeyondTags::Event_Death;
		Payload.Instigator = Killer;
		Payload.Target = this;
		AbilitySystemComponent->HandleGameplayEvent(BeyondTags::Event_Death, &Payload);
	}

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Dead"));
		}
	}

	HandleDeath();
	OnKilled(Killer);
	OnCharacterKilled.Broadcast(this, Killer);

	if (DestroyDelayAfterDeath > 0.0f)
	{
		SetLifeSpan(DestroyDelayAfterDeath);
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

void ABeyondCharacterBase::Revive(float HealthFraction)
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->HasMatchingGameplayTag(BeyondTags::State_Dead))
	{
		return;
	}

	// Undo the ragdoll: bring the capsule to where the body ended up and snap the mesh back
	USkeletalMeshComponent* MeshComp = GetMesh();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (MeshComp->IsSimulatingPhysics())
	{
		const FVector BodyLocation = MeshComp->GetComponentLocation() + FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight());
		MeshComp->SetSimulatePhysics(false);
		SetActorLocation(BodyLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	MeshComp->AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	MeshComp->SetRelativeTransform(MeshRelativeTransform);
	MeshComp->SetCollisionEnabled(MeshCollision);
	Capsule->SetCollisionEnabled(CapsuleCollision);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	SetLifeSpan(0.0f);

	// Removing the tag calls OnRevived; heal afterwards because dead characters can't be healed
	AbilitySystemComponent->SetLooseGameplayTagCount(BeyondTags::State_Dead, 0);
	UBeyondCombatLibrary::ApplyHeal(this, this, FMath::Max(1.0f, AttributeSet->GetMaxHealth() * FMath::Clamp(HealthFraction, 0.0f, 1.0f)));
}

UAbilitySystemComponent* ABeyondCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

FGenericTeamId ABeyondCharacterBase::GetGenericTeamId() const
{
	return TeamAffiliation == EBeyondTeam::Neutral ? FGenericTeamId::NoTeam : FGenericTeamId(static_cast<uint8>(TeamAffiliation));
}

void ABeyondCharacterBase::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	TeamAffiliation = NewTeamID == FGenericTeamId::NoTeam ? EBeyondTeam::Neutral : static_cast<EBeyondTeam>(NewTeamID.GetId());
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
		if (!Ability)
		{
			continue;
		}

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
	EventData.EventTag = BeyondTags::Event_Abilities_Changed;
	EventData.Instigator = this;
	EventData.Target = this;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, EventData.EventTag, EventData);
}

void ABeyondCharacterBase::ServerSendGameplayEventToSelf_Implementation(FGameplayEventData EventData)
{
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, EventData.EventTag, EventData);
}

void ABeyondCharacterBase::CollectSpecsWithInputTag(const FGameplayTag& InputTag, TArray<FGameplayAbilitySpecHandle>& OutHandles) const
{
	if (!AbilitySystemComponent || !InputTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			OutHandles.Add(Spec.Handle);
		}
	}
}

bool ABeyondCharacterBase::TryActivateAbilityByInputTag(FGameplayTag InputTag)
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	CollectSpecsWithInputTag(InputTag, Handles);

	bool bActivated = false;
	for (const FGameplayAbilitySpecHandle& Handle : Handles)
	{
		bActivated |= AbilitySystemComponent->TryActivateAbility(Handle);
	}
	return bActivated;
}

void ABeyondCharacterBase::Input_AbilityPressed(FGameplayTag InputTag)
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	CollectSpecsWithInputTag(InputTag, Handles);

	for (const FGameplayAbilitySpecHandle& Handle : Handles)
	{
		FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
		if (!Spec)
		{
			continue;
		}

		if (Spec->IsActive())
		{
			// Already running (combos, charge-ups): forward the press so WaitInputPress tasks fire
			AbilitySystemComponent->AbilitySpecInputPressed(*Spec);
			if (UGameplayAbility* Instance = Spec->GetPrimaryInstance())
			{
				AbilitySystemComponent->InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Handle,
					Instance->GetCurrentActivationInfoRef().GetActivationPredictionKey());
			}
		}
		else
		{
			AbilitySystemComponent->TryActivateAbility(Handle);
		}
	}
}

void ABeyondCharacterBase::Input_AbilityReleased(FGameplayTag InputTag)
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	CollectSpecsWithInputTag(InputTag, Handles);

	for (const FGameplayAbilitySpecHandle& Handle : Handles)
	{
		FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->IsActive())
		{
			continue;
		}

		AbilitySystemComponent->AbilitySpecInputReleased(*Spec);
		if (UGameplayAbility* Instance = Spec->GetPrimaryInstance())
		{
			AbilitySystemComponent->InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Handle,
				Instance->GetCurrentActivationInfoRef().GetActivationPredictionKey());
		}
	}
}
