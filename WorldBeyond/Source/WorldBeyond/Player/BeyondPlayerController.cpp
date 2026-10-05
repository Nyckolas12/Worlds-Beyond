// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/BeyondPlayerController.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Player/BeyondPartyComponent.h"
#include "TimerManager.h"
#include "UI/BeyondBondMeterWidget.h"
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
}

void ABeyondPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideBossBar();
	Super::EndPlay(EndPlayReason);
}

void ABeyondPlayerController::CreateBondMeter()
{
	if (!BondWidgetClass || BondWidget)
	{
		return;
	}

	BondWidget = CreateWidget<UUserWidget>(this, BondWidgetClass);
	if (!BondWidget)
	{
		return;
	}

	BondWidget->AddToViewport(1);
	BondWidget->SetAnchorsInViewport(FAnchors(0.5f, 1.0f));
	BondWidget->SetAlignmentInViewport(FVector2D(0.5f, 1.0f));
	BondWidget->SetPositionInViewport(BondMeterOffset, false);

	PartyComponent->OnBondChanged.AddUniqueDynamic(this, &ThisClass::HandleBondChanged);
	HandleBondChanged(PartyComponent->GetBond(), PartyComponent->MaxBond);
	RefreshDuoIcon();
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
