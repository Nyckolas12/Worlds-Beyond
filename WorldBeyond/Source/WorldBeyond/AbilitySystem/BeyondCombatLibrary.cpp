// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "AbilitySystem/BeyondGameplayEffects.h"
#include "AI/BeyondAIAbilityUtils.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Characters/BeyondCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "BeyondGameplayTags.h"
#include "CharacterAttributeSet.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Player/BeyondPartyComponent.h"

UAbilitySystemComponent* UBeyondCombatLibrary::GetASC(const AActor* Actor)
{
	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Actor));
}

bool UBeyondCombatLibrary::ApplyDamage(AActor* Source, AActor* Target, float Amount, FGameplayTag DamageType, FGameplayTag HitResponse,
	bool bUnblockable, AActor* Causer, bool bUnparryable, bool bIgnoreInvincible, bool bForceInterrupt)
{
	UAbilitySystemComponent* TargetASC = GetASC(Target);
	if (!TargetASC || Amount <= 0.0f)
	{
		return false;
	}

	// Blueprint logic may have toggled blocking / invincibility on the old damage component
	if (ABeyondCharacterBase* TargetCharacter = Cast<ABeyondCharacterBase>(Target))
	{
		TargetCharacter->SyncLegacyDamageState();
	}

	UAbilitySystemComponent* SourceASC = GetASC(Source);
	UAbilitySystemComponent* SpecOwner = SourceASC ? SourceASC : TargetASC;

	// The attacker's stats: Strength for melee, Arcana for projectiles / explosions (spells, abilities)
	Amount *= GetDamageScale(SourceASC, DamageType);

	FGameplayEffectContextHandle Context = SpecOwner->MakeEffectContext();
	Context.AddInstigator(Source, Causer ? Causer : Source);

	const FGameplayEffectSpecHandle SpecHandle = SpecOwner->MakeOutgoingSpec(UBeyondGE_Damage::StaticClass(), 1.0f, Context);
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		return false;
	}

	Spec->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Damage, Amount);
	if (DamageType.IsValid()) { Spec->AddDynamicAssetTag(DamageType); }
	if (HitResponse.IsValid()) { Spec->AddDynamicAssetTag(HitResponse); }
	if (bUnblockable) { Spec->AddDynamicAssetTag(BeyondTags::Damage_Unblockable); }
	if (bUnparryable) { Spec->AddDynamicAssetTag(BeyondTags::Damage_Unparryable); }
	if (bIgnoreInvincible) { Spec->AddDynamicAssetTag(BeyondTags::Damage_IgnoreInvincible); }
	if (bForceInterrupt) { Spec->AddDynamicAssetTag(BeyondTags::Damage_ForceInterrupt); }

	if (SourceASC)
	{
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec, TargetASC);
	}
	else
	{
		TargetASC->ApplyGameplayEffectSpecToSelf(*Spec);
	}
	return true;
}

bool UBeyondCombatLibrary::ApplyDamageOverTime(AActor* Source, AActor* Target, float DamagePerSecond, float Duration, FGameplayTag DamageTag, const FBeyondFX& TargetFX)
{
	UAbilitySystemComponent* TargetASC = GetASC(Target);
	if (!TargetASC || DamagePerSecond <= 0.0f || Duration <= 0.0f || IsActorDead(Target))
	{
		return false;
	}

	// Refresh: drop the DoT this source already has running with this tag
	bool bWasActive = false;
	FGameplayEffectQuery Query;
	Query.EffectDefinition = UBeyondGE_DamageOverTime::StaticClass();
	for (const FActiveGameplayEffectHandle& Handle : TargetASC->GetActiveEffects(Query))
	{
		const FActiveGameplayEffect* Active = TargetASC->GetActiveGameplayEffect(Handle);
		if (!Active)
		{
			continue;
		}
		FGameplayTagContainer Tags;
		Active->Spec.GetAllAssetTags(Tags);
		if ((!DamageTag.IsValid() || Tags.HasTagExact(DamageTag)) && Active->Spec.GetEffectContext().GetOriginalInstigator() == Source)
		{
			TargetASC->RemoveActiveGameplayEffect(Handle);
			bWasActive = true;
		}
	}

	UAbilitySystemComponent* SourceASC = GetASC(Source);
	UAbilitySystemComponent* SpecOwner = SourceASC ? SourceASC : TargetASC;
	FGameplayEffectContextHandle Context = SpecOwner->MakeEffectContext();
	Context.AddInstigator(Source, Source);
	const FGameplayEffectSpecHandle Spec = SpecOwner->MakeOutgoingSpec(UBeyondGE_DamageOverTime::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Damage, DamagePerSecond);
	Spec.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Duration, Duration);
	if (DamageTag.IsValid())
	{
		Spec.Data->AddDynamicAssetTag(DamageTag);
	}
	const FActiveGameplayEffectHandle Applied = SourceASC
		? SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC)
		: TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);

	if (Applied.IsValid() && !bWasActive && Target->GetRootComponent())
	{
		BeyondFX::SpawnAttached(TargetFX, Target->GetRootComponent());
	}
	return Applied.IsValid();
}

