// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_DuoStrike.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "BeyondGameplayTags.h"
#include "Blueprint/UserWidget.h"
#include "Characters/BeyondCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "TimerManager.h"

UBeyondGA_DuoStrike::UBeyondGA_DuoStrike()
{
	InputTag = BeyondTags::Ability_Input_Duo;
	ShockwaveHitResponse = BeyondTags::Event_Hit_KnockBack;
	bAIUsable = false;
	bUsableDuringDuo = true;
}

UBeyondPartyComponent* UBeyondGA_DuoStrike::GetParty() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const ABeyondPlayerController* PC = Avatar ? Cast<ABeyondPlayerController>(UGameplayStatics::GetPlayerController(Avatar, 0)) : nullptr;
	return PC ? PC->PartyComponent.Get() : nullptr;
}

bool UBeyondGA_DuoStrike::FindDuo(ABeyondCharacterBase*& OutConduit, ABeyondCharacterBase*& OutStriker) const
{
	return FindDuoFor(GetAvatarActorFromActorInfo(), OutConduit, OutStriker);
}

bool UBeyondGA_DuoStrike::FindDuoFor(const AActor* Avatar, ABeyondCharacterBase*& OutConduit, ABeyondCharacterBase*& OutStriker) const
{
	OutConduit = nullptr;
	OutStriker = nullptr;

	const AActor* ContextActor = Avatar ? Avatar : GetAvatarActorFromActorInfo();
	const ABeyondPlayerController* PC = ContextActor ? Cast<ABeyondPlayerController>(UGameplayStatics::GetPlayerController(ContextActor, 0)) : nullptr;
	const UBeyondPartyComponent* Party = PC ? PC->PartyComponent.Get() : nullptr;
	if (!Party)
	{
		return false;
	}

	const TArray<ABeyondCharacterBase*> Members = Party->GetMembers();
	if (!Members.Contains(Avatar))
	{
		return false;
	}

	for (ABeyondCharacterBase* Member : Members)
	{
		if (!Member || UBeyondCombatLibrary::IsActorDead(Member))
		{
			continue;
		}
		if (Member->DuoRole == EBeyondDuoRole::Conduit && !OutConduit)
		{
			OutConduit = Member;
		}
		else if (Member->DuoRole == EBeyondDuoRole::Striker && !OutStriker)
		{
			OutStriker = Member;
		}
	}

	return OutConduit && OutStriker && OutConduit != OutStriker
		&& FVector::Dist(OutConduit->GetActorLocation(), OutStriker->GetActorLocation()) <= PartnerRange;
}

bool UBeyondGA_DuoStrike::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) || !ActorInfo)
	{
		return false;
	}

	const AActor* Avatar = ActorInfo->AvatarActor.Get();
	const ABeyondPlayerController* PC = Avatar ? Cast<ABeyondPlayerController>(UGameplayStatics::GetPlayerController(Avatar, 0)) : nullptr;
	if (!PC || !PC->PartyComponent || !PC->PartyComponent->IsBondFull())
	{
		return false;
	}

	ABeyondCharacterBase* FoundConduit = nullptr;
	ABeyondCharacterBase* FoundStriker = nullptr;
	return FindDuoFor(Avatar, FoundConduit, FoundStriker);
}

void UBeyondGA_DuoStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ABeyondCharacterBase* FoundConduit = nullptr;
	ABeyondCharacterBase* FoundStriker = nullptr;
	UBeyondPartyComponent* Party = GetParty();
	if (!Party || !FindDuoFor(ActorInfo->AvatarActor.Get(), FoundConduit, FoundStriker)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo) || !Party->ConsumeBond())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Conduit = FoundConduit;
	Striker = FoundStriker;
	FlashesDone = 0;
	bShockwaveDone = false;

	// Both demigods commit to the move: stop what they were doing, can't be hurt or knocked out of it
	SetDuoState(FoundConduit, true);
	SetDuoState(FoundStriker, true);

	// The player stays put while the move plays out
	const APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
	if (AController* Controller = AvatarPawn ? AvatarPawn->GetController() : nullptr)
	{
		Controller->SetIgnoreMoveInput(true);
		bIgnoringMoveInput = true;
	}

	// The Conduit turns to the Striker it is about to empower
	const FVector ToStriker = (FoundStriker->GetActorLocation() - FoundConduit->GetActorLocation()).GetSafeNormal2D();
	if (!ToStriker.IsNearlyZero())
	{
		FoundConduit->SetActorRotation(ToStriker.Rotation());
	}

	OnPhaseStarted(1, FoundConduit, FoundStriker);
	PlayMontageOn(FoundConduit, ConduitMontage);

	if (FlashCount > 0)
	{
		Flash();
		GetWorld()->GetTimerManager().SetTimer(FlashTimer, this, &ThisClass::Flash, FlashInterval, true);
	}
	else
	{
		StartAbsorb();
	}
}

