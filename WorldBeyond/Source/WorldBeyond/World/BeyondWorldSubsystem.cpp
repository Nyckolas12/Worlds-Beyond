// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondWorldSubsystem.h"
#include "WorldBeyond.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AI/BeyondCompanionController.h"
#include "AI/BeyondEnemyController.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/BeyondCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Dialogue/BeyondBanter.h"
#include "Dialogue/BeyondBanterComponent.h"
#include "Dialogue/BeyondDialogueSubsystem.h"
#include "Enemies/BeyondBossCharacter.h"
#include "Enemies/BeyondBossDefinition.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/BeyondBossArena.h"
#include "Game/BeyondGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "NavigationSystem.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UI/BeyondRegionBannerWidget.h"
#include "World/BeyondHazardVolume.h"
#include "World/BeyondOpenWorldSettings.h"
#include "World/BeyondPointOfInterest.h"
#include "World/BeyondRegionDefinition.h"
#include "World/BeyondRegionVolume.h"
#include "World/BeyondWaystone.h"
#include "World/BeyondWorldDemo.h"
#include "World/BeyondWorldInfo.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

#define LOCTEXT_NAMESPACE "BeyondWorld"

namespace BeyondWorldSubsystemLocal
{
	const UBeyondOpenWorldSettings* Settings()
	{
		return GetDefault<UBeyondOpenWorldSettings>();
	}

	double Now(const UWorld* World)
	{
		return World ? World->GetTimeSeconds() : 0.0;
	}

	template <typename T>
	TArray<T*> Pin(const TArray<TWeakObjectPtr<T>>& Weak)
	{
		TArray<T*> Result;
		for (const TWeakObjectPtr<T>& Entry : Weak)
		{
			if (T* Object = Entry.Get())
			{
				Result.Add(Object);
			}
		}
		return Result;
	}

	FAutoConsoleCommandWithWorldAndArgs TravelCommand(
		TEXT("Beyond.Travel"),
		TEXT("Beyond.Travel <waystone id>: attune that waystone and travel there (no combat checks)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UBeyondWorldSubsystem* Subsystem = UBeyondWorldSubsystem::Get(World);
			ABeyondWaystone* Waystone = Subsystem && !Args.IsEmpty() ? Subsystem->FindWaystone(FName(*Args[0])) : nullptr;
			if (!Waystone)
			{
				UE_LOG(LogBeyond, Warning, TEXT("Beyond.Travel: no waystone '%s'"), Args.IsEmpty() ? TEXT("") : *Args[0]);
				return;
			}
			if (UBeyondPartyComponent* Party = UBeyondWorldSubsystem::GetParty(World))
			{
				Party->AttuneWaystone(Waystone->WaystoneId);
			}
			Subsystem->TravelPartyTo(Waystone->GetArrivalTransform(), EBeyondTravelReason::Cheat);
		}));

	FAutoConsoleCommandWithWorldAndArgs DiscoverCommand(
		TEXT("Beyond.Discover"),
		TEXT("Beyond.Discover [all | <region / place / waystone id>]: discover it (all: every region, place and waystone, attuned)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UBeyondWorldSubsystem* Subsystem = UBeyondWorldSubsystem::Get(World);
			UBeyondPartyComponent* Party = UBeyondWorldSubsystem::GetParty(World);
			if (!Subsystem || !Party)
			{
				return;
			}
			if (Args.IsEmpty() || Args[0].Equals(TEXT("all"), ESearchCase::IgnoreCase))
			{
				Subsystem->DiscoverEverything();
				return;
			}
			const FName Id(*Args[0]);
			if (Subsystem->FindRegionById(Id))
			{
				Party->DiscoverRegion(Id);
			}
			else if (Subsystem->FindWaystone(Id))
			{
				Party->AttuneWaystone(Id);
			}
			else
			{
				Party->DiscoverPlace(Id);
			}
		}));

	FAutoConsoleCommandWithWorld ResetWorldCommand(
		TEXT("Beyond.ResetWorld"),
		TEXT("Forget every discovered region, place and waystone (the starting waystone stays attuned)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UBeyondPartyComponent* Party = UBeyondWorldSubsystem::GetParty(World))
			{
				Party->ResetWorldProgress();
			}
			if (UBeyondWorldSubsystem* Subsystem = UBeyondWorldSubsystem::Get(World))
			{
				Subsystem->ForgetBanners();
				Subsystem->ScanNow();
			}
		}));

	FAutoConsoleCommandWithWorld RegionCommand(
		TEXT("Beyond.Region"),
		TEXT("Log the region and village the leader is in, and the level band there."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const UBeyondWorldSubsystem* Subsystem = UBeyondWorldSubsystem::Get(World);
			if (!Subsystem)
			{
				return;
			}
			const UBeyondRegionDefinition* Region = Subsystem->GetCurrentRegion();
			const UBeyondRegionDefinition* Village = Subsystem->GetCurrentVillage();
			UE_LOG(LogBeyond, Display, TEXT("Region: %s (%s), village / area: %s, %d region volumes, %d waystones, %d places"),
				Region ? *Region->GetDisplayNameOrId().ToString() : TEXT("none"), Region ? *Region->GetLevelText().ToString() : TEXT("-"),
				Village ? *Village->GetDisplayNameOrId().ToString() : TEXT("none"), Subsystem->GetRegionVolumes().Num(),
				Subsystem->GetWaystones().Num(), Subsystem->GetPlaces().Num());
		}));

	FAutoConsoleCommandWithWorld MapCommand(
		TEXT("Beyond.Map"),
		TEXT("Open / close the world map."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ABeyondPlayerController* PC = World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr)
			{
				PC->ToggleWorldMap();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs WeatherCommand(
		TEXT("Beyond.Weather"),
		TEXT("Beyond.Weather <region id>: blend to that region's weather now (until the party changes region)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const UBeyondWorldSubsystem* Subsystem = UBeyondWorldSubsystem::Get(World);
			const UBeyondRegionDefinition* Region = Subsystem && !Args.IsEmpty() ? Subsystem->FindRegionById(FName(*Args[0])) : nullptr;
			if (ABeyondWorldInfo* Info = Subsystem ? Subsystem->GetWorldInfo() : nullptr; Info && Region)
			{
				Info->SetWeather(Region->Weather, Settings()->WeatherBlendTime);
			}
		}));

	FAutoConsoleCommandWithWorld WorldDemoCommand(
		TEXT("Beyond.WorldDemo"),
		TEXT("Put a small open-world setup next to the leader (region, village, two waystones, a place, lava, deep water) to try the Plan 5 systems in any map."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const TArray<ABeyondCharacterBase*> Party = UBeyondWorldSubsystem::GetPartyMembers(World);
			ABeyondPlayerController* PC = World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr;
			const APawn* Leader = PC ? PC->GetPawn() : nullptr;
			if (Leader)
			{
				BeyondWorldDemo::Spawn(World, Leader->GetActorLocation() - FVector(0.0f, 0.0f, Leader->GetSimpleCollisionHalfHeight()),
					Leader->GetActorForwardVector().GetSafeNormal2D(), false);
			}
		}));
}

