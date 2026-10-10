// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondFX.h"
#include "Animation/AnimEnums.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "BeyondRushComponent.generated.h"

class ABeyondCharacterBase;
class UAnimMontage;
class UFXSystemComponent;

/** How a character rushes somewhere (UBeyondRushComponent): a dash that keeps going until it gets there. */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondRushSettings
{
	GENERATED_BODY()

	// Travel speed (uu/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rush", meta = (ClampMin = "100"))
	float Speed = 2600.0f;

	// Stops this far short of the target actor's capsule (or this close to a target point)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rush", meta = (ClampMin = "0"))
	float StopDistance = 100.0f;

	// Further than this and the character blinks there instead (0: always rush)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rush", meta = (ClampMin = "0"))
	float MaxRushDistance = 3000.0f;

	// Still not there after this long (blocked, target running away): blink the rest of the way
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rush", meta = (ClampMin = "0.1"))
	float Timeout = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rush")
	bool bInvincible = true;

	// Pass through characters instead of stopping at them
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rush")
	bool bPassThroughPawns = true;

	// Played when the rush starts, its root motion ignored; empty: the character's own dash (UBeyondGA_Dash) montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rush|Animation")
	TObjectPtr<UAnimMontage> DashMontage;

	// Looped after the dash montage for as long as the rush lasts
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rush|Animation")
	TObjectPtr<UAnimMontage> LoopMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rush|Animation", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.0f;

	// Attached for the rush; empty: the character's own dash trail
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rush|FX")
	FBeyondFX TrailFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rush|FX")
	FName TrailSocket;

	// Where a blink leaves from and arrives at
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rush|FX")
	FBeyondFX BlinkDepartFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rush|FX")
	FBeyondFX BlinkArriveFX;
};

/**
 * Moves a character to a target actor (followed as it moves) or a point with a root-motion force on its movement
 * component, playing its dash animation and trail, passing through characters and invincible on the way. Too far,
 * blocked or out of time, it blinks (teleports) the rest of the way, so it always arrives.
 *
 * Added to the character being moved and removed when the rush ends, so it works whoever started it: the duo move
 * sends the AI Ji-Woong in from Angel's ability, a boss arena pulls the party inside before its walls rise.
 */
UCLASS(ClassGroup = (Beyond), meta = (BlueprintSpawnableComponent = "false"))
class WORLDBEYOND_API UBeyondRushComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBeyondRushComponent();

	using FOnRushFinished = TFunction<void(bool bArrived)>;

	// Rush Who to Target, or to TargetPoint when Target is null. Replaces a rush already running on Who.
	// OnFinished runs once the character is there and back to normal (bArrived false: it blinked the last part).
	static UBeyondRushComponent* Rush(ABeyondCharacterBase* Who, AActor* Target, const FVector& Point, const FBeyondRushSettings& RushSettings,
		FOnRushFinished FinishedCallback = nullptr);

	// The rush running on Who, if any
	static UBeyondRushComponent* FindRush(const AActor* Who);

	// Teleports Who to Destination (snapped to the ground) with the settings' blink effects
	static void Blink(ABeyondCharacterBase* Who, const FVector& Destination, const FRotator& Facing, const FBeyondRushSettings& RushSettings);

	// Where Who would stop next to Target: short of its capsule, on Who's side, at Who's height
	static FVector GetStopPoint(const ABeyondCharacterBase* Who, const AActor* Target, float StopDistance);

	// Stops now and puts the character back as it was; OnFinished is not called
	void Cancel();

	bool IsRushing() const { return bRushing; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Begin(AActor* Target, const FVector& InTargetPoint, const FBeyondRushSettings& InSettings, FOnRushFinished InOnFinished);
	FVector GetDestination() const;
	bool HasArrived() const;
	void Finish(bool bArrived);
	void Restore();
	void UpdateMontages();
	void FillFromDashAbility();

	ABeyondCharacterBase* GetCharacter() const;

	FBeyondRushSettings Settings;
	FOnRushFinished OnFinished;
	TWeakObjectPtr<AActor> TargetActor;
	FVector TargetPoint = FVector::ZeroVector;

	bool bRushing = false;
	uint16 RootMotionSourceID = 0;
	float Elapsed = 0.0f;
	float Duration = 0.0f;
	float BestDistance = 0.0f;
	float LastProgressTime = 0.0f;

	bool bAddedInvincible = false;
	TEnumAsByte<ECollisionResponse> PreviousPawnResponse = ECR_Block;
	bool bChangedRootMotionMode = false;
	TEnumAsByte<ERootMotionMode::Type> PreviousRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	TWeakObjectPtr<UFXSystemComponent> Trail;
	TArray<TWeakObjectPtr<AActor>> IgnoredActors;
};
