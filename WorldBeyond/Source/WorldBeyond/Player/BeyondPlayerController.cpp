// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/BeyondPlayerController.h"
#include "WorldBeyond.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondAimComponent.h"
#include "Characters/BeyondCharacterBase.h"
#include "AI/BeyondEnemyController.h"
#include "Dialogue/BeyondBanterComponent.h"
#include "Dialogue/BeyondDialogueSettings.h"
#include "Dialogue/BeyondDialogueSubsystem.h"
#include "Dialogue/BeyondNPCCharacter.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Items/BeyondInventoryComponent.h"
#include "Items/BeyondItemLibrary.h"
#include "Items/BeyondLootDrop.h"
#include "Items/BeyondLootSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Player/BeyondPartyComponent.h"
#include "Progression/BeyondSkillTreeComponent.h"
#include "TimerManager.h"
#include "UI/BeyondBondMeterWidget.h"
#include "UI/BeyondCrosshairWidget.h"
#include "UI/BeyondBossBarWidget.h"
#include "UI/BeyondEnemyPlatesWidget.h"
#include "Enemies/BeyondBossCharacter.h"
#include "Enemies/BeyondBossDefinition.h"
#include "UI/BeyondInteractPromptWidget.h"
#include "UI/BeyondInventoryWidget.h"
#include "UI/BeyondProgressWidget.h"
#include "UI/BeyondRegionBannerWidget.h"
#include "UI/BeyondSkillTreeWidget.h"
#include "UI/BeyondWorldMapWidget.h"
#include "World/BeyondWaystone.h"
#include "World/BeyondWorldSubsystem.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "BeyondPlayerController"

namespace
{
	/**
	 * Calls a Blueprint widget function taking a value and its maximum, such as W_BossHealthBar's
	 * UpdateHealthPercentage(Health, MaxHealth). A float parameter named "Max..." gets MaxValue; without such
	 * names the first float gets Value and the second MaxValue.
	 */
	void CallValueFunction(UObject* Widget, FName FunctionName, float Value, float MaxValue)
	{
		UFunction* Function = Widget ? Widget->FindFunction(FunctionName) : nullptr;
		if (!Function)
		{
			return;
		}

		uint8* Params = static_cast<uint8*>(FMemory_Alloca_Aligned(FMath::Max<int32>(Function->ParmsSize, 1), Function->GetMinAlignment()));
		FMemory::Memzero(Params, Function->ParmsSize);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params);
		}

		TArray<FNumericProperty*> Floats;
		bool bNamedMax = false;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FNumericProperty* Numeric = CastField<FNumericProperty>(*It);
			if (Numeric && Numeric->IsFloatingPoint() && !It->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
			{
				Floats.Add(Numeric);
				bNamedMax |= Numeric->GetAuthoredName().Contains(TEXT("Max"));
			}
		}
		for (int32 Index = 0; Index < Floats.Num(); ++Index)
		{
			const bool bIsMax = bNamedMax ? Floats[Index]->GetAuthoredName().Contains(TEXT("Max")) : Index == 1;
			Floats[Index]->SetFloatingPointPropertyValue(Floats[Index]->ContainerPtrToValuePtr<void>(Params), bIsMax ? MaxValue : Value);
		}

		Widget->ProcessEvent(Function, Params);

		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params);
		}
	}
}

ABeyondPlayerController::ABeyondPlayerController()
{
	PartyComponent = CreateDefaultSubobject<UBeyondPartyComponent>(TEXT("PartyComponent"));
	DuoSkillTree = CreateDefaultSubobject<UBeyondDuoSkillTreeComponent>(TEXT("DuoSkillTree"));
	InventoryComponent = CreateDefaultSubobject<UBeyondInventoryComponent>(TEXT("Inventory"));
	BanterComponent = CreateDefaultSubobject<UBeyondBanterComponent>(TEXT("Banter"));
	InteractPromptWidgetClass = UBeyondInteractPromptWidget::StaticClass();
	SkillTreeWidgetClass = UBeyondSkillTreeWidget::StaticClass();
	InventoryWidgetClass = UBeyondInventoryWidget::StaticClass();
	WorldMapWidgetClass = UBeyondWorldMapWidget::StaticClass();
	RegionBannerWidgetClass = UBeyondRegionBannerWidget::StaticClass();
	PickupSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/energy-charge-up.energy-charge-up")));
	BondWidgetClass = UBeyondBondMeterWidget::StaticClass();
	ProgressWidgetClass = UBeyondProgressWidget::StaticClass();
	CrosshairWidgetClass = UBeyondCrosshairWidget::StaticClass();
	EnemyPlatesWidgetClass = UBeyondEnemyPlatesWidget::StaticClass();
	RankedBossBarClass = UBeyondBossBarWidget::StaticClass();
}

