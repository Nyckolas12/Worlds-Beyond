// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "BTTask_BeyondActivateAbility.generated.h"

struct FAbilityEndedData;

UENUM()
enum class EBeyondAbilitySelection : uint8
{
	// Let the AI pick a ready ability using the AI hints on each ability
	BestAvailable,
	// First ready ability whose Ability Tags contain AbilityTag
	ByAbilityTag,
	// Ability bound to an input slot (Ability.Input.*)
	ByInputTag
};

/**
 * Activates a gameplay ability on the controlled pawn and succeeds when it ends.
 * The blackboard key (usually AttackTarget) becomes the AI focus so abilities can aim with GetAimRotation.
 */
UCLASS(meta = (DisplayName = "Activate Ability (Beyond)"))
class WORLDBEYOND_API UBTTask_BeyondActivateAbility : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_BeyondActivateAbility();

	UPROPERTY(EditAnywhere, Category = "Ability")
	EBeyondAbilitySelection Selection = EBeyondAbilitySelection::BestAvailable;

	UPROPERTY(EditAnywhere, Category = "Ability", meta = (EditCondition = "Selection != EBeyondAbilitySelection::BestAvailable", EditConditionHides))
	FGameplayTag AbilityTag;

	// Succeed right after activation instead of waiting for the ability to end
	UPROPERTY(EditAnywhere, Category = "Ability")
	bool bFinishOnActivate = false;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

private:
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);
	void StopListening();

	TWeakObjectPtr<UBehaviorTreeComponent> OwnerCompPtr;
	TWeakObjectPtr<class UAbilitySystemComponent> ListeningASC;
	FGameplayAbilitySpecHandle ActiveHandle;
	FDelegateHandle EndedDelegateHandle;
};
