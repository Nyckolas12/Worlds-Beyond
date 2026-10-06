// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/BeyondPartyComponent.h"
#include "WorldBeyond.h"
#include "AI/BeyondCompanionController.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "Characters/BeyondCharacterBase.h"
#include "EngineUtils.h"
#include "AbilitySystemComponent.h"
#include "BeyondGameplayTags.h"
#include "Game/BeyondCombatSubsystem.h"
#include "Game/BeyondGameMode.h"
#include "Game/BeyondSaveGame.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Items/BeyondEquipmentComponent.h"
#include "Items/BeyondInventoryComponent.h"
#include "Items/BeyondItemLibrary.h"
#include "Items/BeyondLootDrop.h"
#include "Items/BeyondLootSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Progression/BeyondSkillTreeComponent.h"
#include "TimerManager.h"

namespace
{
	TAutoConsoleVariable<bool> CVarBeyondSaveProgress(
		TEXT("Beyond.SaveProgress"), true,
		TEXT("Save the party's levels / EXP to the BeyondProgress slot and load them on start (0: start fresh, nothing is written)."));

	UBeyondPartyComponent* FindParty(const UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->FindComponentByClass<UBeyondPartyComponent>() : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GiveExperienceCommand(
		TEXT("Beyond.GiveExperience"),
		TEXT("Beyond.GiveExperience <Amount>: give every party member EXP (default 100)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UBeyondPartyComponent* Party = FindParty(World))
			{
				Party->AwardExperience(Args.IsEmpty() ? 100.0f : FCString::Atof(*Args[0]));
			}
		}));

	FAutoConsoleCommandWithWorld ResetProgressCommand(
		TEXT("Beyond.ResetProgress"),
		TEXT("Delete the saved party progress and put both demigods back to level 1."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UBeyondPartyComponent* Party = FindParty(World))
			{
				Party->ResetProgress();
			}
			else
			{
				UGameplayStatics::DeleteGameInSlot(UBeyondSaveGame::SlotName, UBeyondSaveGame::UserIndex);
			}
		}));
}

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

void UBeyondPartyComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		Combat->OnDamageDealt.AddUniqueDynamic(this, &ThisClass::HandleDamageDealt);
		Combat->OnCharacterKilled.AddUniqueDynamic(this, &ThisClass::HandleCharacterKilled);
	}
	if (UBeyondSkillTreeComponent* DuoTree = GetDuoTree())
	{
		DuoTree->OnSkillTreeChanged.AddUniqueDynamic(this, &ThisClass::HandleSkillTreeChanged);
	}
	if (UBeyondInventoryComponent* Inventory = GetInventory())
	{
		Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);
	}
}

void UBeyondPartyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SaveProgress();

	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		Combat->OnDamageDealt.RemoveDynamic(this, &ThisClass::HandleDamageDealt);
		Combat->OnCharacterKilled.RemoveDynamic(this, &ThisClass::HandleCharacterKilled);
	}
	Super::EndPlay(EndPlayReason);
}

void UBeyondPartyComponent::HandleCharacterKilled(ABeyondCharacterBase* Victim, AActor* Killer)
{
	// Only kills by the party count (projectiles, brands and the duo move resolve to whoever cast them)
	const ABeyondCharacterBase* KillerMember = FindMemberFor(Killer);
	if (!Victim || Members.Contains(Victim) || !KillerMember || !UBeyondCombatLibrary::AreHostile(KillerMember, Victim))
	{
		return;
	}

	if (bDropLoot)
	{
		DropLoot(Victim);
	}

	const float Reward = Victim->GetExperienceRewardValue();
	if (Reward <= 0.0f)
	{
		return;
	}

	UE_LOG(LogBeyond, Log, TEXT("Party: %s killed %s (rank %d, level %d): +%.0f EXP each"), *KillerMember->GetName(), *Victim->GetName(),
		static_cast<int32>(Victim->Rank), Victim->GetCharacterLevel(), Reward);
	AwardExperience(Reward);
	OnExperienceAwarded.Broadcast(Victim, Reward);

	// Main bosses also give Bond Points for the duo tree
	if (Victim->Rank == EBeyondEnemyRank::Boss && BondPointsPerBoss > 0)
	{
		AddBondPoints(BondPointsPerBoss);
		SaveProgress();
	}
}

