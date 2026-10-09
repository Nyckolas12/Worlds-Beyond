// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemies/BeyondAffixComponent.h"
#include "AbilitySystem/BeyondAreaStrike.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BeyondGameplayTags.h"
#include "Characters/BeyondCharacterBase.h"
#include "EngineUtils.h"
#include "Enemies/BeyondAffixDefinition.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "Game/BeyondCombatSubsystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

UBeyondAffixComponent::UBeyondAffixComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ABeyondCharacterBase* UBeyondAffixComponent::GetCharacter() const
{
	return Cast<ABeyondCharacterBase>(GetOwner());
}

void UBeyondAffixComponent::ActivateAffixes(const TArray<TObjectPtr<UBeyondAffixDefinition>>& InAffixes)
{
	ABeyondCharacterBase* Character = GetCharacter();
	if (bActivated || !Character)
	{
		return;
	}
	bActivated = true;

	for (UBeyondAffixDefinition* Affix : InAffixes)
	{
		if (Affix)
		{
			ActiveAffixes.Add(Affix);
		}
	}
	if (ActiveAffixes.IsEmpty())
	{
		return;
	}

	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	for (UBeyondAffixDefinition* Affix : ActiveAffixes)
	{
		if (ASC)
		{
			if (Affix->AffixTag.IsValid())
			{
				ASC->AddLooseGameplayTag(Affix->AffixTag);
			}
			if (!Affix->GrantedTags.IsEmpty())
			{
				ASC->AddLooseGameplayTags(Affix->GrantedTags);
			}
			if (Affix->GrantedAbilities && Character->HasAuthority())
			{
				Affix->GrantedAbilities->GiveToAbilitySystem(ASC, Character, &GrantedHandles);
			}
			if (Affix->bWard)
			{
				ASC->AddLooseGameplayTag(BeyondTags::State_Warded);
			}
		}

		Auras.Add(BeyondFX::SpawnAttached(Affix->AuraFX, Character->GetRootComponent()));

		if (Affix->PulseInterval > 0.0f)
		{
			FTimerHandle& Timer = PulseTimers.AddDefaulted_GetRef();
			GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::Pulse, TWeakObjectPtr<UBeyondAffixDefinition>(Affix)),
				Affix->PulseInterval, true, Affix->PulseInterval * FMath::FRandRange(0.6f, 1.0f));
		}
	}

	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		HitLandedHandle = Combat->OnHitLanded.AddUObject(this, &ThisClass::HandleHitLanded);
	}
	Character->OnCharacterKilled.AddUniqueDynamic(this, &ThisClass::HandleOwnerKilled);
}

void UBeyondAffixComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		Combat->OnHitLanded.Remove(HitLandedHandle);
	}
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Timer : PulseTimers)
		{
			World->GetTimerManager().ClearTimer(Timer);
		}
		World->GetTimerManager().ClearTimer(WardTimer);
	}
	for (const TWeakObjectPtr<UFXSystemComponent>& Aura : Auras)
	{
		if (UFXSystemComponent* Component = Aura.Get())
		{
			Component->DestroyComponent();
		}
	}
	Super::EndPlay(EndPlayReason);
}

bool UBeyondAffixComponent::HasAffix(FGameplayTag AffixTag) const
{
	return ActiveAffixes.ContainsByPredicate([&AffixTag](const UBeyondAffixDefinition* Affix)
	{
		return Affix && Affix->AffixTag.MatchesTagExact(AffixTag);
	});
}

bool UBeyondAffixComponent::IsWardUp() const
{
	const bool bHasWard = ActiveAffixes.ContainsByPredicate([](const UBeyondAffixDefinition* Affix) { return Affix && Affix->bWard; });
	return bHasWard && GetWorld() && GetWorld()->GetTimeSeconds() >= WardBrokenUntil;
}

bool UBeyondAffixComponent::IsFromOwner(const AActor* DamageInstigator) const
{
	const AActor* Owner = GetOwner();
	// Projectiles and strikes report themselves; walk up to whoever made them
	for (int32 Depth = 0; DamageInstigator && Depth < 4; ++Depth)
	{
		if (DamageInstigator == Owner)
		{
			return true;
		}
		DamageInstigator = DamageInstigator->GetInstigator() && DamageInstigator->GetInstigator() != DamageInstigator
			? DamageInstigator->GetInstigator() : DamageInstigator->GetOwner();
	}
	return false;
}