bool UBeyondCombatLibrary::IsBoss(const AActor* Actor)
{
	const ABeyondCharacterBase* Character = Cast<ABeyondCharacterBase>(Actor);
	return Character && Character->TeamAffiliation == EBeyondTeam::Enemy
		&& (Character->BossBarWidgetClass || Character->Rank == EBeyondEnemyRank::MiniBoss || Character->Rank == EBeyondEnemyRank::Boss);
}

float UBeyondCombatLibrary::GetDamageScale(const UAbilitySystemComponent* SourceASC, FGameplayTag DamageType)
{
	if (!SourceASC)
	{
		return 1.0f;
	}
	FGameplayAttribute Stat;
	if (DamageType.MatchesTag(BeyondTags::DamageType_Melee))
	{
		Stat = UCharacterAttributeSet::GetStrengthAttribute();
	}
	else if (DamageType.MatchesTag(BeyondTags::DamageType_Projectile) || DamageType.MatchesTag(BeyondTags::DamageType_Explosion))
	{
		Stat = UCharacterAttributeSet::GetArcanaAttribute();
	}
	if (!Stat.IsValid() || !SourceASC->HasAttributeSetForAttribute(Stat))
	{
		return 1.0f;
	}
	return 1.0f + FMath::Max(SourceASC->GetNumericAttribute(Stat), 0.0f) / 100.0f;
}

void UBeyondCombatLibrary::GetAbilityBarAbilities(UAbilitySystemComponent* AbilitySystem, TArray<FGameplayAbilitySpecHandle>& OutAbilityHandles)
{
	OutAbilityHandles.Reset();
	if (!AbilitySystem)
	{
		return;
	}

	const FGameplayTag InputRoot = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input"));
	const ABeyondCharacterBase* Character = Cast<ABeyondCharacterBase>(AbilitySystem->GetAvatarActor());
	for (const FGameplayTag& Slot : { FGameplayTag(BeyondTags::Ability_Input_Q), FGameplayTag(BeyondTags::Ability_Input_E), FGameplayTag(BeyondTags::Ability_Input_R) })
	{
		for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
		{
			if (!Spec.Ability || (Character && Character->IsAbilitySuppressed(Spec.Ability->GetClass())))
			{
				continue;
			}

			// Same rule as player input: the ability set's slot, else the ability's own Input Tag
			const FGameplayTagContainer& SourceTags = Spec.GetDynamicSpecSourceTags();
			bool bInSlot = SourceTags.HasTagExact(Slot);
			if (!bInSlot && !SourceTags.HasTag(InputRoot))
			{
				const UBeyondGameplayAbility* BeyondAbility = Cast<UBeyondGameplayAbility>(Spec.Ability);
				bInSlot = BeyondAbility && BeyondAbility->InputTag == Slot;
			}

			const bool bAlreadyListed = OutAbilityHandles.ContainsByPredicate([&](const FGameplayAbilitySpecHandle& Handle)
			{
				const FGameplayAbilitySpec* Listed = AbilitySystem->FindAbilitySpecFromHandle(Handle);
				return Listed && Listed->Ability && Listed->Ability->GetClass() == Spec.Ability->GetClass();
			});
			if (bInSlot && !bAlreadyListed)
			{
				OutAbilityHandles.Add(Spec.Handle);
			}
		}
	}
}

