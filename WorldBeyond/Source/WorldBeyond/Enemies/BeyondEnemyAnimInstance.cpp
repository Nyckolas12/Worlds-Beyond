// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemies/BeyondEnemyAnimInstance.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "GameFramework/Pawn.h"

const FName UBeyondEnemyAnimInstance::SlotName(TEXT("DefaultSlot"));

void FBeyondEnemyAnimProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);
	Slot.SlotName = UBeyondEnemyAnimInstance::SlotName;
	bNodesInitialized = false;
	bNeedsRelink = true;
}

void FBeyondEnemyAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	// Game thread: copy what the worker-thread update needs
	const UBeyondEnemyAnimInstance* Instance = CastChecked<UBeyondEnemyAnimInstance>(InAnimInstance);
	if (Instance->LocomotionBlendSpace != BlendSpace || Instance->IdleAnimation != IdleSequence)
	{
		BlendSpace = Instance->LocomotionBlendSpace;
		IdleSequence = Instance->IdleAnimation;
		bNeedsRelink = true;
	}
	Speed = Instance->Speed;
	Direction = Instance->Direction;
}

void FBeyondEnemyAnimProxy::LinkNodes()
{
	Locomotion.SetBlendSpace(BlendSpace);
	Locomotion.SetLoop(true);
	Idle.SetSequence(IdleSequence);
	Idle.SetLoopAnimation(true);
	Slot.Source.SetLinkNode(BlendSpace ? static_cast<FAnimNode_Base*>(&Locomotion) : static_cast<FAnimNode_Base*>(&Idle));
}

void FBeyondEnemyAnimProxy::EnsureInitialized()
{
	if (bNodesInitialized && !bNeedsRelink)
	{
		return;
	}
	LinkNodes();
	Slot.Initialize_AnyThread(FAnimationInitializeContext(this));
	Slot.CacheBones_AnyThread(FAnimationCacheBonesContext(this));
	bNodesInitialized = true;
	bNeedsRelink = false;
}

FVector FBeyondEnemyAnimProxy::GetBlendPosition() const
{
	FVector Position = FVector::ZeroVector;
	if (!BlendSpace)
	{
		return Position;
	}

	// Axes by name ("Speed", "Direction"); unnamed 2D blendspaces follow the usual X = direction, Y = speed
	const bool bOneDimensional = BlendSpace->IsA<UBlendSpace1D>();
	for (int32 Axis = 0; Axis < (bOneDimensional ? 1 : 2); ++Axis)
	{
		const FBlendParameter& Parameter = BlendSpace->GetBlendParameter(Axis);
		const FString Name = Parameter.DisplayName.ToLower();
		bool bDirection = Name.Contains(TEXT("dir")) || Name.Contains(TEXT("angle")) || Name.Contains(TEXT("yaw"));
		if (!bDirection && !Name.Contains(TEXT("speed")) && !Name.Contains(TEXT("vel")) && !bOneDimensional)
		{
			bDirection = Axis == 0;
		}
		const float Value = bDirection ? Direction : Speed;
		Position[Axis] = Parameter.bWrapInput ? Value : FMath::Clamp(Value, Parameter.Min, Parameter.Max);
	}
	return Position;
}

void FBeyondEnemyAnimProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	EnsureInitialized();
	Locomotion.SetPosition(GetBlendPosition());
	Slot.Update_AnyThread(InContext);
}

void FBeyondEnemyAnimProxy::CacheBones()
{
	if (bBoneCachesInvalidated)
	{
		EnsureInitialized();
		Slot.CacheBones_AnyThread(FAnimationCacheBonesContext(this));
		bBoneCachesInvalidated = false;
	}
}

bool FBeyondEnemyAnimProxy::Evaluate(FPoseContext& Output)
{
	EnsureInitialized();
	Slot.Evaluate_AnyThread(Output);
	return true;
}

void FBeyondEnemyAnimProxy::AddReferencedObjects(UAnimInstance* InAnimInstance, FReferenceCollector& Collector)
{
	FAnimInstanceProxy::AddReferencedObjects(InAnimInstance, Collector);
	Collector.AddReferencedObject(BlendSpace);
	Collector.AddReferencedObject(IdleSequence);
}

void UBeyondEnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (const ABeyondEnemyCharacter* Enemy = Cast<ABeyondEnemyCharacter>(TryGetPawnOwner()))
	{
		if (const UBeyondEnemyDefinition* Definition = Enemy->GetDefinition())
		{
			if (!LocomotionBlendSpace)
			{
				LocomotionBlendSpace = Definition->Locomotion;
			}
			if (!IdleAnimation)
			{
				IdleAnimation = Definition->IdleAnimation;
			}
		}
	}
}

void UBeyondEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const APawn* Pawn = TryGetPawnOwner();
	if (!Pawn)
	{
		Speed = 0.0f;
		Direction = 0.0f;
		return;
	}

	const FVector Velocity = Pawn->GetVelocity();
	Speed = Velocity.Size2D();
	Direction = Speed > 1.0f ? FRotator::NormalizeAxis(Velocity.Rotation().Yaw - Pawn->GetActorRotation().Yaw) : 0.0f;
}

FAnimInstanceProxy* UBeyondEnemyAnimInstance::CreateAnimInstanceProxy()
{
	return new FBeyondEnemyAnimProxy(this);
}

void UBeyondEnemyAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FBeyondEnemyAnimProxy*>(InProxy);
}
