// Fill out your copyright notice in the Description page of Project Settings.

#include "Dialogue/BeyondBanterComponent.h"
#include "WorldBeyond.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "Characters/BeyondCharacterBase.h"
#include "Dialogue/BeyondDialogueBridge.h"
#include "Dialogue/BeyondDialogueSettings.h"
#include "Dialogue/BeyondDialogueSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Game/BeyondCombatSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "TimerManager.h"

namespace
{
	FAutoConsoleCommandWithWorldAndArgs BanterCommand(
		TEXT("Beyond.Banter"),
		TEXT("Beyond.Banter <Id|Trigger> [Context]: play a banter line by id (ignoring cooldowns), or try a trigger "
			"(RegionEntered, LowHealth, BossDefeated, Idle, LevelUp, Revived) with an optional context."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			UBeyondBanterComponent* Banter = PC ? PC->FindComponentByClass<UBeyondBanterComponent>() : nullptr;
			if (!Banter || Args.IsEmpty())
			{
				return;
			}
			const int64 Trigger = StaticEnum<EBeyondBanterTrigger>()->GetValueByNameString(Args[0]);
			const bool bPlayed = Trigger != INDEX_NONE
				? Banter->TriggerBanter(static_cast<EBeyondBanterTrigger>(Trigger), Args.Num() > 1 ? FName(*Args[1]) : NAME_None)
				: Banter->PlayBanter(FName(*Args[0]), true);
			UE_LOG(LogBeyond, Display, TEXT("Banter %s: %s"), *Args[0], bPlayed ? *Banter->GetLastBanterId().ToString() : TEXT("nothing played"));
		}));
}

UBeyondBanterComponent::UBeyondBanterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBeyondBanterComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UBeyondPartyComponent* Party = GetParty())
	{
		Party->OnLeaderChanged.AddUniqueDynamic(this, &ThisClass::HandleLeaderChanged);
		Party->OnMemberLevelUp.AddUniqueDynamic(this, &ThisClass::HandleLevelUp);
		Party->OnReviveProgress.AddUniqueDynamic(this, &ThisClass::HandleReviveProgress);
	}
	if (UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this))
	{
		Dialogue->OnBossDefeated.AddUniqueDynamic(this, &ThisClass::HandleBossDefeated);
	}
	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		Combat->OnDamageDealt.AddUniqueDynamic(this, &ThisClass::HandleDamageDealt);
	}

	LastActivityTime = Now();
	GetWorld()->GetTimerManager().SetTimer(CheckTimer, this, &ThisClass::Check, 0.5f, true);
}

void UBeyondBanterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
		World->GetTimerManager().ClearTimer(BossTimer);
	}
	Super::EndPlay(EndPlayReason);
}

double UBeyondBanterComponent::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

UBeyondBanterSet* UBeyondBanterComponent::GetBanterSet() const
{
	return BanterSet ? BanterSet.Get() : GetDefault<UBeyondDialogueSettings>()->BanterSet.LoadSynchronous();
}

UBeyondPartyComponent* UBeyondBanterComponent::GetParty() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UBeyondPartyComponent>() : nullptr;
}

ABeyondCharacterBase* UBeyondBanterComponent::GetLeader() const
{
	const UBeyondPartyComponent* Party = GetParty();
	return Party ? Party->GetLeader() : nullptr;
}

bool UBeyondBanterComponent::IsInFight() const
{
	return Now() - LastCombatTime < 6.0;
}

void UBeyondBanterComponent::NoteActivity()
{
	LastActivityTime = Now();
}

void UBeyondBanterComponent::ResetCooldowns()
{
	LastPlayed.Reset();
	LastBanterTime = -1.0e9;
}