void ABeyondPlayerController::BeginPlay()
{
	Super::BeginPlay();

	PartyComponent->OnLeaderChanged.AddUniqueDynamic(this, &ThisClass::HandleLeaderChanged);
	AddMappingContexts();

	// Before the party forms: loading the save restores the duo tree's ranks against this asset
	if (DuoSkillTreeAsset)
	{
		DuoSkillTree->SetTree(DuoSkillTreeAsset);
	}
	DuoSkillTree->OnSkillTreeChanged.AddUniqueDynamic(this, &ThisClass::HandleDuoTreeChanged);

	// Possession can happen before BeginPlay; initialize the party with whatever we already control
	if (GetPawn())
	{
		PartyComponent->InitializeParty(GetPawn());
	}

	// Characters may still create their own HUD in their BeginPlay; replace it once everything has started
	FTimerHandle RefreshTimer;
	GetWorldTimerManager().SetTimer(RefreshTimer, this, &ThisClass::RefreshHUD, 0.2f, false);

	if (IsLocalController())
	{
		CreateBondMeter();
		CreateProgressWidget();
		CreateCrosshair();
		if (EnemyPlatesWidgetClass)
		{
			// Under the HUD's own widgets, full screen
			EnemyPlatesWidget = CreateWidget<UUserWidget>(this, EnemyPlatesWidgetClass);
			if (EnemyPlatesWidget)
			{
				EnemyPlatesWidget->AddToViewport(1);
			}
		}
		GetWorldTimerManager().SetTimer(BossBarTimer, this, &ThisClass::UpdateBossBar, 0.25f, true, 0.5f);

		if (InteractPromptWidgetClass)
		{
			InteractPromptWidget = CreateWidget<UUserWidget>(this, InteractPromptWidgetClass);
			if (InteractPromptWidget)
			{
				// Full screen; the prompt draws itself low in the middle
				InteractPromptWidget->AddToViewport(3);
			}
			GetWorldTimerManager().SetTimer(PromptTimer, this, &ThisClass::UpdateInteractPrompt, 0.15f, true, 0.3f);
		}
		if (RegionBannerWidgetClass)
		{
			RegionBannerWidget = CreateWidget<UUserWidget>(this, RegionBannerWidgetClass);
			if (RegionBannerWidget)
			{
				// Full screen; the banner draws itself high in the middle, toasts on the left
				RegionBannerWidget->AddToViewport(4);
			}
		}
		if (UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this))
		{
			Dialogue->OnConversationStarted.AddUniqueDynamic(this, &ThisClass::HandleConversationStarted);
			Dialogue->OnConversationEnded.AddUniqueDynamic(this, &ThisClass::HandleConversationEnded);
		}
	}

	if (bDisableLevelScriptInput)
	{
		if (ALevelScriptActor* LevelScript = GetWorld()->GetLevelScriptActor())
		{
			LevelScript->DisableInput(this);
		}
	}
}

void ABeyondPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (HasActorBegunPlay())
	{
		PartyComponent->InitializeParty(InPawn);
	}
}

void ABeyondPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (SwapAction)
		{
			EnhancedInput->BindAction(SwapAction.Get(), ETriggerEvent::Started, this, &ThisClass::Input_Swap);
		}
		if (SkillTreeAction)
		{
			EnhancedInput->BindAction(SkillTreeAction.Get(), ETriggerEvent::Started, this, &ThisClass::ToggleSkillTree);
		}
		if (InteractAction)
		{
			EnhancedInput->BindAction(InteractAction.Get(), ETriggerEvent::Started, this, &ThisClass::Input_Interact);
		}
		if (InventoryAction)
		{
			EnhancedInput->BindAction(InventoryAction.Get(), ETriggerEvent::Started, this, &ThisClass::ToggleInventory);
		}
		if (WorldMapAction)
		{
			EnhancedInput->BindAction(WorldMapAction.Get(), ETriggerEvent::Started, this, &ThisClass::ToggleWorldMap);
		}
	}
}

void ABeyondPlayerController::AddMappingContexts()
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (const UInputMappingContext* Context : DefaultMappingContexts)
		{
			if (Context)
			{
				Subsystem->AddMappingContext(Context, 0);
			}
		}
	}
}

void ABeyondPlayerController::Input_Swap()
{
	PartyComponent->SwapLeader();
}

void ABeyondPlayerController::Input_Interact()
{
	// Talking wins over a waystone, a waystone over loot at its foot
	if (!TalkTo())
	{
		ABeyondWaystone* Waystone = FindWaystoneTarget();
		UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this);
		if (!Waystone || !WorldSubsystem || !WorldSubsystem->UseWaystone(Waystone))
		{
			PickUpNearestLoot();
		}
	}
	UpdateInteractPrompt();
}

