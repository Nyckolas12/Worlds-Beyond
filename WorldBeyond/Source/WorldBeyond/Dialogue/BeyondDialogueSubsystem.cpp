// Fill out your copyright notice in the Description page of Project Settings.

#include "Dialogue/BeyondDialogueSubsystem.h"
#include "WorldBeyond.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "Characters/BeyondCharacterBase.h"
#include "Blueprint/UserWidget.h"
#include "Dialogue/BeyondDialogueBridge.h"
#include "Dialogue/BeyondDialogueSettings.h"
#include "Enemies/BeyondBossCharacter.h"
#include "Enemies/BeyondBossDefinition.h"
#include "Engine/World.h"
#include "Game/BeyondCombatSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Player/BeyondPartyComponent.h"
#include "TimerManager.h"

UBeyondDialogueSubsystem* UBeyondDialogueSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBeyondDialogueSubsystem>() : nullptr;
}

void UBeyondDialogueSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(&InWorld))
	{
		Combat->OnCharacterKilled.AddUniqueDynamic(this, &ThisClass::HandleCharacterKilled);
	}
}

void UBeyondDialogueSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimer);
	}
	Super::Deinitialize();
}

bool UBeyondDialogueSubsystem::StartConversation(ACharacter* InSpeaker, FName Row, AActor* InPartner, EBeyondConversationKind InKind)
{
	if (!InSpeaker || Row.IsNone())
	{
		return false;
	}

	FBeyondDialogueRowView RowView;
	if (!BeyondDialogue::ReadDialogueRow(Row, RowView))
	{
		UE_LOG(LogBeyond, Warning, TEXT("Dialogue: no row %s in DT_Dialogue"), *Row.ToString());
		return false;
	}

	// One at a time: talking to someone ends the banter
	if (bActive)
	{
		EndConversation();
	}

	UActorComponent* Component = BeyondDialogue::FindDialogueComponent(InSpeaker);
	if (!Component)
	{
		UE_LOG(LogBeyond, Warning, TEXT("Dialogue: %s has no BP_AC_Dialogue"), *InSpeaker->GetName());
		return false;
	}
	if (BeyondDialogue::IsInDialogue(Component))
	{
		// Something else (a BP_StartDialogue trigger box) is running on it
		BeyondDialogue::CloseConversation(Component);
	}

	// The first row's event fires while starting, so everything is set up before
	ActiveComponent = Component;
	Speaker = InSpeaker;
	Partner = InPartner;
	Kind = InKind;
	StartRow = Row;
	LastRow = NAME_None;
	bActive = true;
	bRowsBound = BeyondDialogue::BindRowChanged(Component, this, GET_FUNCTION_NAME_CHECKED(UBeyondDialogueSubsystem, HandleRowChanged));

	if (!BeyondDialogue::StartConversation(Component, Row, InPartner))
	{
		UE_LOG(LogBeyond, Warning, TEXT("Dialogue: %s could not start %s"), *InSpeaker->GetName(), *Row.ToString());
		bActive = false;
		ActiveComponent = nullptr;
		return false;
	}

	if (!bRowsBound)
	{
		HandleRow(BeyondDialogue::GetCurrentRow(Component));
	}
	// Banter keeps the HUD on screen: its box goes above the ability bar
	if (UUserWidget* Main = BeyondDialogue::GetMainWidget(Component))
	{
		Main->SetRenderTranslation(InKind == EBeyondConversationKind::Banter ? GetDefault<UBeyondDialogueSettings>()->BanterBoxOffset : FVector2D::ZeroVector);
	}
	GetWorld()->GetTimerManager().SetTimer(PollTimer, this, &ThisClass::Poll, 0.1f, true);
	if (InPartner)
	{
		UE_LOG(LogBeyond, Log, TEXT("Dialogue: %s talks to %s (%s)"), *InSpeaker->GetName(), *InPartner->GetName(), *Row.ToString());
	}
	else
	{
		UE_LOG(LogBeyond, Log, TEXT("Dialogue: %s %s (%s)"), *InSpeaker->GetName(),
			InKind == EBeyondConversationKind::Banter ? TEXT("banters") : TEXT("starts a conversation"), *Row.ToString());
	}
	OnConversationStarted.Broadcast(InSpeaker, InPartner, Row);
	return true;
}

void UBeyondDialogueSubsystem::EndConversation()
{
	if (!bActive)
	{
		return;
	}
	if (UActorComponent* Component = ActiveComponent.Get())
	{
		BeyondDialogue::CloseConversation(Component);
	}
	Finish();
}

FName UBeyondDialogueSubsystem::GetCurrentRow() const
{
	return bActive ? BeyondDialogue::GetCurrentRow(ActiveComponent.Get()) : NAME_None;
}

