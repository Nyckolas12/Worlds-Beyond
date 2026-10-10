// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/BeyondProjectile.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

ABeyondProjectile::ABeyondProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(CollisionRadius);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetCanEverAffectNavigation(false);
	SetRootComponent(Collision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = 0.0f;
}

void ABeyondProjectile::BeginPlay()
{
	Super::BeginPlay();

	Collision->SetSphereRadius(CollisionRadius);
	if (AActor* Shooter = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Shooter, true);
	}
	Collision->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleOverlap);
	Collision->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandleHit);

	// Straight at where the target was when it left the hand
	FVector Direction = GetActorForwardVector();
	if (!TargetLocation.IsNearlyZero())
	{
		Direction = (TargetLocation - GetActorLocation()).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			Direction = GetActorForwardVector();
		}
	}
	SetActorRotation(Direction.Rotation());
	Movement->Velocity = Direction * Speed;

	BeyondFX::SpawnAttached(TrailFX, Collision);
	SetLifeSpan(MaxLifetime);
}

void ABeyondProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AActor* Shooter = GetInstigator();
	if (bImpacted || !OtherActor || OtherActor == Shooter || OtherActor == GetOwner() || UBeyondCombatLibrary::IsActorDead(OtherActor)
		|| (Shooter && !UBeyondCombatLibrary::AreHostile(Shooter, OtherActor)))
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
	if (!TargetASC)
	{
		return;
	}

	if (const FGameplayEffectSpec* Spec = EffectSpecHandle.Data.Get())
	{
		if (UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Shooter))
		{
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec, TargetASC);
		}
		else
		{
			TargetASC->ApplyGameplayEffectSpecToSelf(*Spec);
		}
	}
	Impact(bFromSweep ? FVector(SweepResult.ImpactPoint) : OtherActor->GetActorLocation());
}

void ABeyondProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	Impact(Hit.ImpactPoint);
}

void ABeyondProjectile::Impact(const FVector& Location)
{
	if (bImpacted)
	{
		return;
	}
	bImpacted = true;
	BeyondFX::SpawnAtLocation(this, ImpactFX, Location, GetActorRotation());
	Destroy();
}