bool ABeyondPlayerController::IsFightNearby() const
{
	const APawn* Leader = GetPawn();
	const UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(this);
	if (!Leader || !Enemies)
	{
		return false;
	}
	const float RadiusSq = FMath::Square(GetDefault<UBeyondDialogueSettings>()->CombatRadius);
	for (const ABeyondEnemyCharacter* Enemy : Enemies->GetLiveEnemies())
	{
		const ABeyondEnemyController* Brain = Enemy ? Cast<ABeyondEnemyController>(Enemy->GetController()) : nullptr;
		if (Brain && Brain->GetAIState() == EBeyondEnemyAIState::Combat
			&& FVector::DistSquared(Enemy->GetActorLocation(), Leader->GetActorLocation()) <= RadiusSq)
		{
			return true;
		}
	}
	return false;
}

ABeyondNPCCharacter* ABeyondPlayerController::FindTalkTarget() const
{
	const APawn* Leader = GetPawn();
	const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	if (!Leader || !Dialogue || Dialogue->IsTalking() || UBeyondCombatLibrary::IsActorDead(Leader) || IsFightNearby())
	{
		return nullptr;
	}

	const UBeyondDialogueSettings* Settings = GetDefault<UBeyondDialogueSettings>();
	const FVector From = Leader->GetActorLocation();
	const FVector Facing = Leader->GetActorForwardVector().GetSafeNormal2D();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(Settings->TalkAngle));
	const float LeaderRadius = Leader->GetSimpleCollisionRadius();

	ABeyondNPCCharacter* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<ABeyondNPCCharacter> It(GetWorld()); It; ++It)
	{
		ABeyondNPCCharacter* NPC = *It;
		const FVector ToNPC = (NPC->GetActorLocation() - From) * FVector(1.0f, 1.0f, 0.0f);
		const float Distance = ToNPC.Size() - LeaderRadius - NPC->GetSimpleCollisionRadius();
		if (Distance > Settings->TalkRange || FMath::Abs(NPC->GetActorLocation().Z - From.Z) > 200.0f)
		{
			continue;
		}
		// Right next to it, any side will do
		if (Distance > 40.0f && FVector::DotProduct(Facing, ToNPC.GetSafeNormal()) < MinDot)
		{
			continue;
		}
		if (Distance < BestDistance && NPC->CanTalkNow())
		{
			Best = NPC;
			BestDistance = Distance;
		}
	}
	return Best;
}

bool ABeyondPlayerController::TalkTo(ABeyondNPCCharacter* NPC)
{
	if (!NPC)
	{
		NPC = FindTalkTarget();
	}
	ACharacter* Leader = Cast<ACharacter>(GetPawn());
	if (!NPC || !Leader || IsAnyMenuOpen())
	{
		return false;
	}
	return NPC->TalkTo(Leader);
}