UBeyondWorldSubsystem* UBeyondWorldSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBeyondWorldSubsystem>() : nullptr;
}

UBeyondPartyComponent* UBeyondWorldSubsystem::GetParty(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? PC->FindComponentByClass<UBeyondPartyComponent>() : nullptr;
}

TArray<ABeyondCharacterBase*> UBeyondWorldSubsystem::GetPartyMembers(const UObject* WorldContext)
{
	const UBeyondPartyComponent* Party = GetParty(WorldContext);
	return Party ? Party->GetMembers() : TArray<ABeyondCharacterBase*>();
}

bool UBeyondWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && Super::ShouldCreateSubsystem(Outer);
}

void UBeyondWorldSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScanTimer);
		World->GetTimerManager().ClearTimer(TravelTimer);
		World->GetTimerManager().ClearTimer(StartTimer);
	}
	Super::Deinitialize();
}

ABeyondPlayerController* UBeyondWorldSubsystem::GetPlayerController() const
{
	const UWorld* World = GetWorld();
	return World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr;
}

ABeyondCharacterBase* UBeyondWorldSubsystem::GetLeader() const
{
	const ABeyondPlayerController* PC = GetPlayerController();
	return PC && PC->PartyComponent ? PC->PartyComponent->GetLeader() : nullptr;
}

//~ Registration

void UBeyondWorldSubsystem::RegisterRegionVolume(ABeyondRegionVolume* Volume)
{
	RegionVolumes.AddUnique(Volume);
	UpdateScanning();
}

void UBeyondWorldSubsystem::UnregisterRegionVolume(ABeyondRegionVolume* Volume)
{
	RegionVolumes.Remove(Volume);
}

void UBeyondWorldSubsystem::RegisterWaystone(ABeyondWaystone* Waystone)
{
	Waystones.AddUnique(Waystone);
	if (Waystone)
	{
		Waystone->RefreshAttunedLook(IsWaystoneAttuned(Waystone));
	}
	UpdateScanning();
}

void UBeyondWorldSubsystem::UnregisterWaystone(ABeyondWaystone* Waystone)
{
	Waystones.Remove(Waystone);
}

void UBeyondWorldSubsystem::RegisterPointOfInterest(ABeyondPointOfInterest* Place)
{
	Places.AddUnique(Place);
	UpdateScanning();
}

void UBeyondWorldSubsystem::UnregisterPointOfInterest(ABeyondPointOfInterest* Place)
{
	Places.Remove(Place);
}

void UBeyondWorldSubsystem::RegisterArena(ABeyondBossArena* Arena)
{
	Arenas.AddUnique(Arena);
}

void UBeyondWorldSubsystem::UnregisterArena(ABeyondBossArena* Arena)
{
	Arenas.Remove(Arena);
}

void UBeyondWorldSubsystem::RegisterWorldInfo(ABeyondWorldInfo* Info)
{
	WorldInfo = Info;
	UpdateScanning();
	RefreshAmbient(true);
}

void UBeyondWorldSubsystem::UnregisterWorldInfo(ABeyondWorldInfo* Info)
{
	if (WorldInfo.Get() == Info)
	{
		WorldInfo.Reset();
	}
}

bool UBeyondWorldSubsystem::HasAnythingToTrack() const
{
	return WorldInfo.IsValid() || !RegionVolumes.IsEmpty() || !Waystones.IsEmpty() || !Places.IsEmpty();
}

void UBeyondWorldSubsystem::UpdateScanning()
{
	UWorld* World = GetWorld();
	if (!World || ScanTimer.IsValid() || !HasAnythingToTrack())
	{
		return;
	}
	World->GetTimerManager().SetTimer(ScanTimer, this, &ThisClass::Scan, BeyondWorldSubsystemLocal::Settings()->ScanInterval, true, 0.1f);
}

//~ Queries

UBeyondRegionDefinition* UBeyondWorldSubsystem::FindRegionAt(const FVector& Location, bool bSubLocation) const
{
	const ABeyondRegionVolume* Best = nullptr;
	for (const TWeakObjectPtr<ABeyondRegionVolume>& Entry : RegionVolumes)
	{
		const ABeyondRegionVolume* Volume = Entry.Get();
		if (!Volume || !Volume->Region)
		{
			continue;
		}
		const bool bIsSub = Volume->Region->Kind != EBeyondRegionKind::Region;
		if (bIsSub != bSubLocation || !Volume->ContainsLocation(Location))
		{
			continue;
		}
		if (!Best || Volume->Priority > Best->Priority)
		{
			Best = Volume;
		}
	}
	return Best ? Best->Region.Get() : nullptr;
}

bool UBeyondWorldSubsystem::GetLevelBandAt(const FVector& Location, int32& OutMin, int32& OutMax) const
{
	const UBeyondRegionDefinition* Region = FindRegionAt(Location, true);
	if (!Region)
	{
		Region = FindRegionAt(Location, false);
	}
	if (!Region)
	{
		OutMin = OutMax = 1;
		return false;
	}
	OutMin = FMath::Max(Region->LevelMin, 1);
	OutMax = FMath::Max(Region->LevelMax, OutMin);
	return true;
}

UBeyondRegionDefinition* UBeyondWorldSubsystem::FindRegionById(FName RegionId) const
{
	for (const TWeakObjectPtr<ABeyondRegionVolume>& Entry : RegionVolumes)
	{
		if (const ABeyondRegionVolume* Volume = Entry.Get(); Volume && Volume->Region && Volume->Region->RegionId == RegionId)
		{
			return Volume->Region;
		}
	}
	return nullptr;
}