void UBeyondGA_DuoStrike::SetDuoState(ABeyondCharacterBase* Character, bool bActive)
{
	UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	if (bActive)
	{
		// Anything the partner was in the middle of (combo, cast) makes way
		ASC->CancelAllAbilities(this);
		ASC->AddLooseGameplayTag(BeyondTags::State_Duo);
		ASC->AddLooseGameplayTag(BeyondTags::State_Invincible);
		ASC->AddLooseGameplayTag(BeyondTags::State_Uninterruptible);
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
		TaggedCharacters.AddUnique(Character);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(BeyondTags::State_Duo);
		ASC->RemoveLooseGameplayTag(BeyondTags::State_Invincible);
		ASC->RemoveLooseGameplayTag(BeyondTags::State_Uninterruptible);
	}
}

void UBeyondGA_DuoStrike::Flash()
{
	ABeyondCharacterBase* StrikerCharacter = Striker.Get();
	ABeyondCharacterBase* ConduitCharacter = Conduit.Get();
	if (!IsActive() || !StrikerCharacter || !ConduitCharacter)
	{
		return;
	}

	// Strike a random enemy near the Striker; with no enemies around, the sky still cracks for show
	const FVector Center = StrikerCharacter->GetActorLocation();
	const TArray<AActor*> Hostiles = FindHostilesInRadius(Center, FlashRadius);
	AActor* Target = Hostiles.IsEmpty() ? nullptr : Hostiles[FMath::RandHelper(Hostiles.Num())];
	const FVector2D Scatter = FMath::RandPointInCircle(FlashRadius * 0.6f);
	const FVector Location = Target ? Target->GetActorLocation() : Center + FVector(Scatter.X, Scatter.Y, 0.0f);

	if (!FlashFX.IsEmpty())
	{
		BeyondFX::SpawnAtLocation(this, FlashFX[FlashesDone % FlashFX.Num()], Location);
	}

	if (Target)
	{
		UBeyondCombatLibrary::ApplyDamage(ConduitCharacter, Target, FlashDamage, BeyondTags::DamageType_Projectile, BeyondTags::Event_Hit_Stun, true);
		if (ACharacter* TargetCharacter = Cast<ACharacter>(Target); TargetCharacter && FlashLaunchSpeed > 0.0f && !IsBoss(Target))
		{
			TargetCharacter->LaunchCharacter(FVector(0.0f, 0.0f, FlashLaunchSpeed), false, true);
		}
	}

	if (++FlashesDone >= FlashCount)
	{
		GetWorld()->GetTimerManager().ClearTimer(FlashTimer);
		GetWorld()->GetTimerManager().SetTimer(PhaseTimer, this, &ThisClass::StartAbsorb, FlashInterval, false);
	}
}

void UBeyondGA_DuoStrike::StartAbsorb()
{
	ABeyondCharacterBase* StrikerCharacter = Striker.Get();
	ABeyondCharacterBase* ConduitCharacter = Conduit.Get();
	if (!IsActive() || !StrikerCharacter || !ConduitCharacter)
	{
		Finish();
		return;
	}

	OnPhaseStarted(2, ConduitCharacter, StrikerCharacter);

	USkeletalMeshComponent* ConduitMesh = ConduitCharacter->GetCombatMesh();
	BeyondFX::SpawnAttached(ConduitReleaseFX, ConduitMesh, ConduitMesh && ConduitMesh->DoesSocketExist(TEXT("hand_r")) ? FName(TEXT("hand_r")) : NAME_None);

	// The lightning travels from Angel to Ji-Woong in a line of strikes
	const FVector From = ConduitCharacter->GetActorLocation();
	const FVector To = StrikerCharacter->GetActorLocation();
	for (int32 Step = 1; Step <= ArcSteps; ++Step)
	{
		BeyondFX::SpawnAtLocation(this, ArcFX, FMath::Lerp(From, To, static_cast<float>(Step) / static_cast<float>(ArcSteps)));
	}

	USkeletalMeshComponent* StrikerMesh = StrikerCharacter->GetCombatMesh();
	for (const FBeyondFX& Aura : ChargedAuraFX)
	{
		if (UFXSystemComponent* Spawned = BeyondFX::SpawnAttached(Aura, StrikerMesh ? static_cast<USceneComponent*>(StrikerMesh) : StrikerCharacter->GetRootComponent()))
		{
			AuraComponents.Add(Spawned);
		}
	}

	PlayMontageOn(StrikerCharacter, StrikerChargeMontage);
	GetWorld()->GetTimerManager().SetTimer(PhaseTimer, this, &ThisClass::StartJudgment, FMath::Max(AbsorbDuration, 0.01f), false);
}

