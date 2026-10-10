// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_DuoStrike.h"
#include "WorldBeyond.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AI/BeyondCompanionController.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "BeyondGameplayTags.h"
#include "Blueprint/UserWidget.h"
#include "Characters/BeyondAimComponent.h"
#include "Characters/BeyondCharacterBase.h"
#include "Characters/BeyondRushComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
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
	// Chosen before anyone moves: the enemy the lightning and the final blow are for
	LockedTarget = ChooseTarget(FoundStriker);
	UE_LOG(LogBeyond, Log, TEXT("Duo %s: aimed at %s"), *GetClass()->GetName(), *GetNameSafe(LockedTarget.Get()));

	// Both demigods commit to the move: stop what they were doing, can't be hurt or knocked out of it
	SetDuoState(FoundConduit, true);
	SetDuoState(FoundStriker, true);

	// The player stays put while the move plays out
	const APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
	if (AController* Controller = AvatarPawn ? AvatarPawn->GetController() : nullptr)
	{
		Controller->SetIgnoreMoveInput(true);
		LockedController = Controller;
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

AActor* UBeyondGA_DuoStrike::ChooseTarget(const ABeyondCharacterBase* StrikerCharacter) const
{
	const UBeyondPartyComponent* Party = GetParty();
	const ABeyondCharacterBase* Leader = Party ? Party->GetLeader() : nullptr;
	if (!Leader || !StrikerCharacter)
	{
		return nullptr;
	}
	const FVector LeaderLocation = Leader->GetActorLocation();
	auto IsCandidate = [this, StrikerCharacter, &LeaderLocation](const AActor* Actor)
	{
		return Actor && !UBeyondCombatLibrary::IsActorDead(Actor) && UBeyondCombatLibrary::AreHostile(StrikerCharacter, Actor)
			&& FVector::Dist(Actor->GetActorLocation(), LeaderLocation) <= TargetSearchRadius;
	};

	// 1. What the player is aiming at (Angel's crosshair)
	if (const UBeyondAimComponent* Aim = Leader->GetAimComponent(); Aim && IsCandidate(Aim->GetAimTarget()))
	{
		return Aim->GetAimTarget();
	}

	// 2. The nearest boss or mini-boss, 3. the buddy's target, 4. the nearest enemy
	const TArray<AActor*> Hostiles = FindHostilesInRadius(LeaderLocation, TargetSearchRadius);
	AActor* NearestBoss = nullptr;
	AActor* Nearest = nullptr;
	float NearestBossDistance = TNumericLimits<float>::Max();
	float NearestDistance = TNumericLimits<float>::Max();
	for (AActor* Hostile : Hostiles)
	{
		if (!IsCandidate(Hostile))
		{
			continue;
		}
		const float Distance = FVector::Dist(Hostile->GetActorLocation(), LeaderLocation);
		if (IsBoss(Hostile) && Distance < NearestBossDistance)
		{
			NearestBoss = Hostile;
			NearestBossDistance = Distance;
		}
		if (Distance < NearestDistance)
		{
			Nearest = Hostile;
			NearestDistance = Distance;
		}
	}
	if (NearestBoss)
	{
		return NearestBoss;
	}
	for (const ABeyondCharacterBase* Member : Party->GetMembers())
	{
		const ABeyondCompanionController* Companion = Member ? Cast<ABeyondCompanionController>(Member->GetController()) : nullptr;
		if (Companion && IsCandidate(Companion->GetCombatTarget()))
		{
			return Companion->GetCombatTarget();
		}
	}
	return Nearest;
}

void UBeyondGA_DuoStrike::Flash()
{
	ABeyondCharacterBase* StrikerCharacter = Striker.Get();
	ABeyondCharacterBase* ConduitCharacter = Conduit.Get();
	if (!IsActive() || !StrikerCharacter || !ConduitCharacter)
	{
		return;
	}

	// Strike a random enemy around the locked one (or the Striker); with no enemies around, the sky still cracks for show
	const AActor* Locked = LockedTarget.Get();
	const FVector Center = Locked && !UBeyondCombatLibrary::IsActorDead(Locked) ? Locked->GetActorLocation() : StrikerCharacter->GetActorLocation();
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
		UBeyondCombatLibrary::ApplyDamage(ConduitCharacter, Target, FlashDamage * GetLevelDamageScale(), BeyondTags::DamageType_Projectile, BeyondTags::Event_Hit_Stun, true);

		// Eclipse Brand: the struck enemy is marked for the Striker's blade and the shockwave
		if (bBrandStruckEnemies && !UBeyondCombatLibrary::IsActorDead(Target))
		{
			if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(Target))
			{
				FBeyondBrandSettings Ranked = StruckBrand;
				Ranked.DetonateDamage *= GetLevelDamageScale();
				Combat->ApplyBrand(StrikerCharacter, Target, Ranked);
			}
		}
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

	// The storm leaves Angel's hand (skipped if the Conduit went down mid-move)
	if (!UBeyondCombatLibrary::IsActorDead(ConduitCharacter))
	{
		USkeletalMeshComponent* ConduitMesh = ConduitCharacter->GetCombatMesh();
		if (UFXSystemComponent* Spawned = BeyondFX::SpawnAttached(ConduitReleaseFX, ConduitMesh,
			ConduitMesh && ConduitMesh->DoesSocketExist(TEXT("hand_r")) ? FName(TEXT("hand_r")) : NAME_None))
		{
			ConduitComponents.Add(Spawned);
		}
	}

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
	GetWorld()->GetTimerManager().SetTimer(PhaseTimer, this, &ThisClass::StartApproach, FMath::Max(AbsorbDuration, 0.01f), false);
}