int32 UBeyondPartyComponent::GetPartyLevel() const
{
	int32 Level = 1;
	for (const ABeyondCharacterBase* Member : Members)
	{
		if (IsValid(Member))
		{
			Level = FMath::Max(Level, Member->GetCharacterLevel());
		}
	}
	return Level;
}

void UBeyondPartyComponent::AddBondPoints(int32 Amount)
{
	if (Amount > 0)
	{
		BondPoints += Amount;
		OnBondPointsChanged.Broadcast(BondPoints);
	}
}

bool UBeyondPartyComponent::SpendBondPoints(int32 Amount)
{
	if (Amount < 0 || BondPoints < Amount)
	{
		return false;
	}
	BondPoints -= Amount;
	OnBondPointsChanged.Broadcast(BondPoints);
	return true;
}

void UBeyondPartyComponent::AwardBondPointsForLevel(int32 Level)
{
	// One per LevelsPerBondPoint party levels (3, 6, 9 ...); both demigods levelling together only counts once
	int32 Earned = 0;
	while (BondPointsLevel + LevelsPerBondPoint <= Level)
	{
		BondPointsLevel += LevelsPerBondPoint;
		++Earned;
	}
	if (Earned > 0)
	{
		UE_LOG(LogBeyond, Log, TEXT("Party: level %d, +%d Bond Point%s"), Level, Earned, Earned == 1 ? TEXT("") : TEXT("s"));
		AddBondPoints(Earned);
	}
}

void UBeyondPartyComponent::SetBondModifiers(float GainMultiplier, float EchoFraction)
{
	BondGainMultiplier = FMath::Max(GainMultiplier, 0.0f);
	BondEchoFraction = FMath::Clamp(EchoFraction, 0.0f, 0.9f);
}

UBeyondSkillTreeComponent* UBeyondPartyComponent::GetDuoTree() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UBeyondDuoSkillTreeComponent>() : nullptr;
}

void UBeyondPartyComponent::HandleSkillTreeChanged(UBeyondSkillTreeComponent* Tree)
{
	SaveProgress();
}

void UBeyondPartyComponent::HandleInventoryChanged()
{
	QueueSave();
}

void UBeyondPartyComponent::HandleEquipmentChanged(UBeyondEquipmentComponent* Equipment)
{
	QueueSave();
}

void UBeyondPartyComponent::QueueSave()
{
	if (bSaveQueued || bRestoringProgress || !bProgressLoaded || !IsSavingEnabled())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		bSaveQueued = true;
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::FlushQueuedSave);
	}
}

void UBeyondPartyComponent::FlushQueuedSave()
{
	if (bSaveQueued)
	{
		bSaveQueued = false;
		SaveProgress();
	}
}

UBeyondInventoryComponent* UBeyondPartyComponent::GetInventory() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UBeyondInventoryComponent>() : nullptr;
}

int32 UBeyondPartyComponent::DropLoot(ABeyondCharacterBase* Victim)
{
	if (!Victim)
	{
		return 0;
	}
	FRandomStream Stream(FMath::Rand());
	const TArray<FBeyondItemInstance> Items = UBeyondItemLibrary::RollLoot(Victim, Stream);
	UBeyondLootSubsystem* Loot = UBeyondLootSubsystem::Get(this);
	if (Items.IsEmpty() || !Loot)
	{
		return 0;
	}

	for (const FBeyondItemInstance& Item : Items)
	{
		UE_LOG(LogBeyond, Log, TEXT("Loot: %s dropped %s (%s, item level %d)"), *Victim->GetName(), *UBeyondItemLibrary::GetItemName(Item).ToString(),
			*UBeyondItemLibrary::GetTierName(Item.Tier).ToString(), Item.ItemLevel);
	}
	return Loot->SpawnDrops(Items, Victim->GetActorLocation()).Num();
}