void ABeyondPlayerController::HandleConversationStarted(AActor* Speaker, AActor* Partner, FName StartRow)
{
	const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	if (!bHideHUDWhileTalking || !Dialogue || !Dialogue->IsTalking())
	{
		return;
	}
	UpdateInteractPrompt();
	for (UUserWidget* Widget : { HUDWidget.Get(), BondWidget.Get(), ProgressWidget.Get(), CrosshairWidget.Get(), EnemyPlatesWidget.Get(), BossBarWidget.Get() })
	{
		const bool bAlreadyHidden = HiddenForTalk.ContainsByPredicate([Widget](const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Entry)
		{
			return Entry.Key.Get() == Widget;
		});
		if (Widget && !bAlreadyHidden && Widget->GetVisibility() != ESlateVisibility::Collapsed && Widget->GetVisibility() != ESlateVisibility::Hidden)
		{
			HiddenForTalk.Emplace(Widget, Widget->GetVisibility());
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void ABeyondPlayerController::HandleConversationEnded(AActor* Speaker, AActor* Partner, FName StartRow)
{
	for (const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Entry : HiddenForTalk)
	{
		if (UUserWidget* Widget = Entry.Key.Get())
		{
			Widget->SetVisibility(Entry.Value);
		}
	}
	HiddenForTalk.Reset();
	UpdateCrosshair();
}

void ABeyondPlayerController::UpdateInteractPrompt()
{
	UBeyondInteractPromptWidget* Prompt = Cast<UBeyondInteractPromptWidget>(InteractPromptWidget);
	if (!Prompt)
	{
		return;
	}

	const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	if ((Dialogue && Dialogue->IsTalking()) || IsAnyMenuOpen())
	{
		Prompt->SetPrompt(FText::GetEmpty(), FText::GetEmpty());
		return;
	}
	if (const ABeyondNPCCharacter* NPC = FindTalkTarget())
	{
		Prompt->SetPrompt(LOCTEXT("PromptTalk", "Talk"), NPC->GetNPCName());
		return;
	}
	if (const ABeyondWaystone* Waystone = FindWaystoneTarget())
	{
		const UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this);
		const bool bAttuned = WorldSubsystem && WorldSubsystem->IsWaystoneAttuned(Waystone);
		Prompt->SetPrompt(bAttuned ? LOCTEXT("PromptRest", "Rest") : LOCTEXT("PromptAttune", "Attune"), Waystone->GetDisplayNameOrId());
		return;
	}
	if (FindPickupTarget())
	{
		Prompt->SetPrompt(LOCTEXT("PromptPickUp", "Pick up"), FText::GetEmpty());
		return;
	}
	Prompt->SetPrompt(FText::GetEmpty(), FText::GetEmpty());
}

ABeyondLootDrop* ABeyondPlayerController::FindPickupTarget() const
{
	const APawn* Leader = GetPawn();
	const UBeyondLootSubsystem* Loot = UBeyondLootSubsystem::Get(this);
	if (!Leader || !Loot || UBeyondCombatLibrary::IsActorDead(Leader))
	{
		return nullptr;
	}
	return Loot->FindNearestDrop(Leader->GetActorLocation(), GetDefault<UBeyondLootSettings>()->PickupRange);
}

bool ABeyondPlayerController::PickUpNearestLoot()
{
	ABeyondLootDrop* Drop = FindPickupTarget();
	if (!Drop)
	{
		return false;
	}
	if (InventoryComponent->IsFull())
	{
		ShowNotice(LOCTEXT("BagFull", "Your bag is full: press I and discard something"));
		return false;
	}
	if (!InventoryComponent->AddItem(Drop->GetItem()))
	{
		return false;
	}
	if (USoundBase* Sound = PickupSound.IsNull() ? nullptr : PickupSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySound2D(this, Sound, 0.6f, 1.25f);
	}
	Drop->Destroy();
	return true;
}

void ABeyondPlayerController::ShowNotice(const FText& Text)
{
	if (UBeyondProgressWidget* Progress = Cast<UBeyondProgressWidget>(ProgressWidget))
	{
		Progress->ShowNotice(Text);
	}
	UE_LOG(LogBeyond, Log, TEXT("Notice: %s"), *Text.ToString());
}

void ABeyondPlayerController::HandleLeaderChanged(ABeyondCharacterBase* NewLeader, ABeyondCharacterBase* OldLeader)
{
	RefreshHUD();
	RefreshDuoIcon();
}

void ABeyondPlayerController::RefreshHUD()
{
	if (!HUDWidgetClass || !IsLocalController())
	{
		return;
	}

	// Also removes a HUD a character Blueprint created itself, so there is only ever one
	TArray<UUserWidget*> ExistingHUDs;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, ExistingHUDs, HUDWidgetClass, false);
	for (UUserWidget* Existing : ExistingHUDs)
	{
		Existing->RemoveFromParent();
	}
	HUDWidget = nullptr;

	HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport();

		// The ability bar (W_AbilitesBar) only fills on Event.Abilities.Changed, which fired long before this
		// HUD existed; its wait is armed during AddToViewport, so send the event again now
		if (ABeyondCharacterBase* Leader = Cast<ABeyondCharacterBase>(GetPawn()))
		{
			Leader->SendAbilitiesChangedEvent();
		}
	}

	// The Bond meter and the level display go into the new HUD
	AttachBondMeter();
	AttachProgressWidget();
}

UUserWidget* ABeyondPlayerController::AttachToHUD(UUserWidget* OwnWidget, UClass* PlacedType, const FHUDPlacement& Placement, bool& bInViewport, const TCHAR*& Where)
{
	// A widget of this type placed in the HUD in the designer wins
	UUserWidget* Placed = nullptr;
	if (PlacedType && HUDWidget && HUDWidget->WidgetTree)
	{
		HUDWidget->WidgetTree->ForEachWidget([&Placed, PlacedType, OwnWidget](UWidget* Widget)
		{
			if (!Placed && Widget && Widget != OwnWidget && Widget->IsA(PlacedType))
			{
				Placed = Cast<UUserWidget>(Widget);
			}
		});
	}

	if (Placed)
	{
		if (OwnWidget)
		{
			OwnWidget->RemoveFromParent();
			bInViewport = false;
		}
		Where = TEXT("placed in the designer");
		return Placed;
	}
	if (!OwnWidget)
	{
		return nullptr;
	}

	if (UCanvasPanel* HUDCanvas = HUDWidget ? Cast<UCanvasPanel>(HUDWidget->GetRootWidget()) : nullptr)
	{
		// A child of the HUD's canvas: ordinary UMG layout
		OwnWidget->RemoveFromParent();
		bInViewport = false;
		if (UCanvasPanelSlot* CanvasSlot = HUDCanvas->AddChildToCanvas(OwnWidget))
		{
			if (Placement.bFillScreen)
			{
				CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				CanvasSlot->SetAlignment(FVector2D::ZeroVector);
				CanvasSlot->SetOffsets(FMargin(0.0f));
			}
			else
			{
				CanvasSlot->SetAnchors(Placement.Anchors);
				CanvasSlot->SetAlignment(Placement.Alignment);
				CanvasSlot->SetAutoSize(true);
				CanvasSlot->SetPosition(Placement.Offset);
			}
			CanvasSlot->SetZOrder(Placement.ZOrder);
		}
		Where = TEXT("added by code");
		return OwnWidget;
	}

	Where = TEXT("viewport");
	if (!bInViewport)
	{
		OwnWidget->RemoveFromParent();
		OwnWidget->AddToViewport(1);
		if (!Placement.bFillScreen)
		{
			// A point anchor needs an explicit size on the viewport (UE5 reads the desired size once, before Slate
			// has measured the widget), so measure it first
			OwnWidget->SetAnchorsInViewport(Placement.Anchors);
			OwnWidget->SetAlignmentInViewport(Placement.Alignment);
			OwnWidget->ForceLayoutPrepass();
			FVector2D WidgetSize = OwnWidget->GetDesiredSize();
			if (WidgetSize.X < 1.0 || WidgetSize.Y < 1.0)
			{
				WidgetSize = Placement.FallbackSize;
			}
			OwnWidget->SetDesiredSizeInViewport(WidgetSize);
			OwnWidget->SetPositionInViewport(Placement.Offset, false);
		}
		bInViewport = true;
	}
	return OwnWidget;
}