void UBeyondGA_DuoStrike::StartApproach()
{
	ABeyondCharacterBase* StrikerCharacter = Striker.Get();
	if (!IsActive() || !StrikerCharacter)
	{
		Finish();
		return;
	}

	// The locked enemy fell during the charge: go for whoever stood nearest to it
	AActor* Target = LockedTarget.Get();
	if (Target && UBeyondCombatLibrary::IsActorDead(Target))
	{
		const FVector Where = Target->GetActorLocation();
		Target = nullptr;
		float Best = TNumericLimits<float>::Max();
		for (AActor* Hostile : FindHostilesInRadius(Where, 1000.0f))
		{
			const float Distance = FVector::Dist(Hostile->GetActorLocation(), Where);
			if (Distance < Best)
			{
				Target = Hostile;
				Best = Distance;
			}
		}
		LockedTarget = Target;
	}
	if (!Target)
	{
		StartJudgment();
		return;
	}

	const FVector ToTarget = (Target->GetActorLocation() - StrikerCharacter->GetActorLocation()).GetSafeNormal2D();
	if (FVector::Dist2D(StrikerCharacter->GetActorLocation(), Target->GetActorLocation()) <= DashAnimationDistance)
	{
		if (!ToTarget.IsNearlyZero())
		{
			StrikerCharacter->SetActorRotation(ToTarget.Rotation());
		}
		StartJudgment();
		return;
	}

	// Charged up beside the Conduit, the Striker rushes in (the aura goes with him); he slams once he is there
	StopMontageOn(StrikerCharacter, StrikerChargeMontage);
	TWeakObjectPtr<UBeyondGA_DuoStrike> WeakThis(this);
	UBeyondRushComponent::Rush(StrikerCharacter, Target, FVector::ZeroVector, Approach, [WeakThis](bool bArrived)
	{
		UBeyondGA_DuoStrike* Self = WeakThis.Get();
		if (!Self || !Self->IsActive())
		{
			return;
		}
		ABeyondCharacterBase* Arrived = Self->Striker.Get();
		const AActor* Aimed = Self->LockedTarget.Get();
		if (Arrived && Aimed)
		{
			const FVector Facing = (Aimed->GetActorLocation() - Arrived->GetActorLocation()).GetSafeNormal2D();
			if (!Facing.IsNearlyZero())
			{
				Arrived->SetActorRotation(Facing.Rotation());
			}
		}
		Self->StartJudgment();
	});
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

	// The power has passed to the Striker: let the lightning in the Conduit's hand fade out
	RemoveEffects(ConduitComponents, true);

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
	RemoveEffects(AuraComponents, false);

	const FVector Center = StrikerCharacter->GetActorLocation();
	const FVector Ground = Center - FVector(0.0f, 0.0f, StrikerCharacter->GetDefaultHalfHeight());
	for (const FBeyondFX& Wave : ShockwaveFX)
	{
		BeyondFX::SpawnAtLocation(this, Wave, Ground);
	}

	// The locked enemy always takes the blow at full force, wherever the slam landed
	AActor* Locked = LockedTarget.Get();
	TArray<AActor*> Targets = FindHostilesInRadius(Center, ShockwaveRadius);
	if (Locked && !UBeyondCombatLibrary::IsActorDead(Locked))
	{
		Targets.AddUnique(Locked);
	}
	for (AActor* Target : Targets)
	{
		const float Distance = FVector::Dist2D(Center, Target->GetActorLocation());
		const float Alpha = Target == Locked ? 0.0f : FMath::Clamp(Distance / ShockwaveRadius, 0.0f, 1.0f);
		const float Damage = FMath::Lerp(ShockwaveDamageCenter, ShockwaveDamageEdge, Alpha) * GetLevelDamageScale();
		UBeyondCombatLibrary::ApplyDamage(StrikerCharacter, Target, Damage, BeyondTags::DamageType_Explosion, ShockwaveHitResponse, true, StrikerCharacter, true);

		if (ACharacter* TargetCharacter = Cast<ACharacter>(Target); TargetCharacter && KnockbackSpeed > 0.0f && !IsBoss(Target))
		{
			const FVector Away = (Target->GetActorLocation() - Center).GetSafeNormal2D();
			TargetCharacter->LaunchCharacter(Away * KnockbackSpeed * (1.0f - 0.5f * Alpha) + FVector(0.0f, 0.0f, 350.0f), true, true);
		}
	}

	if (bShockwaveDetonatesBrands)
	{
		DetonateBrandsAround(Center);
	}

	// Tempest Aegis: the storm settles on both of them as a shield
	if (Aegis.Duration > 0.0f)
	{
		if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(StrikerCharacter))
		{
			for (ABeyondCharacterBase* Member : { StrikerCharacter, Conduit.Get() })
			{
				if (Member && !UBeyondCombatLibrary::IsActorDead(Member))
				{
					Combat->ApplyAegis(Member, Aegis);
				}
			}
		}
	}

	GetWorld()->GetTimerManager().SetTimer(PhaseTimer, this, &ThisClass::Finish, FMath::Max(RecoveryTime, 0.01f), false);
}