void UBeyondPartyComponent::GiveStarterKit()
{
	bStarterKitGiven = true;
	UBeyondInventoryComponent* Inventory = GetInventory();
	for (const TSoftObjectPtr<UBeyondItemDefinition>& StarterEntry : GetDefault<UBeyondLootSettings>()->StarterItems)
	{
		const UBeyondItemDefinition* Definition = StarterEntry.LoadSynchronous();
		if (!Definition)
		{
			continue;
		}

		// A copy for everyone who can use it and has the slot free (weapons only fit their own demigod)
		bool bGiven = false;
		for (ABeyondCharacterBase* Member : Members)
		{
			UBeyondEquipmentComponent* Equipment = IsValid(Member) ? Member->GetEquipmentComponent() : nullptr;
			FBeyondItemInstance Worn;
			if (Equipment && !Equipment->GetEquipped(Definition->Slot, Worn))
			{
				bGiven |= Equipment->EquipItem(UBeyondItemLibrary::MakeItem(Definition, EBeyondItemTier::Common, 1));
			}
		}
		if (!bGiven && Inventory)
		{
			Inventory->ReturnItem(UBeyondItemLibrary::MakeItem(Definition, EBeyondItemTier::Common, 1));
		}
	}
	UE_LOG(LogBeyond, Log, TEXT("Party: starter kit given"));
}

void UBeyondPartyComponent::AwardExperience(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}
	// Shared like a JRPG party: the companion and a downed member get it too
	for (ABeyondCharacterBase* Member : Members)
	{
		if (IsValid(Member))
		{
			Member->GrantExperience(Amount);
		}
	}
}

void UBeyondPartyComponent::HandleMemberLevelUp(ABeyondCharacterBase* Member, int32 NewLevel)
{
	AwardBondPointsForLevel(NewLevel);
	OnMemberLevelUp.Broadcast(Member, NewLevel);
	SaveProgress();
}

bool UBeyondPartyComponent::IsSavingEnabled() const
{
	return bSaveProgress && CVarBeyondSaveProgress.GetValueOnGameThread();
}

bool UBeyondPartyComponent::SaveProgress()
{
	if (!bProgressLoaded || bRestoringProgress || !IsSavingEnabled() || Members.IsEmpty())
	{
		return false;
	}

	UBeyondSaveGame* Save = Cast<UBeyondSaveGame>(UGameplayStatics::LoadGameFromSlot(UBeyondSaveGame::SlotName, UBeyondSaveGame::UserIndex));
	if (!Save)
	{
		Save = Cast<UBeyondSaveGame>(UGameplayStatics::CreateSaveGameObject(UBeyondSaveGame::StaticClass()));
	}
	if (!Save)
	{
		return false;
	}

	for (const ABeyondCharacterBase* Member : Members)
	{
		if (IsValid(Member) && Member->CanGainExperience())
		{
			FBeyondMemberProgress& Progress = Save->Members.FindOrAdd(Member->GetClass()->GetFName());
			Progress.Level = Member->GetCharacterLevel();
			Progress.Experience = Member->GetExperience();
			Progress.SkillPoints = Member->GetSkillPoints();
			if (const UBeyondSkillTreeComponent* Tree = Member->GetSkillTreeComponent())
			{
				Progress.SkillRanks = Tree->GetRanks();
			}
			if (const UBeyondEquipmentComponent* Equipment = Member->GetEquipmentComponent())
			{
				Progress.Equipped = Equipment->GetEquippedItems();
			}
		}
	}

	bSaveQueued = false;
	Save->Version = 3;
	Save->BondPoints = BondPoints;
	Save->BondPointsLevel = BondPointsLevel;
	if (const UBeyondDuoSkillTreeComponent* DuoTree = Cast<UBeyondDuoSkillTreeComponent>(GetDuoTree()))
	{
		Save->DuoRanks = DuoTree->GetRanks();
		Save->DuoLoadout = FSoftClassPath(DuoTree->GetDuoLoadout().Get());
	}
	if (const UBeyondInventoryComponent* Inventory = GetInventory())
	{
		Save->Inventory = Inventory->GetItems();
	}
	Save->bStarterKitGiven = bStarterKitGiven;

	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, UBeyondSaveGame::SlotName, UBeyondSaveGame::UserIndex);
	UE_LOG(LogBeyond, Log, TEXT("Party: progress %s"), bSaved ? TEXT("saved") : TEXT("could not be saved"));
	return bSaved;
}

