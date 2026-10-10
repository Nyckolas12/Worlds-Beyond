// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Abilities/BeyondGA_DuoStrike.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "BeyondGameplayTags.h"
#include "BrainComponent.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Game/BeyondCombatSubsystem.h"
#include "Game/BeyondSaveGame.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/AutomationTest.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Progression/BeyondProgressionSettings.h"
#include "Progression/BeyondSkillTreeComponent.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/BeyondSkillTreeWidget.h"

/**
 * Plan 1B in MAP_Demo_Main (Play In Editor): the three skill trees from migrate_pass8.py, level and prerequisite
 * gates, unlocking stats / ability ranks / max health, resetting with a refund, Bond Points, the duo tree (Bond gain,
 * Eclipse Brand unlocked on both demigods, the duo loadout on G), the skill tree screen (opens paused, hold to unlock,
 * closes), saved ranks, a boss's Bond Point and Tempest Aegis's storm shield. Saving is off for the run.
 *
 * UnrealEditor-Cmd.exe WorldBeyond.uproject -ExecCmds="Automation RunTests WorldsBeyond.Prototype;Quit" -unattended -nullrhi -nosplash
 */

namespace BeyondSkillTreeTest
{
	struct FState
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ABeyondPlayerController> PC;
		TWeakObjectPtr<ABeyondCharacterBase> Angel;
		TWeakObjectPtr<ABeyondCharacterBase> JiWoong;
		TWeakObjectPtr<ABeyondCharacterBase> Enemy;
		// Ranked Boss for a moment and defeated (the map's real boss is left alone: its death may run level scripting)
		TWeakObjectPtr<ABeyondCharacterBase> StandInBoss;
		TWeakObjectPtr<AAIController> Holder;
		TWeakObjectPtr<AController> CompanionController;
		TMap<FString, float> Numbers;
		FString SavedSaveProgress;
		double SettleStartTime = 0.0;
	};

	UWorld* GetPlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	float ReadAttribute(const ABeyondCharacterBase* Character, const FGameplayAttribute& Attribute)
	{
		const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		return ASC ? ASC->GetNumericAttribute(Attribute) : 0.0f;
	}

	// Level of the spec granting AbilityClass (0 when not granted)
	int32 SpecLevel(const ABeyondCharacterBase* Character, const UClass* AbilityClass)
	{
		const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		if (ASC && AbilityClass)
		{
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass)
				{
					return Spec.Level;
				}
			}
		}
		return 0;
	}

	bool HasSpecWithTag(const ABeyondCharacterBase* Character, const UClass* AbilityClass, const FGameplayTag& Tag)
	{
		const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		if (ASC && AbilityClass)
		{
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass)
				{
					return Spec.GetDynamicSpecSourceTags().HasTagExact(Tag);
				}
			}
		}
		return false;
	}

	// The ability an effect of this type on this node points at
	UClass* NodeAbility(const UBeyondSkillTreeComponent* Tree, FName NodeId, EBeyondSkillEffectType Type)
	{
		const FBeyondSkillNode* Node = Tree && Tree->Tree ? Tree->Tree->FindNode(NodeId) : nullptr;
		if (Node)
		{
			for (const FBeyondSkillEffect& Effect : Node->Effects)
			{
				if (Effect.Type == Type && Effect.Ability)
				{
					return Effect.Ability.Get();
				}
			}
		}
		return nullptr;
	}

	void HoldStill(ABeyondCharacterBase* Enemy)
	{
		if (!Enemy)
		{
			return;
		}
		if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
		{
			AI->StopMovement();
			if (UBrainComponent* Brain = AI->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Skill tree test"));
			}
		}
		if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
		{
			ASC->SetLooseGameplayTagCount(BeyondTags::State_Blocking, 0);
			ASC->SetLooseGameplayTagCount(BeyondTags::State_Parrying, 0);
			ASC->SetLooseGameplayTagCount(BeyondTags::State_Invincible, 0);
		}
		Enemy->GetCharacterMovement()->StopMovementImmediately();
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondSkillTreeStep, TFunction<bool()>, Step);
bool FBeyondSkillTreeStep::Update()
{
	return Step();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondSkillTreeTest, "WorldsBeyond.Prototype.SkillTree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondSkillTreeTest::RunTest(const FString& Parameters)
{
	using namespace BeyondSkillTreeTest;
	TSharedRef<FState> State = MakeShared<FState>();
	State->Test = this;

	// Start fresh and never write the player's save
	if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
	{
		State->SavedSaveProgress = SaveProgress->GetString();
		SaveProgress->Set(TEXT("0"), ECVF_SetByCode);
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/SICKA_PERSEPOLIS/MAPS/MAP_Demo_Main")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.0f));

	// Setup: trees from migrate_pass8.py, valid, gated at level 1
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondSkillTreeStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* World = GetPlayWorld();
		ABeyondPlayerController* PC = World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!T.TestNotNull(TEXT("Beyond player controller"), PC))
		{
			return true;
		}
		State->PC = PC;
		UBeyondPartyComponent* Party = PC->PartyComponent;

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
		// Ji-Woong's companion AI would fight on its own; park him on a plain AI controller
		State->CompanionController = JiWoong->GetController();
		if (AAIController* Holder = World->SpawnActor<AAIController>())
		{
			Holder->Possess(JiWoong);
			State->Holder = Holder;
		}
		for (TActorIterator<ABeyondCharacterBase> It(World); It; ++It)
		{
			if (It->TeamAffiliation == EBeyondTeam::Enemy && !UBeyondCombatLibrary::IsActorDead(*It))
			{
				HoldStill(*It);
				if (It->BossBarWidgetClass)
				{
					continue;
				}
				if (!State->Enemy.IsValid())
				{
					State->Enemy = *It;
				}
				else if (!State->StandInBoss.IsValid())
				{
					State->StandInBoss = *It;
				}
			}
		}

		UBeyondSkillTreeComponent* AngelTree = Angel->GetSkillTreeComponent();
		UBeyondSkillTreeComponent* JiWoongTree = JiWoong->GetSkillTreeComponent();
		UBeyondDuoSkillTreeComponent* DuoTree = PC->DuoSkillTree;
		if (!T.TestTrue(TEXT("migrate_pass8: Angel has a skill tree"), AngelTree && AngelTree->Tree)
			|| !T.TestTrue(TEXT("migrate_pass8: Ji-Woong has a skill tree"), JiWoongTree && JiWoongTree->Tree)
			|| !T.TestTrue(TEXT("migrate_pass8: the party has the duo tree"), DuoTree && DuoTree->Tree))
		{
			return true;
		}
		for (const UBeyondSkillTreeComponent* Tree : { static_cast<const UBeyondSkillTreeComponent*>(AngelTree), static_cast<const UBeyondSkillTreeComponent*>(JiWoongTree),
			static_cast<const UBeyondSkillTreeComponent*>(DuoTree) })
		{
			const TArray<FString> Problems = Tree->Tree->ValidateTree();
			T.TestTrue(*FString::Printf(TEXT("%s is valid%s"), *Tree->Tree->GetName(), Problems.IsEmpty() ? TEXT("") : *(TEXT(": ") + Problems[0])), Problems.IsEmpty());
		}
		T.TestTrue(TEXT("The duo tree pays in Bond Points"), DuoTree->Tree->Currency == EBeyondSkillCurrency::BondPoints);
		T.TestNotNull(TEXT("The duo tree's starter is Heaven's Judgment"), DuoTree->GetStarterDuoPower().Get());
		T.TestTrue(TEXT("Heaven's Judgment starts on G"), DuoTree->GetDuoLoadout() == DuoTree->GetStarterDuoPower());

		// Gates at level 1 with no points
		FText Reason;
		T.TestEqual(TEXT("Starts with no skill points"), AngelTree->GetAvailablePoints(), 0);
		T.TestFalse(TEXT("No points: can't unlock a root node"), AngelTree->CanUnlock(TEXT("Angel.Arcana1"), Reason));
		T.TestTrue(TEXT("The reason names the points"), Reason.ToString().Contains(TEXT("Skill Points")));
		T.TestTrue(TEXT("A root node is available"), AngelTree->GetNodeState(TEXT("Angel.Arcana1")) == EBeyondSkillNodeState::Available);
		T.TestTrue(TEXT("A gated node is locked"), AngelTree->GetNodeState(TEXT("Angel.Spikes")) == EBeyondSkillNodeState::Locked);
		T.TestEqual(TEXT("No Bond Points yet"), Party->GetBondPoints(), 0);

		// Up to level 10 for everyone: nine skill points each, Bond Points at 3, 6 and 9
		const float Needed = UBeyondProgressionSettings::GetTotalExperience(10, 0.0f) - UBeyondProgressionSettings::GetTotalExperience(Angel->GetCharacterLevel(), Angel->GetExperience());
		Party->AwardExperience(Needed);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));

	// Angel's tree: stats, ability ranks, max health, prerequisites, reset
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondSkillTreeStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondPlayerController* PC = State->PC.Get();
		UBeyondSkillTreeComponent* Tree = Angel ? Angel->GetSkillTreeComponent() : nullptr;
		if (!PC || !Tree || !Tree->Tree)
		{
			return true;
		}
		T.TestEqual(TEXT("Angel reached level 10"), Angel->GetCharacterLevel(), 10);
		const int32 Points = Tree->GetAvailablePoints();
		T.TestEqual(TEXT("Nine skill points"), Points, 9 * GetDefault<UBeyondProgressionSettings>()->SkillPointsPerLevel);
		T.TestEqual(TEXT("Three Bond Points (levels 3, 6, 9)"), PC->PartyComponent->GetBondPoints(), 3);

		const FGameplayAttribute Arcana = UCharacterAttributeSet::GetArcanaAttribute();
		const float ArcanaBefore = ReadAttribute(Angel, Arcana);
		T.TestTrue(TEXT("Unlock Storm Affinity"), Tree->Unlock(TEXT("Angel.Arcana1")));
		T.TestEqual(TEXT("Storm Affinity: +4 Arcana"), ReadAttribute(Angel, Arcana) - ArcanaBefore, 4.0f, 0.01f);
		T.TestEqual(TEXT("It cost a point"), Tree->GetAvailablePoints(), Points - 1);
		T.TestTrue(TEXT("Acquired after one rank"), Tree->GetNodeState(TEXT("Angel.Arcana1")) == EBeyondSkillNodeState::Acquired);
		Tree->Unlock(TEXT("Angel.Arcana1"));
		Tree->Unlock(TEXT("Angel.Arcana1"));
		T.TestEqual(TEXT("Three ranks: +12 Arcana"), ReadAttribute(Angel, Arcana) - ArcanaBefore, 12.0f, 0.01f);
		T.TestTrue(TEXT("Maxed at three ranks"), Tree->GetNodeState(TEXT("Angel.Arcana1")) == EBeyondSkillNodeState::Maxed);
		T.TestFalse(TEXT("No fourth rank"), Tree->Unlock(TEXT("Angel.Arcana1")));

		// Ability rank: Arcane Spikes' spec goes up a level
		UClass* Spikes = NodeAbility(Tree, TEXT("Angel.Spikes"), EBeyondSkillEffectType::AbilityRank);
		T.TestTrue(TEXT("Crystal Eruption ranks up Arcane Spikes"), Spikes && SpecLevel(Angel, Spikes) == 1);
		T.TestTrue(TEXT("Unlock Crystal Eruption"), Tree->Unlock(TEXT("Angel.Spikes")));
		T.TestEqual(TEXT("Arcane Spikes is level 2"), SpecLevel(Angel, Spikes), 2);
		if (const UBeyondGameplayAbility* SpikesCDO = Spikes ? Cast<UBeyondGameplayAbility>(Spikes->GetDefaultObject()) : nullptr)
		{
			T.TestTrue(TEXT("Level 2 deals more and recharges faster"), SpikesCDO->DamagePerLevel > 0.0f && SpikesCDO->GetLevelCooldownScale(2) < 1.0f);
		}

		// Max health grows, and the new health comes with it
		const FGameplayAttribute MaxHealth = UCharacterAttributeSet::GetMaxHealthAttribute();
		const float MaxBefore = ReadAttribute(Angel, MaxHealth);
		const float HealthBefore = UBeyondCombatLibrary::GetActorHealth(Angel);
		T.TestTrue(TEXT("Unlock Thunder-Tempered"), Tree->Unlock(TEXT("Angel.Vitality1")));
		T.TestEqual(TEXT("+12 max health"), ReadAttribute(Angel, MaxHealth) - MaxBefore, 12.0f, 0.01f);
		T.TestEqual(TEXT("+12 current health"), UBeyondCombatLibrary::GetActorHealth(Angel) - HealthBefore, 12.0f, 0.5f);

		// Prerequisites and level gates
		FText Reason;
		T.TestTrue(TEXT("Static Ward: level 6 and Thunder-Tempered met"), Tree->CanUnlock(TEXT("Angel.Ward"), Reason));
		T.TestFalse(TEXT("Tempest Incarnate needs level 18"), Tree->CanUnlock(TEXT("Angel.Tempest"), Reason));
		T.TestTrue(TEXT("The reason names the level"), Reason.ToString().Contains(TEXT("18")));

		// Reset refunds everything and takes the effects off
		const int32 Spent = Tree->GetSpentPoints();
		T.TestEqual(TEXT("Points spent so far"), Spent, 5);
		T.TestEqual(TEXT("Reset refunds them"), Tree->ResetTree(), Spent);
		T.TestEqual(TEXT("All points back"), Tree->GetAvailablePoints(), Points);
		T.TestEqual(TEXT("Arcana back"), ReadAttribute(Angel, Arcana), ArcanaBefore, 0.01f);
		T.TestEqual(TEXT("Arcane Spikes back to level 1"), SpecLevel(Angel, Spikes), 1);
		T.TestEqual(TEXT("Max health back"), ReadAttribute(Angel, MaxHealth), MaxBefore, 0.01f);
		return true;
	}));

	// The duo tree: Bond gain, a new duo power on both demigods, the loadout on G
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondSkillTreeStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		ABeyondPlayerController* PC = State->PC.Get();
		UBeyondDuoSkillTreeComponent* Duo = PC ? PC->DuoSkillTree.Get() : nullptr;
		if (!Angel || !JiWoong || !Duo || !Duo->Tree)
		{
			return true;
		}
		UBeyondPartyComponent* Party = PC->PartyComponent;
		// Level 10 gave three; this step spends four
		Party->AddBondPoints(2);
		const int32 BondPointsBefore = Party->GetBondPoints();

		T.TestTrue(TEXT("Unlock Kindred Spirits"), Duo->Unlock(TEXT("Duo.Kindred")));
		T.TestEqual(TEXT("Bond meter fills 15% faster"), Party->GetBondGainMultiplier(), 1.15f, 0.001f);
		T.TestEqual(TEXT("It cost a Bond Point"), Party->GetBondPoints(), BondPointsBefore - 1);

		UClass* Eclipse = NodeAbility(Duo, TEXT("Duo.Eclipse"), EBeyondSkillEffectType::DuoPower);
		UClass* Starter = Duo->GetStarterDuoPower().Get();
		if (!T.TestNotNull(TEXT("migrate_pass8: Eclipse Brand exists"), Eclipse) || !T.TestNotNull(TEXT("Starter duo power"), Starter))
		{
			return true;
		}
		if (const UBeyondGA_DuoStrike* EclipseCDO = Cast<UBeyondGA_DuoStrike>(Eclipse->GetDefaultObject()))
		{
			T.TestTrue(TEXT("Eclipse Brand brands and detonates"), EclipseCDO->bBrandStruckEnemies && EclipseCDO->bShockwaveDetonatesBrands);
		}
		UClass* Aegis = NodeAbility(Duo, TEXT("Duo.Aegis"), EBeyondSkillEffectType::DuoPower);
		if (const UBeyondGA_DuoStrike* AegisCDO = Aegis ? Cast<UBeyondGA_DuoStrike>(Aegis->GetDefaultObject()) : nullptr)
		{
			T.TestTrue(TEXT("Tempest Aegis shields"), AegisCDO->Aegis.Duration > 0.0f && AegisCDO->Aegis.DamageReduction > 0.0f);
		}

		T.TestEqual(TEXT("Eclipse Brand not granted before it's unlocked"), SpecLevel(Angel, Eclipse), 0);
		T.TestTrue(TEXT("Unlock Eclipse Brand (2 Bond Points, party level 6)"), Duo->Unlock(TEXT("Duo.Eclipse")));
		for (const ABeyondCharacterBase* Member : { static_cast<const ABeyondCharacterBase*>(Angel), static_cast<const ABeyondCharacterBase*>(JiWoong) })
		{
			T.TestTrue(*FString::Printf(TEXT("%s has Eclipse Brand, off the duo slot"), *Member->GetName()),
				SpecLevel(Member, Eclipse) > 0 && HasSpecWithTag(Member, Eclipse, BeyondTags::Ability_Input_Unbound));
			const UGameplayAbility* OnG = Member->FindAbilityOnInput(BeyondTags::Ability_Input_Duo);
			T.TestTrue(*FString::Printf(TEXT("%s: Heaven's Judgment still on G"), *Member->GetName()), OnG && OnG->GetClass() == Starter);
		}

		T.TestTrue(TEXT("Put Eclipse Brand on G"), Duo->SetDuoLoadout(Eclipse));
		for (const ABeyondCharacterBase* Member : { static_cast<const ABeyondCharacterBase*>(Angel), static_cast<const ABeyondCharacterBase*>(JiWoong) })
		{
			const UGameplayAbility* OnG = Member->FindAbilityOnInput(BeyondTags::Ability_Input_Duo);
			T.TestTrue(*FString::Printf(TEXT("%s: G fires Eclipse Brand"), *Member->GetName()), OnG && OnG->GetClass() == Eclipse);
			T.TestTrue(*FString::Printf(TEXT("%s: Heaven's Judgment off the slot"), *Member->GetName()), HasSpecWithTag(Member, Starter, BeyondTags::Ability_Input_Unbound));
		}
		T.TestFalse(TEXT("A locked duo power can't go on G"), Aegis && Duo->SetDuoLoadout(Aegis));
		T.TestTrue(TEXT("Back to Heaven's Judgment"), Duo->SetDuoLoadout(Starter));

		// Duo ability ranks reach both demigods' Heaven's Judgment
		T.TestTrue(TEXT("Unlock Heaven's Wrath"), Duo->Unlock(TEXT("Duo.Wrath")));
		T.TestEqual(TEXT("Angel's Heaven's Judgment is level 2"), SpecLevel(Angel, Starter), 2);
		T.TestEqual(TEXT("Ji-Woong's Heaven's Judgment is level 2"), SpecLevel(JiWoong, Starter), 2);

		// Resetting the duo tree takes the power away and puts the starter back on G
		Duo->SetDuoLoadout(Eclipse);
		T.TestEqual(TEXT("Duo reset refunds 4 Bond Points"), Duo->ResetTree(), 4);
		T.TestEqual(TEXT("Eclipse Brand removed"), SpecLevel(Angel, Eclipse), 0);
		const UGameplayAbility* OnG = Angel->FindAbilityOnInput(BeyondTags::Ability_Input_Duo);
		T.TestTrue(TEXT("Heaven's Judgment back on G after a reset"), OnG && OnG->GetClass() == Starter);
		T.TestEqual(TEXT("Bond gain back to normal"), Party->GetBondGainMultiplier(), 1.0f, 0.001f);
		return true;
	}));

	// The skill tree screen: opens paused on the leader's tree, hold to unlock, explains a locked node, closes
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondSkillTreeStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondCharacterBase* Angel = State->Angel.Get();
		if (!PC || !Angel)
		{
			return true;
		}
		PC->OpenSkillTree();
		UBeyondSkillTreeWidget* Screen = Cast<UBeyondSkillTreeWidget>(PC->GetSkillTreeWidget());
		if (!T.TestTrue(TEXT("K opens the skill tree screen"), PC->IsSkillTreeOpen() && Screen))
		{
			return true;
		}
		T.TestEqual(TEXT("Three tabs: Angel, Ji-Woong, Duo"), Screen->GetTabCount(), 3);
		T.TestTrue(TEXT("Opens on the leader's tree"), Screen->GetActiveTree() == Angel->GetSkillTreeComponent());
		T.TestTrue(TEXT("The game is paused while it's open"), PC->GetWorld()->IsPaused());

		UBeyondSkillTreeComponent* Tree = Angel->GetSkillTreeComponent();
		T.TestTrue(TEXT("Holding starts on an available node"), Screen->BeginHold(TEXT("Angel.Arcana1")));
		Screen->AdvanceAnimation(Screen->HoldDuration * 0.5f);
		T.TestEqual(TEXT("Not yet unlocked half-way through the hold"), Tree->GetRank(TEXT("Angel.Arcana1")), 0);
		Screen->AdvanceAnimation(Screen->HoldDuration);
		T.TestEqual(TEXT("Holding long enough unlocks it"), Tree->GetRank(TEXT("Angel.Arcana1")), 1);
		T.TestFalse(TEXT("A locked node can't be held"), Screen->BeginHold(TEXT("Angel.Tempest")));
		T.TestTrue(TEXT("... and says why"), Screen->GetMessage().ToString().Contains(TEXT("18")));

		Screen->SelectTab(2);
		T.TestTrue(TEXT("The third tab is the duo tree"), Screen->GetActiveTree() == static_cast<UBeyondSkillTreeComponent*>(PC->DuoSkillTree.Get()));

		PC->CloseSkillTree();
		T.TestFalse(TEXT("Closed"), PC->IsSkillTreeOpen());
		T.TestFalse(TEXT("Unpaused"), PC->GetWorld()->IsPaused());
		Tree->ResetTree();
		return true;
	}));

	// Saves: ranks round-trip, restoring applies them without spending points
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondSkillTreeStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		UBeyondSkillTreeComponent* Tree = Angel ? Angel->GetSkillTreeComponent() : nullptr;
		if (!Tree)
		{
			return true;
		}

		UBeyondSaveGame* Save = Cast<UBeyondSaveGame>(UGameplayStatics::CreateSaveGameObject(UBeyondSaveGame::StaticClass()));
		Save->Members.Add(FName(TEXT("BP_Test_C"))).SkillRanks.Add(FName(TEXT("Angel.Arcana1")), 2);
		Save->DuoRanks.Add(FName(TEXT("Duo.Kindred")), 1);
		Save->BondPoints = 5;
		TArray<uint8> Bytes;
		T.TestTrue(TEXT("Save game serialises"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
		const UBeyondSaveGame* Loaded = Cast<UBeyondSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		const FBeyondMemberProgress* Member = Loaded ? Loaded->Members.Find(FName(TEXT("BP_Test_C"))) : nullptr;
		T.TestTrue(TEXT("Skill ranks, duo ranks and Bond Points round-trip"),
			Member && Member->SkillRanks.FindRef(FName(TEXT("Angel.Arcana1"))) == 2 && Loaded->DuoRanks.FindRef(FName(TEXT("Duo.Kindred"))) == 1 && Loaded->BondPoints == 5);

		const int32 Points = Tree->GetAvailablePoints();
		const float Arcana = ReadAttribute(Angel, UCharacterAttributeSet::GetArcanaAttribute());
		TMap<FName, int32> Ranks;
		Ranks.Add(FName(TEXT("Angel.Arcana1")), 2);
		Ranks.Add(FName(TEXT("NotANode")), 3);
		Tree->RestoreRanks(Ranks);
		T.TestEqual(TEXT("Restored rank"), Tree->GetRank(TEXT("Angel.Arcana1")), 2);
		T.TestEqual(TEXT("Unknown nodes are dropped"), Tree->GetRank(TEXT("NotANode")), 0);
		T.TestEqual(TEXT("Restored ranks apply their stats"), ReadAttribute(Angel, UCharacterAttributeSet::GetArcanaAttribute()) - Arcana, 8.0f, 0.01f);
		T.TestEqual(TEXT("Restoring spends no points"), Tree->GetAvailablePoints(), Points);
		Tree->RestoreRanks(TMap<FName, int32>());
		return true;
	}));

	// A main boss gives a Bond Point; Tempest Aegis's shield cuts damage and reflects it
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondSkillTreeStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondPlayerController* PC = State->PC.Get();
		if (!Angel || !PC)
		{
			return true;
		}
		State->Numbers.Add(TEXT("BondPoints"), static_cast<float>(PC->PartyComponent->GetBondPoints()));
		if (ABeyondCharacterBase* StandIn = State->StandInBoss.Get())
		{
			StandIn->Rank = EBeyondEnemyRank::Boss;
			HoldStill(StandIn);
			UBeyondCombatLibrary::ApplyDamage(Angel, StandIn, 10000000.0f, BeyondTags::DamageType_Projectile, BeyondTags::Event_Hit_Light, true, nullptr, true, true);
		}
		else
		{
			T.AddWarning(TEXT("Only one regular enemy in the map; the boss Bond Point wasn't checked"));
			State->Numbers.Add(TEXT("SkipBoss"), 1.0f);
		}

		ABeyondCharacterBase* Enemy = State->Enemy.Get();
		UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(Angel);
		if (!T.TestNotNull(TEXT("An enemy to hit Angel"), Enemy) || !Combat)
		{
			return true;
		}
		HoldStill(Enemy);
		UAbilitySystemComponent* ASC = Angel->GetAbilitySystemComponent();
		ASC->SetLooseGameplayTagCount(BeyondTags::State_Invincible, 0);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute()));

		float Before = UBeyondCombatLibrary::GetActorHealth(Angel);
		UBeyondCombatLibrary::ApplyDamage(Enemy, Angel, 40.0f, BeyondTags::DamageType_Environment, FGameplayTag(), true, nullptr, true, true);
		const float Plain = Before - UBeyondCombatLibrary::GetActorHealth(Angel);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute()));

		FBeyondAegisSettings Shield;
		Shield.Duration = 5.0f;
		Shield.DamageReduction = 0.5f;
		Shield.ReflectFraction = 0.5f;
		T.TestTrue(TEXT("Storm shield applied"), Combat->ApplyAegis(Angel, Shield) && Combat->HasAegis(Angel));
		T.TestTrue(TEXT("Shielded characters carry State.Aegis"), ASC->HasMatchingGameplayTag(BeyondTags::State_Aegis));

		// Lots of health so the reflected hit can't kill the enemy
		UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
		EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 100000.0f);
		EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), 100000.0f);
		State->Numbers.Add(TEXT("EnemyHealth"), UBeyondCombatLibrary::GetActorHealth(Enemy));
		Before = UBeyondCombatLibrary::GetActorHealth(Angel);
		UBeyondCombatLibrary::ApplyDamage(Enemy, Angel, 40.0f, BeyondTags::DamageType_Environment, FGameplayTag(), true, nullptr, true, true);
		const float Shielded = Before - UBeyondCombatLibrary::GetActorHealth(Angel);
		T.TestEqual(TEXT("The shield halves the hit"), Shielded, Plain * 0.5f, 0.5f);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondSkillTreeStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondPlayerController* PC = State->PC.Get();
		if (!Angel || !PC)
		{
			return true;
		}
		if (!State->Numbers.Contains(TEXT("SkipBoss")))
		{
			T.TestTrue(TEXT("Killing a main boss gives a Bond Point"), PC->PartyComponent->GetBondPoints() >= FMath::RoundToInt(State->Numbers.FindRef(TEXT("BondPoints"))) + 1);
			T.TestTrue(TEXT("The stand-in boss died"), UBeyondCombatLibrary::IsActorDead(State->StandInBoss.Get()));
		}
		if (ABeyondCharacterBase* Enemy = State->Enemy.Get())
		{
			T.TestTrue(TEXT("The shield reflected lightning at the attacker"), UBeyondCombatLibrary::GetActorHealth(Enemy) < State->Numbers.FindRef(TEXT("EnemyHealth")));
		}
		if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(Angel))
		{
			Combat->RemoveAegis(Angel);
			T.TestFalse(TEXT("Shield removed"), Combat->HasAegis(Angel));
		}

		// Hand Ji-Woong back to his companion AI
		if (AAIController* Holder = State->Holder.Get())
		{
			Holder->UnPossess();
			Holder->Destroy();
		}
		if (AController* Companion = State->CompanionController.Get())
		{
			if (ABeyondCharacterBase* JiWoong = State->JiWoong.Get())
			{
				Companion->Possess(JiWoong);
			}
		}
		return true;
	}));

	// Let the legacy enemies' hit reactions play out first: BP_Enemy_Base's montage callbacks call its AI controller,
	// which PIE teardown has already destroyed (a Blueprint runtime error)
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondSkillTreeStep([State]()
	{
		UWorld* World = GetPlayWorld();
		const double Now = FPlatformTime::Seconds();
		if (State->SettleStartTime <= 0.0)
		{
			State->SettleStartTime = Now;
		}
		bool bMontagePlaying = false;
		for (TActorIterator<ABeyondCharacterBase> It(World); World && It; ++It)
		{
			const USkeletalMeshComponent* Mesh = It->TeamAffiliation == EBeyondTeam::Enemy ? It->GetCombatMesh() : nullptr;
			const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
			bMontagePlaying |= Anim && Anim->IsAnyMontagePlaying();
		}
		return !bMontagePlaying || Now - State->SettleStartTime > 3.0;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondSkillTreeStep([State]()
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