bool UBeyondCombatLibrary::ApplyLegacyDamage(AActor* Source, AActor* Target, float Amount, uint8 DamageType, uint8 DamageResponse,
	bool bShouldDamageInvincible, bool bCanBeBlocked, bool bCanBeParried, bool bShouldForceInterrupt)
{
	return ApplyDamage(Source, Target, Amount, DamageTypeFromLegacy(DamageType), HitResponseFromLegacy(DamageResponse),
		!bCanBeBlocked, Source, !bCanBeParried, bShouldDamageInvincible, bShouldForceInterrupt);
}

DEFINE_FUNCTION(UBeyondCombatLibrary::execApplyDamageInfo)
{
	P_GET_OBJECT(AActor, Target);
	P_GET_OBJECT(AActor, DamageCauser);

	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const FStructProperty* InfoProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	const void* InfoData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	P_NATIVE_BEGIN;
	*static_cast<bool*>(RESULT_PARAM) = ApplyDamageInfoImpl(Target, DamageCauser, InfoProperty ? InfoProperty->Struct : nullptr, InfoData);
	P_NATIVE_END;
}

bool UBeyondCombatLibrary::ApplyDamageInfoImpl(AActor* Target, AActor* DamageCauser, const UStruct* InfoStruct, const void* InfoData)
{
	if (!InfoStruct || !InfoData)
	{
		return false;
	}

	double Amount = 0.0;
	uint8 DamageType = 0;
	uint8 DamageResponse = 0;
	bool bShouldDamageInvincible = false;
	bool bCanBeBlocked = true;
	bool bCanBeParried = true;
	bool bShouldForceInterrupt = false;

	// User-defined struct members have generated names; match on the name the user typed
	for (TFieldIterator<FProperty> It(InfoStruct); It; ++It)
	{
		const FString Name = It->GetAuthoredName();
		const void* Value = It->ContainerPtrToValuePtr<void>(InfoData);

		if (const FNumericProperty* Numeric = CastField<FNumericProperty>(*It))
		{
			if (Name == TEXT("Amount"))
			{
				Amount = Numeric->IsFloatingPoint() ? Numeric->GetFloatingPointPropertyValue(Value) : static_cast<double>(Numeric->GetSignedIntPropertyValue(Value));
			}
			else if (Name == TEXT("DamageType"))
			{
				DamageType = static_cast<uint8>(Numeric->GetUnsignedIntPropertyValue(Value));
			}
			else if (Name == TEXT("DamageResponse"))
			{
				DamageResponse = static_cast<uint8>(Numeric->GetUnsignedIntPropertyValue(Value));
			}
		}
		else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(*It))
		{
			const uint8 EnumValue = static_cast<uint8>(EnumProp->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(Value));
			if (Name == TEXT("DamageType")) { DamageType = EnumValue; }
			else if (Name == TEXT("DamageResponse")) { DamageResponse = EnumValue; }
		}
		else if (const FBoolProperty* Bool = CastField<FBoolProperty>(*It))
		{
			const bool bValue = Bool->GetPropertyValue(Value);
			if (Name == TEXT("ShouldDamageInvincible")) { bShouldDamageInvincible = bValue; }
			else if (Name == TEXT("CanBeBlocked")) { bCanBeBlocked = bValue; }
			else if (Name == TEXT("CanBeParried")) { bCanBeParried = bValue; }
			else if (Name == TEXT("ShouldForceInterrupt")) { bShouldForceInterrupt = bValue; }
		}
	}

	// Remember health to report whether damage actually landed (blocked / invincible hits return false)
	const float HealthBefore = GetActorHealth(Target);
	// Projectiles / weapons: credit the pawn that fired them
	AActor* Source = DamageCauser;
	if (DamageCauser && DamageCauser->GetInstigator())
	{
		Source = DamageCauser->GetInstigator();
	}

	const bool bApplied = ApplyDamage(Source, Target, static_cast<float>(Amount), DamageTypeFromLegacy(DamageType), HitResponseFromLegacy(DamageResponse),
		!bCanBeBlocked, DamageCauser, !bCanBeParried, bShouldDamageInvincible, bShouldForceInterrupt);

	return bApplied && GetActorHealth(Target) < HealthBefore;
}