bool UBeyondBanterComponent::CanBanterNow(EBeyondBanterTrigger Trigger, FString& WhyNot) const
{
	const ABeyondCharacterBase* Leader = GetLeader();
	const UBeyondPartyComponent* Party = GetParty();
	const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	const ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetOwner());

	if (!bBanterEnabled || !GetBanterSet())
	{
		WhyNot = TEXT("banter is off or there is no banter set");
		return false;
	}
	if (!Leader || !Party || !Dialogue)
	{
		WhyNot = TEXT("no party yet");
		return false;
	}
	for (const ABeyondCharacterBase* Member : Party->GetMembers())
	{
		if (!IsValid(Member) || UBeyondCombatLibrary::IsActorDead(Member))
		{
			WhyNot = TEXT("a demigod is down");
			return false;
		}
	}
	const bool bUrgent = Trigger == EBeyondBanterTrigger::RegionEntered || Trigger == EBeyondBanterTrigger::BossDefeated
		|| Trigger == EBeyondBanterTrigger::Scripted;
	const bool bRunningUrgent = CurrentTrigger == EBeyondBanterTrigger::RegionEntered || CurrentTrigger == EBeyondBanterTrigger::BossDefeated
		|| CurrentTrigger == EBeyondBanterTrigger::Scripted;
	if (Dialogue->IsTalking() || (Dialogue->IsBanterActive() && (!bUrgent || bRunningUrgent)))
	{
		WhyNot = TEXT("a conversation is running");
		return false;
	}
	if (PC && PC->IsAnyMenuOpen())
	{
		WhyNot = TEXT("a menu is open");
		return false;
	}
	if (UBeyondCombatLibrary::IsInCutscene(Leader))
	{
		WhyNot = TEXT("in a cutscene");
		return false;
	}
	if (!bUrgent && Now() - LastBanterTime < GetDefault<UBeyondDialogueSettings>()->BanterCooldown)
	{
		WhyNot = TEXT("the last banter was too recent");
		return false;
	}
	return true;
}

bool UBeyondBanterComponent::IsRowPlayable(FName Row) const
{
	const UDataTable* Table = BeyondDialogue::GetDialogueTable();
	return Table && !Row.IsNone() && Table->FindRowUnchecked(Row) != nullptr;
}

bool UBeyondBanterComponent::TriggerBanter(EBeyondBanterTrigger Trigger, FName Context)
{
	FString WhyNot;
	if (!CanBanterNow(Trigger, WhyNot))
	{
		UE_LOG(LogBeyond, Verbose, TEXT("Banter: %s skipped (%s)"), *StaticEnum<EBeyondBanterTrigger>()->GetNameStringByValue(static_cast<int64>(Trigger)), *WhyNot);
		return false;
	}

	const UBeyondBanterSet* Set = GetBanterSet();
	const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	const ABeyondCharacterBase* Leader = GetLeader();
	const double Time = Now();

	TArray<const FBeyondBanterLine*> Exact;
	TArray<const FBeyondBanterLine*> General;
	for (const FBeyondBanterLine& Line : Set->Lines)
	{
		if (Line.Trigger != Trigger || (!Line.Context.IsNone() && Line.Context != Context) || !IsRowPlayable(Line.StartRow))
		{
			continue;
		}
		if (!Dialogue->PassesFlags(Line.RequiredFlags, Line.BlockedByFlags))
		{
			continue;
		}
		if (Line.bOnce && Dialogue->HasFlag(FName(*FString::Printf(TEXT("Banter.%s"), *Line.Id.ToString()))))
		{
			continue;
		}
		if (const double* Played = LastPlayed.Find(Line.Id); Played && Time - *Played < Line.Cooldown)
		{
			continue;
		}
		if (!Line.RequiredLeader.IsNull())
		{
			const UClass* LeaderClass = Line.RequiredLeader.LoadSynchronous();
			if (!LeaderClass || !Leader->IsA(LeaderClass))
			{
				continue;
			}
		}
		(Line.Context.IsNone() ? General : Exact).Add(&Line);
	}

	// Lines for this exact place / boss win, and among them a first-time line (a first look) beats repeatable ones
	TArray<const FBeyondBanterLine*> Candidates = Exact.IsEmpty() ? General : Exact;
	if (Candidates.ContainsByPredicate([](const FBeyondBanterLine* Line) { return Line->bOnce; }))
	{
		Candidates.RemoveAll([](const FBeyondBanterLine* Line) { return !Line->bOnce; });
	}
	if (Candidates.IsEmpty())
	{
		return false;
	}

	float TotalWeight = 0.0f;
	for (const FBeyondBanterLine* Line : Candidates)
	{
		TotalWeight += Line->Weight;
	}
	float Pick = FMath::FRandRange(0.0f, TotalWeight);
	for (const FBeyondBanterLine* Line : Candidates)
	{
		Pick -= Line->Weight;
		if (Pick <= 0.0f)
		{
			return Play(*Line);
		}
	}
	return Play(*Candidates.Last());
}

