// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/Layout/Anchors.h"
#include "BeyondPlayerController.generated.h"

class ABeyondCharacterBase;
class ABeyondNPCCharacter;
class UBeyondBanterComponent;
class UBeyondBossBarWidget;
class ABeyondLootDrop;
class ABeyondWaystone;
class UBeyondBondMeterWidget;
class UBeyondDuoSkillTreeComponent;
class UBeyondInventoryComponent;
class UBeyondPartyComponent;
class UBeyondSkillTreeAsset;
class UBeyondSkillTreeComponent;
struct FOnAttributeChangeData;
class UInputAction;
class UInputMappingContext;
class USoundBase;
class UUserWidget;

/**
 * Owns the party (character swapping), the input mapping contexts and the HUD.
 * The HUD is recreated whenever the controlled demigod changes, so widgets that read
 * GetOwningPlayerPawn on construct always show the current leader.
 * Also shows the Bond meter, the level / EXP display and the health bar of a nearby boss (characters with a Boss Bar
 * Widget Class). The Bond meter and the level display go into W_PlayerHud's canvas (see AttachToHUD).
 * Holds the party's bag (Inventory Component), picks loot up with F and opens the menus: the skill tree (K) and the
 * equipment & inventory screen (I), one at a time, game paused.
 * F also talks to the NPC in front of the leader (Plan 4: talking wins over loot); a prompt shows what F would do.
 * The Banter Component makes the demigods talk while you play.
 * Plan 5: F at a waystone attunes it / rests (after talking, before loot), M opens the world map (the third menu), and
 * the region banner shows place names and discovery toasts.
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

	// The party's shared bag
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Party")
	TObjectPtr<UBeyondInventoryComponent> InventoryComponent;

	// Angel and Ji-Woong's banter (Plan 4)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Party")
	TObjectPtr<UBeyondBanterComponent> BanterComponent;

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

	// Talks to the NPC in front of the leader, else picks up the nearest loot (F)
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	// Opens / closes the equipment & inventory screen (I)
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InventoryAction;

	// The equipment & inventory screen; leave empty to disable it
	UPROPERTY(EditDefaultsOnly, Category = "UI|Inventory")
	TSubclassOf<UUserWidget> InventoryWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Inventory")
	bool bPauseWhileInventoryOpen = true;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Inventory")
	TSoftObjectPtr<USoundBase> PickupSound;

	// Opens the screen on a demigod's tab (-1: the leader)
	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void OpenInventory(int32 Tab = -1);

	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void CloseInventory();

	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void ToggleInventory();

	UFUNCTION(BlueprintPure, Category = "UI|Inventory")
	bool IsInventoryOpen() const;

	UFUNCTION(BlueprintPure, Category = "UI|Inventory")
	UUserWidget* GetInventoryWidget() const { return InventoryWidget; }

	// Opens / closes the world map (M)
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> WorldMapAction;

	// The world map screen; leave empty to disable it
	UPROPERTY(EditDefaultsOnly, Category = "UI|World Map")
	TSubclassOf<UUserWidget> WorldMapWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI|World Map")
	bool bPauseWhileWorldMapOpen = true;

	UFUNCTION(BlueprintCallable, Category = "UI|World Map")
	void OpenWorldMap();

	UFUNCTION(BlueprintCallable, Category = "UI|World Map")
	void CloseWorldMap();

	UFUNCTION(BlueprintCallable, Category = "UI|World Map")
	void ToggleWorldMap();

	UFUNCTION(BlueprintPure, Category = "UI|World Map")
	bool IsWorldMapOpen() const;

	UFUNCTION(BlueprintPure, Category = "UI|World Map")
	UUserWidget* GetWorldMapWidget() const { return WorldMapWidget; }

	// The skill tree, the inventory or the world map is open
	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsAnyMenuOpen() const;

	// Region names and discovery toasts (Plan 5); leave empty to hide them
	UPROPERTY(EditDefaultsOnly, Category = "UI|World Map")
	TSubclassOf<UUserWidget> RegionBannerWidgetClass;

	UFUNCTION(BlueprintPure, Category = "UI|World Map")
	UUserWidget* GetRegionBannerWidget() const { return RegionBannerWidget; }

	// The waystone F would use (after an NPC to talk to, before loot)
	UFUNCTION(BlueprintPure, Category = "World")
	ABeyondWaystone* FindWaystoneTarget() const;

	// The drop F would pick up: the nearest one within the pickup range of the leader (Project Settings -> Worlds Beyond Loot)
	UFUNCTION(BlueprintPure, Category = "Loot")
	ABeyondLootDrop* FindPickupTarget() const;

	// Puts the nearest drop in the bag; false if there is none in range or the bag is full (the HUD says so)
	UFUNCTION(BlueprintCallable, Category = "Loot")
	bool PickUpNearestLoot();

	// A short line on the HUD ("Bag is full")
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowNotice(const FText& Text);

	// The NPC F would talk to: in Talk Range, in front of the leader, with something to say, and no fight nearby
	UFUNCTION(BlueprintPure, Category = "Dialogue")
	ABeyondNPCCharacter* FindTalkTarget() const;

	// Starts the conversation with NPC (null: the talk target); false if there is nobody to talk to
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	bool TalkTo(ABeyondNPCCharacter* NPC = nullptr);

	// An enemy near the leader is fighting (no talking then)
	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool IsFightNearby() const;

	// "[F] Talk - Elder Maren"; leave empty to hide it
	UPROPERTY(EditDefaultsOnly, Category = "UI|Prompt")
	TSubclassOf<UUserWidget> InteractPromptWidgetClass;

	UFUNCTION(BlueprintPure, Category = "UI|Prompt")
	UUserWidget* GetInteractPromptWidget() const { return InteractPromptWidget; }

	// Refreshes the prompt now (it also refreshes on a timer)
	void UpdateInteractPrompt();

	// Hide the HUD (health, ability bar, Bond meter, crosshair...) while talking face to face
	UPROPERTY(EditDefaultsOnly, Category = "UI|Dialogue")
	bool bHideHUDWhileTalking = true;

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

	// Health plates over enemies in a fight (Plan 3); leave empty to hide them
	UPROPERTY(EditDefaultsOnly, Category = "UI|Enemies")
	TSubclassOf<UUserWidget> EnemyPlatesWidgetClass;

	UFUNCTION(BlueprintPure, Category = "UI|Enemies")
	UUserWidget* GetEnemyPlatesWidget() const { return EnemyPlatesWidget; }

	// Seconds the boss bar stays up after the boss dies
	UPROPERTY(EditDefaultsOnly, Category = "UI|Boss", meta = (ClampMin = "0"))
	float BossBarLingerAfterDeath = 2.0f;

	// Bar for mini-bosses and bosses by rank (Plan 3); a Blueprint enemy's own Boss Bar Widget Class still wins for it
	UPROPERTY(EditDefaultsOnly, Category = "UI|Boss")
	TSubclassOf<UBeyondBossBarWidget> RankedBossBarClass;

	// The boss bar on screen (a Blueprint boss's widget or the C++ bar); null when none shows
	UFUNCTION(BlueprintPure, Category = "UI|Boss")
	UUserWidget* GetBossBarWidget() const;

	// The (nearest) boss whose bar shows
	UFUNCTION(BlueprintPure, Category = "UI|Boss")
	ABeyondCharacterBase* GetShownBoss() const;

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

	UFUNCTION()
	void HandleConversationStarted(AActor* Speaker, AActor* Partner, FName StartRow);

	UFUNCTION()
	void HandleConversationEnded(AActor* Speaker, AActor* Partner, FName StartRow);

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
	void Input_Interact();

	// Shows a menu screen (creating it the first time), closes any other menu, UI-only input, optional pause
	UUserWidget* OpenMenuWidget(TObjectPtr<UUserWidget>& Widget, const TSubclassOf<UUserWidget>& WidgetClass, bool bPause);
	void CloseMenuWidget(UUserWidget* Widget);

	void CreateBondMeter();
	// Puts the Bond meter in the HUD (or the viewport when there is no HUD canvas); called again whenever the HUD is rebuilt
	void AttachBondMeter();
	void CreateProgressWidget();
	void AttachProgressWidget();
	void CreateCrosshair();
	void UpdateCrosshair();
	void RefreshDuoIcon();
	void UpdateBossBar();
	void UpdateRankedBossBars();
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
	TObjectPtr<UUserWidget> EnemyPlatesWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SkillTreeWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> InventoryWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> InteractPromptWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> WorldMapWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> RegionBannerWidget;

	FTimerHandle PromptTimer;

	// Real time of the last F at a waystone (held or hammered F doesn't rest again and again)
	double LastWaystoneInteractTime = -1000.0;

	// Widgets hidden for a talk and the visibility they get back
	TArray<TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>> HiddenForTalk;

	// A menu paused the game (and unpauses it when it closes)
	bool bPausedByMenu = false;

	FTimerHandle CrosshairTimer;

	TWeakObjectPtr<ABeyondCharacterBase> ShownBoss;

	UPROPERTY(Transient)
	TObjectPtr<UBeyondBossBarWidget> RankedBossBar;

	TArray<TWeakObjectPtr<ABeyondCharacterBase>> RankedBosses;
	TMap<TWeakObjectPtr<ABeyondCharacterBase>, float> BossDeathTimes;
	FDelegateHandle BossHealthHandle;
	FDelegateHandle BossMaxHealthHandle;
	FTimerHandle BossBarTimer;
	float BossDiedTime = -1.0f;
};
