// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondHazardVolume.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "BeyondGameplayTags.h"
#include "Characters/BeyondCharacterBase.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "World/BeyondWorldSubsystem.h"

ABeyondHazardVolume::ABeyondHazardVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(500.0f, 500.0f, 200.0f));
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetCanEverAffectNavigation(false);
	Box->ShapeColor = FColor(255, 90, 30);
}

void ABeyondHazardVolume::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimer(CheckTimer, this, &ThisClass::Check, 0.25f, true, FMath::FRandRange(0.0f, 0.25f));
}

void ABeyondHazardVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CheckTimer);
	Super::EndPlay(EndPlayReason);
}

bool ABeyondHazardVolume::ContainsLocation(const FVector& Location) const
{
	const FVector Local = Box->GetComponentTransform().InverseTransformPosition(Location);
	const FVector Extent = Box->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

bool ABeyondHazardVolume::IsInsideAnyHazard(const UWorld* World, const FVector& Location)
{
	for (TActorIterator<ABeyondHazardVolume> It(World); It; ++It)
	{
		if (It->ContainsLocation(Location))
		{
			return true;
		}
	}
	return false;
}

void ABeyondHazardVolume::Check()
{
	UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this);
	for (ABeyondCharacterBase* Member : UBeyondWorldSubsystem::GetPartyMembers(this))
	{
		if (!Member || UBeyondCombatLibrary::IsActorDead(Member))
		{
			continue;
		}
		const bool bInside = ContainsLocation(Member->GetActorLocation());
		if (Kind == EBeyondHazardKind::Burn)
		{
			// Once a second while inside (a damage-over-time refreshed this often would never tick), then a burn that lingers
			const double Now = GetWorld()->GetTimeSeconds();
			const double* LastBurn = Burning.Find(Member);
			if (bInside && (!LastBurn || Now - *LastBurn >= 1.0))
			{
				UBeyondCombatLibrary::ApplyDamage(this, Member, BurnDamagePerSecond, BeyondTags::DamageType_Proc_Burn, FGameplayTag(), true, this, true);
				if (BurnFX.IsSet())
				{
					BeyondFX::SpawnAtLocation(this, BurnFX, Member->GetActorLocation());
				}
				Burning.Add(Member, Now);
			}
			else if (!bInside && LastBurn)
			{
				Burning.Remove(Member);
				UBeyondCombatLibrary::ApplyDamageOverTime(this, Member, BurnDamagePerSecond, BurnLinger, BeyondTags::DamageType_Proc_Burn, FBeyondFX());
			}
			continue;
		}

		// Deep water
		if (!bInside)
		{
			Returning.Remove(Member);
			continue;
		}
		if (!Returning.Contains(Member) && WorldSubsystem)
		{
			Returning.Add(Member);
			const float Damage = UBeyondCombatLibrary::GetActorMaxHealth(Member) * DeepWaterDamageFraction;
			if (Damage > 0.0f && UBeyondCombatLibrary::GetActorHealth(Member) > Damage + 1.0f)
			{
				UBeyondCombatLibrary::ApplyDamage(this, Member, Damage, BeyondTags::DamageType_Environment, FGameplayTag(), true, this, true, true);
			}
			WorldSubsystem->ReturnToSafeGround(Member);
		}
	}
}