void ABeyondPlayerController::CreateProgressWidget()
{
	if (bProgressWidgetCreated)
	{
		return;
	}
	bProgressWidgetCreated = true;

	// Like the Bond meter: into the HUD once RefreshHUD made it, straight away when there is no HUD
	if (!HUDWidgetClass || HUDWidget)
	{
		AttachProgressWidget();
	}
}

void ABeyondPlayerController::AttachProgressWidget()
{
	if (!bProgressWidgetCreated || !IsLocalController() || !ProgressWidgetClass)
	{
		return;
	}

	if (!OwnProgressWidget)
	{
		OwnProgressWidget = CreateWidget<UUserWidget>(this, ProgressWidgetClass);
	}

	FHUDPlacement Placement;
	Placement.bFillScreen = true;
	Placement.ZOrder = 9;
	const TCHAR* Where = TEXT("");
	ProgressWidget = AttachToHUD(OwnProgressWidget, UBeyondProgressWidget::StaticClass(), Placement, bProgressWidgetInViewport, Where);
	if (ProgressWidget)
	{
		UE_LOG(LogBeyond, Log, TEXT("Level display: %s in %s (%s)"), *GetNameSafe(ProgressWidget->GetClass()),
			HUDWidget ? *GetNameSafe(HUDWidget->GetClass()) : TEXT("the viewport"), Where);
	}
}

UUserWidget* ABeyondPlayerController::OpenMenuWidget(TObjectPtr<UUserWidget>& Widget, const TSubclassOf<UUserWidget>& WidgetClass, bool bPause)
{
	if (!IsLocalController() || !WidgetClass)
	{
		return nullptr;
	}
	if (!Widget)
	{
		Widget = CreateWidget<UUserWidget>(this, WidgetClass);
	}
	if (!Widget)
	{
		return nullptr;
	}

	// One menu at a time: the other one just goes away (input mode and pause carry over)
	for (UUserWidget* Other : { SkillTreeWidget.Get(), InventoryWidget.Get(), WorldMapWidget.Get() })
	{
		if (Other && Other != Widget && Other->IsInViewport())
		{
			Other->RemoveFromParent();
		}
	}
	if (!Widget->IsInViewport())
	{
		Widget->AddToViewport(50);
	}

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);

	if (bPause && !IsPaused())
	{
		bPausedByMenu = SetPause(true);
	}
	else if (!bPause && bPausedByMenu)
	{
		SetPause(false);
		bPausedByMenu = false;
	}
	return Widget;
}

void ABeyondPlayerController::CloseMenuWidget(UUserWidget* Widget)
{
	if (Widget)
	{
		Widget->RemoveFromParent();
	}
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
	if (bPausedByMenu)
	{
		SetPause(false);
		bPausedByMenu = false;
	}
}

void ABeyondPlayerController::OpenSkillTree(int32 Tab)
{
	UBeyondSkillTreeWidget* Tree = Cast<UBeyondSkillTreeWidget>(OpenMenuWidget(SkillTreeWidget, SkillTreeWidgetClass, bPauseWhileSkillTreeOpen));
	if (Tree)
	{
		Tree->RefreshTabs();
		Tree->SelectTab(Tab >= 0 ? Tab : Tree->FindTabFor(GetPawn()));
	}
}

void ABeyondPlayerController::CloseSkillTree()
{
	CloseMenuWidget(SkillTreeWidget);
}

void ABeyondPlayerController::ToggleSkillTree()
{
	if (IsSkillTreeOpen())
	{
		CloseSkillTree();
	}
	else
	{
		OpenSkillTree();
	}
}

