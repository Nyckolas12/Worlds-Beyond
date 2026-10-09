// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Abilities/BeyondGA_AreaAttack.h"
#include "AbilitySystem/BeyondAbilitySet.h"
#include "AbilitySystem/BeyondAreaStrike.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "AI/BeyondEnemyController.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "BeyondGameplayTags.h"
#include "BrainComponent.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Enemies/BeyondAffixComponent.h"
#include "Enemies/BeyondAffixDefinition.h"
#include "Enemies/BeyondEnemyAnimInstance.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Enemies/BeyondEnemySettings.h"
#include "Enemies/BeyondEnemySpawner.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Game/BeyondCombatSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/AutomationTest.h"
#include "NavigationSystem.h"
#include "Player/BeyondPartyComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "Player/BeyondPlayerController.h"
#include "Progression/BeyondProgressionSettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tools/BeyondEditorLibrary.h"
#include "UI/BeyondEnemyPlatesWidget.h"

/**
 * Plan 3A in the test arena /Game/WorldsBeyond/Maps/TestArena (Play In Editor): the roster from migrate_pass10.py spawned through UBeyondEnemySubsystem
 * (the arena's own camps are far away), Beyond.Spawn, the C++ anim instance, sight and aggro, attack tokens, telegraphs
 * locking their spot, leashing home, montage deaths and EXP, every elite affix, the health plates, the spawner and the
 * party-wipe reset. Saving is off for the run (Beyond.SaveProgress 0).
 *
 * UnrealEditor-Cmd.exe WorldBeyond.uproject -ExecCmds="Automation RunTests WorldsBeyond.Prototype.Enemies;Quit" -unattended -nullrhi -nosplash
 */