ABeyondWaystone* UBeyondWorldSubsystem::FindWaystone(FName WaystoneId) const
{
	for (const TWeakObjectPtr<ABeyondWaystone>& Entry : Waystones)
	{
		if (ABeyondWaystone* Waystone = Entry.Get(); Waystone && Waystone->WaystoneId == WaystoneId)
		{
			return Waystone;
		}
	}
	return nullptr;
}

TArray<ABeyondWaystone*> UBeyondWorldSubsystem::GetWaystones() const
{
	return BeyondWorldSubsystemLocal::Pin(Waystones);
}

TArray<ABeyondPointOfInterest*> UBeyondWorldSubsystem::GetPlaces() const
{
	return BeyondWorldSubsystemLocal::Pin(Places);
}

TArray<ABeyondRegionVolume*> UBeyondWorldSubsystem::GetRegionVolumes() const
{
	return BeyondWorldSubsystemLocal::Pin(RegionVolumes);
}

TArray<ABeyondBossArena*> UBeyondWorldSubsystem::GetArenas() const
{
	return BeyondWorldSubsystemLocal::Pin(Arenas);
}

//~ Waystones

ABeyondWaystone* UBeyondWorldSubsystem::FindWaystoneInReach() const
{
	const ABeyondCharacterBase* Leader = GetLeader();
	if (!Leader || UBeyondCombatLibrary::IsActorDead(Leader) || Travel.bActive)
	{
		return nullptr;
	}
	ABeyondWaystone* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<ABeyondWaystone>& Entry : Waystones)
	{
		ABeyondWaystone* Waystone = Entry.Get();
		if (!Waystone)
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Waystone->GetActorLocation(), Leader->GetActorLocation());
		if (Distance <= Waystone->InteractRange && FMath::Abs(Waystone->GetActorLocation().Z - Leader->GetActorLocation().Z) < 400.0f && Distance < BestDistance)
		{
			Best = Waystone;
			BestDistance = Distance;
		}
	}
	return Best;
}

bool UBeyondWorldSubsystem::IsWaystoneAttuned(const ABeyondWaystone* Waystone) const
{
	if (!Waystone)
	{
		return false;
	}
	const UBeyondPartyComponent* Party = GetParty(this);
	return Waystone->bStartAttuned || (Party && Party->IsWaystoneAttuned(Waystone->WaystoneId));
}

bool UBeyondWorldSubsystem::IsWaystoneDiscovered(const ABeyondWaystone* Waystone) const
{
	const UBeyondPartyComponent* Party = GetParty(this);
	return IsWaystoneAttuned(Waystone) || (Waystone && Party && Party->IsPlaceDiscovered(Waystone->WaystoneId));
}

bool UBeyondWorldSubsystem::UseWaystone(ABeyondWaystone* Waystone)
{
	if (!Waystone)
	{
		return false;
	}
	if (!IsWaystoneAttuned(Waystone))
	{
		return AttuneWaystone(Waystone);
	}
	RestAt(Waystone);
	return true;
}

bool UBeyondWorldSubsystem::AttuneWaystone(ABeyondWaystone* Waystone)
{
	UBeyondPartyComponent* Party = GetParty(this);
	if (!Waystone || !Party)
	{
		return false;
	}
	Party->AttuneWaystone(Waystone->WaystoneId);
	Party->SetLastWaystone(Waystone->WaystoneId);
	SetCheckpoint(Waystone->GetArrivalTransform());
	Party->SaveProgress();
	Waystone->RefreshAttunedLook(true);
	Waystone->PlayActivateFX();
	ShowToast(LOCTEXT("Attuned", "Waystone attuned"), Waystone->GetDisplayNameOrId(), FLinearColor(0.35f, 0.6f, 1.0f));
	UE_LOG(LogBeyond, Log, TEXT("World: waystone %s attuned"), *Waystone->WaystoneId.ToString());
	OnWaystoneAttuned.Broadcast(Waystone);
	return true;
}

void UBeyondWorldSubsystem::RestAt(ABeyondWaystone* Waystone)
{
	UBeyondPartyComponent* Party = GetParty(this);
	if (!Waystone || !Party)
	{
		return;
	}
	Party->HealParty();
	Party->SetLastWaystone(Waystone->WaystoneId);
	SetCheckpoint(Waystone->GetArrivalTransform());
	Party->SaveProgress();
	Waystone->PlayActivateFX();
	ShowToast(LOCTEXT("Rested", "Rested"), Waystone->GetDisplayNameOrId(), FLinearColor(1.0f, 0.78f, 0.3f));
	UE_LOG(LogBeyond, Log, TEXT("World: rested at %s"), *Waystone->WaystoneId.ToString());
	OnPartyRested.Broadcast();
}

void UBeyondWorldSubsystem::SetCheckpoint(const FTransform& Transform)
{
	if (ABeyondGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABeyondGameMode>() : nullptr)
	{
		GameMode->SetCheckpoint(Transform);
	}
}

//~ Fast travel

bool UBeyondWorldSubsystem::CanFastTravel(FName WaystoneId, FText& OutWhyNot) const
{
	const ABeyondWaystone* Waystone = FindWaystone(WaystoneId);
	const ABeyondCharacterBase* Leader = GetLeader();
	const UBeyondPartyComponent* Party = GetParty(this);
	if (!Waystone)
	{
		OutWhyNot = LOCTEXT("NoWaystone", "There is no such waystone");
		return false;
	}
	if (!IsWaystoneAttuned(Waystone))
	{
		OutWhyNot = LOCTEXT("NotAttuned", "Attune this waystone first");
		return false;
	}
	if (Travel.bActive)
	{
		OutWhyNot = LOCTEXT("Travelling", "Already travelling");
		return false;
	}
	if (!Leader || !Party)
	{
		OutWhyNot = LOCTEXT("NoParty", "No party");
		return false;
	}
	for (const ABeyondCharacterBase* Member : Party->GetMembers())
	{
		if (UBeyondCombatLibrary::IsActorDead(Member))
		{
			OutWhyNot = LOCTEXT("Downed", "Can't travel with a demigod down");
			return false;
		}
	}
	if (const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this); Dialogue && Dialogue->IsTalking())
	{
		OutWhyNot = LOCTEXT("Talking", "Can't travel during a conversation");
		return false;
	}
	if (UBeyondCombatLibrary::IsInCutscene(Leader))
	{
		OutWhyNot = LOCTEXT("Cutscene", "Can't travel now");
		return false;
	}
	if (ABeyondBossArena::FindSealedArenaAt(GetWorld(), Leader->GetActorLocation()))
	{
		OutWhyNot = LOCTEXT("Arena", "Can't travel during a boss fight");
		return false;
	}
	if (const UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(this))
	{
		const float RadiusSq = FMath::Square(BeyondWorldSubsystemLocal::Settings()->FastTravelCombatRadius);
		for (const ABeyondEnemyCharacter* Enemy : Enemies->GetLiveEnemies())
		{
			const ABeyondEnemyController* Brain = Enemy ? Cast<ABeyondEnemyController>(Enemy->GetController()) : nullptr;
			if (!Brain || Brain->GetAIState() != EBeyondEnemyAIState::Combat || UBeyondCombatLibrary::IsActorDead(Enemy))
			{
				continue;
			}
			for (const ABeyondCharacterBase* Member : Party->GetMembers())
			{
				if (FVector::DistSquared(Enemy->GetActorLocation(), Member->GetActorLocation()) <= RadiusSq)
				{
					OutWhyNot = LOCTEXT("Combat", "Can't travel while enemies are nearby");
					return false;
				}
			}
		}
	}
	return true;
}

