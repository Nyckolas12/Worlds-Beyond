// Fill out your copyright notice in the Description page of Project Settings.

#include "Dialogue/BeyondNPCCharacter.h"
#include "WorldBeyond.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dialogue/BeyondDialogueBridge.h"
#include "Dialogue/BeyondDialogueSettings.h"
#include "Dialogue/BeyondDialogueSubsystem.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

ABeyondNPCCharacter::ABeyondNPCCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Villagers walk, and turn where they walk
	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = 160.0f;
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
	}
}

void ABeyondNPCCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyAppearance();
}

void ABeyondNPCCharacter::ApplyAppearance()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !NPCMesh)
	{
		return;
	}
	Body->SetSkeletalMeshAsset(NPCMesh);
	if (NPCAnimClass)
	{
		Body->SetAnimInstanceClass(NPCAnimClass);
	}
	for (int32 Slot = 0; Slot < NPCMaterials.Num(); ++Slot)
	{
		if (NPCMaterials[Slot])
		{
			Body->SetMaterial(Slot, NPCMaterials[Slot]);
		}
	}
	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 90.0f;
	Body->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -HalfHeight), FRotator(0.0f, MeshYaw, 0.0f));
	Body->SetRelativeScale3D(FVector(NPCScale));
}

void ABeyondNPCCharacter::BeginPlay()
{
	// Before Super: the pack's components look up the over-head text and camera in their own BeginPlay
	BeyondDialogue::EnsureParticipant(this, false);
	ApplyAppearance();
	Super::BeginPlay();

	Home = GetActorLocation();
	NextWanderTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(WanderPause.X, WanderPause.Y);
	// Spread the NPCs' thinking over the interval
	GetWorldTimerManager().SetTimer(ThinkTimer, this, &ThisClass::Think, 0.5f, true, FMath::FRandRange(0.1f, 0.5f));
}

void ABeyondNPCCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	Super::EndPlay(EndPlayReason);
}

FText ABeyondNPCCharacter::GetNPCName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	return FText::FromName(NPCId.IsNone() ? GetFName() : NPCId);
}

FName ABeyondNPCCharacter::GetTalkedFlag(FName Row) const
{
	return FName(*FString::Printf(TEXT("Talked.%s.%s"), *(NPCId.IsNone() ? GetClass()->GetName() : NPCId.ToString()), *Row.ToString()));
}

const FBeyondNPCConversation* ABeyondNPCCharacter::FindConversation() const
{
	const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	if (!Dialogue)
	{
		return nullptr;
	}
	for (const FBeyondNPCConversation& Conversation : Conversations)
	{
		if (Conversation.StartRow.IsNone() || !Dialogue->PassesFlags(Conversation.RequiredFlags, Conversation.BlockedByFlags))
		{
			continue;
		}
		if (Conversation.bOnce && Dialogue->HasFlag(GetTalkedFlag(Conversation.StartRow)))
		{
			continue;
		}
		return &Conversation;
	}
	return nullptr;
}

FName ABeyondNPCCharacter::PickConversation() const
{
	const FBeyondNPCConversation* Conversation = FindConversation();
	return Conversation ? Conversation->StartRow : NAME_None;
}

bool ABeyondNPCCharacter::CanTalkNow() const
{
	return bCanTalk && !BeyondDialogue::IsActorInDialogue(this) && FindConversation() != nullptr;
}

bool ABeyondNPCCharacter::TalkTo(ACharacter* Speaker)
{
	UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	const FBeyondNPCConversation* Conversation = FindConversation();
	if (!bCanTalk || !Speaker || !Dialogue || !Conversation || Dialogue->IsTalking())
	{
		return false;
	}

	const FBeyondNPCConversation Picked = *Conversation;
	// The pack's own interface reads this variable (BP_I_Dialogue.RowName)
	BeyondDialogue::SetActorRowName(this, Picked.StartRow);
	if (AController* Brain = GetController())
	{
		Brain->StopMovement();
	}
	if (!Dialogue->StartConversation(Speaker, Picked.StartRow, this, EBeyondConversationKind::Talk))
	{
		return false;
	}
	if (Picked.bOnce)
	{
		Dialogue->SetFlag(GetTalkedFlag(Picked.StartRow), true);
	}
	return true;
}

bool ABeyondNPCCharacter::IsBusy() const
{
	if (BeyondDialogue::IsActorInDialogue(this) || BeyondDialogue::IsInDialogue(BeyondDialogue::FindOverHeadComponent(this)))
	{
		return true;
	}
	const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	return Dialogue && Dialogue->IsTalking() && Dialogue->GetPartner() == this;
}