float UBeyondAffixComponent::ModifyDamageTaken(float Damage, AActor* DamageInstigator, const FGameplayTagContainer& DamageTags) const
{
	const UWorld* World = GetWorld();
	if (ActiveAffixes.IsEmpty() || !World)
	{
		return Damage;
	}

	const float Now = World->GetTimeSeconds();
	for (const UBeyondAffixDefinition* Affix : ActiveAffixes)
	{
		if (!Affix)
		{
			continue;
		}

		if (Affix->bWard)
		{
			// A melee hit cracks the ward open; spells hurt properly until it closes again
			if (Affix->WardBreakDamageType.IsValid() && DamageTags.HasTag(Affix->WardBreakDamageType))
			{
				const bool bWasUp = Now >= WardBrokenUntil;
				WardBrokenUntil = Now + Affix->WardBreakDuration;
				if (const ABeyondCharacterBase* Character = GetCharacter())
				{
					if (bWasUp)
					{
						BeyondFX::SpawnAttached(Affix->WardBreakFX, Character->GetRootComponent());
					}
					if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
					{
						ASC->SetLooseGameplayTagCount(BeyondTags::State_Warded, 0);
					}
				}
				TWeakObjectPtr<const UBeyondAffixComponent> WeakThis(this);
				GetWorld()->GetTimerManager().SetTimer(WardTimer, FTimerDelegate::CreateLambda([WeakThis]()
				{
					if (const UBeyondAffixComponent* Self = WeakThis.Get())
					{
						Self->RestoreWard();
					}
				}), Affix->WardBreakDuration, false);
				continue;
			}
			if (Now < WardBrokenUntil)
			{
				continue;
			}
		}

		for (const TPair<FGameplayTag, float>& Multiplier : Affix->DamageTakenMultipliers)
		{
			if (Multiplier.Key.IsValid() && DamageTags.HasTag(Multiplier.Key))
			{
				Damage *= FMath::Max(Multiplier.Value, 0.0f);
			}
		}
	}
	return Damage;
}

void UBeyondAffixComponent::RestoreWard() const
{
	const ABeyondCharacterBase* Character = GetCharacter();
	if (Character && !UBeyondCombatLibrary::IsActorDead(Character) && IsWardUp())
	{
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		{
			ASC->SetLooseGameplayTagCount(BeyondTags::State_Warded, 1);
		}
	}
}

void UBeyondAffixComponent::HandleHitLanded(AActor* DamageInstigator, AActor* Target, float Damage, const FGameplayTagContainer& DamageTags)
{
	ABeyondCharacterBase* Character = GetCharacter();
	// Damage over time ticks never set off more on-hit effects
	if (!Character || Damage <= 0.0f || !Target || Target == Character || DamageTags.HasTag(BeyondTags::DamageType_Proc)
		|| !IsFromOwner(DamageInstigator) || !UBeyondCombatLibrary::AreHostile(Character, Target))
	{
		return;
	}

	float Lifesteal = 0.0f;
	for (const UBeyondAffixDefinition* Affix : ActiveAffixes)
	{
		if (!Affix)
		{
			continue;
		}
		if (Affix->OnHitDamagePerSecond > 0.0f && Affix->OnHitDuration > 0.0f)
		{
			UBeyondCombatLibrary::ApplyDamageOverTime(Character, Target, Affix->OnHitDamagePerSecond, Affix->OnHitDuration,
				Affix->OnHitDamageType.IsValid() ? Affix->OnHitDamageType : BeyondTags::DamageType_Proc_Burn, Affix->OnHitTargetFX);
		}
		Lifesteal += Affix->Lifesteal;
	}

	if (Lifesteal > 0.0f && !UBeyondCombatLibrary::IsActorDead(Character))
	{
		UBeyondCombatLibrary::ApplyHeal(Character, Character, Damage * Lifesteal);
	}
}

void UBeyondAffixComponent::Pulse(TWeakObjectPtr<UBeyondAffixDefinition> Affix)
{
	const ABeyondCharacterBase* Character = GetCharacter();
	const UBeyondAffixDefinition* Definition = Affix.Get();
	if (!Character || !Definition || UBeyondCombatLibrary::IsActorDead(Character))
	{
		return;
	}
	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (ASC && (ASC->HasMatchingGameplayTag(BeyondTags::State_Resetting) || ASC->HasMatchingGameplayTag(BeyondTags::State_Stunned)))
	{
		return;
	}

	// Only when a demigod is close enough to care
	bool bHostileNear = false;
	for (TActorIterator<ABeyondCharacterBase> It(GetWorld()); It && !bHostileNear; ++It)
	{
		bHostileNear = UBeyondCombatLibrary::AreHostile(Character, *It) && !UBeyondCombatLibrary::IsActorDead(*It)
			&& FVector::Dist(It->GetActorLocation(), Character->GetActorLocation()) <= Definition->PulseRadius * 2.5f;
	}
	if (bHostileNear)
	{
		FirePulse(Definition);
	}
}

void UBeyondAffixComponent::PulseNow()
{
	for (const UBeyondAffixDefinition* Affix : ActiveAffixes)
	{
		if (Affix && Affix->PulseInterval > 0.0f)
		{
			FirePulse(Affix);
		}
	}
}