bool UBeyondWorldSubsystem::FastTravelTo(FName WaystoneId)
{
	FText WhyNot;
	if (!CanFastTravel(WaystoneId, WhyNot))
	{
		if (ABeyondPlayerController* PC = GetPlayerController())
		{
			PC->ShowNotice(WhyNot);
		}
		UE_LOG(LogBeyond, Log, TEXT("World: no fast travel to %s (%s)"), *WaystoneId.ToString(), *WhyNot.ToString());
		return false;
	}
	ABeyondWaystone* Waystone = FindWaystone(WaystoneId);
	if (!TravelPartyTo(Waystone->GetArrivalTransform(), EBeyondTravelReason::FastTravel))
	{
		return false;
	}
	if (UBeyondPartyComponent* Party = GetParty(this))
	{
		Party->SetLastWaystone(WaystoneId);
	}
	SetCheckpoint(Waystone->GetArrivalTransform());
	return true;
}

bool UBeyondWorldSubsystem::ShouldHoldOnTravel() const
{
	const UWorld* World = GetWorld();
	return WorldInfo.IsValid() || (World && World->IsPartitionedWorld());
}

bool UBeyondWorldSubsystem::TravelPartyTo(const FTransform& Destination, EBeyondTravelReason Reason, bool bHeal)
{
	UWorld* World = GetWorld();
	if (Travel.bActive || !World || !GetParty(this))
	{
		return false;
	}
	Travel = FTravelState();
	Travel.bActive = true;
	Travel.Reason = Reason;
	Travel.Destination = Destination;
	Travel.bHeal = bHeal;
	UE_LOG(LogBeyond, Log, TEXT("World: moving the party to (%.0f, %.0f, %.0f), %s"), Destination.GetLocation().X, Destination.GetLocation().Y,
		Destination.GetLocation().Z, *StaticEnum<EBeyondTravelReason>()->GetNameStringByValue(static_cast<int64>(Reason)));
	OnTravelStarted.Broadcast(Reason);

	const float FadeOut = BeyondWorldSubsystemLocal::Settings()->FadeOutTime;
	const bool bAlreadyDark = Reason == EBeyondTravelReason::Start || Reason == EBeyondTravelReason::Resume;
	if (bAlreadyDark || FadeOut <= 0.0f)
	{
		FadeCamera(1.0f, 1.0f, 0.0f, true);
		BeginTeleport();
	}
	else
	{
		FadeCamera(0.0f, 1.0f, FadeOut, true);
		World->GetTimerManager().SetTimer(TravelTimer, this, &ThisClass::BeginTeleport, FadeOut, false);
	}
	return true;
}

void UBeyondWorldSubsystem::BeginTeleport()
{
	UWorld* World = GetWorld();
	UBeyondPartyComponent* Party = GetParty(this);
	if (!World || !Party)
	{
		Travel = FTravelState();
		return;
	}
	if (Travel.Reason != EBeyondTravelReason::Start)
	{
		Party->TeleportPartyTo(Travel.Destination, Travel.bHeal);
	}
	else if (const ABeyondCharacterBase* Leader = GetLeader())
	{
		// Stay at the player start, the companion beside the leader
		Travel.Destination = FTransform(Leader->GetActorRotation(), Leader->GetActorLocation());
		Party->TeleportPartyTo(Travel.Destination, Travel.bHeal);
	}
	FreezeParty(true);
	Travel.bTeleported = true;
	Travel.TeleportTime = BeyondWorldSubsystemLocal::Now(World);
	Travel.GroundReadyTime = -1.0;
	World->GetTimerManager().SetTimer(TravelTimer, this, &ThisClass::TickTravel, 0.1f, true, 0.0f);
}

void UBeyondWorldSubsystem::TickTravel()
{
	const UWorld* World = GetWorld();
	if (!Travel.bActive || !World)
	{
		return;
	}
	const UBeyondOpenWorldSettings* Settings = BeyondWorldSubsystemLocal::Settings();
	const double Now = BeyondWorldSubsystemLocal::Now(World);
	const FVector Destination = Travel.Destination.GetLocation();

	bool bGroundReady = IsStreamingReadyAt(Destination);
	if (bGroundReady)
	{
		for (const ABeyondCharacterBase* Member : GetPartyMembers(this))
		{
			FVector Ground;
			bGroundReady &= FindGroundUnder(Member->GetActorLocation(), Member, Ground);
		}
	}

	if (bGroundReady)
	{
		if (Travel.GroundReadyTime < 0.0)
		{
			Travel.GroundReadyTime = Now;
		}
		if (IsNavigationReadyAt(Destination) || Now - Travel.GroundReadyTime >= Settings->NavigationWait)
		{
			FinishTravel();
		}
		return;
	}

	if (Now - Travel.TeleportTime < Settings->StreamingTimeout)
	{
		return;
	}
	// Nothing to stand on there: the player start, once; after that give up waiting
	const AActor* Start = nullptr;
	if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
	{
		Start = GameMode->FindPlayerStart(GetPlayerController());
	}
	if (!Travel.bTriedFallback && Start)
	{
		UE_LOG(LogBeyond, Warning, TEXT("World: no ground at (%.0f, %.0f, %.0f) after %.0f s; going to the player start"), Destination.X, Destination.Y,
			Destination.Z, Settings->StreamingTimeout);
		Travel.bTriedFallback = true;
		Travel.Destination = Start->GetActorTransform();
		if (UBeyondPartyComponent* Party = GetParty(this))
		{
			Party->TeleportPartyTo(Travel.Destination, Travel.bHeal);
		}
		Travel.TeleportTime = Now;
		return;
	}
	UE_LOG(LogBeyond, Warning, TEXT("World: still no ground under the party; letting go"));
	FinishTravel();
}