void UBeyondGA_DuoStrike::StartJudgment()
{
	ABeyondCharacterBase* StrikerCharacter = Striker.Get();
	if (!IsActive() || !StrikerCharacter)
	{
		Finish();
		return;
	}

	OnPhaseStarted(3, Conduit.Get(), StrikerCharacter);

	if (UAbilitySystemComponent* StrikerASC = StrikerCharacter->GetAbilitySystemComponent())
	{
		StrikerASC->GenericGameplayEventCallbacks.FindOrAdd(BeyondTags::Event_Montage_Trigger).AddUObject(this, &ThisClass::HandleStrikerEvent);
	}

	const float Duration = PlayMontageOn(StrikerCharacter, StrikerSlamMontage);
	const float ImpactTime = Duration > 0.0f ? FMath::Clamp(SlamImpactTime, 0.01f, Duration) : 0.01f;
	GetWorld()->GetTimerManager().SetTimer(ImpactTimer, this, &ThisClass::Shockwave, ImpactTime, false);
}

void UBeyondGA_DuoStrike::HandleStrikerEvent(const FGameplayEventData* Payload)
{
	Shockwave();
}

void UBeyondGA_DuoStrike::Shockwave()
{
	ABeyondCharacterBase* StrikerCharacter = Striker.Get();
	if (bShockwaveDone || !IsActive() || !StrikerCharacter)
	{
		return;
	}
	bShockwaveDone = true;
	GetWorld()->GetTimerManager().ClearTimer(ImpactTimer);
	UnbindStrikerEvent();

	// The absorbed storm leaves the blade
	for (const TWeakObjectPtr<UFXSystemComponent>& Aura : AuraComponents)
	{
		if (UFXSystemComponent* Component = Aura.Get())
		{
			Component->DestroyComponent();
		}
	}
	AuraComponents.Reset();

	const FVector Center = StrikerCharacter->GetActorLocation();
	const FVector Ground = Center - FVector(0.0f, 0.0f, StrikerCharacter->GetDefaultHalfHeight());
	for (const FBeyondFX& Wave : ShockwaveFX)
	{
		BeyondFX::SpawnAtLocation(this, Wave, Ground);
	}

	for (AActor* Target : FindHostilesInRadius(Center, ShockwaveRadius))
	{
		const float Distance = FVector::Dist2D(Center, Target->GetActorLocation());
		const float Alpha = FMath::Clamp(Distance / ShockwaveRadius, 0.0f, 1.0f);
		const float Damage = FMath::Lerp(ShockwaveDamageCenter, ShockwaveDamageEdge, Alpha);
		UBeyondCombatLibrary::ApplyDamage(StrikerCharacter, Target, Damage, BeyondTags::DamageType_Explosion, ShockwaveHitResponse, true, StrikerCharacter, true);

		if (ACharacter* TargetCharacter = Cast<ACharacter>(Target); TargetCharacter && KnockbackSpeed > 0.0f && !IsBoss(Target))
		{
			const FVector Away = (Target->GetActorLocation() - Center).GetSafeNormal2D();
			TargetCharacter->LaunchCharacter(Away * KnockbackSpeed * (1.0f - 0.5f * Alpha) + FVector(0.0f, 0.0f, 350.0f), true, true);
		}
	}

	GetWorld()->GetTimerManager().SetTimer(PhaseTimer, this, &ThisClass::Finish, FMath::Max(RecoveryTime, 0.01f), false);
}

void UBeyondGA_DuoStrike::Finish()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UBeyondGA_DuoStrike::UnbindStrikerEvent()
{
	if (const ABeyondCharacterBase* StrikerCharacter = Striker.Get())
	{
		if (UAbilitySystemComponent* ASC = StrikerCharacter->GetAbilitySystemComponent())
		{
			if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(BeyondTags::Event_Montage_Trigger))
			{
				Delegate->RemoveAll(this);
			}
		}
	}
}

float UBeyondGA_DuoStrike::PlayMontageOn(ABeyondCharacterBase* Character, UAnimMontage* Montage)
{
	const USkeletalMeshComponent* Mesh = Character ? Character->GetCombatMesh() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	return (Montage && AnimInstance) ? AnimInstance->Montage_Play(Montage) : 0.0f;
}

bool UBeyondGA_DuoStrike::IsBoss(const AActor* Actor)
{
	const ABeyondCharacterBase* Character = Cast<ABeyondCharacterBase>(Actor);
	return Character && Character->BossBarWidgetClass;
}

void UBeyondGA_DuoStrike::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(FlashTimer);
		Timers.ClearTimer(PhaseTimer);
		Timers.ClearTimer(ImpactTimer);
	}
	UnbindStrikerEvent();

	for (const TWeakObjectPtr<UFXSystemComponent>& Aura : AuraComponents)
	{
		if (UFXSystemComponent* Component = Aura.Get())
		{
			Component->DestroyComponent();
		}
	}
	AuraComponents.Reset();

	for (const TWeakObjectPtr<ABeyondCharacterBase>& Character : TaggedCharacters)
	{
		SetDuoState(Character.Get(), false);
	}
	TaggedCharacters.Reset();

	if (bIgnoringMoveInput)
	{
		bIgnoringMoveInput = false;
		if (const APawn* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr)
		{
			if (AController* Controller = Pawn->GetController())
			{
				Controller->SetIgnoreMoveInput(false);
			}
		}
	}

	Conduit.Reset();
	Striker.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
