// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/Layout/Anchors.h"
#include "BeyondPlayerController.generated.h"

class ABeyondCharacterBase;
class UBeyondBondMeterWidget;
class UBeyondDuoSkillTreeComponent;
class UBeyondPartyComponent;
class UBeyondSkillTreeAsset;
class UBeyondSkillTreeComponent;
struct FOnAttributeChangeData;
class UInputAction;
class UInputMappingContext;
class UUserWidget;

/**
 * Owns the party (character swapping), the input mapping contexts and the HUD.
 * The HUD is recreated whenever the controlled demigod changes, so widgets that read
 * GetOwningPlayerPawn on construct always show the current leader.
 * Also shows the Bond meter, the level / EXP display and the health bar of a nearby boss (characters with a Boss Bar
 * Widget Class). The Bond meter and the level display go into W_PlayerHud's canvas (see AttachToHUD).
 */
UCLASS()
class WORLDBEYOND_API ABeyondPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABeyondPlayerController();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Party")
	TObjectPtr<UBeyondPartyComponent> PartyComponent;

	// The duo skill tree (Bond Points, duo powers, duo loadout)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Party")
	TObjectPtr<UBeyondDuoSkillTreeComponent> DuoSkillTree;

	// Given to Duo Skill Tree on BeginPlay
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party|Skill Tree")
	TObjectPtr<UBeyondSkillTreeAsset> DuoSkillTreeAsset;

	// Opens / closes the skill tree screen (K)
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SkillTreeAction;

	// The skill tree screen; leave empty to disable it
	UPROPERTY(EditDefaultsOnly, Category = "UI|Skill Tree")
	TSubclassOf<UUserWidget> SkillTreeWidgetClass;

	// Pause the game while the skill tree is open
	UPROPERTY(EditDefaultsOnly, Category = "UI|Skill Tree")
	bool bPauseWhileSkillTreeOpen = true;

	// Opens the skill tree on a tab (0: the leader's tree; the duo tree is the last tab)
	UFUNCTION(BlueprintCallable, Category = "UI|Skill Tree")
	void OpenSkillTree(int32 Tab = -1);

	UFUNCTION(BlueprintCallable, Category = "UI|Skill Tree")
	void CloseSkillTree();

	UFUNCTION(BlueprintCallable, Category = "UI|Skill Tree")
	void ToggleSkillTree();

	UFUNCTION(BlueprintPure, Category = "UI|Skill Tree")
	bool IsSkillTreeOpen() const;

	UFUNCTION(BlueprintPure, Category = "UI|Skill Tree")
	UUserWidget* GetSkillTreeWidget() const { return SkillTreeWidget; }

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

	// Offset from the bottom centre of the screen (the arc meter's ends flank the ability bar)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Bond")
	FVector2D BondMeterOffset = FVector2D(0.0f, -50.0f);

	UFUNCTION(BlueprintPure, Category = "UI|Bond")
	UUserWidget* GetBondWidget() const { return BondWidget; }

	// Level badge, EXP bar and level-up banner (stretched over the HUD); leave empty to hide them
	UPROPERTY(EditDefaultsOnly, Category = "UI|Progression")
	TSubclassOf<UUserWidget> ProgressWidgetClass;

	UFUNCTION(BlueprintPure, Category = "UI|Progression")
	UUserWidget* GetProgressWidget() const { return ProgressWidget; }

	// Crosshair shown while the leader is a combat-ready caster (Angel with Aim Settings); leave empty to hide it
	UPROPERTY(EditDefaultsOnly, Category = "UI|Crosshair")
	TSubclassOf<UUserWidget> CrosshairWidgetClass;

	UFUNCTION(BlueprintPure, Category = "UI|Crosshair")
	UUserWidget* GetCrosshairWidget() const { return CrosshairWidget; }

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

	UFUNCTION()
	void HandleDuoTreeChanged(UBeyondSkillTreeComponent* Tree);

private:
	// Where AttachToHUD puts a widget in the HUD canvas (or the viewport without one)
	struct FHUDPlacement
	{
		FAnchors Anchors = FAnchors(0.5f, 1.0f);
		FVector2D Alignment = FVector2D(0.5f, 1.0f);
		FVector2D Offset = FVector2D::ZeroVector;
		// Stretch over the whole HUD instead of sizing to content at Offset
		bool bFillScreen = false;
		int32 ZOrder = 10;
		// Size on the viewport when the widget reports none
		FVector2D FallbackSize = FVector2D(800.0f, 320.0f);
	};

	/**
	 * Puts OwnWidget into the current HUD's root canvas. A widget of PlacedType already placed in the HUD in the
	 * designer wins (OwnWidget is then taken off screen); without a HUD canvas OwnWidget goes on the viewport.
	 * Returns the widget in use; Where says which of the three it was (for the log).
	 */
	UUserWidget* AttachToHUD(UUserWidget* OwnWidget, UClass* PlacedType, const FHUDPlacement& Placement, bool& bInViewport, const TCHAR*& Where);

	void AddMappingContexts();
	void RefreshHUD();
	void Input_Swap();

	void CreateBondMeter();
	// Puts the Bond meter in the HUD (or the viewport when there is no HUD canvas); called again whenever the HUD is rebuilt
	void AttachBondMeter();
	void CreateProgressWidget();
	void AttachProgressWidget();
	void CreateCrosshair();
	void UpdateCrosshair();
	void RefreshDuoIcon();
	void UpdateBossBar();
	void ShowBossBar(ABeyondCharacterBase* Boss);
	void HideBossBar();
	void RefreshBossHealth();
	void HandleBossHealthChanged(const FOnAttributeChangeData& ChangeData);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HUDWidget;

	// The Bond meter in use: ours, or one placed in the HUD in the designer
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> BondWidget;

	// The meter this controller created (kept across HUD rebuilds so its fill and animation carry on)
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> OwnBondWidget;

	bool bBondMeterCreated = false;
	bool bBondMeterInViewport = false;

	// The level display in use, and the one this controller created
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ProgressWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> OwnProgressWidget;

	bool bProgressWidgetCreated = false;
	bool bProgressWidgetInViewport = false;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> BossBarWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CrosshairWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SkillTreeWidget;

	bool bPausedBySkillTree = false;

	FTimerHandle CrosshairTimer;

	TWeakObjectPtr<ABeyondCharacterBase> ShownBoss;
	FDelegateHandle BossHealthHandle;
	FDelegateHandle BossMaxHealthHandle;
	FTimerHandle BossBarTimer;
	float BossDiedTime = -1.0f;
};