void UBeyondWorldSubsystem::FinishTravel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(TravelTimer);

	// Snap everyone onto the ground, then let them move again
	for (ABeyondCharacterBase* Member : GetPartyMembers(this))
	{
		FVector Ground;
		if (FindGroundUnder(Member->GetActorLocation(), Member, Ground))
		{
			const float HalfHeight = Member->GetCapsuleComponent() ? Member->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
			Member->SetActorLocation(Ground + FVector(0.0f, 0.0f, HalfHeight + 2.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	FreezeParty(false);
	for (ABeyondCharacterBase* Member : GetPartyMembers(this))
	{
		if (ABeyondCompanionController* Companion = Cast<ABeyondCompanionController>(Member->GetController()))
		{
			Companion->ResetEngagement();
		}
	}

	const EBeyondTravelReason Reason = Travel.Reason;
	LastSafeGround = Travel.Destination.GetLocation();
	bHasSafeGround = true;
	Travel = FTravelState();
	FadeCamera(1.0f, 0.0f, BeyondWorldSubsystemLocal::Settings()->FadeInTime, false);
	UE_LOG(LogBeyond, Log, TEXT("World: the party has arrived"));
	OnTravelFinished.Broadcast(Reason);
	Scan();
	if (Reason == EBeyondTravelReason::Start || Reason == EBeyondTravelReason::Resume)
	{
		RefreshAmbient(true);
	}
}

bool UBeyondWorldSubsystem::IsStreamingReadyAt(const FVector& Location) const
{
	const UWorld* World = GetWorld();
	if (!World || !World->IsPartitionedWorld())
	{
		return true;
	}
	const UWorldPartitionSubsystem* WorldPartition = World->GetSubsystem<UWorldPartitionSubsystem>();
	if (!WorldPartition)
	{
		return true;
	}
	FWorldPartitionStreamingQuerySource Source(Location);
	Source.bUseGridLoadingRange = false;
	Source.Radius = 6000.0f;
	return WorldPartition->IsStreamingCompleted(EWorldPartitionRuntimeCellState::Activated, { Source }, false);
}

bool UBeyondWorldSubsystem::FindGroundUnder(const FVector& Location, const AActor* Ignore, FVector& OutGround) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondTravelGround), false, Ignore);
	for (const ABeyondCharacterBase* Member : GetPartyMembers(this))
	{
		Params.AddIgnoredActor(Member);
	}
	FHitResult Hit;
	const FVector Start = Location + FVector(0.0f, 0.0f, 1500.0f);
	const FVector End = Location - FVector(0.0f, 0.0f, 20000.0f);
	if (World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		OutGround = Hit.ImpactPoint;
		return true;
	}
	return false;
}

bool UBeyondWorldSubsystem::IsNavigationReadyAt(const FVector& Location) const
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem || !NavSystem->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
	{
		return true;
	}
	FNavLocation Projected;
	return NavSystem->ProjectPointToNavigation(Location, Projected, FVector(300.0f, 300.0f, 600.0f));
}

void UBeyondWorldSubsystem::FreezeParty(bool bFreeze)
{
	for (ABeyondCharacterBase* Member : GetPartyMembers(this))
	{
		UCharacterMovementComponent* Movement = Member->GetCharacterMovement();
		if (!Movement)
		{
			continue;
		}
		if (bFreeze)
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
		else if (Movement->MovementMode == MOVE_None && !UBeyondCombatLibrary::IsActorDead(Member))
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
}

void UBeyondWorldSubsystem::FadeCamera(float From, float To, float Time, bool bHold) const
{
	const ABeyondPlayerController* PC = GetPlayerController();
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(From, To, Time, FLinearColor::Black, false, bHold);
	}
}

void UBeyondWorldSubsystem::ReturnToSafeGround(ABeyondCharacterBase* Member)
{
	const ABeyondCharacterBase* Leader = GetLeader();
	if (!Member || Travel.bActive)
	{
		return;
	}
	if (Member == Leader)
	{
		const FVector Back = bHasSafeGround ? LastSafeGround : Leader->GetActorLocation();
		TravelPartyTo(FTransform(Leader->GetActorRotation(), Back), EBeyondTravelReason::SafeGround);
		return;
	}
	// The companion: next to the leader
	if (Leader)
	{
		const FVector Beside = Leader->GetActorLocation() - Leader->GetActorForwardVector() * 200.0f;
		Member->TeleportTo(Beside, Leader->GetActorRotation());
	}
}

//~ Start of play

void UBeyondWorldSubsystem::HandlePartyFormed(UBeyondPartyComponent* Party)
{
	UWorld* World = GetWorld();
	if (!World || bStartDecided)
	{
		return;
	}
	// In a streamed world the ground may not be there yet: hold everyone in place while deciding
	if (World->IsPartitionedWorld())
	{
		FadeCamera(1.0f, 1.0f, 0.0f, true);
		FreezeParty(true);
	}
	// Once every actor of the map has begun play (the world info may come after the party)
	World->GetTimerManager().SetTimer(StartTimer, this, &ThisClass::DecideStart, 0.2f, false);
}

void UBeyondWorldSubsystem::DecideStart()
{
	UWorld* World = GetWorld();
	UBeyondPartyComponent* Party = GetParty(this);
	if (bStartDecided || !World || !Party)
	{
		return;
	}
	bStartDecided = true;
	UpdateWaystoneLooks();
	if (!ShouldHoldOnTravel())
	{
		return;
	}

	ABeyondWaystone* Resume = FindWaystone(Party->GetLastWaystone());
	if (Resume && WorldInfo.IsValid())
	{
		UE_LOG(LogBeyond, Log, TEXT("World: resuming at waystone %s"), *Resume->WaystoneId.ToString());
		SetCheckpoint(Resume->GetArrivalTransform());
		TravelPartyTo(Resume->GetArrivalTransform(), EBeyondTravelReason::Resume);
	}
	else
	{
		TravelPartyTo(FTransform::Identity, EBeyondTravelReason::Start);
	}
}

