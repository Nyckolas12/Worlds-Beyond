// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AI/BeyondEnemyController.h"
#include "BeyondGameplayTags.h"
#include "Characters/BeyondCharacterBase.h"
#include "Editor.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Enemies/BeyondEnemySettings.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "EngineUtils.h"
#include "Game/BeyondCombatSubsystem.h"
#include "Game/BeyondSaveGame.h"
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
#include "UObject/StrongObjectPtr.h"

/**
 * Plan 6, the Bond level: the table climbs and its perks never shrink; save v7 keeps level and EXP. In MAP_Demo_Main
 * (Play In Editor): Bond EXP levels the party up to the top and stops there, and never gives Bond Points; a duo move and a
 * synergy give Bond EXP; at Bond level 3 the duo deals more and takes less while together, nothing extra once apart, and
 * the environment's damage is never reduced; a duo node that needs Bond level 3 stays locked below it. Saving is off.
 */
namespace BeyondBondTest
{
	struct FState
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ABeyondEnemyCharacter> Wolf;
		TStrongObjectPtr<UBeyondSkillTreeAsset> TestTree;
		FString SavedSaveProgress;
	};

	UWorld* GetPlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondBondStep, TFunction<bool()>, Step);
bool FBeyondBondStep::Update()
{
	return Step();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondBondTest, "WorldsBeyond.Prototype.Bond",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondBondTest::RunTest(const FString& Parameters)
{
	using namespace BeyondBondTest;
	TSharedRef<FState> State = MakeShared<FState>();
	State->Test = this;
	const UBeyondProgressionSettings* Progression = GetDefault<UBeyondProgressionSettings>();

	//~ The table (no world needed)
	TestTrue(TEXT("Ten Bond levels"), Progression->GetMaxBondLevel() >= 10);
	for (int32 Level = 2; Level <= Progression->GetMaxBondLevel(); ++Level)
	{
		const FBeyondBondLevel& Previous = Progression->GetBondLevel(Level - 1);
		const FBeyondBondLevel& This = Progression->GetBondLevel(Level);
		TestTrue(FString::Printf(TEXT("Bond level %d's perks are at least level %d's"), Level, Level - 1),
			This.DamageDealtBonus >= Previous.DamageDealtBonus && This.DamageTakenReduction >= Previous.DamageTakenReduction);
		if (Level < Progression->GetMaxBondLevel())
		{
			TestTrue(FString::Printf(TEXT("Bond level %d takes more EXP than %d"), Level, Level - 1), This.ExperienceToNext > Previous.ExperienceToNext);
		}
	}
	TestEqual(TEXT("Bond level 1 gives nothing"), Progression->GetBondLevel(1).DamageDealtBonus, 0.0f);
	{
		UBeyondSaveGame* Save = NewObject<UBeyondSaveGame>();
		Save->BondLevel = 4;
		Save->BondExperience = 33.0f;
		TArray<uint8> Bytes;
		const UBeyondSaveGame* Loaded = UGameplayStatics::SaveGameToMemory(Save, Bytes) ? Cast<UBeyondSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)) : nullptr;
		TestTrue(TEXT("Save v7 keeps the Bond level and its EXP"), Loaded && Loaded->BondLevel == 4 && FMath::IsNearlyEqual(Loaded->BondExperience, 33.0f));
	}

	if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
	{
		State->SavedSaveProgress = SaveProgress->GetString();
		SaveProgress->Set(TEXT("0"), ECVF_SetByCode);
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/SICKA_PERSEPOLIS/MAPS/MAP_Demo_Main")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.0f));

	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBondStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const UBeyondProgressionSettings* Progression = GetDefault<UBeyondProgressionSettings>();
		UWorld* World = GetPlayWorld();
		ABeyondPlayerController* PC = World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr;
		UBeyondPartyComponent* Party = PC ? PC->PartyComponent.Get() : nullptr;
		if (!T.TestNotNull(TEXT("The party"), Party))
		{
			return true;
		}
		for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
		{
			if (ULevelSequencePlayer* Player = It->GetSequencePlayer(); Player && Player->IsPlaying())
			{
				Player->GoToEndAndStop();
			}
		}
		ABeyondCharacterBase* Angel = nullptr;
		ABeyondCharacterBase* JiWoong = nullptr;
		for (ABeyondCharacterBase* Member : Party->GetMembers())
		{
			(Member->DuoRole == EBeyondDuoRole::Conduit ? Angel : JiWoong) = Member;
		}
		if (!T.TestNotNull(TEXT("Angel"), Angel) || !T.TestNotNull(TEXT("Ji-Woong"), JiWoong))
		{
			return true;
		}

		//~ Levels
		Party->RestoreBondLevel(1, 0.0f);
		const int32 BondPoints = Party->GetBondPoints();
		T.TestEqual(TEXT("A new party is Bond level 1"), Party->GetBondLevel(), 1);
		Party->AddBondExperience(Party->GetBondExperienceToNext() + 5.0f);
		T.TestEqual(TEXT("Enough Bond EXP: level 2"), Party->GetBondLevel(), 2);
		T.TestEqual(TEXT("...with the rest carried over"), Party->GetBondExperience(), 5.0f, 0.01f);
		Party->AddBondExperience(100000.0f);
		T.TestEqual(TEXT("It stops at the top level"), Party->GetBondLevel(), Progression->GetMaxBondLevel());
		T.TestEqual(TEXT("...where nothing more is needed"), Party->GetBondExperienceToNext(), 0.0f);
		T.TestEqual(TEXT("Bond levels never give Bond Points"), Party->GetBondPoints(), BondPoints);

		//~ Sources: a duo move, a synergy
		Party->RestoreBondLevel(1, 0.0f);
		Party->AddBond(Party->MaxBond);
		T.TestTrue(TEXT("A full meter is spent"), Party->ConsumeBond());
		T.TestEqual(TEXT("A duo move gives Bond EXP"), Party->GetBondExperience(), Progression->BondExperiencePerDuoMove, 0.01f);
		const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
		UBeyondEnemyDefinition* WolfDefinition = Roster ? Roster->FindEnemy(TEXT("forest_wolf")) : nullptr;
		UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(World);
		ABeyondEnemyCharacter* Wolf = WolfDefinition && Enemies
			? Enemies->SpawnEnemy(WolfDefinition, FTransform(Angel->GetActorLocation() + Angel->GetActorForwardVector() * 800.0f), FBeyondEnemySpawnParams()) : nullptr;
		if (!T.TestNotNull(TEXT("A wolf"), Wolf))
		{
			return true;
		}
		State->Wolf = Wolf;
		if (ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(Wolf->GetController()))
		{
			AI->SetBrainEnabled(false);
		}
		UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(World);
		if (Combat)
		{
			const float Before = Party->GetBondExperience();
			Combat->OnDamageDealt.Broadcast(Angel, Wolf, 5.0f);
			Combat->OnDamageDealt.Broadcast(JiWoong, Wolf, 5.0f);
			T.TestEqual(TEXT("Both demigods on one enemy: a synergy's Bond EXP"), Party->GetBondExperience(), Before + Progression->BondExperiencePerSynergy, 0.01f);
		}

		//~ Perks at Bond level 3: together only, never against the environment
		JiWoong->SetActorLocation(Angel->GetActorLocation() + Angel->GetActorRightVector() * 300.0f, false, nullptr, ETeleportType::TeleportPhysics);
		T.TestTrue(TEXT("The demigods are together"), Party->AreTogether());
		Party->RestoreBondLevel(1, 0.0f);
		FGameplayTagContainer Tags;
		const float Level1 = UBeyondCombatLibrary::ComputeOutgoingDamage(Angel, Wolf, 100.0f, BeyondTags::DamageType_Melee, EBeyondElement::None, Tags);
		Party->RestoreBondLevel(3, 0.0f);
		const FBeyondBondLevel& Three = Progression->GetBondLevel(3);
		Tags.Reset();
		const float Level3 = UBeyondCombatLibrary::ComputeOutgoingDamage(Angel, Wolf, 100.0f, BeyondTags::DamageType_Melee, EBeyondElement::None, Tags);
		T.TestEqual(TEXT("Bond level 3: the duo deals more together"), Level3, Level1 * (1.0f + Three.DamageDealtBonus), 0.01f);
		if (Combat)
		{
			UAbilitySystemComponent* AngelASC = Angel->GetAbilitySystemComponent();
			FGameplayTagContainer Bite;
			Bite.AddTag(BeyondTags::DamageType_Melee);
			FGameplayTagContainer Fall;
			Fall.AddTag(BeyondTags::DamageType_Environment);
			const float Bitten = Combat->ModifyIncomingDamage(*AngelASC, Wolf, Bite, 100.0f);
			const float Fallen = Combat->ModifyIncomingDamage(*AngelASC, nullptr, Fall, 100.0f);
			Party->RestoreBondLevel(1, 0.0f);
			const float BittenAtOne = Combat->ModifyIncomingDamage(*AngelASC, Wolf, Bite, 100.0f);
			const float FallenAtOne = Combat->ModifyIncomingDamage(*AngelASC, nullptr, Fall, 100.0f);
			T.TestEqual(TEXT("...and takes less"), Bitten, BittenAtOne * (1.0f - Three.DamageTakenReduction), 0.01f);
			T.TestEqual(TEXT("...but not from the environment"), Fallen, FallenAtOne, 0.01f);
			Party->RestoreBondLevel(3, 0.0f);
		}
		JiWoong->SetActorLocation(Angel->GetActorLocation() + Angel->GetActorRightVector() * (Progression->BondPerkDistance + 2000.0f), false, nullptr,
			ETeleportType::TeleportPhysics);
		T.TestFalse(TEXT("Far apart they aren't together"), Party->AreTogether());
		Tags.Reset();
		T.TestEqual(TEXT("...and the perk is gone"), UBeyondCombatLibrary::ComputeOutgoingDamage(Angel, Wolf, 100.0f, BeyondTags::DamageType_Melee, EBeyondElement::None, Tags),
			Level1, 0.01f);
		JiWoong->SetActorLocation(Angel->GetActorLocation() + Angel->GetActorRightVector() * 300.0f, false, nullptr, ETeleportType::TeleportPhysics);

		//~ A duo node that needs Bond level 3
		UBeyondSkillTreeComponent* DuoTree = PC->DuoSkillTree.Get();
		if (T.TestNotNull(TEXT("The duo tree"), DuoTree))
		{
			UBeyondSkillTreeAsset* TestTree = NewObject<UBeyondSkillTreeAsset>(GetTransientPackage());
			State->TestTree.Reset(TestTree);
			TestTree->Currency = EBeyondSkillCurrency::BondPoints;
			FBeyondSkillNode& Node = TestTree->Nodes.AddDefaulted_GetRef();
			Node.Id = TEXT("Resonance");
			Node.Cost = 0;
			Node.RequiredBondLevel = 3;
			UBeyondSkillTreeAsset* OldTree = DuoTree->Tree;
			const TMap<FName, int32> OldRanks = DuoTree->GetRanks();
			DuoTree->SetTree(TestTree);
			FText Reason;
			Party->RestoreBondLevel(2, 0.0f);
			T.TestFalse(TEXT("Bond level 2: the node is locked"), DuoTree->CanUnlock(Node.Id, Reason));
			T.TestTrue(TEXT("...and says why"), Reason.ToString().Contains(TEXT("Bond level 3")));
			T.TestEqual(TEXT("...and shows as locked"), DuoTree->GetNodeState(Node.Id), EBeyondSkillNodeState::Locked);
			Party->RestoreBondLevel(3, 0.0f);
			T.TestTrue(TEXT("Bond level 3: it can be learned"), DuoTree->CanUnlock(Node.Id, Reason));
			DuoTree->SetTree(OldTree);
			DuoTree->RestoreRanks(OldRanks);
		}
		Party->RestoreBondLevel(1, 0.0f);
		Wolf->Destroy();
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondBondStep([State]()
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
