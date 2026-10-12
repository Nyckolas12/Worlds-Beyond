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
#include "Components/CapsuleComponent.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "World/BeyondFloraTile.h"
#include "World/BeyondHazardVolume.h"
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
 * while the party is near and exactly one after leaving and coming back. Pass 18 (only a warning before it): the
 * landscape wears MI_Beyond_Landscape, the three lakes have water and deep water under their middles (not at their
 * shores), the caldera's lava burns in its pools but not on Kael'thar's floor or at the Caldera Breach Waystone, and the
 * party stands on Glassmere's ice. Pass 19 (only a warning before it): trees, shrubs and rocks are loaded round Mossbrook
 * and nothing that blocks stands at its waystone.
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
		TWeakObjectPtr<AActor> Ice;
		float IceTop = 0.0f;
	};

	UWorld* PlayWorld();

	AActor* FindLabelled(const FString& Label)
	{
		for (TActorIterator<AActor> It(PlayWorld()); It; ++It)
		{
			if (It->GetActorLabel() == Label)
			{
				return *It;
			}
		}
		return nullptr;
	}

	// The material a landscape actor wears (read by reflection: this module doesn't link the landscape's)
	const UMaterialInterface* LandscapeMaterialOf(const AActor* Actor)
	{
		const FObjectProperty* Property = Actor ? FindFProperty<FObjectProperty>(Actor->GetClass(), TEXT("LandscapeMaterial")) : nullptr;
		return Property ? Cast<UMaterialInterface>(Property->GetObjectPropertyValue_InContainer(Actor)) : nullptr;
	}

	// Pass 19's flora round a point (2D): plants and rocks loaded within Radius, and how many that block stand (by their
	// pivots) within Clear of Centre: the foliage's components and the flora tiles' trunk colliders. The foliage actor's
	// class is found by name: this module doesn't link the Foliage module
	void CountFlora(UWorld* World, const FVector& Centre, float Radius, float Clear, int32& OutNear, int32& OutBlocking)
	{
		OutNear = 0;
		OutBlocking = 0;
		const UClass* FoliageClass = FindObject<UClass>(nullptr, TEXT("/Script/Foliage.InstancedFoliageActor"));
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			ABeyondFloraTile* Tile = Cast<ABeyondFloraTile>(*It);
			if (!Tile && !(FoliageClass && It->IsA(FoliageClass)))
			{
				continue;
			}
			TArray<UInstancedStaticMeshComponent*> Meshes;
			It->GetComponents<UInstancedStaticMeshComponent>(Meshes);
			for (const UInstancedStaticMeshComponent* Mesh : Meshes)
			{
				const bool bColliders = Mesh->ComponentHasTag(ABeyondFloraTile::ColliderTag);
				const bool bBlocks = Mesh->IsCollisionEnabled();
				for (int32 Index = 0; Index < Mesh->GetInstanceCount(); ++Index)
				{
					FTransform Transform;
					if (!Mesh->GetInstanceTransform(Index, Transform, true))
					{
						continue;
					}
					const double Distance = FVector::Dist2D(Transform.GetLocation(), Centre);
					OutNear += !bColliders && Distance <= Radius ? 1 : 0;
					OutBlocking += bBlocks && Distance <= Clear ? 1 : 0;
				}
			}
			if (Tile)
			{
				TArray<UInstancedSkinnedMeshComponent*> Plants;
				Tile->GetPlantComponents(Plants);
				for (const UInstancedSkinnedMeshComponent* Plant : Plants)
				{
					for (int32 Index = 0; Index < Plant->GetInstanceCount(); ++Index)
					{
						FTransform Transform;
						if (Plant->GetInstanceTransform(Plant->GetInstanceId(Index), Transform, true) && FVector::Dist2D(Transform.GetLocation(), Centre) <= Radius)
						{
							++OutNear;
						}
					}
				}
			}
		}
	}

	void CheckFlora(FAutomationTestBase& T, UBeyondWorldSubsystem& Subsystem, const FVector& Here)
	{
		if (!FPackageName::DoesPackageExist(TEXT("/Game/WorldsBeyond/World/Flora/FT_spruce_full_01")))
		{
			T.AddWarning(TEXT("The trees and rocks aren't there yet: run migrate_pass19.py"));
			return;
		}
		int32 Near = 0;
		int32 Blocking = 0;
		CountFlora(PlayWorld(), Here, 40000.0f, 0.0f, Near, Blocking);
		T.TestTrue(*FString::Printf(TEXT("Trees, shrubs and rocks are loaded round Mossbrook (%d within 400 m)"), Near), Near >= 50);
		if (const ABeyondWaystone* Waystone = Subsystem.FindWaystone(TEXT("mossbrook")))
		{
			CountFlora(PlayWorld(), Waystone->GetActorLocation(), 0.0f, 1500.0f, Near, Blocking);
			T.TestEqual(TEXT("Nothing that blocks stands within 15 m of Mossbrook's waystone"), Blocking, 0);
		}
	}

	float BoxTop(const ABeyondHazardVolume* Hazard)
	{
		FVector Origin;
		FVector Extent;
		Hazard->GetActorBounds(false, Origin, Extent);
		return Origin.Z + Extent.Z;
	}

	// Pass 18's ground: the landscape's material, the lakes' water and deep water, the caldera's lava
	void CheckGround(FAutomationTestBase& T, UBeyondWorldSubsystem& Subsystem)
	{
		if (!FPackageName::DoesPackageExist(TEXT("/Game/WorldsBeyond/World/Landscape/MI_Beyond_Landscape")))
		{
			T.AddWarning(TEXT("The textured ground isn't built yet: run migrate_pass18.py"));
			return;
		}
		int32 Landscapes = 0;
		int32 Textured = 0;
		for (TActorIterator<AActor> It(PlayWorld()); It; ++It)
		{
			if (FindFProperty<FObjectProperty>(It->GetClass(), TEXT("LandscapeMaterial")))
			{
				++Landscapes;
				const UMaterialInterface* Material = LandscapeMaterialOf(*It);
				Textured += Material && Material->GetName() == TEXT("MI_Beyond_Landscape") ? 1 : 0;
			}
		}
		T.TestTrue(*FString::Printf(TEXT("The loaded landscape wears MI_Beyond_Landscape (%d of %d)"), Textured, Landscapes), Landscapes > 0 && Textured == Landscapes);

		for (const TCHAR* Lake : { TEXT("mirrormere"), TEXT("reedpool"), TEXT("blackmere") })
		{
			T.TestNotNull(*FString::Printf(TEXT("%s has water"), Lake), FindLabelled(FString::Printf(TEXT("WB_Water_%s"), Lake)));
			const ABeyondHazardVolume* Deep = Cast<ABeyondHazardVolume>(FindLabelled(FString::Printf(TEXT("WB_Hazard_%s"), Lake)));
			if (T.TestNotNull(*FString::Printf(TEXT("%s has deep water"), Lake), Deep))
			{
				const FVector Centre = Deep->GetActorLocation();
				const float Top = BoxTop(Deep);
				T.TestTrue(*FString::Printf(TEXT("2 m under %s's middle is deep"), Lake), Deep->ContainsLocation(FVector(Centre.X, Centre.Y, Top - 150.0f)));
				T.TestFalse(*FString::Printf(TEXT("...%s's shore isn't"), Lake),
					Deep->ContainsLocation(FVector(Centre.X + Deep->Radius + 200.0f, Centre.Y, Top - 50.0f)));
			}
		}
		T.TestNotNull(TEXT("Glassmere is frozen"), FindLabelled(TEXT("WB_Ice_glassmere")));
		T.TestNull(TEXT("...with no deep water under it"), FindLabelled(TEXT("WB_Hazard_glassmere")));

		T.TestNotNull(TEXT("The caldera has lava"), FindLabelled(TEXT("WB_Lava_molten_heart")));
		const ABeyondHazardVolume* Lava = Cast<ABeyondHazardVolume>(FindLabelled(TEXT("WB_Hazard_molten_heart")));
		if (T.TestNotNull(TEXT("...that burns"), Lava))
		{
			const FVector Centre = Lava->GetActorLocation();
			T.TestTrue(TEXT("Standing in a lava pool burns"), Lava->ContainsLocation(FVector(Centre.X, Centre.Y, BoxTop(Lava) - 40.0f)));
			for (ABeyondBossArena* Arena : Subsystem.GetArenas())
			{
				if (Arena->ArenaId == TEXT("molten_heart"))
				{
					T.TestFalse(TEXT("Kael'thar's floor doesn't"), Lava->ContainsLocation(Arena->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f)));
				}
			}
			if (const ABeyondWaystone* Breach = Subsystem.FindWaystone(TEXT("molten_heart")))
			{
				T.TestFalse(TEXT("The Caldera Breach Waystone doesn't"), Lava->ContainsLocation(Breach->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f)));
			}
		}
	}

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

		CheckGround(T, *Subsystem);
		CheckFlora(T, *Subsystem, Here);

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

		// Pass 18: onto Glassmere's ice
		AActor* Ice = FindLabelled(TEXT("WB_Ice_glassmere"));
		UBeyondWorldSubsystem* Subsystem = World();
		if (Ice && Subsystem)
		{
			FVector Origin;
			FVector Extent;
			Ice->GetActorBounds(false, Origin, Extent);
			State->Ice = Ice;
			State->IceTop = Origin.Z + Extent.Z;
			Subsystem->TravelPartyTo(FTransform(FVector(Origin.X, Origin.Y, State->IceTop + 300.0f)), EBeyondTravelReason::Cheat);
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep(WaitUntil(State, [State]() { return !State->Ice.IsValid() || PartySettled(*State); }, 40.0f,
		TEXT("travel onto Glassmere's ice"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondOpenWorldStep([State]()
	{
		const ABeyondCharacterBase* Who = Leader(*State);
		if (State->Ice.IsValid() && Who)
		{
			const float Feet = Who->GetActorLocation().Z - Who->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			State->Test->TestTrue(*FString::Printf(TEXT("The party stands on Glassmere's ice (feet %.0f, ice %.0f)"), Feet, State->IceTop),
				FMath::Abs(Feet - State->IceTop) < 60.0f);
		}
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
