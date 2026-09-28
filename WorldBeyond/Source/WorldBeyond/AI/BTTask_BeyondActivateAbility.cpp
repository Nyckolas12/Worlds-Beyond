// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BTTask_BeyondActivateAbility.h"
#include "AI/BeyondAIAbilityUtils.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"

UBTTask_BeyondActivateAbility::UBTTask_BeyondActivateAbility()
{
	NodeName = TEXT("Activate Ability (Beyond)");
	bCreateNodeInstance = true;
	bNotifyTaskFinished = true;
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_BeyondActivateAbility, BlackboardKey), AActor::StaticClass());
	BlackboardKey.AllowNoneAsValue(true);
}

EBTNodeResult::Type UBTTask_BeyondActivateAbility::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn);
	if (!ASC)
	{
		return EBTNodeResult::Failed;
	}

	AActor* Target = nullptr;
	if (const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent(); Blackboard && BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass())
	{
		Target = Cast<AActor>(Blackboard->GetValue<UBlackboardKeyType_Object>(BlackboardKey.GetSelectedKeyID()));
	}

	FGameplayAbilitySpecHandle Handle;
	switch (Selection)
	{
	case EBeyondAbilitySelection::BestAvailable:
	{
		BeyondAI::FAbilityChoice Choice;
		if (BeyondAI::SelectAbility(ASC, Target, nullptr, Choice))
		{
			Handle = Choice.Handle;
		}
		break;
	}
	case EBeyondAbilitySelection::ByAbilityTag:
	case EBeyondAbilitySelection::ByInputTag:
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const bool bMatches = Selection == EBeyondAbilitySelection::ByInputTag
				? Spec.GetDynamicSpecSourceTags().HasTagExact(AbilityTag)
				: (Spec.Ability && Spec.Ability->GetAssetTags().HasTag(AbilityTag));
			if (bMatches && !Spec.IsActive())
			{
				Handle = Spec.Handle;
				break;
			}
		}
		break;
	}

	if (!Handle.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	if (Target)
	{
		AIController->SetFocus(Target, EAIFocusPriority::Gameplay);
	}

	if (!bFinishOnActivate)
	{
		OwnerCompPtr = &OwnerComp;
		ListeningASC = ASC;
		ActiveHandle = Handle;
		EndedDelegateHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
	}

	if (!ASC->TryActivateAbility(Handle))
	{
		StopListening();
		return EBTNodeResult::Failed;
	}

	// Instant abilities may already have ended inside TryActivateAbility
	if (bFinishOnActivate || !ActiveHandle.IsValid())
	{
		StopListening();
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_BeyondActivateAbility::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (EndedData.AbilitySpecHandle != ActiveHandle)
	{
		return;
	}

	ActiveHandle = FGameplayAbilitySpecHandle();
	if (UBehaviorTreeComponent* OwnerComp = OwnerCompPtr.Get(); OwnerComp && OwnerComp->GetTaskStatus(this) == EBTTaskStatus::Active)
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_BeyondActivateAbility::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (UAbilitySystemComponent* ASC = ListeningASC.Get(); ASC && ActiveHandle.IsValid())
	{
		ASC->CancelAbilityHandle(ActiveHandle);
	}
	return EBTNodeResult::Aborted;
}

void UBTTask_BeyondActivateAbility::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	StopListening();
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_BeyondActivateAbility::StopListening()
{
	if (UAbilitySystemComponent* ASC = ListeningASC.Get())
	{
		ASC->OnAbilityEnded.Remove(EndedDelegateHandle);
	}
	ListeningASC.Reset();
	EndedDelegateHandle.Reset();
	ActiveHandle = FGameplayAbilitySpecHandle();
}

FString UBTTask_BeyondActivateAbility::GetStaticDescription() const
{
	const FString TargetText = BlackboardKey.IsSet() ? FString::Printf(TEXT("\nTarget: %s"), *BlackboardKey.SelectedKeyName.ToString()) : FString();
	switch (Selection)
	{
	case EBeyondAbilitySelection::ByAbilityTag: return FString::Printf(TEXT("Ability tag: %s%s"), *AbilityTag.ToString(), *TargetText);
	case EBeyondAbilitySelection::ByInputTag: return FString::Printf(TEXT("Input slot: %s%s"), *AbilityTag.ToString(), *TargetText);
	default: return FString::Printf(TEXT("Best available ability%s"), *TargetText);
	}
}
