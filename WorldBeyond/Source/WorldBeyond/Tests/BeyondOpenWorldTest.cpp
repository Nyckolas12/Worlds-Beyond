// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AI/BeyondEnemyController.h"
#include "Characters/BeyondCharacterBase.h"
#include "Dialogue/BeyondNPCCharacter.h"
#include "Editor.h"
#include "Enemies/BeyondBossCharacter.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemySpawner.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Game/BeyondBossArena.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/BeyondWorldMapWidget.h"
#include "World/BeyondRegionDefinition.h"
#include "World/BeyondRegionVolume.h"
#include "World/BeyondWaystone.h"
#include "World/BeyondWorldSubsystem.h"

/**
 * Plan 5B's open world in Play In Editor (/Game/WorldsBeyond/Maps/Dominion, World Partition; passes only warn when the
 * map isn't built yet): the start hold lands the party on the ground at Mossbrook's player start, the map knows its 4
 * regions, 14 villages, 22 waystones and 10 arenas (4 with bosses), the leader is in the Elderwood at Mossbrook, the
 * world map has its picture, Mossbrook has its houses and its seven people on the ground (pass 17; only a warning before
 * it), Beyond.Travel to Frostholm lands on the ground in the Rimewood Reach, a camp puts its
 * enemies away when the party leaves and has exactly as many when it comes back, Gorehide's arena has its boss only
 * while the party is near and exactly one after leaving and coming back.
 *
 * UnrealEditor-Cmd.exe WorldBeyond.uproject -ExecCmds="Automation RunTests WorldsBeyond.Prototype.OpenWorld;Quit" -unattended -nullrhi -nosplash
 */

namespace BeyondOpenWorldTest
{
	const TCHAR* MapPath = TEXT("/Game/WorldsBeyond/Maps/Dominion");