bool UBeyondPartyComponent::LoadProgress()
{
	bProgressLoaded = true;
	if (!IsSavingEnabled() || !UGameplayStatics::DoesSaveGameExist(UBeyondSaveGame::SlotName, UBeyondSaveGame::UserIndex))
	{
		return false;
	}

	const UBeyondSaveGame* Save = Cast<UBeyondSaveGame>(UGameplayStatics::LoadGameFromSlot(UBeyondSaveGame::SlotName, UBeyondSaveGame::UserIndex));
	if (!Save)
	{
		return false;
	}

	TGuardValue<bool> Restoring(bRestoringProgress, true);
	for (ABeyondCharacterBase* Member : Members)
	{
		const FBeyondMemberProgress* Progress = IsValid(Member) ? Save->Members.Find(Member->GetClass()->GetFName()) : nullptr;
		if (Progress && Member->CanGainExperience())
		{
			Member->RestoreProgress(Progress->Level, Progress->Experience, Progress->SkillPoints);
			if (UBeyondSkillTreeComponent* Tree = Member->GetSkillTreeComponent())
			{
				Tree->RestoreRanks(Progress->SkillRanks);
			}
			if (UBeyondEquipmentComponent* Equipment = Member->GetEquipmentComponent())
			{
				Equipment->RestoreEquipped(Progress->Equipped);
			}
			UE_LOG(LogBeyond, Log, TEXT("Party: %s restored at level %d (%.0f EXP, %d skill points, %d skills)"), *Member->GetName(),
				Progress->Level, Progress->Experience, Progress->SkillPoints, Progress->SkillRanks.Num());
		}
	}

	BondPoints = FMath::Max(Save->BondPoints, 0);
	BondPointsLevel = FMath::Max(Save->BondPointsLevel, 0);
	OnBondPointsChanged.Broadcast(BondPoints);
	if (UBeyondDuoSkillTreeComponent* DuoTree = Cast<UBeyondDuoSkillTreeComponent>(GetDuoTree()))
	{
		DuoTree->RestoreRanks(Save->DuoRanks);
		if (UClass* Loadout = Save->DuoLoadout.TryLoadClass<UGameplayAbility>())
		{
			DuoTree->SetDuoLoadout(Loadout);
		}
	}
	// Saves from before Bond Points existed: catch up on the levels already reached
	AwardBondPointsForLevel(GetPartyLevel());

	// Saves from before items existed (version 2) have an empty bag and get the starter kit
	if (UBeyondInventoryComponent* Inventory = GetInventory())
	{
		Inventory->RestoreItems(Save->Inventory);
	}
	bStarterKitGiven = Save->bStarterKitGiven;
	UE_LOG(LogBeyond, Log, TEXT("Party: bag restored with %d items"), Save->Inventory.Num());
	return true;
}

void UBeyondPartyComponent::ResetProgress()
{
	UGameplayStatics::DeleteGameInSlot(UBeyondSaveGame::SlotName, UBeyondSaveGame::UserIndex);
	TGuardValue<bool> Restoring(bRestoringProgress, true);
	for (ABeyondCharacterBase* Member : Members)
	{
		if (IsValid(Member) && Member->CanGainExperience())
		{
			// Ranks first (no refund), then level 1 with no points
			if (UBeyondSkillTreeComponent* Tree = Member->GetSkillTreeComponent())
			{
				Tree->RestoreRanks(TMap<FName, int32>());
			}
			Member->RestoreProgress(1, 0.0f, 0);
			if (UBeyondEquipmentComponent* Equipment = Member->GetEquipmentComponent())
			{
				Equipment->RestoreEquipped(TMap<EBeyondItemSlot, FBeyondItemInstance>());
			}
		}
	}
	if (UBeyondInventoryComponent* Inventory = GetInventory())
	{
		Inventory->RestoreItems(TArray<FBeyondItemInstance>());
	}
	GiveStarterKit();
	BondPoints = 0;
	BondPointsLevel = 0;
	OnBondPointsChanged.Broadcast(BondPoints);
	if (UBeyondSkillTreeComponent* DuoTree = GetDuoTree())
	{
		DuoTree->RestoreRanks(TMap<FName, int32>());
	}
	UE_LOG(LogBeyond, Log, TEXT("Party: progress reset to level 1"));
}

ABeyondCharacterBase* UBeyondPartyComponent::FindMemberFor(const AActor* Actor) const
{
	// Projectiles and weapons report themselves as the damage source; walk up to whoever fired them
	for (int32 Depth = 0; Actor && Depth < 4; ++Depth)
	{
		for (ABeyondCharacterBase* Member : Members)
		{
			if (Member && Member == Actor)
			{
				return Member;
			}
		}
		Actor = Actor->GetInstigator() && Actor->GetInstigator() != Actor ? Actor->GetInstigator() : Actor->GetOwner();
	}
	return nullptr;
}

