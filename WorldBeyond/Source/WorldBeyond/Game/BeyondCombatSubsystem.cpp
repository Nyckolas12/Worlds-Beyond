// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/BeyondCombatSubsystem.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayEffects.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "BeyondGameplayTags.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

namespace
{
	UAbilitySystemComponent* GetASC(const AActor* Actor)
	{
		return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Actor));
	}

	// The damage came from Source itself, or from a weapon / projectile it owns
	bool IsFrom(const AActor* DamageInstigator, const AActor* Source)
	{
		return DamageInstigator && Source
			&& (DamageInstigator == Source || DamageInstigator->GetOwner() == Source || DamageInstigator->GetInstigator() == Source);
	}
}

UBeyondCombatSubsystem* UBeyondCombatSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBeyondCombatSubsystem>() : nullptr;
}

bool UBeyondCombatSubsystem::ApplyBrand(AActor* Source, AActor* Target, const FBeyondBrandSettings& Settings)
{
	UAbilitySystemComponent* TargetASC = GetASC(Target);
	if (!TargetASC || UBeyondCombatLibrary::IsActorDead(Target))
	{
		return false;
	}

	ClearBrand(TargetASC);

	UAbilitySystemComponent* SourceASC = GetASC(Source);
	UAbilitySystemComponent* SpecOwner = SourceASC ? SourceASC : TargetASC;
	FGameplayEffectContextHandle Context = SpecOwner->MakeEffectContext();
	Context.AddInstigator(Source, Source);

	const FGameplayEffectSpecHandle Spec = SpecOwner->MakeOutgoingSpec(UBeyondGE_Brand::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Duration, Settings.Duration);

	const FActiveGameplayEffectHandle Effect = SourceASC
		? SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC)
		: TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	if (!Effect.IsValid())
	{
		return false;
	}

	FBrandState& State = Brands.Add(TargetASC);
	State.Source = Source;
	State.Target = Target;
	State.Effect = Effect;
	State.Settings = Settings;
	State.Mark = BeyondFX::SpawnAttached(Settings.MarkFX, Target->GetRootComponent());

	TargetASC->AddLooseGameplayTag(BeyondTags::State_Branded);

	// Expired or removed: drop the mark (the handle check keeps a newer brand alive)
	if (FOnActiveGameplayEffectRemoved_Info* Removed = TargetASC->OnGameplayEffectRemoved_InfoDelegate(Effect))
	{
		TWeakObjectPtr<UAbilitySystemComponent> WeakASC(TargetASC);
		Removed->AddWeakLambda(this, [this, WeakASC, Effect](const FGameplayEffectRemovalInfo&)
		{
			ClearBrand(WeakASC.Get(), Effect, false);
		});
	}
	return true;
}

bool UBeyondCombatSubsystem::IsBranded(const AActor* Target) const
{
	UAbilitySystemComponent* TargetASC = GetASC(Target);
	return TargetASC && Brands.Contains(TargetASC);
}

float UBeyondCombatSubsystem::ModifyIncomingDamage(UAbilitySystemComponent& TargetASC, AActor* DamageInstigator, const FGameplayTagContainer& DamageTags, float Damage)
{
	FBrandState* State = Brands.Find(&TargetASC);
	if (!State || State->bDetonating)
	{
		return Damage;
	}

	Damage *= State->Settings.DamageTakenMultiplier;

	// The brander's own blade sets it off; detonate next tick, outside this attribute callback
	if (DamageTags.HasTagExact(BeyondTags::DamageType_Melee) && IsFrom(DamageInstigator, State->Source.Get()))
	{
		State->bDetonating = true;
		TWeakObjectPtr<UAbilitySystemComponent> WeakASC(&TargetASC);
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, WeakASC]()
		{
			Detonate(WeakASC);
		}));
	}
	return Damage;
}