namespace BeyondEnemiesTest
{
	struct FState
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ABeyondPlayerController> PC;
		TWeakObjectPtr<ABeyondCharacterBase> Angel;
		TWeakObjectPtr<ABeyondCharacterBase> JiWoong;
		TWeakObjectPtr<AAIController> Holder;
		TWeakObjectPtr<AController> CompanionController;
		FString SavedSaveProgress;
		FVector Anchor = FVector::ZeroVector;
		FVector Forward = FVector::ForwardVector;
		TArray<TWeakObjectPtr<ABeyondEnemyCharacter>> Spawned;
		TMap<FString, TWeakObjectPtr<ABeyondEnemyCharacter>> Named;
		TMap<FString, float> Numbers;
		TWeakObjectPtr<ABeyondAreaStrike> Strike;
		TWeakObjectPtr<ABeyondEnemySpawner> Spawner;
	};

	UWorld* GetEnemiesPlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	float EnemiesHealth(const AActor* Actor)
	{
		return UBeyondCombatLibrary::GetActorHealth(Actor);
	}

	void SetEnemiesBaseStat(ABeyondCharacterBase* Character, const FGameplayAttribute& Attribute, float Value)
	{
		if (UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr)
		{
			ASC->SetNumericAttributeBase(Attribute, Value);
		}
	}

	UBeyondEnemyDefinition* FindEnemyDefinition(const TCHAR* Id)
	{
		const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
		return Roster ? Roster->FindEnemy(FName(Id)) : nullptr;
	}

	UBeyondAffixDefinition* FindAffixDefinition(const TCHAR* Id)
	{
		const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
		return Roster ? Roster->FindAffix(FName(Id)) : nullptr;
	}

	// Spawn an enemy Distance in front of the anchor (Side to the right), its brain off unless asked
	ABeyondEnemyCharacter* SpawnTestEnemy(FState& State, const TCHAR* Id, float Distance, float Side = 0.0f, int32 Level = 1,
		TArray<UBeyondAffixDefinition*> Affixes = {}, bool bBrain = false)
	{
		UWorld* World = GetEnemiesPlayWorld();
		UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(World);
		UBeyondEnemyDefinition* Definition = FindEnemyDefinition(Id);
		if (!Enemies || !Definition)
		{
			return nullptr;
		}
		FBeyondEnemySpawnParams Params;
		Params.Level = Level;
		for (UBeyondAffixDefinition* Affix : Affixes)
		{
			if (Affix)
			{
				Params.Affixes.Add(Affix);
				Params.bElite = true;
			}
		}
		const FVector Right = FVector::CrossProduct(FVector::UpVector, State.Forward);
		const FVector Location = State.Anchor + State.Forward * Distance + Right * Side;
		ABeyondEnemyCharacter* Enemy = Enemies->SpawnEnemy(Definition, FTransform((-State.Forward).Rotation(), Location), Params);
		if (Enemy)
		{
			State.Spawned.Add(Enemy);
			if (ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(Enemy->GetController()))
			{
				AI->SetBrainEnabled(bBrain);
			}
		}
		return Enemy;
	}

	void ClearTestEnemies(FState& State)
	{
		for (const TWeakObjectPtr<ABeyondEnemyCharacter>& Enemy : State.Spawned)
		{
			if (ABeyondEnemyCharacter* Alive = Enemy.Get())
			{
				Alive->Destroy();
			}
		}
		State.Spawned.Reset();
		State.Named.Reset();
		if (UWorld* World = GetEnemiesPlayWorld())
		{
			for (TActorIterator<ABeyondAreaStrike> It(World); It; ++It)
			{
				It->Destroy();
			}
		}
	}

	int32 CountDoTs(const AActor* Target, const AActor* Source)
	{
		const ABeyondCharacterBase* Character = Cast<ABeyondCharacterBase>(Target);
		const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			return 0;
		}
		FGameplayEffectQuery Query;
		Query.EffectDefinition = UBeyondGE_DamageOverTime::StaticClass();
		int32 Count = 0;
		for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffects(Query))
		{
			const FActiveGameplayEffect* Active = ASC->GetActiveGameplayEffect(Handle);
			if (Active && Active->Spec.GetEffectContext().GetOriginalInstigator() == Source)
			{
				++Count;
			}
		}
		return Count;
	}

	void ClearDoTs(const AActor* Target)
	{
		const ABeyondCharacterBase* Character = Cast<ABeyondCharacterBase>(Target);
		if (UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr)
		{
			FGameplayEffectQuery Query;
			Query.EffectDefinition = UBeyondGE_DamageOverTime::StaticClass();
			ASC->RemoveActiveEffects(Query);
		}
	}

	bool HasAbilityNamed(const ABeyondCharacterBase* Character, const TCHAR* Name)
	{
		const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			return false;
		}
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->GetClass()->GetName().Contains(Name))
			{
				return true;
			}
		}
		return false;
	}

	UBeyondGA_AreaAttack* FindAreaAttack(const ABeyondCharacterBase* Character, const TCHAR* Name, FGameplayAbilitySpecHandle& OutHandle)
	{
		const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			return nullptr;
		}
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->GetClass()->GetName().Contains(Name))
			{
				OutHandle = Spec.Handle;
				return Cast<UBeyondGA_AreaAttack>(Spec.GetPrimaryInstance() ? Spec.GetPrimaryInstance() : Spec.Ability.Get());
			}
		}
		return nullptr;
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondEnemiesStep, TFunction<bool()>, Step);
bool FBeyondEnemiesStep::Update() { return Step(); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondEnemiesTest, "WorldsBeyond.Prototype.Enemies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondEnemiesTest::RunTest(const FString& Parameters)
{
	using namespace BeyondEnemiesTest;
	TSharedRef<FState> State = MakeShared<FState>();
	State->Test = this;

	if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
	{
		State->SavedSaveProgress = SaveProgress->GetString();
		SaveProgress->Set(TEXT("0"), ECVF_SetByCode);
	}

	// BP_Angel / BP_Ji-Woong play a hit voice on every hit: one at a time and a pause between lines (migrate_pass12.py)
	for (const TCHAR* VoicePath : { TEXT("/Game/WorldsBeyond/Sounds/Angel/Angel_hitReact.Angel_hitReact"),
		TEXT("/Game/WorldsBeyond/Sounds/JI-Woong/JI-Woong_HitReact.JI-Woong_HitReact") })
	{
		const USoundBase* Voice = LoadObject<USoundBase>(nullptr, VoicePath);
		if (!TestNotNull(FString::Printf(TEXT("Hit voice %s"), VoicePath), Voice))
		{
			continue;
		}
		const FSoundConcurrencySettings* VoiceConcurrency = Voice->bOverrideConcurrency ? &Voice->ConcurrencyOverrides : nullptr;
		for (const USoundConcurrency* Concurrency : Voice->ConcurrencySet)
		{
			VoiceConcurrency = Concurrency ? &Concurrency->Concurrency : VoiceConcurrency;
		}
		TestTrue(FString::Printf(TEXT("%s plays one at a time, at most every 2+ s"), *Voice->GetName()),
			VoiceConcurrency && VoiceConcurrency->GetMaxCount() == 1 && VoiceConcurrency->RetriggerTime >= 2.0f);
	}

	// The flat test arena from migrate_pass10.py: open ground with a navmesh
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/WorldsBeyond/Maps/TestArena")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.0f));

	// Setup: the party, legacy enemies held, the roster from migrate_pass10.py
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* World = GetEnemiesPlayWorld();
		ABeyondPlayerController* PC = World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!T.TestNotNull(TEXT("Beyond player controller"), PC))
		{
			return true;
		}
		State->PC = PC;
		UBeyondPartyComponent* Party = PC->PartyComponent;
		// Path failures show up in the log
		GEngine->Exec(World, TEXT("log LogBeyond Verbose"));

		for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
		{
			if (ULevelSequencePlayer* Player = It->GetSequencePlayer(); Player && Player->IsPlaying())
			{
				Player->GoToEndAndStop();
			}
		}

		for (ABeyondCharacterBase* Member : Party->GetMembers())
		{
			if (Member->DuoRole == EBeyondDuoRole::Conduit) { State->Angel = Member; }
			if (Member->DuoRole == EBeyondDuoRole::Striker) { State->JiWoong = Member; }
		}
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!T.TestNotNull(TEXT("Angel"), Angel) || !T.TestNotNull(TEXT("Ji-Woong"), JiWoong))
		{
			return true;
		}
		if (Party->GetLeader() != Angel)
		{
			Party->SwapLeader();
		}
		State->CompanionController = JiWoong->GetController();
		if (AAIController* Holder = World->SpawnActor<AAIController>())
		{
			Holder->Possess(JiWoong);
			State->Holder = Holder;
		}

		// The demigods soak everything for the run
		for (ABeyondCharacterBase* Member : { Angel, JiWoong })
		{
			SetEnemiesBaseStat(Member, UCharacterAttributeSet::GetMaxHealthAttribute(), 100000.0f);
			SetEnemiesBaseStat(Member, UCharacterAttributeSet::GetCurrentHealthAttribute(), 100000.0f);
		}

		// Any Blueprint enemies stand still
		for (TActorIterator<ABeyondCharacterBase> It(World); It; ++It)
		{
			if (It->TeamAffiliation == EBeyondTeam::Enemy && !Cast<ABeyondEnemyCharacter>(*It))
			{
				if (AAIController* AI = Cast<AAIController>(It->GetController()))
				{
					AI->StopMovement();
					if (UBrainComponent* Brain = AI->GetBrainComponent())
					{
						Brain->StopLogic(TEXT("Enemies test"));
					}
				}
				It->GetCharacterMovement()->StopMovementImmediately();
			}
		}

		// A spot on the navmesh with open ground ahead (enemies path there); Angel's start, else where the map's enemies stand
		TArray<FVector> Candidates = { Angel->GetActorLocation() };
		for (TActorIterator<ABeyondCharacterBase> It(World); It; ++It)
		{
			if (It->TeamAffiliation == EBeyondTeam::Enemy)
			{
				Candidates.Add(It->GetActorLocation());
			}
		}
		const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		T.TestNotNull(TEXT("The arena has a navigation system"), NavSystem);
		bool bFound = false;
		const FCollisionShape Probe = FCollisionShape::MakeSphere(60.0f);
		for (const FVector& Candidate : Candidates)
		{
			FNavLocation OnNav;
			if (!NavSystem || !NavSystem->ProjectPointToNavigation(Candidate, OnNav, FVector(200.0f, 200.0f, 400.0f)))
			{
				continue;
			}
			for (int32 Step = 0; Step < 16 && !bFound; ++Step)
			{
				const FVector Direction = FRotator(0.0f, Step * 22.5f, 0.0f).Vector();
				const FVector Start = OnNav.Location + FVector(0.0f, 0.0f, 100.0f);
				FHitResult Hit;
				FNavLocation Ahead;
				const bool bBlocked = World->SweepSingleByObjectType(Hit, Start, Start + Direction * 1500.0f, FQuat::Identity,
					FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllStaticObjects), Probe);
				if (!bBlocked && NavSystem->ProjectPointToNavigation(OnNav.Location + Direction * 1400.0f, Ahead, FVector(100.0f, 100.0f, 300.0f)))
				{
					State->Anchor = OnNav.Location + FVector(0.0f, 0.0f, Angel->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
					State->Forward = Direction;
					bFound = true;
				}
			}
			if (bFound)
			{
				break;
			}
		}
		if (!T.TestTrue(TEXT("Found navmesh with open ground for the enemy tests"), bFound))
		{
			State->Anchor = Angel->GetActorLocation();
			State->Forward = Angel->GetActorForwardVector().GetSafeNormal2D();
		}
		Angel->SetActorLocationAndRotation(State->Anchor, State->Forward.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
		JiWoong->SetActorLocation(State->Anchor - State->Forward * 600.0f, false, nullptr, ETeleportType::TeleportPhysics);

		const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
		if (T.TestNotNull(TEXT("migrate_pass10: DA_EnemyRoster (Project Settings -> Worlds Beyond Enemies)"), Roster))
		{
			T.TestTrue(TEXT("migrate_pass10: at least 7 roster enemies"), Roster->Enemies.Num() >= 7);
			T.TestTrue(TEXT("migrate_pass10: 8 affixes"), Roster->Affixes.Num() >= 8);
			for (const TCHAR* Id : { TEXT("forest_wolf"), TEXT("forest_raider"), TEXT("forest_slinger"), TEXT("forest_treant"),
				TEXT("woods_wraith"), TEXT("woods_knight"), TEXT("woods_beast") })
			{
				T.TestNotNull(*FString::Printf(TEXT("Roster has %s"), Id), Roster->FindEnemy(FName(Id)));
			}
			for (const TCHAR* Id : { TEXT("molten"), TEXT("venomous"), TEXT("stormcharged"), TEXT("warded"), TEXT("juggernaut"),
				TEXT("swift"), TEXT("vampiric"), TEXT("brood") })
			{
				T.TestNotNull(*FString::Printf(TEXT("Roster has the %s affix"), Id), Roster->FindAffix(FName(Id)));
			}
		}
		T.TestNotNull(TEXT("migrate_pass10: telegraph material"), GetDefault<UBeyondEnemySettings>()->TelegraphMaterial.LoadSynchronous());
		T.TestNotNull(TEXT("migrate_pass10: tint material"), GetDefault<UBeyondEnemySettings>()->TintMaterial.LoadSynchronous());
		return true;
	}));

	// Every roster entry spawns at level 5, set up from its definition
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
		const ABeyondCharacterBase* Angel = State->Angel.Get();
		if (!Roster || !Angel)
		{
			return true;
		}

		float Side = -900.0f;
		for (const UBeyondEnemyDefinition* Definition : Roster->Enemies)
		{
			if (!Definition || Definition->Rank == EBeyondEnemyRank::MiniBoss || Definition->Rank == EBeyondEnemyRank::Boss)
			{
				continue;
			}
			const FString Id = Definition->EnemyId.ToString();
			ABeyondEnemyCharacter* Enemy = SpawnTestEnemy(*State, *Id, 1400.0f, Side, 5);
			Side += 300.0f;
			if (!T.TestNotNull(*FString::Printf(TEXT("%s spawns"), *Id), Enemy))
			{
				continue;
			}
			State->Named.Add(Id, Enemy);
			T.TestTrue(*FString::Printf(TEXT("%s is on the Enemy team"), *Id), Enemy->TeamAffiliation == EBeyondTeam::Enemy);
			T.TestTrue(*FString::Printf(TEXT("%s is hostile to Angel"), *Id), UBeyondCombatLibrary::AreHostile(Enemy, Angel));
			T.TestEqual(*FString::Printf(TEXT("%s is level 5"), *Id), Enemy->GetCharacterLevel(), 5);
			const float Expected = Definition->MaxHealth + Definition->Growth.MaxHealth * 4.0f;
			T.TestTrue(*FString::Printf(TEXT("%s has its definition's health (%.0f, wanted %.0f)"), *Id, UBeyondCombatLibrary::GetActorMaxHealth(Enemy), Expected),
				FMath::IsNearlyEqual(UBeyondCombatLibrary::GetActorMaxHealth(Enemy), Expected, 1.0f));
			T.TestNotNull(*FString::Printf(TEXT("%s has the enemy brain"), *Id), Cast<ABeyondEnemyController>(Enemy->GetController()));
			T.TestTrue(*FString::Printf(TEXT("%s has its mesh"), *Id), Enemy->GetMesh()->GetSkeletalMeshAsset() == Definition->Mesh);
			const UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance();
			T.TestTrue(*FString::Printf(TEXT("%s animates with the C++ enemy anim instance"), *Id), Anim && Anim->IsA<UBeyondEnemyAnimInstance>());
			const int32 Granted = Enemy->GetAbilitySystemComponent()->GetActivatableAbilities().Num();
			const int32 InSet = Definition->AbilitySet ? Definition->AbilitySet->Abilities.Num() : 0;
			T.TestTrue(*FString::Printf(TEXT("%s has its %d abilities (and its hit reaction)"), *Id, InSet), InSet > 0 && Granted >= InSet);
			T.TestTrue(*FString::Printf(TEXT("%s has a hit reaction ability"), *Id), Definition->bUninterruptible || HasAbilityNamed(Enemy, TEXT("HitReact")));
			for (const TPair<FGameplayTag, TObjectPtr<UAnimMontage>>& Reaction : Definition->HitReactions)
			{
				T.TestEqual(*FString::Printf(TEXT("%s: %s plays on DefaultSlot"), *Id, *GetNameSafe(Reaction.Value)),
					UBeyondEditorLibrary::GetMontageSlot(Reaction.Value), UBeyondEnemyAnimInstance::SlotName);
			}
			T.TestTrue(*FString::Printf(TEXT("%s has death animations"), *Id), !Definition->DeathMontages.IsEmpty());
			T.TestTrue(*FString::Printf(TEXT("%s stands on the ground (capsule %.0f)"), *Id, Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
				Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() > 10.0f);
		}

		if (const ABeyondEnemyCharacter* Knight = State->Named.FindRef(TEXT("woods_knight")).Get())
		{
			T.TestTrue(TEXT("The Cursed Knight is an elite"), Knight->Rank == EBeyondEnemyRank::Elite);
		}
		ClearTestEnemies(*State);
		return true;
	}));

	// Beyond.Spawn <id> <level> <affix> x<count>
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* World = GetEnemiesPlayWorld();
		UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(World);
		if (!Enemies)
		{
			return true;
		}
		const int32 Before = Enemies->GetLiveEnemies().Num();
		GEngine->Exec(World, TEXT("Beyond.Spawn forest_wolf 3 molten x1"));
		ABeyondEnemyCharacter* Wolf = nullptr;
		for (ABeyondEnemyCharacter* Enemy : Enemies->GetLiveEnemies())
		{
			if (Enemy->GetDefinition() && Enemy->GetDefinition()->EnemyId == TEXT("forest_wolf"))
			{
				Wolf = Enemy;
			}
		}
		T.TestEqual(TEXT("Beyond.Spawn made one enemy"), Enemies->GetLiveEnemies().Num(), Before + 1);
		if (T.TestNotNull(TEXT("Beyond.Spawn forest_wolf 3 molten"), Wolf))
		{
			State->Spawned.Add(Wolf);
			T.TestEqual(TEXT("At level 3"), Wolf->GetCharacterLevel(), 3);
			T.TestTrue(TEXT("An elite"), Wolf->IsElite() && Wolf->Rank == EBeyondEnemyRank::Elite);
			T.TestTrue(TEXT("Molten"), Wolf->GetAffixComponent()->HasAffix(BeyondTags::Enemy_Affix_Molten));
			T.TestTrue(TEXT("Named after its affix"), Wolf->GetEnemyName().ToString().StartsWith(TEXT("Molten")));
			T.TestTrue(TEXT("Its home is where it spawned"), Wolf->GetHomeTransform().GetLocation().Equals(Wolf->GetActorLocation(), 60.0f));
		}
		ClearTestEnemies(*State);
		return true;
	}));

	// It walks (the anim instance follows), sees Angel and fights
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondEnemyCharacter* Raider = SpawnTestEnemy(*State, TEXT("forest_raider"), 900.0f, 0.0f, 1, {}, true);
		ABeyondEnemyCharacter* Wolf = SpawnTestEnemy(*State, TEXT("forest_wolf"), 1300.0f, 400.0f, 1, {}, true);
		State->Named.Add(TEXT("raider"), Raider);
		State->Named.Add(TEXT("wolf"), Wolf);
		if (ABeyondEnemyController* AI = Raider ? Cast<ABeyondEnemyController>(Raider->GetController()) : nullptr)
		{
			AI->EngageTarget(Angel, false);
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.8f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondEnemyCharacter* Raider = State->Named.FindRef(TEXT("raider")).Get();
		ABeyondEnemyCharacter* Wolf = State->Named.FindRef(TEXT("wolf")).Get();
		if (T.TestNotNull(TEXT("Raider"), Raider))
		{
			// Why a move might fail: the raider's and Angel's places on the navmesh and a path between them
			if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetEnemiesPlayWorld()); NavSystem && State->Angel.IsValid())
			{
				const FNavAgentProperties& Agent = Raider->GetNavAgentPropertiesRef();
				FNavLocation From;
				FNavLocation To;
				const bool bFrom = NavSystem->ProjectPointToNavigation(Raider->GetNavAgentLocation(), From, INVALID_NAVEXTENT, &Agent);
				const bool bTo = NavSystem->ProjectPointToNavigation(State->Angel->GetNavAgentLocation(), To, INVALID_NAVEXTENT, &Agent);
				const ANavigationData* NavData = NavSystem->GetNavDataForProps(Agent, Raider->GetNavAgentLocation());
				FString PathText = TEXT("no nav data");
				if (NavData)
				{
					FPathFindingQuery Query(Raider, *NavData, Raider->GetNavAgentLocation(), State->Angel->GetNavAgentLocation());
					const FPathFindingResult Path = NavSystem->FindPathSync(Agent, Query);
					PathText = FString::Printf(TEXT("result %d, %d points"), static_cast<int32>(Path.Result), Path.Path.IsValid() ? Path.Path->GetPathPoints().Num() : 0);
				}
				T.AddInfo(FString::Printf(TEXT("Raider agent r%.0f h%.0f at %s (on nav %d), Angel at %s (on nav %d), nav data %s, path %s"),
					Agent.AgentRadius, Agent.AgentHeight, *Raider->GetNavAgentLocation().ToString(), bFrom ? 1 : 0,
					*State->Angel->GetNavAgentLocation().ToString(), bTo ? 1 : 0, *GetNameSafe(NavData), *PathText));
			}
			const UBeyondEnemyAnimInstance* Anim = Cast<UBeyondEnemyAnimInstance>(Raider->GetMesh()->GetAnimInstance());
			T.TestTrue(*FString::Printf(TEXT("The raider runs at Angel (%.0f cm/s)"), Raider->GetVelocity().Size2D()), Raider->GetVelocity().Size2D() > 50.0f);
			T.TestTrue(TEXT("Its anim instance reads the speed"), Anim && Anim->Speed > 50.0f);
		}
		if (T.TestNotNull(TEXT("Wolf"), Wolf))
		{
			const ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(Wolf->GetController());
			T.TestTrue(TEXT("The wolf saw Angel (or was alerted by the raider) and fights"), AI && AI->GetAIState() == EBeyondEnemyAIState::Combat);
			T.TestTrue(TEXT("It targets a demigod"), AI && Cast<ABeyondCharacterBase>(AI->GetCombatTarget())
				&& Cast<ABeyondCharacterBase>(AI->GetCombatTarget())->TeamAffiliation == EBeyondTeam::Player);
		}
		ClearTestEnemies(*State);
		return true;
	}));

	// Attack tokens: four raiders on Angel, at most his Max Attack Tokens swing at once
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		ABeyondCharacterBase* Angel = State->Angel.Get();
		for (int32 Index = 0; Index < 4; ++Index)
		{
			ABeyondEnemyCharacter* Raider = SpawnTestEnemy(*State, TEXT("forest_raider"), 260.0f, -300.0f + Index * 200.0f, 1, {}, true);
			if (ABeyondEnemyController* AI = Raider ? Cast<ABeyondEnemyController>(Raider->GetController()) : nullptr)
			{
				AI->EngageTarget(Angel, false);
			}
		}
		State->Numbers.Add(TEXT("MaxTokens"), Angel ? static_cast<float>(Angel->MaxAttackTokens) : 0.0f);
		State->Numbers.Add(TEXT("PeakHolders"), 0.0f);
		State->Numbers.Add(TEXT("Waited"), 0.0f);
		State->Numbers.Add(TEXT("Polls"), 0.0f);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		// Poll for ~2.5 s: count the raiders holding tokens and whether any had to wait
		int32 Holders = 0;
		bool bWaiting = false;
		for (const TWeakObjectPtr<ABeyondEnemyCharacter>& Enemy : State->Spawned)
		{
			if (const ABeyondEnemyController* AI = Enemy.IsValid() ? Cast<ABeyondEnemyController>(Enemy->GetController()) : nullptr)
			{
				Holders += AI->GetHeldTokens() > 0 ? AI->GetHeldTokens() : 0;
				bWaiting |= AI->IsWaitingForToken();
			}
		}
		float& Peak = State->Numbers.FindOrAdd(TEXT("PeakHolders"));
		Peak = FMath::Max(Peak, static_cast<float>(Holders));
		if (bWaiting)
		{
			State->Numbers.FindOrAdd(TEXT("Waited")) = 1.0f;
		}
		float& Polls = State->Numbers.FindOrAdd(TEXT("Polls"));
		Polls += 1.0f;
		return Polls >= 150.0f;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		const float MaxTokens = State->Numbers.FindRef(TEXT("MaxTokens"));
		T.TestTrue(*FString::Printf(TEXT("Tokens taken at once (%.0f) never exceed Angel's %.0f"), State->Numbers.FindRef(TEXT("PeakHolders")), MaxTokens),
			State->Numbers.FindRef(TEXT("PeakHolders")) <= MaxTokens && State->Numbers.FindRef(TEXT("PeakHolders")) > 0.0f);
		T.TestTrue(TEXT("A raider without a token circled (waited)"), State->Numbers.FindRef(TEXT("Waited")) > 0.0f);
		for (const TWeakObjectPtr<ABeyondEnemyCharacter>& Enemy : State->Spawned)
		{
			if (Enemy.IsValid())
			{
				UBeyondCombatLibrary::ApplyDamage(Angel, Enemy.Get(), 1.0e6f, BeyondTags::DamageType_Environment, FGameplayTag(), true, nullptr, true, true);
			}
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondCharacterBase* Angel = State->Angel.Get();
		T.TestEqual(TEXT("Dead raiders gave their tokens back"), Angel ? Angel->GetAvailableAttackTokens() : -1, Angel ? Angel->MaxAttackTokens : 0);
		ClearTestEnemies(*State);
		return true;
	}));

	// Telegraphs lock their spot: step out and it misses, stay and it hits
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondEnemyCharacter* Treant = SpawnTestEnemy(*State, TEXT("forest_treant"), 700.0f);
		if (!Angel || !Treant)
		{
			return true;
		}
		State->Named.Add(TEXT("treant"), Treant);
		FBeyondStrikeSettings Settings;
		Settings.Shape = EBeyondStrikeShape::Circle;
		Settings.Radius = 250.0f;
		Settings.WindUp = 0.5f;
		Settings.Damage = 50.0f;
		Settings.DamageType = BeyondTags::DamageType_Explosion;
		State->Strike = ABeyondAreaStrike::SpawnStrike(Treant, Treant, Settings, Angel->GetActorLocation(), 0.0f);
		State->Numbers.Add(TEXT("HealthBefore"), EnemiesHealth(Angel));
		// Step out at once
		Angel->SetActorLocation(State->Anchor - State->Forward * 700.0f, false, nullptr, ETeleportType::TeleportPhysics);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.8f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondEnemyCharacter* Treant = State->Named.FindRef(TEXT("treant")).Get();
		T.TestEqual(TEXT("Stepping out of the telegraph: no damage"), EnemiesHealth(Angel), State->Numbers.FindRef(TEXT("HealthBefore")));
		if (!Angel || !Treant)
		{
			return true;
		}
		// Stand in the next one
		Angel->SetActorLocation(State->Anchor, false, nullptr, ETeleportType::TeleportPhysics);
		FBeyondStrikeSettings Settings;
		Settings.Radius = 250.0f;
		Settings.WindUp = 0.5f;
		Settings.Damage = 50.0f;
		Settings.DamageType = BeyondTags::DamageType_Explosion;
		ABeyondAreaStrike* Strike = ABeyondAreaStrike::SpawnStrike(Treant, Treant, Settings, Angel->GetActorLocation(), 0.0f);
		T.TestTrue(TEXT("Angel stands inside the new marker"), Strike && Strike->IsInside(Angel));
		T.TestFalse(TEXT("It hasn't landed yet"), Strike && Strike->HasStruck());
		State->Numbers.Add(TEXT("HealthBefore"), EnemiesHealth(Angel));

		// The ability: the treant's smash locks a marker on itself
		if (ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(Treant->GetController()))
		{
			AI->SetFocus(Angel, EAIFocusPriority::Gameplay);
		}
		FGameplayAbilitySpecHandle Smash;
		FindAreaAttack(Treant, TEXT("GA_Treant_Smash"), Smash);
		T.TestTrue(TEXT("The treant's ground smash activates"), Smash.IsValid() && Treant->GetAbilitySystemComponent()->TryActivateAbility(Smash));
		FGameplayAbilitySpecHandle Again;
		if (const UBeyondGA_AreaAttack* Ability = FindAreaAttack(Treant, TEXT("GA_Treant_Smash"), Again))
		{
			const TArray<ABeyondAreaStrike*> Strikes = Ability->GetLastStrikes();
			T.TestEqual(TEXT("It put down one marker"), Strikes.Num(), 1);
			if (Strikes.Num() == 1)
			{
				T.TestTrue(TEXT("Centred on the treant"), FVector::Dist2D(Strikes[0]->GetActorLocation(), Treant->GetActorLocation()) < 80.0f);
				T.TestTrue(TEXT("The marker winds up before it hits"), !Strikes[0]->HasStruck());
			}
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.8f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondCharacterBase* Angel = State->Angel.Get();
		T.TestTrue(TEXT("Staying in the telegraph: hit"), EnemiesHealth(Angel) < State->Numbers.FindRef(TEXT("HealthBefore")));
		ClearTestEnemies(*State);
		return true;
	}));

	// Leash: too far from home it walks back invulnerable and refills
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondEnemyCharacter* Wolf = SpawnTestEnemy(*State, TEXT("forest_wolf"), 800.0f, 0.0f, 1, {}, true);
		ABeyondEnemyController* AI = Wolf ? Cast<ABeyondEnemyController>(Wolf->GetController()) : nullptr;
		if (!T.TestNotNull(TEXT("Wolf for the leash"), AI))
		{
			return true;
		}
		UBeyondCombatLibrary::ApplyDamage(Angel, Wolf, 30.0f, BeyondTags::DamageType_Melee, FGameplayTag(), true, nullptr, true);
		T.TestTrue(TEXT("Hurt"), EnemiesHealth(Wolf) < UBeyondCombatLibrary::GetActorMaxHealth(Wolf));
		// Dragged far from home mid-fight
		FTransform Home = Wolf->GetHomeTransform();
		Home.SetLocation(Wolf->GetActorLocation() - State->Forward * 5000.0f);
		Wolf->SetHomeTransform(Home);
		AI->EngageTarget(Angel, false);
		AI->ThinkNow();
		T.TestTrue(TEXT("Past its leash it gives up"), AI->GetAIState() == EBeyondEnemyAIState::Returning);
		T.TestTrue(TEXT("Walking home it carries State.Resetting"), Wolf->GetAbilitySystemComponent()->HasMatchingGameplayTag(BeyondTags::State_Resetting));
		const float Health = EnemiesHealth(Wolf);
		UBeyondCombatLibrary::ApplyDamage(Angel, Wolf, 20.0f, BeyondTags::DamageType_Melee, FGameplayTag(), true, nullptr, true);
		T.TestEqual(TEXT("It can't be hurt on the way home"), EnemiesHealth(Wolf), Health);
		// Back home (teleport): refilled and idle
		Home.SetLocation(Wolf->GetActorLocation());
		Wolf->SetHomeTransform(Home);
		AI->ReturnHome(true);
		T.TestTrue(TEXT("Home again: idle"), AI->GetAIState() == EBeyondEnemyAIState::Idle);
		T.TestEqual(TEXT("Home again: full health"), EnemiesHealth(Wolf), UBeyondCombatLibrary::GetActorMaxHealth(Wolf));
		T.TestFalse(TEXT("Home again: no State.Resetting"), Wolf->GetAbilitySystemComponent()->HasMatchingGameplayTag(BeyondTags::State_Resetting));
		ClearTestEnemies(*State);
		return true;
	}));

	// Death: a montage, not physics (the jungle creatures have no physics asset), and EXP for the party
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondEnemyCharacter* Treant = SpawnTestEnemy(*State, TEXT("forest_treant"), 700.0f);
		if (!Angel || !T.TestNotNull(TEXT("Treant for the death"), Treant))
		{
			return true;
		}
		State->Named.Add(TEXT("treant"), Treant);
		State->Numbers.Add(TEXT("Exp"), UBeyondProgressionSettings::GetTotalExperience(Angel->GetCharacterLevel(), Angel->GetExperience()));
		UBeyondCombatLibrary::ApplyDamage(Angel, Treant, 1.0e6f, BeyondTags::DamageType_Melee, FGameplayTag(), true, nullptr, true, true);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondEnemyCharacter* Treant = State->Named.FindRef(TEXT("treant")).Get();
		if (T.TestNotNull(TEXT("The body stays a while"), Treant))
		{
			T.TestTrue(TEXT("Dead"), UBeyondCombatLibrary::IsActorDead(Treant));
			T.TestFalse(TEXT("No ragdoll"), Treant->GetMesh()->IsSimulatingPhysics());
			const UAnimInstance* Anim = Treant->GetMesh()->GetAnimInstance();
			T.TestTrue(TEXT("A death animation plays"), Anim && Anim->GetCurrentActiveMontage()
				&& Treant->GetDefinition()->DeathMontages.Contains(Anim->GetCurrentActiveMontage()));
			T.TestTrue(TEXT("The capsule no longer blocks"), Treant->GetCapsuleComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		}
		if (Angel)
		{
			const float Now = UBeyondProgressionSettings::GetTotalExperience(Angel->GetCharacterLevel(), Angel->GetExperience());
			T.TestTrue(*FString::Printf(TEXT("The kill gave Angel EXP (%.0f -> %.0f)"), State->Numbers.FindRef(TEXT("Exp")), Now),
				Now > State->Numbers.FindRef(TEXT("Exp")));
		}
		ClearTestEnemies(*State);
		return true;
	}));

	// Affixes: stats, tags and abilities on spawn
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondEnemyCharacter* Plain = SpawnTestEnemy(*State, TEXT("forest_raider"), 700.0f, -600.0f);
		ABeyondEnemyCharacter* Juggernaut = SpawnTestEnemy(*State, TEXT("forest_raider"), 700.0f, -300.0f, 1, { FindAffixDefinition(TEXT("juggernaut")) });
		ABeyondEnemyCharacter* Swift = SpawnTestEnemy(*State, TEXT("forest_raider"), 700.0f, 0.0f, 1, { FindAffixDefinition(TEXT("swift")) });
		ABeyondEnemyCharacter* Warded = SpawnTestEnemy(*State, TEXT("forest_raider"), 700.0f, 300.0f, 1, { FindAffixDefinition(TEXT("warded")) });
		ABeyondEnemyCharacter* Venomous = SpawnTestEnemy(*State, TEXT("forest_raider"), 700.0f, 600.0f, 1, { FindAffixDefinition(TEXT("venomous")) });
		if (!Angel || !Plain || !Juggernaut || !Swift || !Warded || !Venomous)
		{
			T.AddError(TEXT("Could not spawn the affix test raiders"));
			return true;
		}
		const float PlainHealth = UBeyondCombatLibrary::GetActorMaxHealth(Plain);
		const float EliteHealth = PlainHealth * GetDefault<UBeyondEnemySettings>()->EliteHealthMultiplier;
		T.TestTrue(*FString::Printf(TEXT("Juggernaut: x1.5 an elite's health (%.0f)"), UBeyondCombatLibrary::GetActorMaxHealth(Juggernaut)),
			FMath::IsNearlyEqual(UBeyondCombatLibrary::GetActorMaxHealth(Juggernaut), EliteHealth * 1.5f, 2.0f));
		T.TestTrue(TEXT("Juggernaut: can't be interrupted"), Juggernaut->GetAbilitySystemComponent()->HasMatchingGameplayTag(BeyondTags::State_Uninterruptible));
		T.TestTrue(TEXT("Juggernaut: bigger"), Juggernaut->GetActorScale3D().X > Plain->GetActorScale3D().X * 1.2f);
		T.TestTrue(TEXT("Swift: faster"), Swift->GetCharacterMovement()->MaxWalkSpeed > Plain->GetCharacterMovement()->MaxWalkSpeed * 1.4f);
		T.TestTrue(TEXT("Swift: has the lunge"), HasAbilityNamed(Swift, TEXT("GA_Affix_Lunge")));
		T.TestTrue(TEXT("Venomous: has the venom spit"), HasAbilityNamed(Venomous, TEXT("GA_Affix_VenomSpit")));
		T.TestTrue(TEXT("Warded: its ward is up"), Warded->GetAffixComponent()->IsWardUp()
			&& Warded->GetAbilitySystemComponent()->HasMatchingGameplayTag(BeyondTags::State_Warded));

		// Warded: a spell does 40 %, a sword hit breaks the ward, then spells hurt fully
		float Before = EnemiesHealth(Warded);
		UBeyondCombatLibrary::ApplyDamage(Angel, Warded, 100.0f, BeyondTags::DamageType_Projectile, FGameplayTag(), true, nullptr, true);
		const float Warded1 = Before - EnemiesHealth(Warded);
		UBeyondCombatLibrary::ApplyDamage(Angel, Warded, 1.0f, BeyondTags::DamageType_Melee, FGameplayTag(), true, nullptr, true);
		T.TestFalse(TEXT("Warded: a melee hit broke the ward"), Warded->GetAffixComponent()->IsWardUp());
		Before = EnemiesHealth(Warded);
		UBeyondCombatLibrary::ApplyDamage(Angel, Warded, 100.0f, BeyondTags::DamageType_Projectile, FGameplayTag(), true, nullptr, true);
		const float Open = Before - EnemiesHealth(Warded);
		T.TestTrue(*FString::Printf(TEXT("Warded: spells do 40 %% through the ward (%.1f vs %.1f)"), Warded1, Open),
			Open > 0.0f && FMath::IsNearlyEqual(Warded1 / Open, 0.4f, 0.03f));

		// Venomous: its hit poisons Angel (next tick)
		ClearDoTs(Angel);
		UBeyondCombatLibrary::ApplyDamage(Venomous, Angel, 10.0f, BeyondTags::DamageType_Melee, FGameplayTag(), true, nullptr, true);
		State->Named.Add(TEXT("venomous"), Venomous);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondCharacterBase* Angel = State->Angel.Get();
		T.TestEqual(TEXT("Venomous: its hit poisoned Angel"), CountDoTs(Angel, State->Named.FindRef(TEXT("venomous")).Get()), 1);
		ClearDoTs(Angel);
		ClearTestEnemies(*State);
		return true;
	}));

	// Affixes in a fight: Molten (burn + lava on death), Vampiric (heals), Stormcharged (pulse), Brood (splits)
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondEnemyCharacter* Molten = SpawnTestEnemy(*State, TEXT("forest_raider"), 700.0f, -450.0f, 1, { FindAffixDefinition(TEXT("molten")) });
		ABeyondEnemyCharacter* Vampiric = SpawnTestEnemy(*State, TEXT("forest_raider"), 700.0f, -150.0f, 1, { FindAffixDefinition(TEXT("vampiric")) });
		ABeyondEnemyCharacter* Storm = SpawnTestEnemy(*State, TEXT("forest_raider"), 700.0f, 150.0f, 1, { FindAffixDefinition(TEXT("stormcharged")) });
		ABeyondEnemyCharacter* Brood = SpawnTestEnemy(*State, TEXT("forest_wolf"), 900.0f, 450.0f, 1, { FindAffixDefinition(TEXT("brood")) });
		if (!Angel || !Molten || !Vampiric || !Storm || !Brood)
		{
			T.AddError(TEXT("Could not spawn the affix fight raiders"));
			return true;
		}
		State->Named.Add(TEXT("molten"), Molten);
		State->Named.Add(TEXT("vampiric"), Vampiric);
		State->Named.Add(TEXT("brood"), Brood);
		ClearDoTs(Angel);
		UBeyondCombatLibrary::ApplyDamage(Molten, Angel, 10.0f, BeyondTags::DamageType_Melee, FGameplayTag(), true, nullptr, true);

		UBeyondCombatLibrary::ApplyDamage(Angel, Vampiric, 80.0f, BeyondTags::DamageType_Melee, FGameplayTag(), true, nullptr, true);
		State->Numbers.Add(TEXT("VampireHealth"), EnemiesHealth(Vampiric));
		UBeyondCombatLibrary::ApplyDamage(Vampiric, Angel, 100.0f, BeyondTags::DamageType_Environment, FGameplayTag(), true, nullptr, true);

		Storm->GetAffixComponent()->PulseNow();
		T.TestEqual(TEXT("Stormcharged: a lightning pulse went off"), Storm->GetAffixComponent()->GetPulseCount(), 1);
		int32 Rings = 0;
		for (TActorIterator<ABeyondAreaStrike> It(GetEnemiesPlayWorld()); It; ++It)
		{
			Rings += It->GetStrikeInstigator() == Storm ? 1 : 0;
		}
		T.TestEqual(TEXT("Stormcharged: its pulse is telegraphed"), Rings, 1);

		UBeyondCombatLibrary::ApplyDamage(Angel, Brood, 1.0e6f, BeyondTags::DamageType_Melee, FGameplayTag(), true, nullptr, true, true);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondEnemyCharacter* Molten = State->Named.FindRef(TEXT("molten")).Get();
		ABeyondEnemyCharacter* Vampiric = State->Named.FindRef(TEXT("vampiric")).Get();
		ABeyondEnemyCharacter* Brood = State->Named.FindRef(TEXT("brood")).Get();
		T.TestEqual(TEXT("Molten: its hit set Angel ablaze"), CountDoTs(Angel, Molten), 1);
		ClearDoTs(Angel);
		T.TestTrue(*FString::Printf(TEXT("Vampiric: it healed off its hit (%.0f -> %.0f)"), State->Numbers.FindRef(TEXT("VampireHealth")), EnemiesHealth(Vampiric)),
			EnemiesHealth(Vampiric) > State->Numbers.FindRef(TEXT("VampireHealth")) + 20.0f);

		int32 Copies = 0;
		if (const UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(GetEnemiesPlayWorld()))
		{
			for (ABeyondEnemyCharacter* Enemy : Enemies->GetLiveEnemies())
			{
				if (Enemy->bSummoned && Brood && Enemy->GetDefinition() == Brood->GetDefinition())
				{
					++Copies;
					State->Spawned.Add(Enemy);
					T.TestTrue(TEXT("Brood copies are smaller"), Enemy->GetActorScale3D().X < Brood->GetActorScale3D().X);
					T.TestFalse(TEXT("Brood copies drop no loot"), Enemy->CanDropLoot());
				}
			}
		}
		T.TestEqual(TEXT("Brood: it split into 2 copies"), Copies, 2);

		// Molten's lava
		if (Molten)
		{
			UBeyondCombatLibrary::ApplyDamage(Angel, Molten, 1.0e6f, BeyondTags::DamageType_Melee, FGameplayTag(), true, nullptr, true, true);
			int32 Pools = 0;
			for (TActorIterator<ABeyondAreaStrike> It(GetEnemiesPlayWorld()); It; ++It)
			{
				Pools += It->GetStrikeInstigator() == Molten && It->GetSettings().LingerDuration > 0.0f ? 1 : 0;
			}
			T.TestEqual(TEXT("Molten: it left burning ground"), Pools, 1);
		}
		ClearTestEnemies(*State);
		return true;
	}));

	// Plates, the spawner, and the party wipe sending everyone home
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		UWorld* World = GetEnemiesPlayWorld();
		const ABeyondPlayerController* PC = State->PC.Get();
		T.TestNotNull(TEXT("The enemy plates widget is up"), PC ? PC->GetEnemyPlatesWidget() : nullptr);

		ABeyondEnemyCharacter* Raider = SpawnTestEnemy(*State, TEXT("forest_raider"), 600.0f);
		State->Named.Add(TEXT("raider"), Raider);
		if (Raider && Angel)
		{
			UBeyondCombatLibrary::ApplyDamage(Angel, Raider, 20.0f, BeyondTags::DamageType_Melee, FGameplayTag(), true, nullptr, true);
			// Walked off its post
			Raider->SetActorLocation(Raider->GetActorLocation() + State->Forward * 400.0f, false, nullptr, ETeleportType::TeleportPhysics);
		}
		ABeyondEnemyCharacter* Summoned = SpawnTestEnemy(*State, TEXT("forest_wolf"), 900.0f);
		if (Summoned)
		{
			Summoned->bSummoned = true;
		}
		State->Named.Add(TEXT("summoned"), Summoned);

		// A camp placed at runtime
		FActorSpawnParameters Params;
		Params.bDeferConstruction = true;
		const FTransform CampTransform(State->Anchor + State->Forward * 1600.0f);
		if (ABeyondEnemySpawner* Spawner = World->SpawnActorDeferred<ABeyondEnemySpawner>(ABeyondEnemySpawner::StaticClass(), CampTransform))
		{
			FBeyondSpawnEntry& Entry = Spawner->Entries.AddDefaulted_GetRef();
			Entry.Enemy = FindEnemyDefinition(TEXT("forest_raider"));
			Entry.Count = 2;
			Entry.Level = 2;
			Spawner->EliteChance = 0.0f;
			Spawner->FinishSpawning(CampTransform);
			State->Spawner = Spawner;
			const TArray<ABeyondEnemyCharacter*> Camp = Spawner->GetSpawnedEnemies();
			T.TestEqual(TEXT("The spawner put down its 2 raiders"), Camp.Num(), 2);
			for (ABeyondEnemyCharacter* Enemy : Camp)
			{
				T.TestEqual(TEXT("At the entry's level"), Enemy->GetCharacterLevel(), 2);
				State->Spawned.Add(Enemy);
			}
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondPlayerController* PC = State->PC.Get();
		ABeyondEnemyCharacter* Raider = State->Named.FindRef(TEXT("raider")).Get();
		const UBeyondEnemyPlatesWidget* Plates = PC ? Cast<UBeyondEnemyPlatesWidget>(PC->GetEnemyPlatesWidget()) : nullptr;
		FVector2D ViewportSize = FVector2D::ZeroVector;
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(ViewportSize);
		}
		if (Plates && Raider && ViewportSize.X > 1.0f)
		{
			T.TestTrue(TEXT("The hit raider has a plate"), Plates->GetShownEnemies().Contains(Raider));
		}
		else
		{
			T.AddInfo(TEXT("No game viewport size (-nullrhi): plate projection not checked"));
		}

		// The party wipe event
		if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(GetEnemiesPlayWorld()))
		{
			Combat->OnPartyWiped.Broadcast();
		}
		if (Raider)
		{
			T.TestTrue(TEXT("Wipe: the raider is back home"), Raider->GetActorLocation().Equals(Raider->GetHomeTransform().GetLocation(), 100.0f));
			T.TestEqual(TEXT("Wipe: refilled"), EnemiesHealth(Raider), UBeyondCombatLibrary::GetActorMaxHealth(Raider));
		}
		const ABeyondEnemyCharacter* Summoned = State->Named.FindRef(TEXT("summoned")).Get();
		T.TestTrue(TEXT("Wipe: summoned enemies vanish"), !Summoned || Summoned->IsActorBeingDestroyed());
		if (ABeyondEnemySpawner* Spawner = State->Spawner.Get())
		{
			Spawner->Destroy();
		}
		ClearTestEnemies(*State);
		return true;
	}));

	// Give Ji-Woong back to his companion AI, Angel his health
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
	{
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (AAIController* Holder = State->Holder.Get())
		{
			Holder->UnPossess();
			Holder->Destroy();
		}
		if (AController* Companion = State->CompanionController.Get(); Companion && JiWoong)
		{
			Companion->Possess(JiWoong);
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEnemiesStep([State]()
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