void UBeyondAffixComponent::FirePulse(const UBeyondAffixDefinition* Affix)
{
	ABeyondCharacterBase* Character = GetCharacter();
	if (!Character)
	{
		return;
	}

	FBeyondStrikeSettings Strike;
	Strike.Shape = EBeyondStrikeShape::Circle;
	Strike.Radius = Affix->PulseRadius;
	Strike.WindUp = Affix->PulseWindUp;
	Strike.Damage = Affix->PulseDamage;
	Strike.DamageType = Affix->PulseDamageType.IsValid() ? Affix->PulseDamageType : BeyondTags::DamageType_Explosion;
	Strike.HitResponse = BeyondTags::Event_Hit_Light;
	Strike.ImpactFX = Affix->PulseFX;
	Strike.Color = FLinearColor(0.35f, 0.55f, 1.0f, 1.0f);
	if (ABeyondAreaStrike::SpawnStrike(Character, Character, Strike, Character->GetActorLocation() - FVector(0.0f, 0.0f, 80.0f), Character->GetActorRotation().Yaw))
	{
		++PulseCount;
	}
}

void UBeyondAffixComponent::HandleOwnerKilled(ABeyondCharacterBase* Character, AActor* Killer)
{
	if (!Character)
	{
		return;
	}

	for (const TWeakObjectPtr<UFXSystemComponent>& Aura : Auras)
	{
		if (UFXSystemComponent* Component = Aura.Get())
		{
			Component->DestroyComponent();
		}
	}
	Auras.Reset();
	for (FTimerHandle& Timer : PulseTimers)
	{
		GetWorld()->GetTimerManager().ClearTimer(Timer);
	}

	const FVector Location = Character->GetActorLocation();
	const float Yaw = Character->GetActorRotation().Yaw;
	for (UBeyondAffixDefinition* Affix : ActiveAffixes)
	{
		if (!Affix)
		{
			continue;
		}

		if (Affix->DeathAction == EBeyondAffixDeathAction::LingeringPool)
		{
			FBeyondStrikeSettings Pool;
			Pool.Shape = EBeyondStrikeShape::Circle;
			Pool.Radius = Affix->PoolRadius;
			Pool.WindUp = 0.5f;
			Pool.Damage = 0.0f;
			Pool.LingerDuration = Affix->PoolDuration;
			Pool.LingerDamagePerSecond = Affix->PoolDamagePerSecond;
			Pool.LingerDamageType = Affix->OnHitDamageType.IsValid() ? Affix->OnHitDamageType : BeyondTags::DamageType_Proc_Burn;
			Pool.LingerFX = Affix->PoolFX;
			if (ABeyondAreaStrike* Strike = ABeyondAreaStrike::SpawnStrike(Character, Character, Pool, Location - FVector(0.0f, 0.0f, 80.0f), Yaw))
			{
				// Its maker is dead: that's the point
				Strike->bCancelWithInstigator = false;
			}
		}
		else if (Affix->DeathAction == EBeyondAffixDeathAction::Split && Affix->SplitCount > 0)
		{
			const ABeyondEnemyCharacter* Enemy = Cast<ABeyondEnemyCharacter>(Character);
			UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(this);
			if (!Enemy || !Enemy->GetDefinition() || !Enemies)
			{
				continue;
			}

			// Next tick: not from inside the death callback
			TWeakObjectPtr<UBeyondEnemySubsystem> WeakEnemies(Enemies);
			TWeakObjectPtr<UBeyondEnemyDefinition> WeakDefinition(Enemy->GetDefinition());
			TWeakObjectPtr<AActor> WeakKiller(Killer);
			const int32 Level = Enemy->GetCharacterLevel();
			const int32 Count = Affix->SplitCount;
			const float HealthFraction = Affix->SplitHealthFraction;
			const float SplitScale = Affix->SplitScale;
			const float Size = Enemy->SizeScale;
			GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Enemies, [WeakEnemies, WeakDefinition, WeakKiller, Location, Yaw, Level, Count, HealthFraction, SplitScale, Size]()
			{
				UBeyondEnemySubsystem* Subsystem = WeakEnemies.Get();
				UBeyondEnemyDefinition* Definition = WeakDefinition.Get();
				if (!Subsystem || !Definition)
				{
					return;
				}
				for (int32 Index = 0; Index < Count; ++Index)
				{
					const float Angle = 2.0f * PI * Index / Count;
					const FVector Offset(FMath::Cos(Angle) * 120.0f, FMath::Sin(Angle) * 120.0f, 0.0f);
					FBeyondEnemySpawnParams Params;
					Params.Level = Level;
					Params.bSummoned = true;
					Params.HealthScale = HealthFraction;
					Params.SizeScale = Size * SplitScale;
					if (ABeyondEnemyCharacter* Copy = Subsystem->SpawnEnemy(Definition, FTransform(FRotator(0.0f, Yaw, 0.0f), Location + Offset), Params))
					{
						Subsystem->AlertEnemy(Copy, WeakKiller.Get());
					}
				}
			}));
		}
	}
}