//~ The scan

void UBeyondWorldSubsystem::ScanNow()
{
	Scan();
}

void UBeyondWorldSubsystem::Scan()
{
	const ABeyondCharacterBase* Leader = GetLeader();
	if (!Leader || Travel.bActive)
	{
		return;
	}
	RegisterInvokers();
	UpdateRegions(Leader);
	UpdateDiscoveries(Leader);
	UpdateSafeGround(Leader);
	UpdateArenaMusic();
	UpdateWaystoneLooks();
}

void UBeyondWorldSubsystem::UpdateRegions(const ABeyondCharacterBase* Leader)
{
	const FVector Location = Leader->GetActorLocation();
	UBeyondRegionDefinition* NewRegion = FindRegionAt(Location, false);
	UBeyondRegionDefinition* NewVillage = FindRegionAt(Location, true);
	UBeyondRegionDefinition* OldRegion = CurrentRegion.Get();
	UBeyondRegionDefinition* OldVillage = CurrentVillage.Get();
	const bool bRegionChanged = NewRegion != OldRegion;
	const bool bVillageChanged = NewVillage != OldVillage;
	if (!bRegionChanged && !bVillageChanged)
	{
		return;
	}
	CurrentRegion = NewRegion;
	CurrentVillage = NewVillage;

	float RegionExp = 0.0f;
	float VillageExp = 0.0f;
	const bool bNewRegion = bRegionChanged && DiscoverRegionVisit(NewRegion, RegionExp);
	const bool bNewVillage = bVillageChanged && DiscoverRegionVisit(NewVillage, VillageExp);

	// One banner: a new region's name beats the village's (arriving in a village of a region seen for the first time)
	if (bRegionChanged && NewRegion && (bNewRegion || !bVillageChanged || !NewVillage))
	{
		TryShowBanner(NewRegion, bNewRegion, RegionExp);
	}
	else if (bVillageChanged && NewVillage)
	{
		TryShowBanner(NewVillage, bNewVillage, VillageExp);
	}

	// Banter: the more specific place first
	ABeyondPlayerController* PC = GetPlayerController();
	if (PC && PC->BanterComponent)
	{
		const bool bVillageBanter = bVillageChanged && NewVillage
			&& PC->BanterComponent->TriggerBanter(EBeyondBanterTrigger::RegionEntered, NewVillage->GetBanterContext());
		if (!bVillageBanter && bRegionChanged && NewRegion)
		{
			PC->BanterComponent->TriggerBanter(EBeyondBanterTrigger::RegionEntered, NewRegion->GetBanterContext());
		}
	}

	if (bRegionChanged)
	{
		UE_LOG(LogBeyond, Log, TEXT("World: region %s -> %s"), OldRegion ? *OldRegion->RegionId.ToString() : TEXT("none"),
			NewRegion ? *NewRegion->RegionId.ToString() : TEXT("none"));
		OnRegionChanged.Broadcast(NewRegion, OldRegion);
	}
	if (bVillageChanged)
	{
		UE_LOG(LogBeyond, Log, TEXT("World: village / area %s -> %s"), OldVillage ? *OldVillage->RegionId.ToString() : TEXT("none"),
			NewVillage ? *NewVillage->RegionId.ToString() : TEXT("none"));
		OnVillageChanged.Broadcast(NewVillage, OldVillage);
	}
	RefreshAmbient(false);
}

bool UBeyondWorldSubsystem::DiscoverRegionVisit(UBeyondRegionDefinition* Region, float& OutExp)
{
	OutExp = 0.0f;
	UBeyondPartyComponent* Party = GetParty(this);
	if (!Region || !Party || !Party->DiscoverRegion(Region->RegionId))
	{
		return false;
	}
	OutExp = GetDiscoveryExp(Region);
	if (OutExp > 0.0f)
	{
		Party->AwardExperience(OutExp);
	}
	UE_LOG(LogBeyond, Log, TEXT("World: discovered %s (+%.0f EXP)"), *Region->RegionId.ToString(), OutExp);
	OnDiscovered.Broadcast(Region->RegionId, Region->GetDisplayNameOrId());
	return true;
}

void UBeyondWorldSubsystem::TryShowBanner(const UBeyondRegionDefinition* Region, bool bFirstVisit, float Exp)
{
	// Walking back and forth across a border doesn't repeat it
	const double Now = BeyondWorldSubsystemLocal::Now(GetWorld());
	const double* Shown = BannerShownTimes.Find(Region->RegionId);
	if (bFirstVisit || !Shown || Now - *Shown >= BeyondWorldSubsystemLocal::Settings()->BannerRepeatCooldown)
	{
		BannerShownTimes.Add(Region->RegionId, Now);
		ShowBanner(Region, bFirstVisit, Exp);
	}
}

float UBeyondWorldSubsystem::GetDiscoveryExp(const UBeyondRegionDefinition* Region) const
{
	if (!Region)
	{
		return 0.0f;
	}
	if (Region->DiscoveryExp > 0.0f)
	{
		return Region->DiscoveryExp;
	}
	const UBeyondOpenWorldSettings* Settings = BeyondWorldSubsystemLocal::Settings();
	return Region->Kind == EBeyondRegionKind::Region ? Settings->RegionDiscoveryExp : Settings->VillageDiscoveryExp;
}