void UBeyondGA_DuoStrike::DetonateBrandsAround(const FVector& Center)
{
	ABeyondCharacterBase* StrikerCharacter = Striker.Get();
	UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(StrikerCharacter);
	if (!StrikerCharacter || !Combat)
	{
		return;
	}

	// Branded enemies in range, dead ones too (the shockwave may just have killed them; their brand still goes off)
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondDuoDetonate), false, StrikerCharacter);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(ShockwaveRadius * 1.5f), Params);

	TArray<AActor*> Branded;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor && !Branded.Contains(Actor) && Combat->IsBranded(Actor) && UBeyondCombatLibrary::AreHostile(StrikerCharacter, Actor))
		{
			Branded.Add(Actor);
		}
	}
	// Nearest first, so the chain runs outward from the Striker
	Branded.Sort([&Center](const AActor& A, const AActor& B)
	{
		return FVector::DistSquared(Center, A.GetActorLocation()) < FVector::DistSquared(Center, B.GetActorLocation());
	});

	ABeyondCharacterBase* Caster = Conduit.IsValid() ? Conduit.Get() : StrikerCharacter;
	for (AActor* Target : Branded)
	{
		BeyondFX::SpawnAtLocation(this, ChainFX, Target->GetActorLocation());
		Combat->DetonateBrand(Target);
		if (ChainDamage > 0.0f && !UBeyondCombatLibrary::IsActorDead(Target))
		{
			UBeyondCombatLibrary::ApplyDamage(Caster, Target, ChainDamage * GetLevelDamageScale(), BeyondTags::DamageType_Projectile,
				BeyondTags::Event_Hit_Stun, true);
		}
	}
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

void UBeyondGA_DuoStrike::StopMontageOn(ABeyondCharacterBase* Character, UAnimMontage* Montage)
{
	const USkeletalMeshComponent* Mesh = Character ? Character->GetCombatMesh() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (Montage && AnimInstance && AnimInstance->Montage_IsPlaying(Montage))
	{
		AnimInstance->Montage_Stop(0.2f, Montage);
	}
}

void UBeyondGA_DuoStrike::RemoveEffects(TArray<TWeakObjectPtr<UFXSystemComponent>>& Components, bool bLetFade)
{
	for (const TWeakObjectPtr<UFXSystemComponent>& Effect : Components)
	{
		if (UFXSystemComponent* Component = Effect.Get())
		{
			// Deactivate lets live particles finish; auto-destroy systems then remove themselves
			if (bLetFade)
			{
				Component->Deactivate();
			}
			else
			{
				Component->DestroyComponent();
			}
		}
	}
	if (!bLetFade)
	{
		Components.Reset();
	}
}

bool UBeyondGA_DuoStrike::IsBoss(const AActor* Actor)
{
	// Mini-bosses and bosses by rank too (Plan 3), not only Blueprint enemies with a boss bar
	return UBeyondCombatLibrary::IsBoss(Actor);
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

	// Cut short mid-rush: the Striker stops where he is and gets his collision back
	if (UBeyondRushComponent* Rush = UBeyondRushComponent::FindRush(Striker.Get()))
	{
		Rush->Cancel();
	}

	RemoveEffects(AuraComponents, false);
	RemoveEffects(ConduitComponents, false);

	// Cancelled part-way: nobody keeps posing
	if (bWasCancelled)
	{
		StopMontageOn(Conduit.Get(), ConduitMontage);
		StopMontageOn(Striker.Get(), StrikerChargeMontage);
		StopMontageOn(Striker.Get(), StrikerSlamMontage);
	}

	for (const TWeakObjectPtr<ABeyondCharacterBase>& Character : TaggedCharacters)
	{
		SetDuoState(Character.Get(), false);
	}
	TaggedCharacters.Reset();

	if (AController* Controller = LockedController.Get())
	{
		Controller->SetIgnoreMoveInput(false);
	}
	LockedController.Reset();

	Conduit.Reset();
	Striker.Reset();
	LockedTarget.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