bool UBeyondCombatLibrary::ApplyHeal(AActor* Source, AActor* Target, float Amount)
{
	UAbilitySystemComponent* TargetASC = GetASC(Target);
	if (!TargetASC || Amount <= 0.0f)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = GetASC(Source);
	UAbilitySystemComponent* SpecOwner = SourceASC ? SourceASC : TargetASC;

	FGameplayEffectContextHandle Context = SpecOwner->MakeEffectContext();
	Context.AddInstigator(Source, Source);

	const FGameplayEffectSpecHandle SpecHandle = SpecOwner->MakeOutgoingSpec(UBeyondGE_Heal::StaticClass(), 1.0f, Context);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Heal, Amount);
	SpecOwner->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return true;
}

float UBeyondCombatLibrary::HealActor(AActor* Target, float Amount)
{
	ApplyHeal(Target, Target, Amount);
	return GetActorHealth(Target);
}

int32 UBeyondCombatLibrary::GetActorTeamNumber(const AActor* Actor)
{
	return FGenericTeamId::GetTeamIdentifier(Actor).GetId();
}

bool UBeyondCombatLibrary::IsActorAttacking(const AActor* Actor)
{
	return BeyondAI::IsUsingAbility(GetASC(Actor));
}

bool UBeyondCombatLibrary::ReserveAttackTokens(AActor* Target, int32 Amount)
{
	ABeyondCharacterBase* Character = Cast<ABeyondCharacterBase>(Target);
	return !Character || Character->TryReserveAttackTokens(Amount);
}

void UBeyondCombatLibrary::ReturnAttackTokens(AActor* Target, int32 Amount)
{
	if (ABeyondCharacterBase* Character = Cast<ABeyondCharacterBase>(Target))
	{
		Character->ReleaseAttackTokens(Amount);
	}
}

bool UBeyondCombatLibrary::IsActorDead(const AActor* Actor)
{
	const UAbilitySystemComponent* ASC = GetASC(Actor);
	return ASC && ASC->HasMatchingGameplayTag(BeyondTags::State_Dead);
}

float UBeyondCombatLibrary::GetActorHealth(const AActor* Actor)
{
	const UAbilitySystemComponent* ASC = GetASC(Actor);
	return ASC ? ASC->GetNumericAttribute(UCharacterAttributeSet::GetCurrentHealthAttribute()) : 0.0f;
}

float UBeyondCombatLibrary::GetActorMaxHealth(const AActor* Actor)
{
	const UAbilitySystemComponent* ASC = GetASC(Actor);
	return ASC ? ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute()) : 0.0f;
}

float UBeyondCombatLibrary::GetActorHealthPercent(const AActor* Actor)
{
	const float Max = GetActorMaxHealth(Actor);
	return Max > 0.0f ? GetActorHealth(Actor) / Max : 0.0f;
}

bool UBeyondCombatLibrary::AreHostile(const AActor* A, const AActor* B)
{
	const FGenericTeamId TeamA = FGenericTeamId::GetTeamIdentifier(A);
	const FGenericTeamId TeamB = FGenericTeamId::GetTeamIdentifier(B);

	// Props and other actors without a team are never hostile
	if (TeamA == FGenericTeamId::NoTeam || TeamB == FGenericTeamId::NoTeam)
	{
		return false;
	}
	return FGenericTeamId::GetAttitude(TeamA, TeamB) == ETeamAttitude::Hostile;
}

bool UBeyondCombatLibrary::AreFriendly(const AActor* A, const AActor* B)
{
	const FGenericTeamId TeamA = FGenericTeamId::GetTeamIdentifier(A);
	const FGenericTeamId TeamB = FGenericTeamId::GetTeamIdentifier(B);
	if (TeamA == FGenericTeamId::NoTeam || TeamB == FGenericTeamId::NoTeam)
	{
		return false;
	}
	return FGenericTeamId::GetAttitude(TeamA, TeamB) == ETeamAttitude::Friendly;
}

void UBeyondCombatLibrary::SetInvincible(AActor* Actor, bool bInvincible)
{
	SetStateTag(Actor, BeyondTags::State_Invincible, bInvincible);
}