bool ABeyondNPCCharacter::ArePartnersFree() const
{
	for (const AActor* Partner : ChatterPartners)
	{
		if (!IsValid(Partner))
		{
			return false;
		}
		const ABeyondNPCCharacter* NPC = Cast<ABeyondNPCCharacter>(Partner);
		if (NPC ? NPC->IsBusy() : BeyondDialogue::IsInDialogue(BeyondDialogue::FindOverHeadComponent(Partner)))
		{
			return false;
		}
	}
	return true;
}

FName ABeyondNPCCharacter::PickChatter() const
{
	const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	if (!Dialogue || Chatter.IsEmpty())
	{
		return NAME_None;
	}
	for (int32 Offset = 0; Offset < Chatter.Num(); ++Offset)
	{
		const FBeyondNPCChatter& Entry = Chatter[(NextChatterIndex + Offset) % Chatter.Num()];
		if (Entry.StartRow.IsNone() || !Dialogue->PassesFlags(Entry.RequiredFlags, Entry.BlockedByFlags))
		{
			continue;
		}
		if (Entry.bNeedsPartners && (ChatterPartners.IsEmpty() || !ArePartnersFree()))
		{
			continue;
		}
		return Entry.StartRow;
	}
	return NAME_None;
}

bool ABeyondNPCCharacter::StartChatter(FName Row)
{
	if (Row.IsNone())
	{
		Row = PickChatter();
	}
	UActorComponent* OverHead = BeyondDialogue::FindOverHeadComponent(this);
	if (Row.IsNone() || !OverHead || IsBusy())
	{
		return false;
	}

	TArray<AActor*> Others;
	for (AActor* Partner : ChatterPartners)
	{
		Others.Add(Partner);
	}
	if (!BeyondDialogue::StartOverHead(OverHead, Row, Others))
	{
		return false;
	}

	// Next time, the entry after this one
	for (int32 Index = 0; Index < Chatter.Num(); ++Index)
	{
		if (Chatter[Index].StartRow == Row)
		{
			NextChatterIndex = Index + 1;
			break;
		}
	}
	UE_LOG(LogBeyond, Verbose, TEXT("Dialogue: %s chats (%s)"), *GetName(), *Row.ToString());
	return true;
}

bool ABeyondNPCCharacter::Greet()
{
	UActorComponent* OverHead = BeyondDialogue::FindOverHeadComponent(this);
	if (GreetRows.IsEmpty() || !OverHead || IsBusy())
	{
		return false;
	}
	LastGreetTime = GetWorld()->GetTimeSeconds();
	return BeyondDialogue::StartOverHead(OverHead, GreetRows[FMath::RandHelper(GreetRows.Num())], TArray<AActor*>());
}

void ABeyondNPCCharacter::ResetChatter()
{
	NextChatterIndex = 0;
	NextChatterTime = 0.0;
	LastGreetTime = -1.0e9;
	bLeaderClose = false;
}

void ABeyondNPCCharacter::Think()
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Leader = PC ? PC->GetPawn() : nullptr;
	const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this);
	if (!Leader || !Dialogue)
	{
		return;
	}

	const UBeyondDialogueSettings* Settings = GetDefault<UBeyondDialogueSettings>();
	const double Now = World->GetTimeSeconds();
	const float Distance = FVector::Dist(Leader->GetActorLocation(), GetActorLocation());
	const bool bBusy = IsBusy();

	if (bBusy || Dialogue->IsTalking())
	{
		if (bBusy && GetController())
		{
			GetController()->StopMovement();
		}
		return;
	}

	// A greeting when the party walks up
	const bool bWasClose = bLeaderClose;
	bLeaderClose = Distance <= Settings->GreetRadius;
	if (bLeaderClose && !bWasClose && Now - LastGreetTime >= Settings->GreetCooldown && Greet())
	{
		NextChatterTime = FMath::Max(NextChatterTime, Now + Settings->ChatterFirstDelay);
		return;
	}

	// Chatter while the party is around
	if (!Chatter.IsEmpty())
	{
		if (Distance > Settings->ChatterRadius)
		{
			NextChatterTime = 0.0;
		}
		else if (NextChatterTime <= 0.0)
		{
			NextChatterTime = Now + Settings->ChatterFirstDelay;
		}
		else if (Now >= NextChatterTime)
		{
			StartChatter();
			NextChatterTime = Now + FMath::FRandRange(Settings->ChatterInterval.X, FMath::Max(Settings->ChatterInterval.X, Settings->ChatterInterval.Y));
		}
	}

	if (WanderRadius > 0.0f && Now >= NextWanderTime)
	{
		Wander();
		NextWanderTime = Now + FMath::FRandRange(WanderPause.X, FMath::Max(WanderPause.X, WanderPause.Y));
	}
}

void ABeyondNPCCharacter::Wander()
{
	AAIController* Brain = Cast<AAIController>(GetController());
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Point;
	if (Brain && Navigation && Navigation->GetRandomReachablePointInRadius(Home, WanderRadius, Point))
	{
		Brain->MoveToLocation(Point.Location, 30.0f);
	}
}