void UBeyondCombatSubsystem::Detonate(TWeakObjectPtr<UAbilitySystemComponent> TargetASC)
{
	FBrandState State;
	if (!Brands.RemoveAndCopyValue(TargetASC, State))
	{
		return;
	}
	CleanUp(TargetASC.Get(), State, true);

	AActor* Source = State.Source.Get();
	const AActor* Target = State.Target.Get();
	if (!Target)
	{
		return;
	}

	const FVector Location = Target->GetActorLocation();
	BeyondFX::SpawnAtLocation(this, State.Settings.DetonateFX, Location);

	if (!Source || UBeyondCombatLibrary::IsActorDead(Source) || State.Settings.DetonateDamage <= 0.0f)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondBrandDetonate), false, Source);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Location, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(State.Settings.DetonateRadius), Params);

	TSet<AActor*> Hit;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor && !Hit.Contains(Actor) && UBeyondCombatLibrary::AreHostile(Source, Actor) && !UBeyondCombatLibrary::IsActorDead(Actor))
		{
			Hit.Add(Actor);
			// Explosion damage, so the blast itself can't set off another brand
			UBeyondCombatLibrary::ApplyDamage(Source, Actor, State.Settings.DetonateDamage, BeyondTags::DamageType_Explosion,
				State.Settings.DetonateHitResponse.IsValid() ? State.Settings.DetonateHitResponse : BeyondTags::Event_Hit_Stagger);
		}
	}

	const float Drain = State.Settings.DetonateDamage * Hit.Num() * State.Settings.DrainFraction;
	if (Drain > 0.0f)
	{
		UBeyondCombatLibrary::ApplyHeal(Source, Source, Drain);
	}
}

void UBeyondCombatSubsystem::ClearBrand(UAbilitySystemComponent* TargetASC, FActiveGameplayEffectHandle OnlyIfEffect, bool bRemoveEffect)
{
	FBrandState* State = TargetASC ? Brands.Find(TargetASC) : nullptr;
	if (!State || (OnlyIfEffect.IsValid() && State->Effect != OnlyIfEffect))
	{
		return;
	}

	FBrandState Removed = MoveTemp(*State);
	Brands.Remove(TargetASC);
	CleanUp(TargetASC, Removed, bRemoveEffect);
}

void UBeyondCombatSubsystem::CleanUp(UAbilitySystemComponent* TargetASC, FBrandState& State, bool bRemoveEffect)
{
	if (UFXSystemComponent* Mark = State.Mark.Get())
	{
		Mark->DestroyComponent();
	}
	State.Mark.Reset();

	if (TargetASC)
	{
		TargetASC->SetLooseGameplayTagCount(BeyondTags::State_Branded, 0);
		if (bRemoveEffect && State.Effect.IsValid() && TargetASC->GetActiveGameplayEffect(State.Effect))
		{
			// Removing fires the removal delegate, which finds no brand left and does nothing
			TargetASC->RemoveActiveGameplayEffect(State.Effect);
		}
	}
}

UAnimMontage* UBeyondCombatSubsystem::GetMontageVariant(UAnimMontage* Source, FName SlotName, bool bMuteNotifies)
{
	if (!Source || SlotName.IsNone())
	{
		return Source;
	}

	const FName Key(*FString::Printf(TEXT("%s|%s|%d"), *Source->GetPathName(), *SlotName.ToString(), bMuteNotifies ? 1 : 0));
	if (const TObjectPtr<UAnimMontage>* Existing = MontageVariants.Find(Key))
	{
		return *Existing;
	}

	const FName VariantName = MakeUniqueObjectName(this, UAnimMontage::StaticClass(), FName(*FString::Printf(TEXT("%s_%s"), *Source->GetName(), *SlotName.ToString())));
	UAnimMontage* Variant = DuplicateObject<UAnimMontage>(Source, this, VariantName);
	if (!Variant)
	{
		return Source;
	}
	// A runtime object, not an asset: never saved, collected with the world
	Variant->ClearFlags(RF_Public | RF_Standalone);
	Variant->SetFlags(RF_Transient);

	for (FSlotAnimationTrack& Track : Variant->SlotAnimTracks)
	{
		Track.SlotName = SlotName;
	}

	if (bMuteNotifies)
	{
		// Left in place (montage branching points index into this array), they just never pass the trigger checks
		for (FAnimNotifyEvent& Notify : Variant->Notifies)
		{
			Notify.NotifyTriggerChance = 0.0f;
			Notify.TriggerWeightThreshold = 2.0f;
		}
	}

	MontageVariants.Add(Key, Variant);
	VariantSources.Add(Variant, Source);
	return Variant;
}

UAnimMontage* UBeyondCombatSubsystem::GetMontageSource(UAnimMontage* Montage)
{
	if (const UBeyondCombatSubsystem* Owner = Montage ? Cast<UBeyondCombatSubsystem>(Montage->GetOuter()) : nullptr)
	{
		if (const TObjectPtr<UAnimMontage>* Source = Owner->VariantSources.Find(Montage))
		{
			return *Source;
		}
	}
	return Montage;
}