bool ABeyondPlayerController::IsSkillTreeOpen() const
{
	return SkillTreeWidget && SkillTreeWidget->IsInViewport();
}

void ABeyondPlayerController::OpenInventory(int32 Tab)
{
	UBeyondInventoryWidget* Screen = Cast<UBeyondInventoryWidget>(OpenMenuWidget(InventoryWidget, InventoryWidgetClass, bPauseWhileInventoryOpen));
	if (Screen)
	{
		Screen->RefreshTabs();
		Screen->SelectTab(Tab >= 0 ? Tab : Screen->FindTabFor(GetPawn()));
	}
}

void ABeyondPlayerController::CloseInventory()
{
	CloseMenuWidget(InventoryWidget);
}

void ABeyondPlayerController::ToggleInventory()
{
	if (IsInventoryOpen())
	{
		CloseInventory();
	}
	else
	{
		OpenInventory();
	}
}

bool ABeyondPlayerController::IsInventoryOpen() const
{
	return InventoryWidget && InventoryWidget->IsInViewport();
}

void ABeyondPlayerController::OpenWorldMap()
{
	if (UBeyondWorldMapWidget* Map = Cast<UBeyondWorldMapWidget>(OpenMenuWidget(WorldMapWidget, WorldMapWidgetClass, bPauseWhileWorldMapOpen)))
	{
		Map->Refresh();
	}
}

void ABeyondPlayerController::CloseWorldMap()
{
	CloseMenuWidget(WorldMapWidget);
}

void ABeyondPlayerController::ToggleWorldMap()
{
	if (IsWorldMapOpen())
	{
		CloseWorldMap();
	}
	else
	{
		OpenWorldMap();
	}
}

bool ABeyondPlayerController::IsWorldMapOpen() const
{
	return WorldMapWidget && WorldMapWidget->IsInViewport();
}

bool ABeyondPlayerController::IsAnyMenuOpen() const
{
	return IsSkillTreeOpen() || IsInventoryOpen() || IsWorldMapOpen();
}

ABeyondWaystone* ABeyondPlayerController::FindWaystoneTarget() const
{
	const UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this);
	const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	if (!WorldSubsystem || (Dialogue && Dialogue->IsTalking()) || IsFightNearby())
	{
		return nullptr;
	}
	return WorldSubsystem->FindWaystoneInReach();
}

void ABeyondPlayerController::HandleDuoTreeChanged(UBeyondSkillTreeComponent* Tree)
{
	RefreshDuoIcon();
}

void ABeyondPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideBossBar();
	Super::EndPlay(EndPlayReason);
}

void ABeyondPlayerController::CreateBondMeter()
{
	if (bBondMeterCreated)
	{
		return;
	}
	bBondMeterCreated = true;
	PartyComponent->OnBondChanged.AddUniqueDynamic(this, &ThisClass::HandleBondChanged);

	// The meter lives in the HUD, which RefreshHUD creates a moment later; without a HUD it goes on the viewport now
	if (!HUDWidgetClass || HUDWidget)
	{
		AttachBondMeter();
	}
}

void ABeyondPlayerController::AttachBondMeter()
{
	if (!bBondMeterCreated || !IsLocalController())
	{
		return;
	}

	if (!OwnBondWidget && BondWidgetClass)
	{
		OwnBondWidget = CreateWidget<UUserWidget>(this, BondWidgetClass);
	}

	// Anchored at the bottom centre like the ability bar
	FHUDPlacement Placement;
	Placement.Anchors = FAnchors(0.5f, 1.0f);
	Placement.Alignment = FVector2D(0.5f, 1.0f);
	Placement.Offset = BondMeterOffset;
	Placement.ZOrder = 10;
	const TCHAR* Where = TEXT("");
	BondWidget = AttachToHUD(OwnBondWidget, UBeyondBondMeterWidget::StaticClass(), Placement, bBondMeterInViewport, Where);
	if (!BondWidget)
	{
		return;
	}

	UE_LOG(LogBeyond, Log, TEXT("Bond meter: %s in %s (%s), offset (%.0f, %.0f) from the bottom centre"),
		*GetNameSafe(BondWidget->GetClass()), HUDWidget ? *GetNameSafe(HUDWidget->GetClass()) : TEXT("the viewport"), Where,
		BondMeterOffset.X, BondMeterOffset.Y);

	HandleBondChanged(PartyComponent->GetBond(), PartyComponent->MaxBond);
	RefreshDuoIcon();
}

void ABeyondPlayerController::CreateCrosshair()
{
	if (!CrosshairWidgetClass || CrosshairWidget)
	{
		return;
	}

	CrosshairWidget = CreateWidget<UUserWidget>(this, CrosshairWidgetClass);
	if (!CrosshairWidget)
	{
		return;
	}
	// Full screen; the crosshair draws itself at the centre
	CrosshairWidget->AddToViewport(2);
	CrosshairWidget->SetVisibility(ESlateVisibility::Collapsed);
	GetWorldTimerManager().SetTimer(CrosshairTimer, this, &ThisClass::UpdateCrosshair, 0.05f, true);
}

