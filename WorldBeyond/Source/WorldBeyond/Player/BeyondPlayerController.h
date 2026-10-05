// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BeyondPlayerController.generated.h"

class ABeyondCharacterBase;
class UBeyondBondMeterWidget;
class UBeyondPartyComponent;
struct FOnAttributeChangeData;
class UInputAction;
class UInputMappingContext;
class UUserWidget;

/**
 * Owns the party (character swapping), the input mapping contexts and the HUD.
 * The HUD is recreated whenever the controlled demigod changes, so widgets that read
 * GetOwningPlayerPawn on construct always show the current leader.
 * Also shows the Bond meter and the health bar of a nearby boss (characters with a Boss Bar Widget Class).
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

	// Stop the level Blueprint from receiving input (it used to handle swapping with its own Possess logic)
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	bool bDisableLevelScriptInput = true;

	// Leave empty if the characters still create their own HUD
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	UFUNCTION(BlueprintPure, Category = "UI")
	UUserWidget* GetHUDWidget() const { return HUDWidget; }

	// Bond meter for the duo super move; leave empty to hide it
	UPROPERTY(EditDefaultsOnly, Category = "UI|Bond")
	TSubclassOf<UUserWidget> BondWidgetClass;

	// Offset from the bottom centre of the screen
	UPROPERTY(EditDefaultsOnly, Category = "UI|Bond")
	FVector2D BondMeterOffset = FVector2D(0.0f, -140.0f);

	// Seconds the boss bar stays up after the boss dies
	UPROPERTY(EditDefaultsOnly, Category = "UI|Boss", meta = (ClampMin = "0"))
	float BossBarLingerAfterDeath = 2.0f;

	UFUNCTION(BlueprintPure, Category = "UI|Boss")
	UUserWidget* GetBossBarWidget() const { return BossBarWidget; }

	UFUNCTION(BlueprintPure, Category = "UI|Boss")
	ABeyondCharacterBase* GetShownBoss() const { return ShownBoss.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

	UFUNCTION()
	void HandleLeaderChanged(ABeyondCharacterBase* NewLeader, ABeyondCharacterBase* OldLeader);

	UFUNCTION()
	void HandleBondChanged(float Bond, float MaxBond);

private:
	void AddMappingContexts();
	void RefreshHUD();
	void Input_Swap();

	void CreateBondMeter();
	void UpdateBossBar();
	void ShowBossBar(ABeyondCharacterBase* Boss);
	void HideBossBar();
	void RefreshBossHealth();
	void HandleBossHealthChanged(const FOnAttributeChangeData& ChangeData);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> BondWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> BossBarWidget;

	TWeakObjectPtr<ABeyondCharacterBase> ShownBoss;
	FDelegateHandle BossHealthHandle;
	FDelegateHandle BossMaxHealthHandle;
	FTimerHandle BossBarTimer;
	float BossDiedTime = -1.0f;
};
