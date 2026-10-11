// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AI/BeyondEnemyController.h"
#include "BeyondGameplayTags.h"
#include "Characters/BeyondCharacterBase.h"
#include "Dialogue/BeyondBanterComponent.h"
#include "Dialogue/BeyondDialogueSubsystem.h"
#include "Editor.h"
#include "Enemies/BeyondBossCharacter.h"
#include "Enemies/BeyondBossDefinition.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Enemies/BeyondEnemySettings.h"
#include "Enemies/BeyondEnemySpawner.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Game/BeyondBossArena.h"
#include "Game/BeyondSaveGame.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Sound/SoundBase.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/BeyondInteractPromptWidget.h"
#include "UI/BeyondRegionBannerWidget.h"
#include "UI/BeyondWorldMapWidget.h"
#include "World/BeyondHazardVolume.h"
#include "World/BeyondPointOfInterest.h"
#include "World/BeyondRegionVolume.h"
#include "World/BeyondWaystone.h"
#include "World/BeyondWorldDemo.h"
#include "World/BeyondWorldInfo.h"
#include "World/BeyondWorldSubsystem.h"

/**
 * Plan 5's world systems in the test arena (Play In Editor), on a small open-world setup spawned at runtime in the
 * east of the map (the map itself isn't changed): region and village detection (priority), the banner, discovery
 * (saved, EXP once), region banter, weather and music, places and waystones (discover, attune with F's logic, rest),
 * fast travel from the world map (both demigods land on the ground) and when it is refused (not attuned, in a fight,
 * a demigod down, sealed in a boss arena), a spawner using the region's level band that puts its camp away when the
 * party leaves and keeps the fallen down (Never), a boss arena that only has its boss while the party is near and
 * remembers a defeat, lava and deep water, the console's Beyond.Travel / Beyond.ResetWorld, save v6.
 * Saving is off for the run (Beyond.SaveProgress 0).
 *
 * UnrealEditor-Cmd.exe WorldBeyond.uproject -ExecCmds="Automation RunTests WorldsBeyond.Prototype.World;Quit" -unattended -nullrhi -nosplash
 */

namespace BeyondWorldTest
{
	// The setup, in the free east of the test arena (the corrupted-woods banter volume ends at x 4100)
	const FVector RegionCentre(4600.0f, 600.0f, 0.0f);
	const FVector VillageCentre(5000.0f, 1300.0f, 0.0f);
	const FVector Outside(2200.0f, -1500.0f, 0.0f);
	const FVector InRegion(4000.0f, 0.0f, 0.0f);
	const FVector NearWaystone(3700.0f, -200.0f, 0.0f);
	const FVector FarWaystone(0.0f, 4600.0f, 0.0f);
	const FVector PlaceSpot(5300.0f, -300.0f, 0.0f);
	const FVector SpawnerSpot(4700.0f, 1900.0f, 0.0f);
	const FVector NearSpawner(4300.0f, 1900.0f, 0.0f);
	const FVector ArenaSpot(0.0f, 3000.0f, 0.0f);
	const FVector LavaSpot(5700.0f, 600.0f, 0.0f);
	const FVector WaterSpot(5700.0f, -200.0f, 0.0f);