void ABeyondPlayerController::UpdateCrosshair()
{
	if (!CrosshairWidget)
	{
		return;
	}

	const ABeyondCharacterBase* Leader = Cast<ABeyondCharacterBase>(GetPawn());
	const UBeyondAimComponent* Aim = Leader ? Leader->GetAimComponent() : nullptr;
	const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	const bool bTalking = bHideHUDWhileTalking && Dialogue && Dialogue->IsTalking();
	const bool bShow = Aim && Aim->ShouldShowCrosshair() && !bTalking;
	const ESlateVisibility Wanted = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (CrosshairWidget->GetVisibility() != Wanted)
	{
		CrosshairWidget->SetVisibility(Wanted);
	}

	if (bShow)
	{
		if (UBeyondCrosshairWidget* Crosshair = Cast<UBeyondCrosshairWidget>(CrosshairWidget))
		{
			const UCharacterMovementComponent* Movement = Leader->GetCharacterMovement();
			const float MaxSpeed = Movement ? FMath::Max(Movement->GetMaxSpeed(), 1.0f) : 600.0f;
			Crosshair->SetAimState(Aim->GetAimTarget() != nullptr, Aim->IsAiming(), Leader->GetVelocity().Size2D() / MaxSpeed);
		}
	}
}

void ABeyondPlayerController::RefreshDuoIcon()
{
	UBeyondBondMeterWidget* Meter = Cast<UBeyondBondMeterWidget>(BondWidget);
	const ABeyondCharacterBase* Leader = PartyComponent->GetLeader();
	const UAbilitySystemComponent* ASC = Leader ? Leader->GetAbilitySystemComponent() : nullptr;
	if (!Meter || !ASC)
	{
		return;
	}

	// The icon of whatever sits on the duo slot (the duo loadout; same rule as the G key)
	const UBeyondGameplayAbility* Ability = Cast<UBeyondGameplayAbility>(Leader->FindAbilityOnInput(BeyondTags::Ability_Input_Duo));
	Meter->SetDuoIcon(Ability ? Ability->Icon.Get() : nullptr);
}

void ABeyondPlayerController::HandleBondChanged(float Bond, float MaxBond)
{
	if (UBeyondBondMeterWidget* Meter = Cast<UBeyondBondMeterWidget>(BondWidget))
	{
		Meter->SetBond(Bond, MaxBond);
	}
	else if (BondWidget)
	{
		// A Widget Blueprint with its own SetBond(Bond, MaxBond) function
		CallValueFunction(BondWidget, TEXT("SetBond"), Bond, MaxBond);
	}
}

UUserWidget* ABeyondPlayerController::GetBossBarWidget() const
{
	if (BossBarWidget)
	{
		return BossBarWidget;
	}
	return RankedBossBar && !RankedBosses.IsEmpty() ? RankedBossBar.Get() : nullptr;
}

ABeyondCharacterBase* ABeyondPlayerController::GetShownBoss() const
{
	if (ABeyondCharacterBase* Legacy = ShownBoss.Get())
	{
		return Legacy;
	}
	return RankedBosses.IsEmpty() ? nullptr : RankedBosses[0].Get();
}

