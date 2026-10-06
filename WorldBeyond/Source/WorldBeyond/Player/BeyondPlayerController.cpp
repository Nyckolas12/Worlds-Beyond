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
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Player/BeyondPartyComponent.h"
#include "TimerManager.h"
#include "UI/BeyondBondMeterWidget.h"
#include "UI/BeyondCrosshairWidget.h"
#include "UI/BeyondProgressWidget.h"
#include "UObject/UnrealType.h"

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
	BondWidgetClass = UBeyondBondMeterWidget::StaticClass();
	ProgressWidgetClass = UBeyondProgressWidget::StaticClass();
	CrosshairWidgetClass = UBeyondCrosshairWidget::StaticClass();
}

void ABeyondPlayerController::BeginPlay()
{
	Super::BeginPlay();

	PartyComponent->OnLeaderChanged.AddUniqueDynamic(this, &ThisClass::HandleLeaderChanged);
	AddMappingContexts();

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
		GetWorldTimerManager().SetTimer(BossBarTimer, this, &ThisClass::UpdateBossBar, 0.25f, true, 0.5f);
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

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent); EnhancedInput && SwapAction)
	{
		EnhancedInput->BindAction(SwapAction.Get(), ETriggerEvent::Started, this, &ThisClass::Input_Swap);
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
	const bool bShow = Aim && Aim->ShouldShowCrosshair();
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

	// The icon of whatever sits on the duo slot (Ability.Input.Duo)
	UTexture2D* Icon = nullptr;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UBeyondGameplayAbility* Ability = Cast<UBeyondGameplayAbility>(Spec.Ability);
		if (Ability && (Spec.GetDynamicSpecSourceTags().HasTagExact(BeyondTags::Ability_Input_Duo) || Ability->InputTag == BeyondTags::Ability_Input_Duo))
		{
			Icon = Ability->Icon;
			break;
		}
	}
	Meter->SetDuoIcon(Icon);
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

void ABeyondPlayerController::UpdateBossBar()
{
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
