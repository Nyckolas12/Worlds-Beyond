// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BeyondPlayerController.generated.h"

class ABeyondCharacterBase;
class UBeyondPartyComponent;
class UInputAction;
class UInputMappingContext;
class UUserWidget;

/**
 * Owns the party (character swapping), the input mapping contexts and the HUD.
 * The HUD is recreated whenever the controlled demigod changes, so widgets that read
 * GetOwningPlayerPawn on construct always show the current leader.
 */
UCLASS()
class WORLDBEYOND_API ABeyondPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABeyondPlayerController();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Party")
	TObjectPtr<UBeyondPartyComponent> PartyComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SwapAction;

	// Leave empty if the characters still create their own HUD
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	UFUNCTION(BlueprintPure, Category = "UI")
	UUserWidget* GetHUDWidget() const { return HUDWidget; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

	UFUNCTION()
	void HandleLeaderChanged(ABeyondCharacterBase* NewLeader, ABeyondCharacterBase* OldLeader);

private:
	void AddMappingContexts();
	void RefreshHUD();
	void Input_Swap();

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HUDWidget;
};