void ABeyondPlayerController::UpdateRankedBossBars()
{
	const ABeyondCharacterBase* Leader = PartyComponent->GetLeader();
	const float Now = GetWorld()->GetTimeSeconds();

	// Mini-bosses and bosses by rank near the leader (two at most, nearest first); the dead linger a moment
	TArray<TPair<float, ABeyondCharacterBase*>> Candidates;
	for (TActorIterator<ABeyondCharacterBase> It(GetWorld()); Leader && It; ++It)
	{
		ABeyondCharacterBase* Candidate = *It;
		if (Candidate->BossBarWidgetClass || !UBeyondCombatLibrary::IsBoss(Candidate))
		{
			continue;
		}
		if (UBeyondCombatLibrary::IsActorDead(Candidate))
		{
			const float* Died = BossDeathTimes.Find(Candidate);
			if (!Died)
			{
				// Only bosses whose bar was up linger
				if (!RankedBosses.Contains(Candidate))
				{
					continue;
				}
				Died = &BossDeathTimes.Add(Candidate, Now);
			}
			if (Now - *Died > BossBarLingerAfterDeath)
			{
				continue;
			}
		}
		const ABeyondBossCharacter* BossCharacter = Cast<ABeyondBossCharacter>(Candidate);
		const UBeyondBossDefinition* Definition = BossCharacter ? BossCharacter->GetBossDefinition() : nullptr;
		const float Radius = Definition ? Definition->BarShowRadius : Candidate->BossBarShowRadius;
		const float Distance = FVector::Dist(Leader->GetActorLocation(), Candidate->GetActorLocation());
		// A bar already up stays until the party is well away
		if (Distance <= (RankedBosses.Contains(Candidate) ? Radius * 1.5f : Radius))
		{
			Candidates.Add(TPair<float, ABeyondCharacterBase*>(Distance, Candidate));
		}
	}
	Candidates.Sort([](const TPair<float, ABeyondCharacterBase*>& A, const TPair<float, ABeyondCharacterBase*>& B) { return A.Key < B.Key; });

	TArray<ABeyondCharacterBase*> Shown;
	RankedBosses.Reset();
	for (const TPair<float, ABeyondCharacterBase*>& Candidate : Candidates)
	{
		if (Shown.Num() < 2)
		{
			Shown.Add(Candidate.Value);
			RankedBosses.Add(Candidate.Value);
		}
	}

	if (Shown.IsEmpty())
	{
		if (RankedBossBar)
		{
			RankedBossBar->SetBosses({});
			RankedBossBar->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	if (!RankedBossBar && RankedBossBarClass)
	{
		RankedBossBar = CreateWidget<UBeyondBossBarWidget>(this, RankedBossBarClass);
		if (RankedBossBar)
		{
			RankedBossBar->AddToViewport(3);
		}
	}
	if (RankedBossBar)
	{
		RankedBossBar->SetBosses(Shown);
		RankedBossBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void ABeyondPlayerController::UpdateBossBar()
{
	UpdateRankedBossBars();

	const ABeyondCharacterBase* Leader = PartyComponent->GetLeader();
	const float Now = GetWorld()->GetTimeSeconds();

	if (ABeyondCharacterBase* Boss = ShownBoss.Get())
	{
		const bool bDead = UBeyondCombatLibrary::IsActorDead(Boss);
		if (bDead && BossDiedTime < 0.0f)
		{
			BossDiedTime = Now;
			RefreshBossHealth();
		}

		const bool bLingerOver = bDead && Now - BossDiedTime >= BossBarLingerAfterDeath;
		const bool bTooFar = Leader && FVector::Dist(Leader->GetActorLocation(), Boss->GetActorLocation()) > Boss->BossBarShowRadius * 1.5f;
		if (bLingerOver || (!bDead && bTooFar))
		{
			HideBossBar();
		}
		return;
	}

	if (!Leader)
	{
		return;
	}

	// Closest living boss in range of the demigod the player controls
	ABeyondCharacterBase* Closest = nullptr;
	float ClosestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<ABeyondCharacterBase> It(GetWorld()); It; ++It)
	{
		ABeyondCharacterBase* Candidate = *It;
		if (!Candidate->BossBarWidgetClass || UBeyondCombatLibrary::IsActorDead(Candidate))
		{
			continue;
		}
		const float Distance = FVector::Dist(Leader->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance <= Candidate->BossBarShowRadius && Distance < ClosestDistance)
		{
			Closest = Candidate;
			ClosestDistance = Distance;
		}
	}

	if (Closest)
	{
		ShowBossBar(Closest);
	}
}

void ABeyondPlayerController::ShowBossBar(ABeyondCharacterBase* Boss)
{
	HideBossBar();

	BossBarWidget = CreateWidget<UUserWidget>(this, Boss->BossBarWidgetClass);
	if (!BossBarWidget)
	{
		return;
	}
	BossBarWidget->AddToViewport(2);

	ShownBoss = Boss;
	BossDiedTime = -1.0f;
	if (UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent())
	{
		BossHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetCurrentHealthAttribute()).AddUObject(this, &ThisClass::HandleBossHealthChanged);
		BossMaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleBossHealthChanged);
	}
	RefreshBossHealth();
}

void ABeyondPlayerController::HideBossBar()
{
	if (ABeyondCharacterBase* Boss = ShownBoss.Get())
	{
		if (UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetCurrentHealthAttribute()).Remove(BossHealthHandle);
			ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxHealthAttribute()).Remove(BossMaxHealthHandle);
		}
	}
	BossHealthHandle.Reset();
	BossMaxHealthHandle.Reset();
	ShownBoss.Reset();
	BossDiedTime = -1.0f;

	if (BossBarWidget)
	{
		BossBarWidget->RemoveFromParent();
		BossBarWidget = nullptr;
	}
}

void ABeyondPlayerController::HandleBossHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshBossHealth();
}

void ABeyondPlayerController::RefreshBossHealth()
{
	if (const ABeyondCharacterBase* Boss = ShownBoss.Get())
	{
		CallValueFunction(BossBarWidget, TEXT("UpdateHealthPercentage"),
			UBeyondCombatLibrary::GetActorHealth(Boss), UBeyondCombatLibrary::GetActorMaxHealth(Boss));
	}
}

#undef LOCTEXT_NAMESPACE
