// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/BeyondPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Characters/BeyondCharacterBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Player/BeyondPartyComponent.h"

ABeyondPlayerController::ABeyondPlayerController()
{
	PartyComponent = CreateDefaultSubobject<UBeyondPartyComponent>(TEXT("PartyComponent"));
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
	}
}