void UBeyondPartyComponent::HandleDamageDealt(AActor* DamageInstigator, AActor* Target, float Damage)
{
	if (Damage <= 0.0f)
	{
		return;
	}

	// The duo move spends the meter; its own hits don't charge it again
	for (const ABeyondCharacterBase* Member : Members)
	{
		const UAbilitySystemComponent* ASC = Member ? Member->GetAbilitySystemComponent() : nullptr;
		if (ASC && ASC->HasMatchingGameplayTag(BeyondTags::State_Duo))
		{
			return;
		}
	}

	if (FindMemberFor(Target))
	{
		AddBond(Damage * BondPerDamageTaken * BondGainMultiplier);
		return;
	}

	ABeyondCharacterBase* Attacker = FindMemberFor(DamageInstigator);
	if (!Attacker || !UBeyondCombatLibrary::AreHostile(Attacker, Target))
	{
		return;
	}

	float Gain = Damage * BondPerDamageDealt;

	// Synergy: the other demigod hit this enemy moments ago
	const float Now = GetWorld()->GetTimeSeconds();
	FRecentHit& Recent = RecentHits.FindOrAdd(Target);
	if (Recent.Member.IsValid() && Recent.Member.Get() != Attacker && Now - Recent.Time <= SynergyWindow && Now - Recent.LastSynergyTime > SynergyWindow)
	{
		Gain += SynergyBond;
		Recent.LastSynergyTime = Now;
	}
	Recent.Member = Attacker;
	Recent.Time = Now;
	Gain *= BondGainMultiplier;

	if (RecentHits.Num() > 64)
	{
		for (auto It = RecentHits.CreateIterator(); It; ++It)
		{
			if (!It->Key.IsValid() || Now - It->Value.Time > SynergyWindow)
			{
				It.RemoveCurrent();
			}
		}
	}

	AddBond(Gain);
}

void UBeyondPartyComponent::AddBond(float Amount)
{
	if (Amount > 0.0f)
	{
		SetBond(Bond + Amount);
	}
}

bool UBeyondPartyComponent::ConsumeBond()
{
	if (!IsBondFull())
	{
		return false;
	}
	// Bond Echo (duo tree) leaves part of the meter filled
	SetBond(MaxBond * BondEchoFraction);
	return true;
}

void UBeyondPartyComponent::SetBond(float NewBond)
{
	NewBond = FMath::Clamp(NewBond, 0.0f, MaxBond);
	if (!FMath::IsNearlyEqual(NewBond, Bond))
	{
		Bond = NewBond;
		OnBondChanged.Broadcast(Bond, MaxBond);
	}
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
		BindMemberComponents(Member);
	}

	LoadProgress();
	if (!bStarterKitGiven)
	{
		GiveStarterKit();
	}
	OnLeaderChanged.Broadcast(Leader, nullptr);
}

void UBeyondPartyComponent::AddMember(ABeyondCharacterBase* Member)
{
	if (Member && !Members.Contains(Member))
	{
		Members.Add(Member);
		Member->OnCharacterKilled.AddUniqueDynamic(this, &ThisClass::HandleMemberKilled);
		Member->OnCharacterLevelUp.AddUniqueDynamic(this, &ThisClass::HandleMemberLevelUp);
		BindMemberComponents(Member);
	}
}

void UBeyondPartyComponent::BindMemberComponents(ABeyondCharacterBase* Member)
{
	if (!Member)
	{
		return;
	}
	if (UBeyondSkillTreeComponent* Tree = Member->GetSkillTreeComponent())
	{
		Tree->OnSkillTreeChanged.AddUniqueDynamic(this, &ThisClass::HandleSkillTreeChanged);
	}
	if (UBeyondEquipmentComponent* Equipment = Member->GetEquipmentComponent())
	{
		Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &ThisClass::HandleEquipmentChanged);
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
	for (const ABeyondCharacterBase* Member : Members)
	{
		const UAbilitySystemComponent* ASC = Member ? Member->GetAbilitySystemComponent() : nullptr;
		if (ASC && ASC->HasMatchingGameplayTag(BeyondTags::State_Duo))
		{
			return false;
		}
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
		SetBond(0.0f);
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