void UBeyondWorldSubsystem::UpdateDiscoveries(const ABeyondCharacterBase* Leader)
{
	UBeyondPartyComponent* Party = GetParty(this);
	if (!Party)
	{
		return;
	}
	const FVector Location = Leader->GetActorLocation();
	for (const TWeakObjectPtr<ABeyondPointOfInterest>& Entry : Places)
	{
		const ABeyondPointOfInterest* Place = Entry.Get();
		if (!Place || Party->IsPlaceDiscovered(Place->PoiId) || FVector::Dist(Place->GetActorLocation(), Location) > Place->DiscoveryRadius)
		{
			continue;
		}
		Party->DiscoverPlace(Place->PoiId);
		const float Exp = Place->DiscoveryExp > 0.0f ? Place->DiscoveryExp : BeyondWorldSubsystemLocal::Settings()->PlaceDiscoveryExp;
		if (Exp > 0.0f)
		{
			Party->AwardExperience(Exp);
		}
		ShowToast(FText::Format(LOCTEXT("PlaceFound", "Discovered: {0}"), Place->GetDisplayNameOrId()),
			FText::Format(LOCTEXT("PlaceDetail", "{0}  ·  +{1} EXP"), ABeyondPointOfInterest::GetKindName(Place->Kind), FMath::RoundToInt(Exp)),
			FLinearColor(1.0f, 0.78f, 0.3f));
		UE_LOG(LogBeyond, Log, TEXT("World: discovered place %s"), *Place->PoiId.ToString());
		OnDiscovered.Broadcast(Place->PoiId, Place->GetDisplayNameOrId());
	}

	for (const TWeakObjectPtr<ABeyondWaystone>& Entry : Waystones)
	{
		const ABeyondWaystone* Waystone = Entry.Get();
		if (!Waystone || IsWaystoneDiscovered(Waystone) || FVector::Dist(Waystone->GetActorLocation(), Location) > Waystone->DiscoveryRadius)
		{
			continue;
		}
		Party->DiscoverPlace(Waystone->WaystoneId);
		ShowToast(LOCTEXT("WaystoneFound", "Waystone found"), Waystone->GetDisplayNameOrId(), FLinearColor(0.35f, 0.6f, 1.0f));
		OnDiscovered.Broadcast(Waystone->WaystoneId, Waystone->GetDisplayNameOrId());
	}
}

void UBeyondWorldSubsystem::UpdateSafeGround(const ABeyondCharacterBase* Leader)
{
	const UCharacterMovementComponent* Movement = Leader->GetCharacterMovement();
	if (!Movement || !Movement->IsMovingOnGround() || UBeyondCombatLibrary::IsActorDead(Leader))
	{
		return;
	}
	const FVector Location = Leader->GetActorLocation();
	if (!ABeyondHazardVolume::IsInsideAnyHazard(GetWorld(), Location))
	{
		LastSafeGround = Location;
		bHasSafeGround = true;
	}
}

void UBeyondWorldSubsystem::UpdateArenaMusic()
{
	ABeyondBossArena* Sealed = nullptr;
	for (const TWeakObjectPtr<ABeyondBossArena>& Entry : Arenas)
	{
		ABeyondBossArena* Arena = Entry.Get();
		const ABeyondBossCharacter* Boss = Arena ? Arena->GetBoss() : nullptr;
		const UBeyondBossDefinition* Definition = Boss ? Boss->GetBossDefinition() : nullptr;
		if (Arena && Arena->IsSealed() && Definition && Definition->Music)
		{
			Sealed = Arena;
			break;
		}
	}
	if (Sealed != MusicArena.Get())
	{
		MusicArena = Sealed;
		RefreshAmbient(false);
	}
}

void UBeyondWorldSubsystem::UpdateWaystoneLooks()
{
	for (const TWeakObjectPtr<ABeyondWaystone>& Entry : Waystones)
	{
		if (ABeyondWaystone* Waystone = Entry.Get())
		{
			Waystone->RefreshAttunedLook(IsWaystoneAttuned(Waystone));
		}
	}
}

void UBeyondWorldSubsystem::RegisterInvokers()
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem || !NavSystem->IsActiveTilesGenerationEnabled())
	{
		return;
	}
	const UBeyondOpenWorldSettings* Settings = BeyondWorldSubsystemLocal::Settings();
	for (ABeyondCharacterBase* Member : GetPartyMembers(this))
	{
		if (!Invokers.Contains(Member))
		{
			NavSystem->RegisterNavigationInvoker(Member, Settings->InvokerRadius, FMath::Max(Settings->InvokerRemovalRadius, Settings->InvokerRadius));
			Invokers.Add(Member);
		}
	}
}

void UBeyondWorldSubsystem::RefreshAmbient(bool bInstant)
{
	ABeyondWorldInfo* Info = WorldInfo.Get();
	if (!Info)
	{
		return;
	}
	const UBeyondOpenWorldSettings* Settings = BeyondWorldSubsystemLocal::Settings();
	const UBeyondRegionDefinition* Region = CurrentRegion.Get();
	const UBeyondRegionDefinition* Village = CurrentVillage.Get();

	// Weather: the village's own, else the region's, else the map's default
	const FBeyondWeatherPreset* Weather = &Info->DefaultWeather;
	if (Village && Village->bOverrideWeather)
	{
		Weather = &Village->Weather;
	}
	else if (Region && Region->bOverrideWeather)
	{
		Weather = &Region->Weather;
	}
	Info->SetWeather(*Weather, bInstant ? 0.0f : Settings->WeatherBlendTime);

	// Music: a sealed boss fight, else the village's, else the region's
	USoundBase* Music = nullptr;
	if (const ABeyondBossArena* Arena = MusicArena.Get())
	{
		const ABeyondBossCharacter* Boss = Arena->GetBoss();
		Music = Boss && Boss->GetBossDefinition() ? Boss->GetBossDefinition()->Music.Get() : nullptr;
	}
	if (!Music && Village && !Village->Music.IsNull())
	{
		Music = Village->Music.LoadSynchronous();
	}
	if (!Music && Region && !Region->Music.IsNull())
	{
		Music = Region->Music.LoadSynchronous();
	}
	Info->SetMusic(Music, bInstant ? 0.5f : Settings->MusicFadeTime);

	USoundBase* AmbienceLoop = nullptr;
	if (Village && !Village->Ambience.IsNull())
	{
		AmbienceLoop = Village->Ambience.LoadSynchronous();
	}
	else if (Region && !Region->Ambience.IsNull())
	{
		AmbienceLoop = Region->Ambience.LoadSynchronous();
	}
	Info->SetAmbience(AmbienceLoop, bInstant ? 0.5f : Settings->MusicFadeTime);
}

//~ Banner and toasts

void UBeyondWorldSubsystem::ShowBanner(const UBeyondRegionDefinition* Region, bool bFirstVisit, float Exp)
{
	const ABeyondPlayerController* PC = GetPlayerController();
	UBeyondRegionBannerWidget* Banner = PC ? Cast<UBeyondRegionBannerWidget>(PC->GetRegionBannerWidget()) : nullptr;
	if (!Region || !Banner)
	{
		return;
	}
	FText Subtitle = Region->Subtitle;
	if (Subtitle.IsEmpty())
	{
		const UBeyondRegionDefinition* Parent = FindRegionById(Region->ParentId);
		Subtitle = Parent ? Parent->GetDisplayNameOrId() : FText::GetEmpty();
	}
	const FText Detail = Region->Kind == EBeyondRegionKind::Region ? Region->GetLevelText() : FText::GetEmpty();
	const FText Reward = bFirstVisit
		? (Exp > 0.0f ? FText::Format(LOCTEXT("Discovered", "Discovered  ·  +{0} EXP"), FMath::RoundToInt(Exp)) : LOCTEXT("DiscoveredNoExp", "Discovered"))
		: FText::GetEmpty();
	Banner->ShowRegion(Region->GetDisplayNameOrId(), Subtitle, Detail, Reward, Region->MapColour);
}

