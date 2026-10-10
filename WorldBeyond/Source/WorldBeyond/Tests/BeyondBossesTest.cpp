// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Abilities/BeyondGA_DuoStrike.h"
#include "AbilitySystem/BeyondAreaStrike.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AI/BeyondEnemyController.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "BeyondGameplayTags.h"
#include "Camera/CameraComponent.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "Characters/BeyondRushComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Enemies/BeyondBossCharacter.h"
#include "Enemies/BeyondBossDefinition.h"
#include "Enemies/BeyondEnemyAnimInstance.h"
#include "Enemies/BeyondEnemySettings.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "EngineUtils.h"
#include "Game/BeyondBossArena.h"
#include "Game/BeyondCombatSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Items/BeyondItemTypes.h"
#include "Items/BeyondLootDrop.h"
#include "Misc/AutomationTest.h"
#include "NavigationSystem.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/BeyondBossBarWidget.h"

/**
 * Plan 3B in the test arena /Game/WorldsBeyond/Maps/TestArena (Play In Editor): the four bosses from
 * migrate_pass11.py spawned through UBeyondEnemySubsystem next to the party, the boss bar (one and two bosses),
 * the health floor and the invulnerable phase transition, every twist (summons and their damage shield, enrage,
 * hazards, shadow clones, darkness), a kill (Bond Point, signature drop, adds gone) and the arena (engage + seal,
 * party-wipe reset, a beaten story boss stays beaten). Saving is off for the run (Beyond.SaveProgress 0).
 *
 * UnrealEditor-Cmd.exe WorldBeyond.uproject -ExecCmds="Automation RunTests WorldsBeyond.Prototype.Bosses;Quit" -unattended -nullrhi -nosplash
 */