	struct FState
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ABeyondPlayerController> PC;
		TWeakObjectPtr<ABeyondEnemySpawner> Camp;
		TWeakObjectPtr<ABeyondBossArena> Arena;
		int32 CampCount = 0;
		FString SavedSaveProgress;
	};

	UWorld* PlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	UBeyondWorldSubsystem* World()
	{
		return UBeyondWorldSubsystem::Get(PlayWorld());
	}

	ABeyondCharacterBase* Leader(const FState& State)
	{
		return State.PC.IsValid() ? State.PC->PartyComponent->GetLeader() : nullptr;
	}

	bool OnGround(const ABeyondCharacterBase* Member)
	{
		const UCharacterMovementComponent* Movement = Member ? Member->GetCharacterMovement() : nullptr;
		return Movement && Movement->MovementMode == MOVE_Walking;
	}

	bool PartySettled(const FState& State)
	{
		const UBeyondWorldSubsystem* Subsystem = World();
		const ABeyondCharacterBase* Who = Leader(State);
		return Subsystem && Who && !Subsystem->IsTravelling() && OnGround(Who);
	}

	TFunction<bool()> WaitUntil(TSharedRef<FState> State, TFunction<bool()> Done, float Timeout, FString What)
	{
		TSharedRef<double> Started = MakeShared<double>(-1.0);
		return [State, Done, Timeout, What, Started]() -> bool
		{
			const UWorld* W = PlayWorld();
			if (!W)
			{
				return true;
			}
			const double Now = W->GetRealTimeSeconds();
			if (*Started < 0.0)
			{
				*Started = Now;
			}
			if (Done())
			{
				return true;
			}
			if (Now - *Started > Timeout)
			{
				State->Test->AddError(FString::Printf(TEXT("Timed out after %.0f s: %s"), Timeout, *What));
				return true;
			}
			return false;
		};
	}

	int32 AliveNear(const FVector& Where, float Radius)
	{
		int32 Count = 0;
		for (TActorIterator<ABeyondEnemyCharacter> It(PlayWorld()); It; ++It)
		{
			if (!It->IsA<ABeyondBossCharacter>() && !UBeyondCombatLibrary::IsActorDead(*It) && FVector::Dist2D(It->GetActorLocation(), Where) <= Radius)
			{
				++Count;
			}
		}
		return Count;
	}

	int32 BossesOf(const ABeyondBossArena* Arena)
	{
		int32 Count = 0;
		for (TActorIterator<ABeyondBossCharacter> It(PlayWorld()); It; ++It)
		{
			Count += It->GetArena() == Arena && !UBeyondCombatLibrary::IsActorDead(*It) && !It->IsActorBeingDestroyed() ? 1 : 0;
		}
		return Count;
	}

	void TravelNear(const AActor* Target, float Distance)
	{
		if (UBeyondWorldSubsystem* Subsystem = World(); Subsystem && Target)
		{
			Subsystem->TravelPartyTo(FTransform(Target->GetActorLocation() + FVector(0.0f, -Distance, 300.0f)), EBeyondTravelReason::Cheat);
		}
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondOpenWorldStep, TFunction<bool()>, Step);
bool FBeyondOpenWorldStep::Update() { return Step(); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondOpenWorldTest, "WorldsBeyond.Prototype.OpenWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondOpenWorldTest::RunTest(const FString& Parameters)
{
	using namespace BeyondOpenWorldTest;
	if (!FPackageName::DoesPackageExist(MapPath))
	{
		AddWarning(TEXT("The open world map isn't built yet (run migrate_pass15.py and migrate_pass16.py); nothing to test"));
		return true;
	}
	TSharedRef<FState> State = MakeShared<FState>();
	State->Test = this;
	if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
	{
		State->SavedSaveProgress = SaveProgress->GetString();
		SaveProgress->Set(TEXT("0"), ECVF_SetByCode);
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(MapPath));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep([State]()
	{
		UWorld* W = PlayWorld();
		State->PC = W ? Cast<ABeyondPlayerController>(W->GetFirstPlayerController()) : nullptr;
		State->Test->TestTrue(TEXT("Beyond player controller"), State->PC.IsValid());
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]() { return PartySettled(*State); }, 45.0f,
		TEXT("the start hold landing the party on the ground"))));

	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* W = PlayWorld();
		UBeyondWorldSubsystem* Subsystem = World();
		ABeyondCharacterBase* Who = Leader(*State);
		if (!W || !Subsystem || !Who)
		{
			return true;
		}
		TActorIterator<APlayerStart> Start(W);
		if (T.TestTrue(TEXT("The map has a player start"), static_cast<bool>(Start)))
		{
			T.TestTrue(*FString::Printf(TEXT("The leader starts at Mossbrook's player start (%.0f uu away)"), FVector::Dist2D(Who->GetActorLocation(), Start->GetActorLocation())),
				FVector::Dist2D(Who->GetActorLocation(), Start->GetActorLocation()) < 1500.0f);
		}
		T.TestEqual(TEXT("Both demigods are in the party"), State->PC->PartyComponent->GetMembers().Num(), 2);

		int32 Regions = 0;
		int32 Villages = 0;
		for (const ABeyondRegionVolume* Volume : Subsystem->GetRegionVolumes())
		{
			Regions += Volume->Region && Volume->Region->Kind == EBeyondRegionKind::Region ? 1 : 0;
			Villages += Volume->Region && Volume->Region->Kind == EBeyondRegionKind::Village ? 1 : 0;
		}
		T.TestEqual(TEXT("4 regions"), Regions, 4);
		T.TestEqual(TEXT("14 villages (a main village and two or three small ones per region)"), Villages, 14);
		T.TestEqual(TEXT("22 waystones"), Subsystem->GetWaystones().Num(), 22);
		int32 Bosses = 0;
		for (ABeyondBossArena* Arena : Subsystem->GetArenas())
		{
			Bosses += Arena->Boss ? 1 : 0;
			if (Arena->ArenaId == TEXT("gorehide_den"))
			{
				State->Arena = Arena;
			}
		}
		T.TestEqual(TEXT("10 arenas"), Subsystem->GetArenas().Num(), 10);
		T.TestEqual(TEXT("4 of them with a boss"), Bosses, 4);
		T.TestTrue(TEXT("Mossbrook's waystone starts attuned"), Subsystem->IsWaystoneAttuned(Subsystem->FindWaystone(TEXT("mossbrook"))));

		Subsystem->ScanNow();
		const UBeyondRegionDefinition* Region = Subsystem->GetCurrentRegion();
		const UBeyondRegionDefinition* Village = Subsystem->GetCurrentVillage();
		T.TestTrue(TEXT("The party starts in the Elderwood"), Region && Region->RegionId == TEXT("region_forest"));
		T.TestTrue(TEXT("...in Mossbrook"), Village && Village->RegionId == TEXT("village_mossbrook"));

		// Pass 17's village round the party: its houses, and its people standing on the ground
		const FVector Here = State->PC->GetPawn() ? State->PC->GetPawn()->GetActorLocation() : FVector::ZeroVector;
		int32 Houses = 0;
		int32 People = 0;
		int32 Standing = 0;
		for (TActorIterator<AActor> It(PlayWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Here) > 9000.0f)
			{
				continue;
			}
			if (It->GetActorLabel().StartsWith(TEXT("WB_VIL_village_mossbrook_Home_")))
			{
				++Houses;
			}
			if (const ABeyondNPCCharacter* NPC = Cast<ABeyondNPCCharacter>(*It))
			{
				++People;
				Standing += FMath::Abs(NPC->GetActorLocation().Z - Here.Z) < 600.0f ? 1 : 0;
			}
		}
		if (Houses == 0)
		{
			T.AddWarning(TEXT("Mossbrook has no houses yet: run migrate_pass17.py"));
		}
		else
		{
			T.TestTrue(*FString::Printf(TEXT("Mossbrook's houses are there (%d)"), Houses), Houses >= 8);
			T.TestTrue(*FString::Printf(TEXT("Mossbrook's seven people are there (%d)"), People), People >= 7);
			T.TestEqual(TEXT("...standing on the ground, none fallen through"), Standing, People);
		}

		State->PC->OpenWorldMap();
		const UBeyondWorldMapWidget* Map = Cast<UBeyondWorldMapWidget>(State->PC->GetWorldMapWidget());
		T.TestTrue(TEXT("The world map has its picture"), Map && Map->GetMapTexture() != nullptr);
		return true;
	}));
	// Open for a moment, so a run with rendering draws it (the region outlines once crashed the paint)
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep([State]()
	{
		if (ABeyondPlayerController* PC = State->PC.Get())
		{
			State->Test->TestTrue(TEXT("The map stayed open while drawn"), PC->IsWorldMapOpen());
			PC->CloseWorldMap();
		}
		if (GEngine)
		{
			GEngine->Exec(PlayWorld(), TEXT("Beyond.Travel frostholm"));
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]() { return PartySettled(*State); }, 40.0f, TEXT("travel to Frostholm"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UBeyondWorldSubsystem* Subsystem = World();
		const ABeyondWaystone* Frostholm = Subsystem ? Subsystem->FindWaystone(TEXT("frostholm")) : nullptr;
		const ABeyondCharacterBase* Who = Leader(*State);
		if (!Subsystem || !Frostholm || !Who)
		{
			return true;
		}
		T.TestTrue(TEXT("Arrived at Frostholm"), FVector::Dist2D(Who->GetActorLocation(), Frostholm->GetArrivalTransform().GetLocation()) < 400.0f);
		Subsystem->ScanNow();
		T.TestTrue(TEXT("...in the Rimewood Reach"), Subsystem->GetCurrentRegion() && Subsystem->GetCurrentRegion()->RegionId == TEXT("region_frost"));

		// The nearest camp
		ABeyondEnemySpawner* Best = nullptr;
		float BestDistance = TNumericLimits<float>::Max();
		for (TActorIterator<ABeyondEnemySpawner> It(PlayWorld()); It; ++It)
		{
			const float Distance = FVector::Dist2D(It->GetActorLocation(), Who->GetActorLocation());
			if (Distance < BestDistance)
			{
				Best = *It;
				BestDistance = Distance;
			}
		}
		State->Camp = Best;
		T.TestNotNull(TEXT("A camp near Frostholm"), Best);
		TravelNear(Best, 2000.0f);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]() { return PartySettled(*State); }, 40.0f, TEXT("travel to the camp"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]()
	{
		return State->Camp.IsValid() && State->Camp->IsActive() && !State->Camp->GetSpawnedEnemies().IsEmpty();
	}, 15.0f, TEXT("the camp putting its enemies out"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep([State]()
	{
		ABeyondEnemySpawner* Camp = State->Camp.Get();
		if (!Camp)
		{
			return true;
		}
		for (ABeyondEnemyCharacter* Enemy : Camp->GetSpawnedEnemies())
		{
			if (ABeyondEnemyController* Brain = Cast<ABeyondEnemyController>(Enemy->GetController()))
			{
				Brain->SetBrainEnabled(false);
				Brain->ReturnHome(true);
			}
		}
		State->CampCount = Camp->GetSpawnedEnemies().Num();
		if (GEngine)
		{
			GEngine->Exec(PlayWorld(), TEXT("Beyond.Travel mossbrook"));
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]() { return PartySettled(*State); }, 40.0f, TEXT("travel back to Mossbrook"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]() { return State->Camp.IsValid() && !State->Camp->IsActive(); }, 5.0f,
		TEXT("the camp going quiet with the party far"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		if (ABeyondEnemySpawner* Camp = State->Camp.Get())
		{
			T.TestEqual(TEXT("The far camp's enemies are put away"), AliveNear(Camp->GetActorLocation(), 2500.0f), 0);
			TravelNear(Camp, 2000.0f);
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]() { return PartySettled(*State); }, 40.0f, TEXT("travel to the camp again"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]()
	{
		return State->Camp.IsValid() && State->Camp->GetSpawnedEnemies().Num() >= State->CampCount;
	}, 15.0f, TEXT("the camp's enemies coming back"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondEnemySpawner* Camp = State->Camp.Get();
		if (!Camp)
		{
			return true;
		}
		T.TestEqual(TEXT("The camp has exactly as many enemies again (no duplicates)"), AliveNear(Camp->GetActorLocation(), 2500.0f), State->CampCount);
		for (ABeyondEnemyCharacter* Enemy : Camp->GetSpawnedEnemies())
		{
			if (ABeyondEnemyController* Brain = Cast<ABeyondEnemyController>(Enemy->GetController()))
			{
				Brain->SetBrainEnabled(false);
			}
		}
		// Gorehide's arena
		TravelNear(State->Arena.Get(), 8000.0f);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]() { return PartySettled(*State); }, 40.0f, TEXT("travel near Gorehide's den"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]() { return State->Arena.IsValid() && State->Arena->GetBoss(); }, 15.0f,
		TEXT("Gorehide appearing with the party near"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep([State]()
	{
		State->Test->TestEqual(TEXT("One Gorehide"), BossesOf(State->Arena.Get()), 1);
		if (GEngine)
		{
			GEngine->Exec(PlayWorld(), TEXT("Beyond.Travel mossbrook"));
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]() { return PartySettled(*State); }, 40.0f, TEXT("travel back to Mossbrook"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]() { return State->Arena.IsValid() && !State->Arena->GetBoss(); }, 5.0f,
		TEXT("Gorehide going away with the party far"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep([State]()
	{
		TravelNear(State->Arena.Get(), 8000.0f);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]() { return PartySettled(*State); }, 40.0f, TEXT("travel near Gorehide's den again"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]() { return State->Arena.IsValid() && State->Arena->GetBoss(); }, 15.0f,
		TEXT("Gorehide appearing again"))));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep([State]()
	{
		State->Test->TestEqual(TEXT("Still exactly one Gorehide"), BossesOf(State->Arena.Get()), 1);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep([State]()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			return false;
		}
		if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
		{
			SaveProgress->Set(State->SavedSaveProgress.IsEmpty() ? TEXT("1") : *State->SavedSaveProgress, ECVF_SetByCode);
		}
		return true;
	}));
	return true;
}

#endif