void UBeyondWorldSubsystem::ShowToast(const FText& Title, const FText& Detail, const FLinearColor& Colour)
{
	const ABeyondPlayerController* PC = GetPlayerController();
	if (UBeyondRegionBannerWidget* Banner = PC ? Cast<UBeyondRegionBannerWidget>(PC->GetRegionBannerWidget()) : nullptr)
	{
		Banner->ShowToast(Title, Detail, Colour);
	}
}

//~ Discovery and the map

void UBeyondWorldSubsystem::DiscoverEverything()
{
	UBeyondPartyComponent* Party = GetParty(this);
	if (!Party)
	{
		return;
	}
	for (const ABeyondRegionVolume* Volume : GetRegionVolumes())
	{
		if (Volume->Region)
		{
			Party->DiscoverRegion(Volume->Region->RegionId);
		}
	}
	for (const ABeyondPointOfInterest* Place : GetPlaces())
	{
		Party->DiscoverPlace(Place->PoiId);
	}
	for (ABeyondWaystone* Waystone : GetWaystones())
	{
		Party->DiscoverPlace(Waystone->WaystoneId);
		Party->AttuneWaystone(Waystone->WaystoneId);
	}
	UpdateWaystoneLooks();
	Party->SaveProgress();
}

TArray<FBeyondMapMarker> UBeyondWorldSubsystem::GetMapMarkers() const
{
	TArray<FBeyondMapMarker> Markers;
	const UBeyondPartyComponent* Party = GetParty(this);
	auto IsRegionKnown = [Party](const UBeyondRegionDefinition* Region)
	{
		return Region && Party && Party->IsRegionDiscovered(Region->RegionId);
	};

	for (const ABeyondRegionVolume* Volume : GetRegionVolumes())
	{
		const UBeyondRegionDefinition* Region = Volume->Region;
		if (!Region)
		{
			continue;
		}
		FBeyondMapMarker& Marker = Markers.AddDefaulted_GetRef();
		Marker.Kind = Region->Kind == EBeyondRegionKind::Region ? EBeyondMapMarkerKind::Region
			: Region->Kind == EBeyondRegionKind::Village ? EBeyondMapMarkerKind::Village : EBeyondMapMarkerKind::Area;
		Marker.Id = Region->RegionId;
		Marker.Label = Region->GetDisplayNameOrId();
		Marker.Detail = Region->Kind == EBeyondRegionKind::Region ? Region->GetLevelText() : FText::GetEmpty();
		Marker.Location = Volume->GetCentre();
		Marker.bDiscovered = IsRegionKnown(Region);
		Marker.Colour = Region->MapColour;
	}
	for (const ABeyondWaystone* Waystone : GetWaystones())
	{
		FBeyondMapMarker& Marker = Markers.AddDefaulted_GetRef();
		Marker.Kind = EBeyondMapMarkerKind::Waystone;
		Marker.Id = Waystone->WaystoneId;
		Marker.Label = Waystone->GetDisplayNameOrId();
		Marker.Location = Waystone->GetActorLocation();
		Marker.bDiscovered = IsWaystoneDiscovered(Waystone);
		Marker.bActive = IsWaystoneAttuned(Waystone);
		Marker.Colour = Waystone->GlowColour;
	}
	for (const ABeyondPointOfInterest* Place : GetPlaces())
	{
		FBeyondMapMarker& Marker = Markers.AddDefaulted_GetRef();
		Marker.Kind = EBeyondMapMarkerKind::Place;
		Marker.Id = Place->PoiId;
		Marker.Label = Place->GetDisplayNameOrId();
		Marker.Detail = ABeyondPointOfInterest::GetKindName(Place->Kind);
		Marker.Location = Place->GetActorLocation();
		Marker.bDiscovered = Party && Party->IsPlaceDiscovered(Place->PoiId);
		// Shown as "?" before it is found
		Marker.bActive = Place->bVisibleBeforeDiscovery;
		Marker.Colour = FLinearColor(1.0f, 0.78f, 0.3f);
	}
	for (const ABeyondBossArena* Arena : GetArenas())
	{
		const UBeyondBossDefinition* Boss = Arena->Boss;
		FBeyondMapMarker& Marker = Markers.AddDefaulted_GetRef();
		Marker.Kind = EBeyondMapMarkerKind::Arena;
		Marker.Id = Arena->ArenaId.IsNone() ? (Boss ? Boss->BossId : Arena->GetFName()) : Arena->ArenaId;
		Marker.Label = Boss ? Boss->DisplayName : LOCTEXT("EmptyArena", "Arena");
		Marker.Detail = Boss ? Boss->Title : FText::GetEmpty();
		Marker.Location = Arena->GetActorLocation();
		Marker.bActive = Arena->IsDefeated();
		Marker.bDiscovered = Marker.bActive || IsRegionKnown(FindRegionAt(Marker.Location, false));
		Marker.Colour = FLinearColor(0.95f, 0.25f, 0.2f);
	}
	if (Party)
	{
		const ABeyondCharacterBase* Leader = Party->GetLeader();
		for (const ABeyondCharacterBase* Member : Party->GetMembers())
		{
			FBeyondMapMarker& Marker = Markers.AddDefaulted_GetRef();
			Marker.Kind = Member == Leader ? EBeyondMapMarkerKind::Leader : EBeyondMapMarkerKind::Companion;
			Marker.Id = Member->GetFName();
			Marker.Label = Member->GetCharacterDisplayName();
			Marker.Location = Member->GetActorLocation();
			Marker.Yaw = Member->GetActorRotation().Yaw;
			Marker.bDiscovered = true;
			Marker.Colour = Member->DuoRole == EBeyondDuoRole::Conduit ? FLinearColor(0.45f, 0.4f, 1.0f) : FLinearColor(1.0f, 0.75f, 0.25f);
		}
	}
	return Markers;
}

#undef LOCTEXT_NAMESPACE