namespace BeyondBossesTest
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
		TMap<FString, TWeakObjectPtr<ABeyondBossCharacter>> Bosses;
		TArray<TWeakObjectPtr<AActor>> Cleanup;
		TMap<FString, float> Numbers;
		TWeakObjectPtr<ABeyondBossArena> Arena;
		TWeakObjectPtr<ABeyondBossCharacter> FirstArenaBoss;
		TWeakObjectPtr<ABeyondCharacterBase> DuoTarget;
	};

	UWorld* GetBossesPlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	UBeyondBossDefinition* FindBossDefinition(const TCHAR* Id)
	{
		const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
		return Roster ? Cast<UBeyondBossDefinition>(Roster->FindEnemy(FName(Id))) : nullptr;
	}

	ABeyondBossCharacter* SpawnTestBoss(FState& State, const TCHAR* Id, float Distance, float Side = 0.0f, int32 Level = 1)
	{
		UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(GetBossesPlayWorld());
		UBeyondBossDefinition* Definition = FindBossDefinition(Id);
		if (!Enemies || !Definition)
		{
			return nullptr;
		}
		FBeyondEnemySpawnParams Params;
		Params.Level = Level;
		const FVector Right = FVector::CrossProduct(FVector::UpVector, State.Forward);
		ABeyondBossCharacter* Boss = Cast<ABeyondBossCharacter>(Enemies->SpawnEnemy(Definition,
			FTransform((-State.Forward).Rotation(), State.Anchor + State.Forward * Distance + Right * Side), Params));
		if (Boss)
		{
			if (ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(Boss->GetController()))
			{
				AI->SetBrainEnabled(false);
			}
			State.Bosses.Add(Id, Boss);
			State.Cleanup.Add(Boss);
		}
		return Boss;
	}

	void ClearBossesTest(FState& State)
	{
		for (const TWeakObjectPtr<AActor>& Actor : State.Cleanup)
		{
			if (AActor* Alive = Actor.Get())
			{
				Alive->Destroy();
			}
		}
		State.Cleanup.Reset();
		State.Bosses.Reset();
	}

	void Hit(AActor* Source, AActor* Target, float Amount, bool bIgnoreInvincible = true)
	{
		UBeyondCombatLibrary::ApplyDamage(Source, Target, Amount, BeyondTags::DamageType_Environment, FGameplayTag(), true, nullptr, true, bIgnoreInvincible);
	}

	bool HasBossTag(const AActor* Actor, const FGameplayTag& Tag)
	{
		const ABeyondCharacterBase* Character = Cast<ABeyondCharacterBase>(Actor);
		const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		return ASC && ASC->HasMatchingGameplayTag(Tag);
	}

	bool HasBossAbility(const ABeyondCharacterBase* Character, const TCHAR* Name)
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
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondBossesStep, TFunction<bool()>, Step);
bool FBeyondBossesStep::Update() { return Step(); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondBossesTest, "WorldsBeyond.Prototype.Bosses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondBossesTest::RunTest(const FString& Parameters)
{
	using namespace BeyondBossesTest;
	TSharedRef<FState> State = MakeShared<FState>();
	State->Test = this;

	if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
	{
		State->SavedSaveProgress = SaveProgress->GetString();
		SaveProgress->Set(TEXT("0"), ECVF_SetByCode);
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/WorldsBeyond/Maps/TestArena")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.0f));

	// Setup: the party, the four bosses in the roster
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* World = GetBossesPlayWorld();
		ABeyondPlayerController* PC = World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!T.TestNotNull(TEXT("Beyond player controller"), PC))
		{
			return true;
		}
		State->PC = PC;
		UBeyondPartyComponent* Party = PC->PartyComponent;
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
		for (ABeyondCharacterBase* Member : { Angel, JiWoong })
		{
			if (UAbilitySystemComponent* ASC = Member->GetAbilitySystemComponent())
			{
				ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 1000000.0f);
				ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), 1000000.0f);
			}
		}
		State->Anchor = Angel->GetActorLocation();
		State->Forward = Angel->GetActorForwardVector().GetSafeNormal2D();
		JiWoong->SetActorLocation(State->Anchor - State->Forward * 500.0f, false, nullptr, ETeleportType::TeleportPhysics);

		for (const TCHAR* Id : { TEXT("boss_kaelthar"), TEXT("boss_hrimgar"), TEXT("boss_gorehide"), TEXT("boss_veyla") })
		{
			const UBeyondBossDefinition* Definition = FindBossDefinition(Id);
			if (T.TestNotNull(*FString::Printf(TEXT("migrate_pass11: %s in the roster"), Id), Definition))
			{
				T.TestTrue(*FString::Printf(TEXT("%s has 2 or more phases"), Id), Definition->Phases.Num() >= 2);
				T.TestFalse(*FString::Printf(TEXT("%s has a character class (the Paragon hero)"), Id), Definition->CharacterClass.IsNull());
				T.TestTrue(*FString::Printf(TEXT("%s has a signature drop"), Id), !Definition->GuaranteedLoot.IsEmpty());
				T.TestFalse(*FString::Printf(TEXT("%s has a title"), Id), Definition->Title.IsEmpty());
			}
		}
		return true;
	}));

	// Each boss spawns on its hero Blueprint, set up from its definition
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		float Side = -1500.0f;
		for (const TCHAR* Id : { TEXT("boss_kaelthar"), TEXT("boss_hrimgar"), TEXT("boss_gorehide"), TEXT("boss_veyla") })
		{
			ABeyondBossCharacter* Boss = SpawnTestBoss(*State, Id, 1800.0f, Side, 5);
			Side += 1000.0f;
			if (!T.TestNotNull(*FString::Printf(TEXT("%s spawns as an ABeyondBossCharacter"), Id), Boss))
			{
				continue;
			}
			const UBeyondBossDefinition* Definition = Boss->GetBossDefinition();
			T.TestTrue(*FString::Printf(TEXT("%s is a boss"), Id), UBeyondCombatLibrary::IsBoss(Boss));
			T.TestEqual(*FString::Printf(TEXT("%s starts in its first phase"), Id), Boss->GetPhaseIndex(), 0);
			T.TestTrue(*FString::Printf(TEXT("%s carries Boss.Phase.1"), Id), HasBossTag(Boss, BeyondTags::Boss_Phase_1));
			T.TestTrue(*FString::Printf(TEXT("%s can't be interrupted"), Id), HasBossTag(Boss, BeyondTags::State_Uninterruptible));
			T.TestNotNull(*FString::Printf(TEXT("%s has its hero mesh"), Id), Boss->GetMesh()->GetSkeletalMeshAsset());
			if (Definition && Definition->Mesh)
			{
				T.TestTrue(*FString::Printf(TEXT("%s wears its skin"), Id), Boss->GetMesh()->GetSkeletalMeshAsset() == Definition->Mesh);
			}
			const UAnimInstance* Anim = Boss->GetMesh()->GetAnimInstance();
			T.TestTrue(*FString::Printf(TEXT("%s animates with the hero's anim Blueprint"), Id), Anim && !Anim->IsA<UBeyondEnemyAnimInstance>());
			TInlineComponentArray<UCameraComponent*> Cameras(Boss);
			T.TestEqual(*FString::Printf(TEXT("%s dropped the player camera"), Id), Cameras.Num(), 0);
			const float Expected = Definition && Definition->Phases.IsValidIndex(1)
				? UBeyondCombatLibrary::GetActorMaxHealth(Boss) * Definition->Phases[1].HealthThreshold : 0.0f;
			T.TestTrue(*FString::Printf(TEXT("%s: health floor at the next phase (%.0f)"), Id, Boss->GetHealthFloor()),
				FMath::IsNearlyEqual(Boss->GetHealthFloor(), Expected, 1.0f) && Expected > 0.0f);
		}
		if (const ABeyondBossCharacter* Kaelthar = State->Bosses.FindRef(TEXT("boss_kaelthar")).Get())
		{
			T.TestTrue(TEXT("Kael'thar is a colossus (x2.5)"), Kaelthar->GetActorScale3D().X > 2.0f);
		}
		ClearBossesTest(*State);
		return true;
	}));

	// Boss bar: Hrimgar near the party, then Gorehide too
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		SpawnTestBoss(*State, TEXT("boss_hrimgar"), 1300.0f, -500.0f, 3);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.7f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondPlayerController* PC = State->PC.Get();
		ABeyondBossCharacter* Hrimgar = State->Bosses.FindRef(TEXT("boss_hrimgar")).Get();
		const UBeyondBossBarWidget* Bar = PC ? Cast<UBeyondBossBarWidget>(PC->GetBossBarWidget()) : nullptr;
		if (T.TestNotNull(TEXT("The C++ boss bar shows for a ranked boss"), Bar))
		{
			T.TestTrue(TEXT("It shows Hrimgar"), PC->GetShownBoss() == Hrimgar && Bar->GetBosses().Num() == 1);
		}
		if (Hrimgar)
		{
			T.TestEqual(TEXT("Hrimgar's bar has a notch per later phase"), Hrimgar->GetPhaseThresholds().Num(), 2);
		}
		SpawnTestBoss(*State, TEXT("boss_gorehide"), 1300.0f, 600.0f, 3);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.7f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondPlayerController* PC = State->PC.Get();
		const UBeyondBossBarWidget* Bar = PC ? Cast<UBeyondBossBarWidget>(PC->GetBossBarWidget()) : nullptr;
		T.TestTrue(TEXT("Two bosses: two bars stacked"), Bar && Bar->GetBosses().Num() == 2);

		// The health floor: a huge hit stops at the next threshold
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondBossCharacter* Hrimgar = State->Bosses.FindRef(TEXT("boss_hrimgar")).Get();
		if (Hrimgar && Angel)
		{
			const float Floor = Hrimgar->GetHealthFloor();
			Hit(Angel, Hrimgar, 1.0e7f, false);
			T.TestTrue(*FString::Printf(TEXT("One hit can't skip a phase (%.0f, floor %.0f)"), UBeyondCombatLibrary::GetActorHealth(Hrimgar), Floor),
				FMath::IsNearlyEqual(UBeyondCombatLibrary::GetActorHealth(Hrimgar), Floor, 1.0f));
			T.TestFalse(TEXT("Hrimgar is still alive"), UBeyondCombatLibrary::IsActorDead(Hrimgar));
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondBossCharacter* Hrimgar = State->Bosses.FindRef(TEXT("boss_hrimgar")).Get();
		if (!Hrimgar || !Angel)
		{
			return true;
		}
		T.TestEqual(TEXT("At the threshold Hrimgar enters phase 2"), Hrimgar->GetPhaseIndex(), 1);
		T.TestTrue(TEXT("Phase 2 tag"), HasBossTag(Hrimgar, BeyondTags::Boss_Phase_2) && !HasBossTag(Hrimgar, BeyondTags::Boss_Phase_1));
		T.TestTrue(TEXT("Transition: can't be hurt"), Hrimgar->IsTransitioning() && HasBossTag(Hrimgar, BeyondTags::State_Invincible));
		const float Before = UBeyondCombatLibrary::GetActorHealth(Hrimgar);
		Hit(Angel, Hrimgar, 500.0f, false);
		T.TestEqual(TEXT("Transition: hits do nothing"), UBeyondCombatLibrary::GetActorHealth(Hrimgar), Before);
		T.TestEqual(TEXT("Twist: the pack is summoned (3)"), Hrimgar->GetMinions().Num(), 3);
		bool bAllSummoned = true;
		for (const ABeyondEnemyCharacter* Minion : Hrimgar->GetMinions())
		{
			bAllSummoned &= Minion->bSummoned && !Minion->CanDropLoot();
		}
		T.TestTrue(TEXT("Summons drop nothing"), bAllSummoned);
		T.TestTrue(TEXT("Phase 2 abilities granted (Avalanche Charge)"), HasBossAbility(Hrimgar, TEXT("GA_Hrimgar_Avalanche")));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.7f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondBossCharacter* Hrimgar = State->Bosses.FindRef(TEXT("boss_hrimgar")).Get();
		if (!Hrimgar)
		{
			return true;
		}
		T.TestFalse(TEXT("After the transition: vulnerable again"), Hrimgar->IsTransitioning() || HasBossTag(Hrimgar, BeyondTags::State_Invincible));
		const ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(Hrimgar->GetController());
		T.TestTrue(TEXT("After the transition: fighting again"), AI && AI->IsBrainEnabled());

		// Enrage at the last phase
		const float Strength = Hrimgar->GetAbilitySystemComponent()->GetNumericAttribute(UCharacterAttributeSet::GetStrengthAttribute());
		Hrimgar->ForcePhase(2);
		T.TestTrue(TEXT("Twist: Hrimgar enrages"), Hrimgar->IsEnraged() && HasBossTag(Hrimgar, BeyondTags::Boss_Enraged));
		T.TestTrue(TEXT("Enraged: +40 Strength"), FMath::IsNearlyEqual(
			Hrimgar->GetAbilitySystemComponent()->GetNumericAttribute(UCharacterAttributeSet::GetStrengthAttribute()), Strength + 40.0f, 0.5f));
		T.TestTrue(TEXT("Enraged: faster"), Hrimgar->CombatSpeedScale > 1.2f);
		T.TestEqual(TEXT("The last phase has no floor"), Hrimgar->GetHealthFloor(), 0.0f);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		// Kill Hrimgar: a Bond Point, his signature drop, his adds fall with him
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondBossCharacter* Hrimgar = State->Bosses.FindRef(TEXT("boss_hrimgar")).Get();
		const ABeyondPlayerController* PC = State->PC.Get();
		if (!Angel || !Hrimgar || !PC)
		{
			return true;
		}
		State->Numbers.Add(TEXT("BondPoints"), static_cast<float>(PC->PartyComponent->GetBondPoints()));
		Hit(Angel, Hrimgar, 1.0e7f, true);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondBossCharacter* Hrimgar = State->Bosses.FindRef(TEXT("boss_hrimgar")).Get();
		const ABeyondPlayerController* PC = State->PC.Get();
		T.TestTrue(TEXT("Hrimgar is dead"), !Hrimgar || UBeyondCombatLibrary::IsActorDead(Hrimgar));
		if (PC)
		{
			// At least one: the boss's EXP can also level the party past a Bond Point level
			T.TestTrue(*FString::Printf(TEXT("A main boss gives a Bond Point (%.0f -> %d)"), State->Numbers.FindRef(TEXT("BondPoints")),
				PC->PartyComponent->GetBondPoints()), PC->PartyComponent->GetBondPoints() >= static_cast<int32>(State->Numbers.FindRef(TEXT("BondPoints"))) + 1);
		}
		if (Hrimgar)
		{
			T.TestEqual(TEXT("His adds fell with him"), Hrimgar->GetMinions().Num(), 0);
		}
		const UBeyondBossDefinition* Definition = FindBossDefinition(TEXT("boss_hrimgar"));
		bool bSignature = false;
		for (TActorIterator<ABeyondLootDrop> It(GetBossesPlayWorld()); It && Definition; ++It)
		{
			bSignature |= Definition->GuaranteedLoot.ContainsByPredicate([&It](const TSoftObjectPtr<UBeyondItemDefinition>& Item)
			{
				return Item == It->GetItem().Definition;
			});
			State->Cleanup.Add(*It);
		}
		T.TestTrue(TEXT("Hrimgar dropped the Glacierheart Rod"), bSignature);
		ClearBossesTest(*State);
		return true;
	}));

	// Kael'thar's burning arena, Veyla's shadow clones, Gorehide's pack shield
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondBossCharacter* Kaelthar = SpawnTestBoss(*State, TEXT("boss_kaelthar"), 1500.0f, -700.0f, 3);
		ABeyondBossCharacter* Veyla = SpawnTestBoss(*State, TEXT("boss_veyla"), 1500.0f, 700.0f, 3);
		ABeyondBossCharacter* Gorehide = SpawnTestBoss(*State, TEXT("boss_gorehide"), 1900.0f, 0.0f, 3);
		if (!Angel || !Kaelthar || !Veyla || !Gorehide)
		{
			T.AddError(TEXT("Could not spawn the twist bosses"));
			return true;
		}

		Kaelthar->ForcePhase(2);
		const TArray<ABeyondAreaStrike*> Hazards = Kaelthar->GetHazards();
		int32 Rings = 0;
		for (const ABeyondAreaStrike* Hazard : Hazards)
		{
			Rings += Hazard->GetSettings().Shape == EBeyondStrikeShape::Ring && Hazard->GetSettings().LingerDuration > 60.0f ? 1 : 0;
		}
		T.TestEqual(TEXT("Twist: the arena's edge turns to lava (one long ring)"), Rings, 1);
		T.TestTrue(*FString::Printf(TEXT("Twist: lava pools fall on the demigods (%d hazards)"), Hazards.Num()), Hazards.Num() >= 3);

		Veyla->ForcePhase(1);
		const TArray<ABeyondEnemyCharacter*> Clones = Veyla->GetMinions();
		T.TestEqual(TEXT("Twist: Veyla splits into 2 shadow clones"), Clones.Num(), 2);
		for (ABeyondEnemyCharacter* Clone : Clones)
		{
			T.TestTrue(TEXT("A clone is no boss (no bar, no phases)"), Clone->bClone && !UBeyondCombatLibrary::IsBoss(Clone) && Clone->GetHealthFloor() == 0.0f);
			T.TestTrue(TEXT("A clone hits for 30 %"), FMath::IsNearlyEqual(Clone->GetOutgoingDamageScale(), 0.3f, 0.01f));
			T.TestTrue(TEXT("A clone has 10 % of the health"), UBeyondCombatLibrary::GetActorMaxHealth(Clone) < UBeyondCombatLibrary::GetActorMaxHealth(Veyla) * 0.2f);
		}

		Gorehide->ForcePhase(1);
		T.TestEqual(TEXT("Twist: Gorehide howls in 3 wolves"), Gorehide->GetMinions().Num(), 3);
		float Before = UBeyondCombatLibrary::GetActorHealth(Gorehide);
		Hit(Angel, Gorehide, 100.0f, true);
		State->Numbers.Add(TEXT("Shielded"), Before - UBeyondCombatLibrary::GetActorHealth(Gorehide));
		for (ABeyondEnemyCharacter* Wolf : Gorehide->GetMinions())
		{
			Wolf->Destroy();
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.1f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondBossCharacter* Gorehide = State->Bosses.FindRef(TEXT("boss_gorehide")).Get();
		ABeyondBossCharacter* Kaelthar = State->Bosses.FindRef(TEXT("boss_kaelthar")).Get();
		if (Gorehide && Angel)
		{
			const float Before = UBeyondCombatLibrary::GetActorHealth(Gorehide);
			Hit(Angel, Gorehide, 100.0f, true);
			const float Open = Before - UBeyondCombatLibrary::GetActorHealth(Gorehide);
			T.TestTrue(*FString::Printf(TEXT("While his wolves live Gorehide takes half damage (%.1f vs %.1f)"), State->Numbers.FindRef(TEXT("Shielded")), Open),
				Open > 0.0f && FMath::IsNearlyEqual(State->Numbers.FindRef(TEXT("Shielded")) / Open, 0.5f, 0.05f));
		}
		if (Kaelthar)
		{
			Kaelthar->Destroy();
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.1f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		int32 Hazards = 0;
		for (TActorIterator<ABeyondAreaStrike> It(GetBossesPlayWorld()); It; ++It)
		{
			Hazards += It->GetSettings().LingerDuration > 60.0f ? 1 : 0;
		}
		T.TestEqual(TEXT("A boss's hazards leave with it"), Hazards, 0);
		ClearBossesTest(*State);
		return true;
	}));

	// The arena: wakes and seals, resets on a wipe, a beaten story boss stays beaten
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		UWorld* World = GetBossesPlayWorld();
		const FTransform Where(State->Anchor + State->Forward * 3000.0f);
		ABeyondBossArena* Arena = World->SpawnActorDeferred<ABeyondBossArena>(ABeyondBossArena::StaticClass(), Where);
		if (Arena)
		{
			Arena->Boss = FindBossDefinition(TEXT("boss_kaelthar"));
			Arena->BossLevel = 2;
			Arena->ArenaRadius = 1200.0f;
			Arena->EngageRadius = 700.0f;
			Arena->RespawnDelay = 0.3f;
			Arena->FinishSpawning(Where);
			State->Arena = Arena;
			State->Cleanup.Add(Arena);
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.9f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondBossArena* Arena = State->Arena.Get();
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondBossCharacter* Boss = Arena ? Arena->GetBoss() : nullptr;
		if (!T.TestNotNull(TEXT("The arena spawned its boss"), Boss) || !Angel)
		{
			return true;
		}
		State->FirstArenaBoss = Boss;
		const ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(Boss->GetController());
		T.TestTrue(TEXT("It waits (dormant) for the party"), Arena->GetArenaState() == EBeyondArenaState::Dormant && AI && !AI->IsBrainEnabled());
		T.TestTrue(TEXT("The boss knows its arena"), Boss->GetArena() == Arena);

		// Both demigods outside the ring: Angel just outside (she dashes in), Ji-Woong far out (he blinks in)
		const FVector Centre = Arena->GetActorLocation();
		Angel->SetActorLocation(Centre - State->Forward * (Arena->ArenaRadius + 250.0f) + FVector(0.0f, 0.0f, 5.0f), false, nullptr, ETeleportType::TeleportPhysics);
		if (ABeyondCharacterBase* JiWoong = State->JiWoong.Get())
		{
			JiWoong->SetActorLocation(Centre - State->Forward * (Arena->ArenaRadius + 2000.0f) + FVector(0.0f, 0.0f, 5.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		Arena->Engage(Angel);
		T.TestTrue(TEXT("Engaged: fighting"), Arena->GetArenaState() == EBeyondArenaState::Fighting);
		T.TestTrue(TEXT("Engaged: the boss's brain is on"), AI && AI->IsBrainEnabled());
		T.TestTrue(TEXT("The walls wait while Angel dashes in (Ji-Woong blinked)"), !Arena->IsSealed() && Arena->GetMembersComingIn() == 1
			&& UBeyondRushComponent::FindRush(Angel) != nullptr);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondBossArena* Arena = State->Arena.Get();
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!Arena || !Angel || !JiWoong)
		{
			return true;
		}
		T.TestTrue(TEXT("Sealed once everyone was in"), Arena->IsSealed() && Arena->GetMembersComingIn() == 0);
		for (const ABeyondCharacterBase* Member : { Angel, JiWoong })
		{
			const float Radius = Member->GetCapsuleComponent()->GetScaledCapsuleRadius();
			T.TestTrue(*FString::Printf(TEXT("%s is inside the ring (%.0f from the centre, ring %.0f)"), *Member->GetName(),
				FVector::Dist2D(Member->GetActorLocation(), Arena->GetActorLocation()), Arena->ArenaRadius), Arena->IsInside(Member->GetActorLocation(), Radius));
			T.TestTrue(*FString::Printf(TEXT("%s is back to normal after the dash in"), *Member->GetName()), !UBeyondRushComponent::FindRush(Member)
				&& !HasBossTag(Member, BeyondTags::State_Dashing) && !Member->GetCharacterMovement()->HasRootMotionSources()
				&& Member->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block);
		}

		// Darkness: Veyla's last phase darkens the arena she fights in
		if (ABeyondBossCharacter* Veyla = SpawnTestBoss(*State, TEXT("boss_veyla"), 900.0f, 800.0f, 2))
		{
			Veyla->SetArena(Arena);
			Veyla->ForcePhase(2);
			T.TestTrue(TEXT("Twist: the arena darkens"), Arena->GetDarkness() > 0.5f);
			Veyla->Destroy();
			T.TestEqual(TEXT("Darkness lifts when its boss is gone"), Arena->GetDarkness(), 0.0f);
		}

		// The party wipes (and respawns back at the start, out of the arena)
		Angel->SetActorLocation(State->Anchor, false, nullptr, ETeleportType::TeleportPhysics);
		JiWoong->SetActorLocation(State->Anchor - State->Forward * 500.0f, false, nullptr, ETeleportType::TeleportPhysics);
		if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(GetBossesPlayWorld()))
		{
			Combat->OnPartyWiped.Broadcast();
		}
		T.TestTrue(TEXT("Wipe: the boss is removed"), !State->FirstArenaBoss.IsValid() || State->FirstArenaBoss->IsActorBeingDestroyed());
		T.TestTrue(TEXT("Wipe: the arena opens again"), !Arena->IsSealed() && Arena->GetArenaState() == EBeyondArenaState::Dormant);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.7f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondBossArena* Arena = State->Arena.Get();
		ABeyondBossCharacter* Boss = Arena ? Arena->GetBoss() : nullptr;
		if (!T.TestNotNull(TEXT("Wipe: a fresh boss stands in the arena again"), Boss))
		{
			return true;
		}
		T.TestTrue(TEXT("Fresh: a new boss, full health, first phase"), Boss != State->FirstArenaBoss.Get()
			&& FMath::IsNearlyEqual(UBeyondCombatLibrary::GetActorHealthPercent(Boss), 1.0f) && Boss->GetPhaseIndex() == 0);
		State->Cleanup.Add(Boss);

		// The buddy walks in first while the leader hangs back outside: the fight starts and the leader is brought in
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (Angel && JiWoong)
		{
			const FVector Centre = Arena->GetActorLocation();
			JiWoong->SetActorLocation(Centre - State->Forward * (Arena->EngageRadius - 150.0f) + FVector(0.0f, 0.0f, 5.0f), false, nullptr, ETeleportType::TeleportPhysics);
			Angel->SetActorLocation(Centre - State->Forward * (Arena->ArenaRadius + 300.0f) + FVector(0.0f, 0.0f, 5.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.8f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondBossArena* Arena = State->Arena.Get();
		ABeyondBossCharacter* Boss = Arena ? Arena->GetBoss() : nullptr;
		const ABeyondCharacterBase* Angel = State->Angel.Get();
		if (!Arena || !Boss || !Angel)
		{
			return true;
		}
		T.TestTrue(TEXT("Ji-Woong walking in started the fight"), Arena->GetArenaState() == EBeyondArenaState::Fighting && Arena->IsSealed());
		T.TestTrue(*FString::Printf(TEXT("The leader (Angel) was brought inside too (%.0f from the centre)"),
			FVector::Dist2D(Angel->GetActorLocation(), Arena->GetActorLocation())),
			Arena->IsInside(Angel->GetActorLocation(), Angel->GetCapsuleComponent()->GetScaledCapsuleRadius()));
		// Down to the last phase, then the kill
		Boss->ForcePhase(Boss->GetPhaseCount() - 1);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondBossArena* Arena = State->Arena.Get();
		ABeyondBossCharacter* Boss = Arena ? Arena->GetBoss() : nullptr;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		const ABeyondPlayerController* PC = State->PC.Get();
		if (!Arena || !Boss || !Angel || !PC)
		{
			return true;
		}
		Arena->Engage(Angel);
		Hit(Angel, Boss, 1.0e8f, true);
		T.TestTrue(TEXT("The arena boss is dead"), UBeyondCombatLibrary::IsActorDead(Boss));
		T.TestTrue(TEXT("The arena is won and open"), Arena->GetArenaState() == EBeyondArenaState::Defeated && !Arena->IsSealed());
		T.TestTrue(TEXT("The story boss is remembered as beaten"), PC->PartyComponent->IsBossDefeated(TEXT("kaelthar")));

		// A second arena for the same boss doesn't bring it back
		UWorld* World = GetBossesPlayWorld();
		const FTransform Where(State->Anchor - State->Forward * 2500.0f);
		if (ABeyondBossArena* Again = World->SpawnActorDeferred<ABeyondBossArena>(ABeyondBossArena::StaticClass(), Where))
		{
			Again->Boss = FindBossDefinition(TEXT("boss_kaelthar"));
			Again->FinishSpawning(Where);
			State->Cleanup.Add(Again);
			State->Arena = Again;
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.8f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondBossArena* Again = State->Arena.Get();
		T.TestTrue(TEXT("A beaten story boss's arena stays empty"), Again && !Again->GetBoss() && Again->GetArenaState() == EBeyondArenaState::Defeated);
		if (const ABeyondPlayerController* PC = State->PC.Get())
		{
			PC->PartyComponent->ResetDefeatedBosses();
			T.TestFalse(TEXT("Beyond.ResetBosses forgets it"), PC->PartyComponent->IsBossDefeated(TEXT("kaelthar")));
		}
		for (TActorIterator<ABeyondLootDrop> It(GetBossesPlayWorld()); It; ++It)
		{
			State->Cleanup.Add(*It);
		}
		ClearBossesTest(*State);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
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

	// The duo move with Ji-Woong (back on his companion AI) far from the enemy: Angel starts it, it locks onto the enemy,
	// Ji-Woong charges beside her, rushes over and his slam lands on it
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		const ABeyondPlayerController* PC = State->PC.Get();
		UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(GetBossesPlayWorld());
		const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
		UBeyondEnemyDefinition* Raider = Roster ? Roster->FindEnemy(TEXT("forest_raider")) : nullptr;
		if (!Angel || !JiWoong || !PC || !Enemies || !T.TestNotNull(TEXT("Duo: a Raider in the roster"), Raider))
		{
			return true;
		}
		const FVector Right = FVector::CrossProduct(FVector::UpVector, State->Forward);
		Angel->SetActorLocation(State->Anchor, false, nullptr, ETeleportType::TeleportPhysics);
		JiWoong->SetActorLocation(State->Anchor + Right * 400.0f, false, nullptr, ETeleportType::TeleportPhysics);
		ABeyondEnemyCharacter* Target = Enemies->SpawnEnemy(Raider, FTransform((-State->Forward).Rotation(), State->Anchor + State->Forward * 1800.0f),
			FBeyondEnemySpawnParams());
		if (!T.TestNotNull(TEXT("Duo: the target enemy spawned"), Target))
		{
			return true;
		}
		State->Cleanup.Add(Target);
		State->DuoTarget = Target;
		if (ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(Target->GetController()))
		{
			AI->SetBrainEnabled(false);
		}
		if (UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent())
		{
			TargetASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 100000.0f);
			TargetASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), 100000.0f);
		}
		T.TestTrue(TEXT("Duo: Ji-Woong starts far out of the shockwave's reach"), FVector::Dist2D(JiWoong->GetActorLocation(), Target->GetActorLocation()) > 1200.0f);

		PC->PartyComponent->AddBond(1000.0f);
		T.TestTrue(TEXT("Duo: Angel (the leader) starts it"), Angel->TryActivateAbilityByInputTag(BeyondTags::Ability_Input_Duo));
		const UBeyondGA_DuoStrike* Duo = nullptr;
		for (const FGameplayAbilitySpec& Spec : Angel->GetAbilitySystemComponent()->GetActivatableAbilities())
		{
			for (const UGameplayAbility* Instance : Spec.GetAbilityInstances())
			{
				if (const UBeyondGA_DuoStrike* Running = Cast<UBeyondGA_DuoStrike>(Instance); Running && Running->IsActive())
				{
					Duo = Running;
				}
			}
		}
		T.TestTrue(TEXT("Duo: it locks onto the enemy"), Duo && Duo->GetLockedTarget() == Target);
		State->Numbers.Add(TEXT("DuoStart"), GetBossesPlayWorld()->GetTimeSeconds());
		State->Numbers.Add(TEXT("DuoClosest"), TNumericLimits<float>::Max());
		return true;
	}));
	// Until the move is over (afterwards his AI walks him back to Angel): how close did Ji-Woong get?
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		const ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		const ABeyondCharacterBase* Target = State->DuoTarget.Get();
		const UWorld* World = GetBossesPlayWorld();
		if (!JiWoong || !Target || !World)
		{
			return true;
		}
		float& Closest = State->Numbers.FindOrAdd(TEXT("DuoClosest"));
		Closest = FMath::Min(Closest, static_cast<float>(FVector::Dist2D(JiWoong->GetActorLocation(), Target->GetActorLocation())));
		const float Elapsed = World->GetTimeSeconds() - State->Numbers.FindRef(TEXT("DuoStart"));
		return (Elapsed > 1.0f && !HasBossTag(JiWoong, BeyondTags::State_Duo)) || Elapsed > 9.0f;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondCharacterBase* Angel = State->Angel.Get();
		const ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		ABeyondCharacterBase* Target = State->DuoTarget.Get();
		if (!Angel || !JiWoong || !Target)
		{
			return true;
		}
		T.TestTrue(*FString::Printf(TEXT("Duo: the final blow landed (%.0f damage)"), 100000.0f - UBeyondCombatLibrary::GetActorHealth(Target)),
			UBeyondCombatLibrary::GetActorHealth(Target) < 100000.0f - 50.0f);
		const float Reach = JiWoong->GetCapsuleComponent()->GetScaledCapsuleRadius() + Target->GetCapsuleComponent()->GetScaledCapsuleRadius() + 400.0f;
		const float Closest = State->Numbers.FindRef(TEXT("DuoClosest"));
		T.TestTrue(*FString::Printf(TEXT("Duo: Ji-Woong rushed to the enemy (closest %.0f, reach %.0f)"), Closest, Reach), Closest <= Reach);
		for (const ABeyondCharacterBase* Member : { Angel, JiWoong })
		{
			T.TestTrue(*FString::Printf(TEXT("Duo: %s is back to normal"), *Member->GetName()),
				!HasBossTag(Member, BeyondTags::State_Duo) && !HasBossTag(Member, BeyondTags::State_Invincible) && !HasBossTag(Member, BeyondTags::State_Dashing)
				&& !UBeyondRushComponent::FindRush(Member) && !Member->GetCharacterMovement()->HasRootMotionSources()
				&& Member->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block);
		}
		ClearBossesTest(*State);
		return true;
	}));

	// Each demigod has one character attribute set, the one GAS uses, so the level it shows (and saves) is its Level
	// attribute and its hits reach its handlers (BP_Angel carried a stale second set: level stuck at 1, no hit reactions)
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondPlayerController* PC = State->PC.Get();
		if (!PC)
		{
			return true;
		}
		const ABeyondCharacterBase* Leader = PC->PartyComponent->GetLeader();
		PC->PartyComponent->AwardExperience(Leader ? Leader->GetExperienceToNextLevel() + 1.0f : 1000.0f);
		for (const ABeyondCharacterBase* Member : PC->PartyComponent->GetMembers())
		{
			const UAbilitySystemComponent* ASC = Member ? Member->GetAbilitySystemComponent() : nullptr;
			if (!ASC)
			{
				continue;
			}
			int32 CharacterSets = 0;
			TArray<FString> SetInfo;
			for (const UAttributeSet* Set : ASC->GetSpawnedAttributes())
			{
				if (const UCharacterAttributeSet* CharacterSet = Cast<UCharacterAttributeSet>(Set))
				{
					++CharacterSets;
					SetInfo.Add(FString::Printf(TEXT("%s level %.0f health %.0f hit-bound %d%s"), *CharacterSet->GetName(), CharacterSet->GetLevel(),
						CharacterSet->GetCurrentHealth(), CharacterSet->OnHitTaken.IsBound(), CharacterSet == ASC->GetSet<UCharacterAttributeSet>() ? TEXT(" [ASC]") : TEXT("")));
				}
			}
			const float Base = ASC->GetNumericAttributeBase(UCharacterAttributeSet::GetLevelAttribute());
			const float Current = ASC->GetNumericAttribute(UCharacterAttributeSet::GetLevelAttribute());
			const FString Sets = FString::Printf(TEXT("%d set(s): %s; Attribute Set %s"), CharacterSets, *FString::Join(SetInfo, TEXT(" | ")),
				*GetNameSafe(Member->AttributeSet));
			const UCharacterAttributeSet* Used = ASC->GetSet<UCharacterAttributeSet>();
			T.TestTrue(*FString::Printf(TEXT("%s: one character attribute set, the one GAS uses, with its hit handler bound (%s)"), *Member->GetName(), *Sets),
				CharacterSets == 1 && Used && Member->AttributeSet == Used && Used->OnHitTaken.IsBound());
			T.TestEqual(*FString::Printf(TEXT("%s: its level is its Level attribute (base %.0f, current %.0f)"), *Member->GetName(), Base, Current),
				Member->GetCharacterLevel(), FMath::RoundToInt(Base));
			T.TestTrue(*FString::Printf(TEXT("%s: levelled up past 1"), *Member->GetName()), Base > 1.0f);
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBossesStep([State]()
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
