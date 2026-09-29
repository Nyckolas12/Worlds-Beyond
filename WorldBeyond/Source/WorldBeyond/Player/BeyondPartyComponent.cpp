// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/BeyondPartyComponent.h"
#include "WorldBeyond.h"
#include "AI/BeyondCompanionController.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "Characters/BeyondCharacterBase.h"
#include "EngineUtils.h"
#include "Game/BeyondGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UBeyondPartyComponent::UBeyondPartyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	// Inactive components never tick; the revive check lives in TickComponent
	bAutoActivate = true;
	CompanionControllerClass = ABeyondCompanionController::StaticClass();
}

APlayerController* UBeyondPartyComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

void UBeyondPartyComponent::InitializeParty(APawn* InitialLeader)
{
	ABeyondCharacterBase* Leader = Cast<ABeyondCharacterBase>(InitialLeader);
	if (bInitialized || !Leader)
	{
		return;
	}
	bInitialized = true;

	AddMember(Leader);

	if (PartyClasses.IsEmpty())
	{
		// Every placed Player-team demigod joins
		for (TActorIterator<ABeyondCharacterBase> It(GetWorld()); It; ++It)
		{
			if (*It != Leader && It->TeamAffiliation == EBeyondTeam::Player)
			{
				AddMember(*It);
			}
		}
	}
	else
	{
		for (const TSubclassOf<ABeyondCharacterBase>& MemberClass : PartyClasses)
		{
			if (!MemberClass || Leader->IsA(MemberClass))
			{
				continue;
			}

			ABeyondCharacterBase* Member = nullptr;
			for (TActorIterator<ABeyondCharacterBase> It(GetWorld(), MemberClass); It; ++It)
			{
				if (!Members.Contains(*It))
				{
					Member = *It;
					break;
				}
			}

			if (!Member)
			{
				const FVector SpawnLocation = Leader->GetActorLocation() - Leader->GetActorForwardVector() * 250.0f + Leader->GetActorRightVector() * 150.0f;
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
				Member = GetWorld()->SpawnActor<ABeyondCharacterBase>(MemberClass, SpawnLocation, Leader->GetActorRotation(), Params);
			}

			AddMember(Member);
		}
	}

	for (ABeyondCharacterBase* Member : Members)
	{
		if (Member != Leader)
		{
			GiveToCompanionController(Member, Leader);
		}
	}

	OnLeaderChanged.Broadcast(Leader, nullptr);
}

void UBeyondPartyComponent::AddMember(ABeyondCharacterBase* Member)
{
	if (Member && !Members.Contains(Member))
	{
		Members.Add(Member);
		Member->OnCharacterKilled.AddUniqueDynamic(this, &ThisClass::HandleMemberKilled);
	}
}

void UBeyondPartyComponent::GiveToCompanionController(ABeyondCharacterBase* Member, ABeyondCharacterBase* NewLeader)
{
	ABeyondCompanionController* Companion = Cast<ABeyondCompanionController>(Member->GetController());
	if (!Companion)
	{
		// Replace whatever controller the level gave it (e.g. auto-possess AI)
		if (AController* OldController = Member->GetController())
		{
			OldController->UnPossess();
			if (!OldController->IsA<APlayerController>())
			{
				OldController->Destroy();
			}
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* ControllerClass = CompanionControllerClass ? CompanionControllerClass.Get() : ABeyondCompanionController::StaticClass();
		Companion = GetWorld()->SpawnActor<ABeyondCompanionController>(ControllerClass, Member->GetActorLocation(), Member->GetActorRotation(), Params);
		Companion->Possess(Member);
	}
	Companion->SetLeader(NewLeader);
}

ABeyondCharacterBase* UBeyondPartyComponent::GetLeader() const
{
	const APlayerController* PC = GetPlayerController();
	return PC ? Cast<ABeyondCharacterBase>(PC->GetPawn()) : nullptr;
}

ABeyondCharacterBase* UBeyondPartyComponent::GetCompanion() const
{
	const ABeyondCharacterBase* Leader = GetLeader();
	for (ABeyondCharacterBase* Member : Members)
	{
		if (Member && Member != Leader)
		{
			return Member;
		}
	}
	return nullptr;
}

TArray<ABeyondCharacterBase*> UBeyondPartyComponent::GetMembers() const
{
	TArray<ABeyondCharacterBase*> Result;
	for (ABeyondCharacterBase* Member : Members)
	{
		if (Member)
		{
			Result.Add(Member);
		}
	}
	return Result;
}

ABeyondCharacterBase* UBeyondPartyComponent::FindNextAliveMember(const ABeyondCharacterBase* After) const
{
	const int32 Start = Members.IndexOfByKey(After);
	for (int32 Offset = 1; Offset <= Members.Num(); ++Offset)
	{
		ABeyondCharacterBase* Candidate = Members[(Start + Offset + Members.Num()) % Members.Num()];
		if (Candidate && Candidate != After && !UBeyondCombatLibrary::IsActorDead(Candidate))
		{
			return Candidate;
		}
	}
	return nullptr;
}

bool UBeyondPartyComponent::SwapLeader()
{
	return SwapTo(FindNextAliveMember(GetLeader()), false);
}

bool UBeyondPartyComponent::SwapTo(ABeyondCharacterBase* NewLeader, bool bIgnoreCooldown)
{
	APlayerController* PC = GetPlayerController();
	ABeyondCharacterBase* OldLeader = GetLeader();
	const float Now = GetWorld()->GetTimeSeconds();

	if (!PC || !NewLeader || NewLeader == OldLeader || UBeyondCombatLibrary::IsActorDead(NewLeader))
	{
		return false;
	}
	if (!bIgnoreCooldown && Now - LastSwapTime < SwapCooldown)
	{
		return false;
	}

	ABeyondCompanionController* Companion = Cast<ABeyondCompanionController>(NewLeader->GetController());
	const FRotator ControlRotation = PC->GetControlRotation();

	// Possess takes the pawn away from the companion controller and releases the old leader
	PC->Possess(NewLeader);
	PC->SetControlRotation(ControlRotation);
	NewLeader->EnableInput(PC);

	if (OldLeader)
	{
		if (Companion)
		{
			Companion->Possess(OldLeader);
			Companion->SetLeader(NewLeader);
		}
		else
		{
			GiveToCompanionController(OldLeader, NewLeader);
		}

		PC->SetViewTarget(OldLeader);
		PC->SetViewTargetWithBlend(NewLeader, CameraBlendTime, VTBlend_Cubic);
	}

	// Any other companions now follow the new leader
	for (ABeyondCharacterBase* Member : Members)
	{
		if (ABeyondCompanionController* Other = Member ? Cast<ABeyondCompanionController>(Member->GetController()) : nullptr)
		{
			Other->SetLeader(NewLeader);
		}
	}

	if (NewLeader->SwapInSound)
	{
		UGameplayStatics::PlaySound2D(this, NewLeader->SwapInSound);
	}

	LastSwapTime = Now;
	ReviveProgress = 0.0f;
	OnLeaderChanged.Broadcast(NewLeader, OldLeader);
	return true;
}

void UBeyondPartyComponent::HandleMemberKilled(ABeyondCharacterBase* Member, AActor* Killer)
{
	const bool bAnyAlive = Members.ContainsByPredicate([](const ABeyondCharacterBase* Candidate)
	{
		return Candidate && !UBeyondCombatLibrary::IsActorDead(Candidate);
	});

	if (!bAnyAlive)
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoSwapTimer);
		OnPartyWiped.Broadcast();
		if (ABeyondGameMode* GameMode = GetWorld()->GetAuthGameMode<ABeyondGameMode>())
		{
			GameMode->HandlePartyWiped(GetPlayerController());
		}
		return;
	}

	if (Member == GetLeader())
	{
		GetWorld()->GetTimerManager().SetTimer(AutoSwapTimer, this, &ThisClass::AutoSwapAfterDeath, FMath::Max(AutoSwapDelayOnDeath, 0.01f), false);
	}
}