void UBeyondCombatLibrary::SetUninterruptible(AActor* Actor, bool bUninterruptible)
{
	SetStateTag(Actor, BeyondTags::State_Uninterruptible, bUninterruptible);
}

void UBeyondCombatLibrary::SetStateTag(AActor* Actor, FGameplayTag StateTag, bool bActive)
{
	if (UAbilitySystemComponent* ASC = GetASC(Actor))
	{
		ASC->SetLooseGameplayTagCount(StateTag, bActive ? 1 : 0);
	}
}

FGameplayTag UBeyondCombatLibrary::DamageTypeFromLegacy(uint8 DamageType)
{
	switch (DamageType)
	{
	case 1: return BeyondTags::DamageType_Melee;
	case 2: return BeyondTags::DamageType_Projectile;
	case 3: return BeyondTags::DamageType_Explosion;
	case 4: return BeyondTags::DamageType_Environment;
	default: return FGameplayTag();
	}
}

FGameplayTag UBeyondCombatLibrary::HitResponseFromLegacy(uint8 DamageResponse)
{
	switch (DamageResponse)
	{
	case 1: return BeyondTags::Event_Hit_Light;
	case 2: return BeyondTags::Event_Hit_Stagger;
	case 3: return BeyondTags::Event_Hit_Stun;
	case 4: return BeyondTags::Event_Hit_KnockBack;
	default: return FGameplayTag();
	}
}

bool UBeyondCombatLibrary::IsInCutscene(const AActor* Actor)
{
	UWorld* World = Actor ? Actor->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()); PC && PC->bCinematicMode)
		{
			return true;
		}
	}

	for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
	{
		ULevelSequencePlayer* Player = It->GetSequencePlayer();
		if (Player && Player->IsPlaying() && !Player->GetObjectBindings(const_cast<AActor*>(Actor)).IsEmpty())
		{
			return true;
		}
	}
	return false;
}

bool UBeyondCombatLibrary::HasAnimSlot(const UAnimInstance* AnimInstance, FName SlotName)
{
	const IAnimClassInterface* AnimClass = AnimInstance ? IAnimClassInterface::GetFromClass(AnimInstance->GetClass()) : nullptr;
	if (!AnimClass || SlotName.IsNone())
	{
		return false;
	}

	for (const FStructProperty* Prop : AnimClass->GetAnimNodeProperties())
	{
		if (Prop && Prop->Struct && Prop->Struct->IsChildOf(FAnimNode_Slot::StaticStruct())
			&& Prop->ContainerPtrToValuePtr<FAnimNode_Slot>(AnimInstance)->SlotName == SlotName)
		{
			return true;
		}
	}
	return false;
}

namespace
{
	// Leeway around the crosshair when looking for a character under it (cm)
	constexpr float AimPawnSweepRadius = 12.0f;
}

bool UBeyondCombatLibrary::TraceAlongView(const APawn* Pawn, float Range, FHitResult& OutHit)
{
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!PC || !World)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondAimTrace), false, Pawn);
	TArray<AActor*> Ignored;
	Pawn->GetAttachedActors(Ignored, true, true);
	// The buddy often stands between the camera and the crosshair
	if (const UBeyondPartyComponent* Party = PC->FindComponentByClass<UBeyondPartyComponent>())
	{
		for (ABeyondCharacterBase* Member : Party->GetMembers())
		{
			if (Member && Member != Pawn)
			{
				Ignored.Add(Member);
				Member->GetAttachedActors(Ignored, false, true);
			}
		}
	}
	Params.AddIgnoredActors(Ignored);

	if (!World->LineTraceSingleByChannel(OutHit, ViewLocation, TraceEnd, ECC_Visibility, Params))
	{
		OutHit = FHitResult(ViewLocation, TraceEnd);
	}

	// Character capsules ignore the Visibility channel: look for one in front of the wall separately (a thin sweep,
	// so the crosshair doesn't have to be pixel-perfect on a limb)
	const FVector PawnEnd = OutHit.bBlockingHit ? OutHit.ImpactPoint : TraceEnd;
	FHitResult PawnHit;
	if (World->SweepSingleByObjectType(PawnHit, ViewLocation, PawnEnd, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(AimPawnSweepRadius), Params))
	{
		OutHit = PawnHit;
	}
	return true;
}
