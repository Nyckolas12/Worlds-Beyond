// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "AbilitySystem/BeyondFX.h"
#include "BeyondProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;

/**
 * A plain C++ projectile for UBeyondGA_Projectile (enemy bolts, shots, spit): flies straight at the target location,
 * applies the damage spec the ability gives it to the first hostile it touches, stops on walls. The ability sets
 * Speed, Target Location and Effect Spec Handle by name before it finishes spawning (the same names as
 * BP_Projectile_GABase). Make a Blueprint child per look and set the FX.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API ABeyondProjectile : public AActor
{
	GENERATED_BODY()

public:
	ABeyondProjectile();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ExposeOnSpawn = "true", ClampMin = "1"))
	float Speed = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ExposeOnSpawn = "true"))
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Projectile", meta = (ExposeOnSpawn = "true"))
	FGameplayEffectSpecHandle EffectSpecHandle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "1"))
	float CollisionRadius = 22.0f;

	// Seconds before it fizzles out
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.1"))
	float MaxLifetime = 4.0f;

	// Attached while it flies (set Max Lifetime 0 on it: it goes with the projectile)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|FX")
	FBeyondFX TrailFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|FX")
	FBeyondFX ImpactFX;

	UFUNCTION(BlueprintPure, Category = "Projectile")
	USphereComponent* GetCollision() const { return Collision; }

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void Impact(const FVector& Location);

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	bool bImpacted = false;
};