void UBeyondPartyComponent::AutoSwapAfterDeath()
{
	SwapTo(FindNextAliveMember(GetLeader()), true);
}

void UBeyondPartyComponent::RespawnPartyAt(const FTransform& Transform)
{
	GetWorld()->GetTimerManager().ClearTimer(AutoSwapTimer);

	int32 Index = 0;
	for (ABeyondCharacterBase* Member : Members)
	{
		if (!Member)
		{
			continue;
		}

		if (UBeyondCombatLibrary::IsActorDead(Member))
		{
			Member->Revive(1.0f);
		}
		else
		{
			UBeyondCombatLibrary::ApplyHeal(Member, Member, UBeyondCombatLibrary::GetActorMaxHealth(Member));
		}

		// Line members up side by side at the checkpoint
		const FVector Offset = Transform.GetRotation().GetRightVector() * 150.0f * Index;
		Member->TeleportTo(Transform.GetLocation() + Offset, Transform.Rotator());
		++Index;
	}

	ReviveTarget.Reset();
	ReviveProgress = 0.0f;
}

void UBeyondPartyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateRevive(DeltaTime);
}

void UBeyondPartyComponent::UpdateRevive(float DeltaTime)
{
	const ABeyondCharacterBase* Leader = GetLeader();
	if (!Leader || UBeyondCombatLibrary::IsActorDead(Leader))
	{
		return;
	}

	// Downed member closest to the leader within range
	ABeyondCharacterBase* Downed = nullptr;
	float BestDistance = ReviveRadius;
	for (ABeyondCharacterBase* Member : Members)
	{
		if (Member && Member != Leader && UBeyondCombatLibrary::IsActorDead(Member))
		{
			// The ragdoll drifts from the capsule, so measure to the body
			const float Distance = FVector::Dist(Leader->GetActorLocation(), Member->GetMesh()->GetComponentLocation());
			if (Distance <= BestDistance)
			{
				BestDistance = Distance;
				Downed = Member;
			}
		}
	}

	if (Downed != ReviveTarget.Get())
	{
		if (ReviveTarget.IsValid())
		{
			OnReviveProgress.Broadcast(ReviveTarget.Get(), 0.0f);
		}
		ReviveTarget = Downed;
		ReviveProgress = 0.0f;
		UE_LOG(LogBeyond, Log, TEXT("Party: revive target %s"), Downed ? *Downed->GetName() : TEXT("none"));
	}

	if (!Downed)
	{
		return;
	}

	ReviveProgress += DeltaTime / ReviveTime;
	OnReviveProgress.Broadcast(Downed, FMath::Min(ReviveProgress, 1.0f));

	if (ReviveProgress >= 1.0f)
	{
		UE_LOG(LogBeyond, Log, TEXT("Party: reviving %s"), *Downed->GetName());
		Downed->Revive(ReviveHealthFraction);
		ReviveTarget.Reset();
		ReviveProgress = 0.0f;
	}
}