bool UBeyondBanterComponent::PlayBanter(FName Id, bool bForce)
{
	const UBeyondBanterSet* Set = GetBanterSet();
	const FBeyondBanterLine* Line = Set ? Set->FindLine(Id) : nullptr;
	if (!Line)
	{
		return false;
	}
	FString WhyNot;
	if (!bForce && !CanBanterNow(Line->Trigger, WhyNot))
	{
		return false;
	}
	const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	if (!Dialogue || Dialogue->IsTalking())
	{
		return false;
	}
	return Play(*Line);
}

bool UBeyondBanterComponent::Play(const FBeyondBanterLine& Line)
{
	UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	ABeyondCharacterBase* Leader = GetLeader();
	if (!Dialogue || !Leader || !Dialogue->StartConversation(Leader, Line.StartRow, nullptr, EBeyondConversationKind::Banter))
	{
		return false;
	}

	const double Time = Now();
	LastPlayed.Add(Line.Id, Time);
	LastBanterTime = Time;
	LastBanterId = Line.Id;
	CurrentTrigger = Line.Trigger;
	if (Line.bOnce)
	{
		Dialogue->SetFlag(FName(*FString::Printf(TEXT("Banter.%s"), *Line.Id.ToString())), true);
	}
	UE_LOG(LogBeyond, Log, TEXT("Banter: %s"), *Line.Id.ToString());
	return true;
}

void UBeyondBanterComponent::Check()
{
	const ABeyondCharacterBase* Leader = GetLeader();
	const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	const ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(GetOwner());
	if (!Leader || !Dialogue)
	{
		return;
	}

	const UBeyondDialogueSettings* Settings = GetDefault<UBeyondDialogueSettings>();
	const double Time = Now();

	// Low health in a fight (once per dip)
	const bool bAlive = !UBeyondCombatLibrary::IsActorDead(Leader);
	const float Health = UBeyondCombatLibrary::GetActorHealthPercent(Leader);
	if (bAlive && Health > 0.0f && Health < Settings->LowHealthFraction)
	{
		if (!bLeaderWasLow && IsInFight())
		{
			bLeaderWasLow = true;
			TriggerBanter(EBeyondBanterTrigger::LowHealth);
		}
	}
	else if (Health > Settings->LowHealthFraction + 0.1f)
	{
		bLeaderWasLow = false;
	}

	// Idle: nobody moving, fighting, talking or in a menu
	const bool bBusy = Leader->GetVelocity().SizeSquared2D() > FMath::Square(20.0f) || IsInFight() || Dialogue->IsConversationActive()
		|| (PC && PC->IsAnyMenuOpen());
	if (bBusy)
	{
		LastActivityTime = Time;
	}
	else if (Time - LastActivityTime >= Settings->IdleSeconds)
	{
		LastActivityTime = Time;
		TriggerBanter(EBeyondBanterTrigger::Idle);
	}
}

void UBeyondBanterComponent::HandleLeaderChanged(ABeyondCharacterBase* NewLeader, ABeyondCharacterBase* OldLeader)
{
	// The banter runs on the old leader's dialogue component; a swap ends it
	UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	if (Dialogue && Dialogue->IsBanterActive() && Dialogue->GetSpeaker() != NewLeader)
	{
		Dialogue->EndConversation();
	}
	NoteActivity();
}

void UBeyondBanterComponent::HandleLevelUp(ABeyondCharacterBase* Member, int32 NewLevel)
{
	TriggerBanter(EBeyondBanterTrigger::LevelUp);
}

void UBeyondBanterComponent::HandleReviveProgress(ABeyondCharacterBase* Member, float Progress)
{
	if (Progress >= 1.0f)
	{
		// Next frame: the revive itself happens right after this broadcast
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			TriggerBanter(EBeyondBanterTrigger::Revived);
		}));
	}
}

void UBeyondBanterComponent::HandleBossDefeated(FName BossId)
{
	PendingBoss = BossId;
	const float Delay = GetDefault<UBeyondDialogueSettings>()->BossBanterDelay;
	GetWorld()->GetTimerManager().SetTimer(BossTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		TriggerBanter(EBeyondBanterTrigger::BossDefeated, PendingBoss);
	}), FMath::Max(Delay, 0.01f), false);
}

void UBeyondBanterComponent::HandleDamageDealt(AActor* DamageInstigator, AActor* Target, float Damage)
{
	const UBeyondPartyComponent* Party = GetParty();
	if (Party && (Party->FindMemberFor(DamageInstigator) || Party->FindMemberFor(Target)))
	{
		LastCombatTime = Now();
		LastActivityTime = LastCombatTime;
	}
}
