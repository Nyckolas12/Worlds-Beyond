// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "BeyondEnemyAnimInstance.generated.h"

class UAnimSequenceBase;
class UBlendSpace;
class UBeyondEnemyAnimInstance;

/**
 * The enemy anim instance's graph, built in C++ (the engine's sequencer instance does the same):
 * locomotion blendspace (or an idle loop) -> DefaultSlot (montages) -> output.
 */
USTRUCT()
struct WORLDBEYOND_API FBeyondEnemyAnimProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FBeyondEnemyAnimProxy() {}
	explicit FBeyondEnemyAnimProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;
	virtual void CacheBones() override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void AddReferencedObjects(UAnimInstance* InAnimInstance, FReferenceCollector& Collector) override;

private:
	void LinkNodes();
	void EnsureInitialized();
	FVector GetBlendPosition() const;

	FAnimNode_BlendSpacePlayer_Standalone Locomotion;
	FAnimNode_SequencePlayer_Standalone Idle;
	FAnimNode_Slot Slot;

	TObjectPtr<UBlendSpace> BlendSpace;
	TObjectPtr<UAnimSequenceBase> IdleSequence;
	float Speed = 0.0f;
	float Direction = 0.0f;
	bool bNodesInitialized = false;
	bool bNeedsRelink = true;
};

/**
 * Animates enemies without an anim Blueprint (the Paragon Minions and jungle creatures ship none): walks / runs
 * through the definition's Locomotion blendspace by speed (and direction, for 2D blendspaces) and plays montages
 * on DefaultSlot (attacks, hit reactions, deaths). ABeyondEnemyCharacter picks it when the definition has no
 * Anim Class.
 */
UCLASS(Transient, Blueprintable)
class WORLDBEYOND_API UBeyondEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Filled from the owner's enemy definition on initialization; can be set on a Blueprint child instead
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy Anim")
	TObjectPtr<UBlendSpace> LocomotionBlendSpace;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy Anim")
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy Anim")
	float Speed = 0.0f;

	// -180..180 degrees between the facing and the movement
	UPROPERTY(BlueprintReadOnly, Category = "Enemy Anim")
	float Direction = 0.0f;

	// Montage slot the C++ graph has
	static const FName SlotName;

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