	struct FState
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ABeyondPlayerController> PC;
		TWeakObjectPtr<ABeyondCharacterBase> Angel;
		TWeakObjectPtr<ABeyondCharacterBase> JiWoong;
		BeyondWorldDemo::FDemoActors Demo;
		TWeakObjectPtr<UBeyondRegionDefinition> RegionDef;
		TWeakObjectPtr<UBeyondRegionDefinition> VillageDef;
		TWeakObjectPtr<ABeyondEnemySpawner> Spawner;
		TWeakObjectPtr<ABeyondBossArena> Arena;
		TWeakObjectPtr<ABeyondEnemyCharacter> Wolf;
		TWeakObjectPtr<USoundBase> RegionMusic;
		TArray<TWeakObjectPtr<AActor>> Cleanup;
		FString SavedSaveProgress;
		int32 LevelBefore = 0;
		float ExperienceBefore = 0.0f;
		float HealthBefore = 0.0f;
	};

	UWorld* PlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	UBeyondWorldSubsystem* World()
	{
		return UBeyondWorldSubsystem::Get(PlayWorld());
	}

	UBeyondPartyComponent* Party(const FState& State)
	{
		return State.PC.IsValid() ? State.PC->PartyComponent.Get() : nullptr;
	}

	void PlaceAt(ACharacter* Who, const FVector& Where)
	{
		if (Who)
		{
			Who->SetActorLocation(Where + FVector(0.0f, 0.0f, 120.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	// Both demigods (the companion would still count as near otherwise)
	void PlaceParty(const FState& State, const FVector& Where)
	{
		if (UBeyondPartyComponent* P = Party(State))
		{
			P->TeleportPartyTo(FTransform(Where + FVector(0.0f, 0.0f, 120.0f)), false);
		}
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
			// Real time: the map pauses the game
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

	bool OnGround(const ABeyondCharacterBase* Member)
	{
		const UCharacterMovementComponent* Movement = Member ? Member->GetCharacterMovement() : nullptr;
		return Movement && Movement->MovementMode == MOVE_Walking;
	}

	bool ProgressChanged(const FState& State)
	{
		const ABeyondCharacterBase* Angel = State.Angel.Get();
		return Angel && (Angel->GetCharacterLevel() != State.LevelBefore || !FMath::IsNearlyEqual(Angel->GetExperience(), State.ExperienceBefore));
	}

	void RememberProgress(FState& State)
	{
		if (const ABeyondCharacterBase* Angel = State.Angel.Get())
		{
			State.LevelBefore = Angel->GetCharacterLevel();
			State.ExperienceBefore = Angel->GetExperience();
		}
	}

	UBeyondRegionBannerWidget* Banner(const FState& State)
	{
		return State.PC.IsValid() ? Cast<UBeyondRegionBannerWidget>(State.PC->GetRegionBannerWidget()) : nullptr;
	}

	bool ToastSays(const FState& State, const FString& Text)
	{
		const UBeyondRegionBannerWidget* Widget = Banner(State);
		if (!Widget)
		{
			return false;
		}
		for (const FText& Title : Widget->GetToastTitles())
		{
			if (Title.ToString().Contains(Text))
			{
				return true;
			}
		}
		return false;
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondWorldStep, TFunction<bool()>, Step);
bool FBeyondWorldStep::Update() { return Step(); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondWorldTest, "WorldsBeyond.Prototype.World",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondWorldTest::RunTest(const FString& Parameters)
{
	using namespace BeyondWorldTest;
	TSharedRef<FState> State = MakeShared<FState>();
	State->Test = this;

	if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
	{
		State->SavedSaveProgress = SaveProgress->GetString();
		SaveProgress->Set(TEXT("0"), ECVF_SetByCode);
	}

	// Save v6 keeps the open world (no PIE needed)
	{
		UBeyondSaveGame* Save = NewObject<UBeyondSaveGame>();
		Save->DiscoveredRegions = { TEXT("region_forest") };
		Save->DiscoveredPlaces = { TEXT("demo_shrine"), TEXT("w1") };
		Save->AttunedWaystones = { TEXT("w1") };
		Save->LastWaystone = TEXT("w1");
		TArray<uint8> Bytes;
		const UBeyondSaveGame* Loaded = UGameplayStatics::SaveGameToMemory(Save, Bytes) ? Cast<UBeyondSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)) : nullptr;
		TestTrue(TEXT("Save v6 keeps discovered regions, places, attuned waystones and the last waystone"), Loaded && Loaded->Version == 6
			&& Loaded->DiscoveredRegions.Num() == 1 && Loaded->DiscoveredPlaces.Num() == 2 && Loaded->AttunedWaystones.Contains(TEXT("w1"))
			&& Loaded->LastWaystone == TEXT("w1"));
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/WorldsBeyond/Maps/TestArena")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.0f));

	// Setup: the party, no enemies in the way, the world setup in the east
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* W = PlayWorld();
		ABeyondPlayerController* PC = W ? Cast<ABeyondPlayerController>(W->GetFirstPlayerController()) : nullptr;
		if (!T.TestNotNull(TEXT("Beyond player controller"), PC) || !T.TestNotNull(TEXT("World subsystem"), World()))
		{
			return true;
		}
		State->PC = PC;
		UBeyondPartyComponent* P = PC->PartyComponent;
		for (ABeyondCharacterBase* Member : P->GetMembers())
		{
			if (Member->DuoRole == EBeyondDuoRole::Conduit) { State->Angel = Member; }
			if (Member->DuoRole == EBeyondDuoRole::Striker) { State->JiWoong = Member; }
		}
		if (!T.TestNotNull(TEXT("Angel"), State->Angel.Get()) || !T.TestNotNull(TEXT("Ji-Woong"), State->JiWoong.Get()))
		{
			return true;
		}
		if (P->GetLeader() != State->Angel.Get())
		{
			P->SwapLeader();
		}
		P->ResetStoryFlags();
		P->ResetWorldProgress();
		World()->ForgetBanners();

		// The test arena's camps and bosses would join in
		if (UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(W))
		{
			for (ABeyondEnemyCharacter* Enemy : Enemies->GetLiveEnemies())
			{
				Enemy->Destroy();
			}
		}

		UBeyondRegionDefinition* Region = BeyondWorldDemo::MakeRegion(TEXT("region_forest"), FText::FromString(TEXT("Test Elderwood")), EBeyondRegionKind::Region, 1, 6);
		Region->Subtitle = FText::FromString(TEXT("The Wandering Dominion"));
		Region->Weather.FogDensity = 0.123f;
		USoundBase* Music = LoadObject<USoundBase>(nullptr, TEXT("/Game/WorldsBeyond/Sounds/temple-ambient-140781.temple-ambient-140781"));
		Region->Music = Music;
		State->RegionMusic = Music;
		UBeyondRegionDefinition* Village = BeyondWorldDemo::MakeRegion(TEXT("test_village"), FText::FromString(TEXT("Test Hamlet")), EBeyondRegionKind::Village, 2, 4,
			Region->RegionId);
		State->RegionDef = Region;
		State->VillageDef = Village;

		BeyondWorldDemo::FDemoActors& Demo = State->Demo;
		Demo.WorldInfo = BeyondWorldDemo::SpawnWorldInfo(W, RegionCentre + FVector(0.0f, 0.0f, 300.0f));
		Demo.Region = BeyondWorldDemo::SpawnRegionVolume(W, Region, RegionCentre, 1500.0f, 0);
		Demo.Village = BeyondWorldDemo::SpawnRegionVolume(W, Village, VillageCentre, 400.0f, 10);
		Demo.Near = BeyondWorldDemo::SpawnWaystone(W, TEXT("w1"), FText::FromString(TEXT("Test Stone")), NearWaystone, 0.0f);
		Demo.Far = BeyondWorldDemo::SpawnWaystone(W, TEXT("w2"), FText::FromString(TEXT("Far Stone")), FarWaystone, 0.0f);
		Demo.Place = BeyondWorldDemo::SpawnPlace(W, TEXT("test_shrine"), FText::FromString(TEXT("Test Shrine")), PlaceSpot, 300.0f);
		Demo.Lava = BeyondWorldDemo::SpawnHazard(W, EBeyondHazardKind::Burn, LavaSpot + FVector(0.0f, 0.0f, 100.0f), FVector(150.0f, 150.0f, 200.0f));
		Demo.DeepWater = BeyondWorldDemo::SpawnHazard(W, EBeyondHazardKind::DeepWater, WaterSpot + FVector(0.0f, 0.0f, 100.0f), FVector(150.0f, 150.0f, 200.0f));
		for (const TWeakObjectPtr<AActor>& Actor : Demo.All())
		{
			State->Cleanup.Add(Actor);
		}

		// A camp at the region's level band (Level 0, +2): forest band 1-6 -> level 3
		const FTransform CampTransform(SpawnerSpot);
		if (ABeyondEnemySpawner* Spawner = W->SpawnActorDeferred<ABeyondEnemySpawner>(ABeyondEnemySpawner::StaticClass(), CampTransform))
		{
			FBeyondSpawnEntry& Entry = Spawner->Entries.AddDefaulted_GetRef();
			const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
			Entry.Enemy = Roster ? Roster->FindEnemy(TEXT("forest_wolf")) : nullptr;
			Entry.Count = 1;
			Entry.Level = 0;
			Spawner->LevelOffset = 2;
			Spawner->EliteChance = 0.0f;
			Spawner->ActivationRadius = 900.0f;
			Spawner->DeactivationRadius = 1800.0f;
			Spawner->Respawn = EBeyondRespawnRule::Never;
			Spawner->FinishSpawning(CampTransform);
			State->Spawner = Spawner;
			State->Cleanup.Add(Spawner);
			T.TestNotNull(TEXT("The roster has forest_wolf"), Entry.Enemy.Get());
		}

		// Gorehide's arena north: the boss only while the party is within 22 m, remembered once beaten
		const FTransform ArenaTransform(ArenaSpot + FVector(0.0f, 0.0f, 20.0f));
		if (ABeyondBossArena* Arena = W->SpawnActorDeferred<ABeyondBossArena>(ABeyondBossArena::StaticClass(), ArenaTransform))
		{
			const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
			Arena->Boss = Roster ? Cast<UBeyondBossDefinition>(Roster->FindEnemy(TEXT("boss_gorehide"))) : nullptr;
			Arena->ArenaId = TEXT("test_arena");
			Arena->BossLevel = 3;
			Arena->ArenaRadius = 900.0f;
			Arena->EngageRadius = 500.0f;
			Arena->bRememberDefeat = true;
			Arena->BossSpawnRadius = 2200.0f;
			Arena->BossDespawnRadius = 3500.0f;
			Arena->FinishSpawning(ArenaTransform);
			State->Arena = Arena;
			State->Cleanup.Add(Arena);
			T.TestNotNull(TEXT("The roster has boss_gorehide"), Arena->Boss.Get());
		}

		PlaceParty(*State, Outside);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.6f));

	// Outside every region; the registry
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UBeyondWorldSubsystem* Subsystem = World();
		if (!Subsystem)
		{
			return true;
		}
		Subsystem->ScanNow();
		T.TestTrue(TEXT("The world knows the world info"), Subsystem->IsOpenWorld());
		T.TestTrue(TEXT("Two region volumes registered"), Subsystem->GetRegionVolumes().Num() >= 2);
		T.TestTrue(TEXT("Two waystones registered"), Subsystem->FindWaystone(TEXT("w1")) && Subsystem->FindWaystone(TEXT("w2")));
		T.TestEqual(TEXT("One place registered"), Subsystem->GetPlaces().Num(), 1);
		T.TestTrue(TEXT("The new arena registered"), State->Arena.IsValid() && Subsystem->GetArenas().Contains(State->Arena.Get()));
		T.TestNull(TEXT("Outside: no region"), Subsystem->GetCurrentRegion());
		int32 Min = 0;
		int32 Max = 0;
		T.TestTrue(TEXT("The level band in the region is 1-6"), Subsystem->GetLevelBandAt(InRegion, Min, Max) && Min == 1 && Max == 6);
		T.TestTrue(TEXT("The village's own band wins inside it (2-4)"), Subsystem->GetLevelBandAt(VillageCentre, Min, Max) && Min == 2 && Max == 4);
		T.TestFalse(TEXT("No band outside"), Subsystem->GetLevelBandAt(Outside, Min, Max));
		T.TestNull(TEXT("The arena's boss isn't there while the party is far"), State->Arena.IsValid() ? State->Arena->GetBoss() : nullptr);
		T.TestFalse(TEXT("The spawner waits for the party"), State->Spawner.IsValid() && State->Spawner->IsActive());

		RememberProgress(*State);
		if (ABeyondPlayerController* PC = State->PC.Get())
		{
			PC->BanterComponent->bBanterEnabled = true;
			PC->BanterComponent->ResetCooldowns();
		}
		PlaceAt(State->Angel.Get(), InRegion);
		Subsystem->ScanNow();
		return true;
	}));

	// Into the region: banner, discovery and EXP, banter, weather, music; the near waystone is found
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UBeyondWorldSubsystem* Subsystem = World();
		UBeyondPartyComponent* P = Party(*State);
		if (!Subsystem || !P)
		{
			return true;
		}
		T.TestTrue(TEXT("In the region"), Subsystem->GetCurrentRegion() == State->RegionDef.Get());
		T.TestNull(TEXT("Not in the village"), Subsystem->GetCurrentVillage());
		T.TestTrue(TEXT("The region is discovered (saved set)"), P->IsRegionDiscovered(TEXT("region_forest")));
		T.TestTrue(TEXT("Discovered.region_forest answers as a story flag"), P->HasStoryFlag(TEXT("Discovered.region_forest")));
		T.TestTrue(TEXT("Discovery gave EXP"), ProgressChanged(*State));
		const UBeyondRegionBannerWidget* Widget = Banner(*State);
		T.TestTrue(TEXT("The banner shows the region's name"), Widget && Widget->IsBannerShowing() && Widget->GetBannerTitle().ToString() == TEXT("Test Elderwood"));
		T.TestTrue(TEXT("...and says it was discovered"), Widget && Widget->GetBannerReward().ToString().Contains(TEXT("Discovered")));
		const ABeyondWorldInfo* Info = State->Demo.WorldInfo.Get();
		T.TestTrue(TEXT("The weather blends to the region's"), Info && FMath::IsNearlyEqual(Info->GetTargetWeather().FogDensity, 0.123f));
		T.TestTrue(TEXT("The region's music plays"), Info && State->RegionMusic.IsValid() && Info->GetCurrentMusic() == State->RegionMusic.Get());
		Subsystem->ScanNow();
		T.TestTrue(TEXT("The near waystone is found (on the map)"), Subsystem->IsWaystoneDiscovered(Subsystem->FindWaystone(TEXT("w1"))));
		T.TestFalse(TEXT("...but not attuned"), Subsystem->IsWaystoneAttuned(Subsystem->FindWaystone(TEXT("w1"))));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep(WaitUntil(State, []()
	{
		const UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(PlayWorld());
		return Dialogue && Dialogue->IsBanterActive();
	}, 2.0f, TEXT("region banter (region_forest) starting"))));

	// Into the village: its banner, the region stays; back out and in again: no second reward
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UBeyondWorldSubsystem* Subsystem = World();
		UBeyondPartyComponent* P = Party(*State);
		if (UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(PlayWorld()))
		{
			Dialogue->EndConversation();
		}
		if (!Subsystem || !P)
		{
			return true;
		}
		PlaceAt(State->Angel.Get(), VillageCentre);
		Subsystem->ScanNow();
		T.TestTrue(TEXT("In the village"), Subsystem->GetCurrentVillage() == State->VillageDef.Get());
		T.TestTrue(TEXT("Still in the region"), Subsystem->GetCurrentRegion() == State->RegionDef.Get());
		const UBeyondRegionBannerWidget* Widget = Banner(*State);
		T.TestTrue(TEXT("The village's banner"), Widget && Widget->GetBannerTitle().ToString() == TEXT("Test Hamlet"));
		T.TestTrue(TEXT("The village is discovered"), P->IsRegionDiscovered(TEXT("test_village")));

		PlaceAt(State->Angel.Get(), Outside);
		Subsystem->ScanNow();
		T.TestNull(TEXT("Out again"), Subsystem->GetCurrentRegion());
		RememberProgress(*State);
		PlaceAt(State->Angel.Get(), InRegion);
		Subsystem->ScanNow();
		T.TestTrue(TEXT("Back in the region"), Subsystem->GetCurrentRegion() == State->RegionDef.Get());
		T.TestFalse(TEXT("No discovery EXP the second time"), ProgressChanged(*State));

		// The place
		PlaceAt(State->Angel.Get(), PlaceSpot);
		Subsystem->ScanNow();
		T.TestTrue(TEXT("The shrine is discovered"), P->IsPlaceDiscovered(TEXT("test_shrine")));
		T.TestTrue(TEXT("A toast says so"), ToastSays(*State, TEXT("Test Shrine")));
		return true;
	}));

	// The near waystone: F's logic attunes it, then rests
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		PlaceAt(State->Angel.Get(), NearWaystone + FVector(220.0f, 0.0f, 0.0f));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.4f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		UBeyondWorldSubsystem* Subsystem = World();
		UBeyondPartyComponent* P = Party(*State);
		ABeyondCharacterBase* Angel = State->Angel.Get();
		if (!PC || !Subsystem || !P || !Angel)
		{
			return true;
		}
		ABeyondWaystone* Near = Subsystem->FindWaystone(TEXT("w1"));
		T.TestTrue(TEXT("F would use the near waystone"), PC->FindWaystoneTarget() == Near);
		PC->UpdateInteractPrompt();
		const UBeyondInteractPromptWidget* Prompt = Cast<UBeyondInteractPromptWidget>(PC->GetInteractPromptWidget());
		T.TestTrue(TEXT("The prompt says Attune - Test Stone"), Prompt && Prompt->GetAction().ToString() == TEXT("Attune") && Prompt->GetTarget().ToString() == TEXT("Test Stone"));
		T.TestTrue(TEXT("F attunes it"), Subsystem->UseWaystone(Near) && Subsystem->IsWaystoneAttuned(Near));
		T.TestEqual(TEXT("It is the last waystone (where a save resumes)"), P->GetLastWaystone(), FName(TEXT("w1")));
		T.TestTrue(TEXT("Waystone.w1 answers as a story flag"), P->HasStoryFlag(TEXT("Waystone.w1")));
		T.TestTrue(TEXT("A toast says it was attuned"), ToastSays(*State, TEXT("attuned")));
		PC->UpdateInteractPrompt();
		T.TestTrue(TEXT("The prompt now says Rest"), Prompt && Prompt->GetAction().ToString() == TEXT("Rest"));

		// Damage from the world (the demigods don't hurt each other)
		AActor* Hazard = State->Demo.Lava.Get();
		UBeyondCombatLibrary::ApplyDamage(Hazard, Angel, 40.0f, BeyondTags::DamageType_Environment, FGameplayTag(), true, nullptr, true, true);
		T.TestTrue(TEXT("Angel is hurt"), UBeyondCombatLibrary::GetActorHealth(Angel) < UBeyondCombatLibrary::GetActorMaxHealth(Angel));
		Subsystem->UseWaystone(Near);
		T.TestEqual(TEXT("Resting heals"), UBeyondCombatLibrary::GetActorHealth(Angel), UBeyondCombatLibrary::GetActorMaxHealth(Angel));

		// Fast travel refused: not attuned
		FText WhyNot;
		T.TestFalse(TEXT("No travel to a waystone that isn't attuned"), Subsystem->CanFastTravel(TEXT("w2"), WhyNot));
		P->AttuneWaystone(TEXT("w2"));
		T.TestTrue(TEXT("Travel is allowed once it is"), Subsystem->CanFastTravel(TEXT("w2"), WhyNot));

		// ...a demigod down
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (JiWoong)
		{
			UBeyondCombatLibrary::ApplyDamage(Hazard, JiWoong, 100000.0f, BeyondTags::DamageType_Environment, FGameplayTag(), true, nullptr, true, true);
			T.TestTrue(TEXT("Ji-Woong is down"), UBeyondCombatLibrary::IsActorDead(JiWoong));
			T.TestFalse(TEXT("No travel with a demigod down"), Subsystem->CanFastTravel(TEXT("w2"), WhyNot));
			P->HealParty();
			T.TestFalse(TEXT("Resting / healing gets him up"), UBeyondCombatLibrary::IsActorDead(JiWoong));
		}

		// ...in a fight
		const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
		UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(PlayWorld());
		UBeyondEnemyDefinition* WolfDefinition = Roster ? Roster->FindEnemy(TEXT("forest_wolf")) : nullptr;
		if (Enemies && WolfDefinition)
		{
			FBeyondEnemySpawnParams Params;
			Params.Level = 1;
			ABeyondEnemyCharacter* Wolf = Enemies->SpawnEnemy(WolfDefinition, FTransform(Angel->GetActorLocation() + FVector(600.0f, 0.0f, 0.0f)), Params);
			if (ABeyondEnemyController* Brain = Wolf ? Cast<ABeyondEnemyController>(Wolf->GetController()) : nullptr)
			{
				Brain->EngageTarget(Angel, false);
				T.TestFalse(TEXT("No travel with an enemy fighting nearby"), Subsystem->CanFastTravel(TEXT("w2"), WhyNot));
				T.TestTrue(TEXT("...and it says why"), WhyNot.ToString().Contains(TEXT("enemies")));
			}
			if (Wolf)
			{
				Wolf->Destroy();
			}
		}
		return true;
	}));

	// Fast travel from the map to the far waystone
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		if (!PC)
		{
			return true;
		}
		PC->OpenInventory();
		PC->OpenWorldMap();
		UBeyondWorldMapWidget* Map = Cast<UBeyondWorldMapWidget>(PC->GetWorldMapWidget());
		T.TestTrue(TEXT("The map opens"), PC->IsWorldMapOpen() && Map != nullptr);
		T.TestFalse(TEXT("...and the inventory goes"), PC->IsInventoryOpen());
		T.TestTrue(TEXT("The game pauses"), PC->IsPaused());
		T.TestTrue(TEXT("M is a menu for the rest of the game (no talking, no banter)"), PC->IsAnyMenuOpen());
		if (!Map)
		{
			return true;
		}
		const TArray<FBeyondMapMarker> Markers = Map->GetMarkers();
		const FBeyondMapMarker* Near = Markers.FindByPredicate([](const FBeyondMapMarker& Marker) { return Marker.Id == TEXT("w1"); });
		const FBeyondMapMarker* Leader = Markers.FindByPredicate([](const FBeyondMapMarker& Marker) { return Marker.Kind == EBeyondMapMarkerKind::Leader; });
		const FBeyondMapMarker* Region = Markers.FindByPredicate([](const FBeyondMapMarker& Marker) { return Marker.Id == TEXT("region_forest"); });
		T.TestTrue(TEXT("The map shows the attuned waystone"), Near && Near->bDiscovered && Near->bActive);
		T.TestTrue(TEXT("...the party"), Leader != nullptr);
		T.TestTrue(TEXT("...the discovered region"), Region && Region->bDiscovered);
		T.TestTrue(TEXT("Pick the far waystone"), Map->SelectMarker(TEXT("w2")));
		T.TestTrue(TEXT("The first confirm asks"), Map->ConfirmSelection() && Map->IsConfirming());
		T.TestTrue(TEXT("The second travels"), Map->ConfirmSelection());
		T.TestFalse(TEXT("The map closes"), PC->IsWorldMapOpen());
		T.TestFalse(TEXT("...unpaused"), PC->IsPaused());
		T.TestTrue(TEXT("The party is on its way"), World() && World()->IsTravelling());
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep(WaitUntil(State, []() { return World() && !World()->IsTravelling(); }, 10.0f, TEXT("fast travel to w2 finishing"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondWaystone* Far = World() ? World()->FindWaystone(TEXT("w2")) : nullptr;
		const ABeyondCharacterBase* Angel = State->Angel.Get();
		const ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!Far || !Angel || !JiWoong)
		{
			return true;
		}
		const FVector Arrival = Far->GetArrivalTransform().GetLocation();
		T.TestTrue(*FString::Printf(TEXT("Angel arrived at the far waystone (%.0f uu away)"), FVector::Dist2D(Angel->GetActorLocation(), Arrival)),
			FVector::Dist2D(Angel->GetActorLocation(), Arrival) < 300.0f);
		T.TestTrue(TEXT("Ji-Woong came along"), FVector::Dist2D(JiWoong->GetActorLocation(), Arrival) < 600.0f);
		T.TestTrue(TEXT("Both stand on the ground and can move"), OnGround(Angel) && OnGround(JiWoong));
		T.TestEqual(TEXT("The far waystone is the last one now"), Party(*State)->GetLastWaystone(), FName(TEXT("w2")));
		return true;
	}));

	// The arena near the far waystone: its boss came; sealed in, no travel
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep(WaitUntil(State, [State]() { return State->Arena.IsValid() && State->Arena->GetBoss(); }, 3.0f,
		TEXT("the arena's boss spawning once the party is near"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		if (ABeyondBossArena* Arena = State->Arena.Get())
		{
			Arena->Engage(State->Angel.Get());
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep(WaitUntil(State, [State]() { return State->Arena.IsValid() && State->Arena->IsSealed(); }, 3.0f,
		TEXT("the arena sealing"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		FText WhyNot;
		T.TestFalse(TEXT("No travel sealed in a boss arena"), World() && World()->CanFastTravel(TEXT("w1"), WhyNot));
		if (ABeyondBossArena* Arena = State->Arena.Get())
		{
			Arena->ResetArena();
		}
		if (UBeyondWorldSubsystem* Subsystem = World())
		{
			T.TestTrue(TEXT("Travel back to the near waystone"), Subsystem->FastTravelTo(TEXT("w1")));
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep(WaitUntil(State, []() { return World() && !World()->IsTravelling(); }, 10.0f, TEXT("fast travel to w1 finishing"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep(WaitUntil(State, [State]()
	{
		// The reset brings it back after 3 s; the party is far, so it goes away again
		return State->Arena.IsValid() && !State->Arena->GetBoss() && !State->Arena->IsSealed();
	}, 6.0f, TEXT("the arena's boss going away with the party far"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		// Beaten (as a boss kill would flag it): it stays away
		if (UBeyondPartyComponent* P = Party(*State))
		{
			P->SetStoryFlag(TEXT("Boss.gorehide"));
		}
		if (UBeyondWorldSubsystem* Subsystem = World())
		{
			Subsystem->FastTravelTo(TEXT("w2"));
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep(WaitUntil(State, []() { return World() && !World()->IsTravelling(); }, 10.0f, TEXT("fast travel to w2 finishing"))));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondBossArena* Arena = State->Arena.Get();
		T.TestTrue(TEXT("A remembered defeat: no boss even with the party near"), Arena && !Arena->GetBoss());
		T.TestTrue(TEXT("...and the arena counts as beaten (the map ticks it)"), Arena && Arena->IsDefeated());
		if (UBeyondPartyComponent* P = Party(*State))
		{
			P->SetStoryFlag(TEXT("Boss.gorehide"), false);
		}
		// To the camp
		PlaceParty(*State, NearSpawner);
		return true;
	}));

	// The camp: Level 0 + 2 in a 1-6 region is level 3; put away when the party leaves; Never keeps the fallen down
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep(WaitUntil(State, [State]()
	{
		return State->Spawner.IsValid() && State->Spawner->GetSpawnedEnemies().Num() == 1;
	}, 3.0f, TEXT("the camp spawning its wolf"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondEnemySpawner* Spawner = State->Spawner.Get();
		ABeyondEnemyCharacter* Wolf = Spawner && !Spawner->GetSpawnedEnemies().IsEmpty() ? Spawner->GetSpawnedEnemies()[0] : nullptr;
		if (!Wolf)
		{
			return true;
		}
		if (ABeyondEnemyController* Brain = Cast<ABeyondEnemyController>(Wolf->GetController()))
		{
			// It may have spotted Angel already: calm it down so leaving counts as leaving a quiet camp
			Brain->SetBrainEnabled(false);
			Brain->ReturnHome(true);
		}
		T.TestEqual(TEXT("The wolf is at the region band's level + 2"), Wolf->GetCharacterLevel(), 3);
		T.TestTrue(TEXT("The camp is active"), Spawner->IsActive());
		State->Wolf = Wolf;
		PlaceParty(*State, FarWaystone + FVector(300.0f, 0.0f, 0.0f));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep(WaitUntil(State, [State]() { return State->Spawner.IsValid() && !State->Spawner->IsActive(); }, 3.0f,
		TEXT("the camp going quiet with the party far"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		T.TestTrue(TEXT("Its wolf was put away"), !State->Wolf.IsValid() || State->Wolf->IsActorBeingDestroyed());
		T.TestEqual(TEXT("...not as a kill"), State->Spawner.IsValid() ? State->Spawner->GetDefeatedCount() : -1, 0);
		PlaceParty(*State, NearSpawner);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep(WaitUntil(State, [State]()
	{
		return State->Spawner.IsValid() && State->Spawner->GetSpawnedEnemies().Num() == 1;
	}, 3.0f, TEXT("the camp's wolf coming back with the party"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondEnemySpawner* Spawner = State->Spawner.Get();
		ABeyondEnemyCharacter* Wolf = Spawner && !Spawner->GetSpawnedEnemies().IsEmpty() ? Spawner->GetSpawnedEnemies()[0] : nullptr;
		T.TestTrue(TEXT("Exactly one wolf again (no duplicate)"), Spawner && Spawner->GetSpawnedEnemies().Num() == 1);
		if (Wolf)
		{
			UBeyondCombatLibrary::ApplyDamage(State->Angel.Get(), Wolf, 100000.0f, BeyondTags::DamageType_Melee, FGameplayTag(), true, nullptr, true, true);
		}
		T.TestEqual(TEXT("The fallen wolf waits (Never)"), Spawner ? Spawner->GetDefeatedCount() : -1, 1);
		PlaceParty(*State, FarWaystone + FVector(300.0f, 0.0f, 0.0f));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		PlaceParty(*State, NearSpawner);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondEnemySpawner* Spawner = State->Spawner.Get();
		int32 Alive = 0;
		for (const ABeyondEnemyCharacter* Enemy : Spawner ? Spawner->GetSpawnedEnemies() : TArray<ABeyondEnemyCharacter*>())
		{
			Alive += Enemy && !UBeyondCombatLibrary::IsActorDead(Enemy) ? 1 : 0;
		}
		T.TestTrue(TEXT("Back at the camp: the fallen wolf stays down (Never)"), Spawner && Spawner->IsActive() && Alive == 0);

		// Lava
		if (UBeyondPartyComponent* P = Party(*State))
		{
			P->HealParty();
		}
		State->HealthBefore = UBeyondCombatLibrary::GetActorHealth(State->Angel.Get());
		PlaceAt(State->Angel.Get(), LavaSpot);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		T.TestTrue(TEXT("Standing in lava burns"), UBeyondCombatLibrary::GetActorHealth(State->Angel.Get()) < State->HealthBefore);
		// Deep water: back to safe ground (the last spot on the ground outside a hazard)
		if (UBeyondPartyComponent* P = Party(*State))
		{
			P->HealParty();
		}
		PlaceAt(State->Angel.Get(), InRegion);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.8f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		State->HealthBefore = UBeyondCombatLibrary::GetActorHealth(State->Angel.Get());
		PlaceAt(State->Angel.Get(), WaterSpot);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep(WaitUntil(State, []() { return World() && World()->IsTravelling(); }, 2.0f, TEXT("deep water sending the party back"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep(WaitUntil(State, []() { return World() && !World()->IsTravelling(); }, 10.0f, TEXT("the trip back to safe ground finishing"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondCharacterBase* Angel = State->Angel.Get();
		const ABeyondHazardVolume* Water = State->Demo.DeepWater.Get();
		T.TestTrue(TEXT("Out of the deep water"), Angel && Water && !Water->ContainsLocation(Angel->GetActorLocation()));
		T.TestTrue(TEXT("...a little hurt"), Angel && UBeyondCombatLibrary::GetActorHealth(Angel) < State->HealthBefore);

		// The console
		if (GEngine)
		{
			GEngine->Exec(PlayWorld(), TEXT("Beyond.Travel w2"));
		}
		T.TestTrue(TEXT("Beyond.Travel starts a trip"), World() && World()->IsTravelling());
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep(WaitUntil(State, []() { return World() && !World()->IsTravelling(); }, 10.0f, TEXT("Beyond.Travel finishing"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		if (GEngine)
		{
			GEngine->Exec(PlayWorld(), TEXT("Beyond.ResetWorld"));
		}
		const UBeyondPartyComponent* P = Party(*State);
		T.TestTrue(TEXT("Beyond.ResetWorld forgets regions and waystones"), P && !P->IsRegionDiscovered(TEXT("region_forest")) && !P->IsWaystoneAttuned(TEXT("w1"))
			&& P->GetLastWaystone().IsNone());

		for (const TWeakObjectPtr<AActor>& Actor : State->Cleanup)
		{
			if (AActor* Alive = Actor.Get())
			{
				Alive->Destroy();
			}
		}
		if (UBeyondPartyComponent* Mutable = Party(*State))
		{
			Mutable->ResetStoryFlags();
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondWorldStep([State]()
	{
		// Only once PIE is gone: the party saves when play ends, and that save must still see saving switched off
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
