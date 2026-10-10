// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_Summon.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AIController.h"
#include "BeyondGameplayTags.h"
#include "Characters/BeyondCharacterBase.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"

UBeyondGA_Summon::UBeyondGA_Summon()
{
	SetAssetTags(FGameplayTagContainer(BeyondTags::Ability_Enemy_Attack));
	AIMinRange = 0.0f;
	AIMaxRange = 2500.0f;
	AIWeight = 1.5f;
}

int32 UBeyondGA_Summon::GetLiveSummonCount() const
{
	int32 Live = 0;
	for (const TWeakObjectPtr<ABeyondEnemyCharacter>& Summon : Summons)
	{
		Live += Summon.IsValid() && !UBeyondCombatLibrary::IsActorDead(Summon.Get()) ? 1 : 0;
	}
	return Live;
}

bool UBeyondGA_Summon::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return GetLiveSummonCount() < MaxAlive && Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UBeyondGA_Summon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (EnemyIds.IsEmpty() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	SummonTarget = GetAIFocusTarget();
	if (const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo()))
	{
		if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
		{
			AI->StopMovement();
		}
	}
	const float Length = PlayMontageOnAvatar(Montage);
	GetWorld()->GetTimerManager().SetTimer(SummonTimer, FTimerDelegate::CreateUObject(this, &ThisClass::SpawnSummons), FMath::Max(SummonDelay, 0.01f), false);
	GetWorld()->GetTimerManager().SetTimer(EndTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsActive())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
	}), FMath::Max(Length, SummonDelay + 0.2f), false);
}

void UBeyondGA_Summon::SpawnSummons()
{
	ABeyondCharacterBase* Caster = GetBeyondCharacter();
	UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(this);
	if (!Caster || !Enemies || !IsActive())
	{
		return;
	}

	Summons.RemoveAll([](const TWeakObjectPtr<ABeyondEnemyCharacter>& Summon) { return !Summon.IsValid() || UBeyondCombatLibrary::IsActorDead(Summon.Get()); });
	const int32 Wanted = FMath::Min(Count, MaxAlive - Summons.Num());
	for (int32 Index = 0; Index < Wanted; ++Index)
	{
		UBeyondEnemyDefinition* Definition = Enemies->FindDefinition(EnemyIds[FMath::RandRange(0, EnemyIds.Num() - 1)]);
		if (!Definition)
		{
			continue;
		}
		const float Angle = 2.0f * PI * Index / FMath::Max(Wanted, 1) + FMath::FRandRange(-0.4f, 0.4f);
		const FVector Location = Caster->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(MinDistance, MaxDistance);
		FBeyondEnemySpawnParams Params;
		Params.Level = Caster->GetCharacterLevel();
		Params.bSummoned = true;
		if (ABeyondEnemyCharacter* Summon = Enemies->SpawnEnemy(Definition, FTransform(Caster->GetActorRotation(), Location), Params))
		{
			BeyondFX::SpawnAtLocation(this, SummonFX, Summon->GetActorLocation());
			Summons.Add(Summon);
			Enemies->AlertEnemy(Summon, SummonTarget.Get());
		}
	}
}