void UBeyondDialogueSubsystem::HandleRowChanged(FName RowName)
{
	if (bActive)
	{
		HandleRow(RowName);
	}
}

void UBeyondDialogueSubsystem::HandleRow(FName Row)
{
	if (Row.IsNone())
	{
		return;
	}
	LastRow = Row;
	OnDialogueRow.Broadcast(Row);

	FBeyondDialogueRowView RowView;
	if (BeyondDialogue::ReadDialogueRow(Row, RowView) && !RowView.SpecialEvent.IsNone())
	{
		RunSpecialEvent(RowView.SpecialEvent, Row);
	}
}

void UBeyondDialogueSubsystem::RunSpecialEvent(FName Event, FName Row)
{
	TArray<FString> Parts;
	Event.ToString().ParseIntoArray(Parts, TEXT(";"));
	for (FString Part : Parts)
	{
		Part.TrimStartAndEndInline();
		if (Part.IsEmpty())
		{
			continue;
		}

		FString Flag;
		if (Part.Split(TEXT("."), nullptr, &Flag) || Part.Split(TEXT(":"), nullptr, &Flag))
		{
			const FString Verb = Part.Left(Part.Len() - Flag.Len() - 1);
			if (Verb.Equals(TEXT("Flag"), ESearchCase::IgnoreCase) || Verb.Equals(TEXT("Unflag"), ESearchCase::IgnoreCase))
			{
				SetFlag(FName(*Flag), Verb.Equals(TEXT("Flag"), ESearchCase::IgnoreCase));
				continue;
			}
		}
		UE_LOG(LogBeyond, Log, TEXT("Dialogue: event %s (%s)"), *Part, *Row.ToString());
		OnDialogueEvent.Broadcast(FName(*Part), Row);
	}
}

UBeyondPartyComponent* UBeyondDialogueSubsystem::GetParty() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return PC ? PC->FindComponentByClass<UBeyondPartyComponent>() : nullptr;
}

bool UBeyondDialogueSubsystem::HasFlag(FName Flag) const
{
	const UBeyondPartyComponent* Party = GetParty();
	return Party && Party->HasStoryFlag(Flag);
}

void UBeyondDialogueSubsystem::SetFlag(FName Flag, bool bSet)
{
	if (UBeyondPartyComponent* Party = GetParty())
	{
		Party->SetStoryFlag(Flag, bSet);
	}
}

bool UBeyondDialogueSubsystem::PassesFlags(const TArray<FName>& Required, const TArray<FName>& Blocked) const
{
	for (const FName& Flag : Required)
	{
		if (!Flag.IsNone() && !HasFlag(Flag))
		{
			return false;
		}
	}
	for (const FName& Flag : Blocked)
	{
		if (!Flag.IsNone() && HasFlag(Flag))
		{
			return false;
		}
	}
	return true;
}

void UBeyondDialogueSubsystem::Poll()
{
	UActorComponent* Component = ActiveComponent.Get();
	if (!bActive || !Component || !BeyondDialogue::IsInDialogue(Component))
	{
		Finish();
		return;
	}
	if (!bRowsBound)
	{
		const FName Row = BeyondDialogue::GetCurrentRow(Component);
		if (Row != LastRow)
		{
			HandleRow(Row);
		}
	}
}

void UBeyondDialogueSubsystem::Finish()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimer);
	}
	if (!bActive)
	{
		return;
	}
	bActive = false;
	AActor* OldSpeaker = Speaker.Get();
	AActor* OldPartner = Partner.Get();
	const FName OldStart = StartRow;
	ActiveComponent = nullptr;
	Speaker = nullptr;
	Partner = nullptr;
	UE_LOG(LogBeyond, Log, TEXT("Dialogue: %s ended"), *OldStart.ToString());
	OnConversationEnded.Broadcast(OldSpeaker, OldPartner, OldStart);
}

void UBeyondDialogueSubsystem::HandleCharacterKilled(ABeyondCharacterBase* Victim, AActor* Killer)
{
	// Clones and summons can't drop loot; only the real boss counts
	if (!Victim || !UBeyondCombatLibrary::IsBoss(Victim) || !Victim->CanDropLoot())
	{
		return;
	}
	const ABeyondBossCharacter* Boss = Cast<ABeyondBossCharacter>(Victim);
	const UBeyondBossDefinition* Definition = Boss ? Boss->GetBossDefinition() : nullptr;
	const FName BossId = Definition && !Definition->BossId.IsNone() ? Definition->BossId : FName(*Victim->GetClass()->GetName());
	SetFlag(FName(*FString::Printf(TEXT("Boss.%s"), *BossId.ToString())), true);
	OnBossDefeated.Broadcast(BossId);
}
