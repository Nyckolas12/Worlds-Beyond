// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapons/BeyondWeapon.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "BeyondGameplayTags.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"

ABeyondWeapon::ABeyondWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}

AActor* ABeyondWeapon::GetWielder() const
{
	if (GetInstigator())
	{
		return GetInstigator();
	}
	if (GetOwner())
	{
		return GetOwner();
	}
	return GetAttachParentActor();
}

ABeyondWeapon* ABeyondWeapon::FindEquippedWeapon(const AActor* Character)
{
	if (!Character)
	{
		return nullptr;
	}

	TArray<AActor*> Attached;
	Character->GetAttachedActors(Attached, true, true);
	for (AActor* Actor : Attached)
	{
		if (ABeyondWeapon* Weapon = Cast<ABeyondWeapon>(Actor); Weapon && !Weapon->bHolstered)
		{
			return Weapon;
		}
	}
	return nullptr;
}

void ABeyondWeapon::HitScanStart(FGameplayEffectSpecHandle EffectSpecHandle)
{
	ActiveSpec = EffectSpecHandle;
	ActiveDamage = BaseDamage;
	ActiveHitResponse = BeyondTags::Event_Hit_Light;
	ActorsHitThisScan.Reset();
	bHasPreviousSegment = false;
	bScanning = true;
	SetActorTickEnabled(true);
}

void ABeyondWeapon::BeginMeleeScan(float Damage, const FGameplayTag& HitResponse)
{
	HitScanStart(FGameplayEffectSpecHandle());
	ActiveDamage = Damage;
	ActiveHitResponse = HitResponse;
}

void ABeyondWeapon::HitScanEnd()
{
	bScanning = false;
	ActiveSpec = FGameplayEffectSpecHandle();
	SetActorTickEnabled(false);
}

bool ABeyondWeapon::GetBladeSegment(FVector& OutStart, FVector& OutEnd) const
{
	TInlineComponentArray<UMeshComponent*> Meshes(this);
	for (const UMeshComponent* Mesh : Meshes)
	{
		if (Mesh->DoesSocketExist(TraceStartSocket) && Mesh->DoesSocketExist(TraceEndSocket))
		{
			OutStart = Mesh->GetSocketLocation(TraceStartSocket);
			OutEnd = Mesh->GetSocketLocation(TraceEndSocket);
			return true;
		}
	}

	// No sockets: use the longest axis of the first mesh's bounds as the blade
	for (const UMeshComponent* Mesh : Meshes)
	{
		const FBoxSphereBounds LocalBounds = Mesh->CalcBounds(FTransform::Identity);
		const FVector Extent = LocalBounds.BoxExtent;
		if (Extent.IsNearlyZero())
		{
			continue;
		}

		FVector Axis = FVector::XAxisVector * Extent.X;
		if (Extent.Y > Extent.X && Extent.Y >= Extent.Z) { Axis = FVector::YAxisVector * Extent.Y; }
		else if (Extent.Z > Extent.X && Extent.Z > Extent.Y) { Axis = FVector::ZAxisVector * Extent.Z; }

		const FTransform& ToWorld = Mesh->GetComponentTransform();
		OutStart = ToWorld.TransformPosition(LocalBounds.Origin - Axis);
		OutEnd = ToWorld.TransformPosition(LocalBounds.Origin + Axis);
		return true;
	}
	return false;
}

void ABeyondWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bScanning)
	{
		return;
	}

	FVector Start, End;
	if (!GetBladeSegment(Start, End))
	{
		return;
	}

	// The blade itself, plus the paths its ends travelled since last frame (fast swings)
	SweepSegment(Start, End);
	if (bHasPreviousSegment)
	{
		SweepSegment(PreviousStart, Start);
		SweepSegment(PreviousEnd, End);
	}

	PreviousStart = Start;
	PreviousEnd = End;
	bHasPreviousSegment = true;
}

void ABeyondWeapon::SweepSegment(const FVector& From, const FVector& To)
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondWeaponSweep), false, this);
	Params.AddIgnoredActor(GetWielder());

	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByChannel(Hits, From, To, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(TraceRadius), Params);
	for (const FHitResult& Hit : Hits)
	{
		HandleHit(Hit.GetActor(), Hit);
	}
}

void ABeyondWeapon::HandleHit(AActor* HitActor, const FHitResult& Hit)
{
	AActor* Wielder = GetWielder();
	if (!HitActor || HitActor == Wielder || ActorsHitThisScan.Contains(HitActor))
	{
		return;
	}
	if (!UBeyondCombatLibrary::AreHostile(Wielder, HitActor) || UBeyondCombatLibrary::IsActorDead(HitActor))
	{
		return;
	}

	ActorsHitThisScan.Add(HitActor);

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Wielder);
	if (ActiveSpec.IsValid() && TargetASC)
	{
		if (SourceASC)
		{
			SourceASC->ApplyGameplayEffectSpecToTarget(*ActiveSpec.Data.Get(), TargetASC);
		}
		else
		{
			TargetASC->ApplyGameplayEffectSpecToSelf(*ActiveSpec.Data.Get());
		}
	}
	else
	{
		UBeyondCombatLibrary::ApplyDamage(Wielder, HitActor, ActiveDamage, BeyondTags::DamageType_Melee, ActiveHitResponse, false, this);
	}

	OnWeaponHit.Broadcast(HitActor, Hit);
}
