// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BeyondGameplayTags.h"
#include "Blueprint/UserWidget.h"
#include "BrainComponent.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "Components/PanelWidget.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Game/BeyondSaveGame.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/AutomationTest.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Progression/BeyondProgressionAttributeSet.h"
#include "Progression/BeyondProgressionSettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/BeyondProgressWidget.h"

/**
 * Plan 1A in MAP_Demo_Main (Play In Editor): EXP for kills shared by both demigods, enemy ranks, level-ups (skill
 * point, stat growth, refill, banner), Strength / Defense in the damage maths, restoring a save and the level display
 * in the HUD. Saving is switched off for the run (Beyond.SaveProgress 0) so the player's own progress is untouched.
 *
 * UnrealEditor-Cmd.exe WorldBeyond.uproject -ExecCmds="Automation RunTests WorldsBeyond.Prototype;Quit" -unattended -nullrhi -nosplash
 */

namespace BeyondProgressionTest
{
	struct FState
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ABeyondPlayerController> PC;
		TWeakObjectPtr<ABeyondCharacterBase> Angel;
		TWeakObjectPtr<ABeyondCharacterBase> JiWoong;
		TArray<TWeakObjectPtr<ABeyondCharacterBase>> Enemies;
		TWeakObjectPtr<ABeyondCharacterBase> Boss;
		TWeakObjectPtr<ABeyondCharacterBase> Victim;
		TMap<FString, float> Numbers;
		FString SavedSaveProgress;
		// Ji-Woong is parked on a plain AI controller so his companion AI can't land extra kills mid-measurement
		TWeakObjectPtr<AAIController> Holder;
		TWeakObjectPtr<AController> CompanionController;
	};

	UWorld* GetPlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	float TotalExperienceOf(const ABeyondCharacterBase* Character)
	{
		return Character ? UBeyondProgressionSettings::GetTotalExperience(Character->GetCharacterLevel(), Character->GetExperience()) : 0.0f;
	}

	float ReadStat(const ABeyondCharacterBase* Character, const FGameplayAttribute& Attribute)
	{
		const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		return ASC ? ASC->GetNumericAttribute(Attribute) : 0.0f;
	}

	void WriteBaseStat(ABeyondCharacterBase* Character, const FGameplayAttribute& Attribute, float Value)
	{
		if (UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr)
		{
			ASC->SetNumericAttributeBase(Attribute, Value);
		}
	}

	// Stop an enemy's AI so it neither attacks nor blocks during a measurement
	void FreezeEnemy(ABeyondCharacterBase* Enemy)
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
				Brain->StopLogic(TEXT("Progression test"));
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

	// Damage that lands for sure (no block, parry or invincibility); returns the health it took
	float MeasureHitDamage(ABeyondCharacterBase* Source, ABeyondCharacterBase* Target, float Amount, const FGameplayTag& DamageType)
	{
		FreezeEnemy(Target);
		// Plenty of health so the hit is never clamped
		WriteBaseStat(Target, UCharacterAttributeSet::GetMaxHealthAttribute(), 100000.0f);
		WriteBaseStat(Target, UCharacterAttributeSet::GetCurrentHealthAttribute(), 100000.0f);
		const float Before = UBeyondCombatLibrary::GetActorHealth(Target);
		UBeyondCombatLibrary::ApplyDamage(Source, Target, Amount, DamageType, FGameplayTag(), true, nullptr, true, true);
		return Before - UBeyondCombatLibrary::GetActorHealth(Target);
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondProgressionStep, TFunction<bool()>, Step);
bool FBeyondProgressionStep::Update()
{
	return Step();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondProgressionTest, "WorldsBeyond.Prototype.Progression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondProgressionTest::RunTest(const FString& Parameters)
{
	using namespace BeyondProgressionTest;
	TSharedRef<FState> State = MakeShared<FState>();
	State->Test = this;

	// Start fresh and never write the player's save
	if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
	{
		State->SavedSaveProgress = SaveProgress->GetString();
		SaveProgress->Set(TEXT("0"), ECVF_SetByCode);
	}
	else
	{
		AddError(TEXT("Beyond.SaveProgress console variable is missing"));
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/SICKA_PERSEPOLIS/MAPS/MAP_Demo_Main")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.0f));

	// Setup: both demigods earn EXP from level 1, enemies have ranks, the level display is in the HUD
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondProgressionStep([State]()
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
		T.TestFalse(TEXT("Saving is off for the test run"), Party->IsSavingEnabled());

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

		for (const ABeyondCharacterBase* Member : { Angel, JiWoong })
		{
			const FString Name = Member->GetName();
			T.TestTrue(*FString::Printf(TEXT("%s earns EXP (progression attribute set)"), *Name), Member->CanGainExperience());
			T.TestEqual(*FString::Printf(TEXT("%s starts at level 1"), *Name), Member->GetCharacterLevel(), 1);
			T.TestEqual(*FString::Printf(TEXT("%s starts with no EXP"), *Name), Member->GetExperience(), 0.0f);
			T.TestEqual(*FString::Printf(TEXT("%s starts with no skill points"), *Name), Member->GetSkillPoints(), 0);
		}
		T.TestFalse(TEXT("Angel has a display name"), Angel->GetCharacterDisplayName().IsEmpty());
		T.TestTrue(TEXT("migrate_pass7: Angel leans Arcana"), Angel->GetStatGrowth().Arcana > Angel->GetStatGrowth().Strength);
		T.TestTrue(TEXT("migrate_pass7: Ji-Woong leans Strength"), JiWoong->GetStatGrowth().Strength > JiWoong->GetStatGrowth().Arcana);

		for (TActorIterator<ABeyondCharacterBase> It(World); It; ++It)
		{
			if (It->TeamAffiliation != EBeyondTeam::Enemy || UBeyondCombatLibrary::IsActorDead(*It))
			{
				continue;
			}
			T.TestFalse(*FString::Printf(TEXT("Enemy %s does not earn EXP"), *It->GetName()), It->CanGainExperience());
			FreezeEnemy(*It);
			if (It->BossBarWidgetClass)
			{
				State->Boss = *It;
			}
			else
			{
				State->Enemies.Add(*It);
			}
		}
		T.TestTrue(TEXT("At least 2 regular enemies in the map"), State->Enemies.Num() >= 2);
		if (const ABeyondCharacterBase* Boss = State->Boss.Get())
		{
			T.TestTrue(TEXT("migrate_pass7: the boss is ranked Boss"), Boss->Rank == EBeyondEnemyRank::Boss);
			const float RegularReward = UBeyondProgressionSettings::GetExperienceReward(EBeyondEnemyRank::Regular, 1);
			T.TestTrue(TEXT("A boss is worth far more EXP than a regular enemy"), Boss->GetExperienceRewardValue() >= RegularReward * 10.0f);
		}
		else
		{
			T.AddError(TEXT("No boss in the map"));
		}

		// Rank rewards climb, and an enemy's level adds 10 % per level
		const float Regular = UBeyondProgressionSettings::GetExperienceReward(EBeyondEnemyRank::Regular, 1);
		const float Elite = UBeyondProgressionSettings::GetExperienceReward(EBeyondEnemyRank::Elite, 1);
		const float MiniBoss = UBeyondProgressionSettings::GetExperienceReward(EBeyondEnemyRank::MiniBoss, 1);
		const float BossReward = UBeyondProgressionSettings::GetExperienceReward(EBeyondEnemyRank::Boss, 1);
		T.TestTrue(TEXT("EXP: regular < elite < mini-boss < boss"), Regular > 0.0f && Regular < Elite && Elite < MiniBoss && MiniBoss < BossReward);
		T.TestTrue(TEXT("EXP: a level 5 enemy is worth more than a level 1 one"),
			UBeyondProgressionSettings::GetExperienceReward(EBeyondEnemyRank::Regular, 5) > Regular);
		T.TestTrue(TEXT("Each level needs more EXP than the last"),
			UBeyondProgressionSettings::GetExperienceToNextLevel(2) > UBeyondProgressionSettings::GetExperienceToNextLevel(1));

		// The level display sits in W_PlayerHud (or on the viewport when there is no HUD)
		const UBeyondProgressWidget* Progress = Cast<UBeyondProgressWidget>(PC->GetProgressWidget());
		if (T.TestNotNull(TEXT("Level display created"), Progress))
		{
			if (const UUserWidget* HUD = PC->GetHUDWidget())
			{
				T.TestTrue(TEXT("Level display is inside the HUD's canvas"), Progress->GetParent() != nullptr && Progress->GetParent() == HUD->GetRootWidget());
			}
			else
			{
				T.TestTrue(TEXT("Level display is on the viewport"), Progress->IsInViewport());
			}
			T.TestEqual(TEXT("Level display shows the leader's level"), Progress->GetShownLevel(), Angel->GetCharacterLevel());
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));

	// Angel kills a regular enemy: both demigods get its EXP
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondProgressionStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		ABeyondCharacterBase* Victim = State->Enemies.Num() > 0 ? State->Enemies[0].Get() : nullptr;
		if (!Angel || !JiWoong || !T.TestNotNull(TEXT("An enemy to defeat"), Victim))
		{
			return true;
		}
		State->Victim = Victim;
		State->Numbers.Add(TEXT("Reward"), Victim->GetExperienceRewardValue());
		State->Numbers.Add(TEXT("AngelTotal"), TotalExperienceOf(Angel));
		State->Numbers.Add(TEXT("JiWoongTotal"), TotalExperienceOf(JiWoong));
		T.TestTrue(TEXT("A regular enemy is worth some EXP"), Victim->GetExperienceRewardValue() > 0.0f);

		FreezeEnemy(Victim);
		UBeyondCombatLibrary::ApplyDamage(Angel, Victim, 1000000.0f, BeyondTags::DamageType_Projectile, BeyondTags::Event_Hit_Light,
			true, nullptr, true, true);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondProgressionStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondCharacterBase* Angel = State->Angel.Get();
		const ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!Angel || !JiWoong || !State->Victim.IsValid())
		{
			return true;
		}
		const float Reward = State->Numbers.FindRef(TEXT("Reward"));
		T.TestTrue(TEXT("The enemy died"), UBeyondCombatLibrary::IsActorDead(State->Victim.Get()));
		T.TestEqual(TEXT("Angel (the killer) got the enemy's EXP"), TotalExperienceOf(Angel) - State->Numbers.FindRef(TEXT("AngelTotal")), Reward, 0.01f);
		T.TestEqual(TEXT("Ji-Woong got the same EXP (shared party EXP)"), TotalExperienceOf(JiWoong) - State->Numbers.FindRef(TEXT("JiWoongTotal")), Reward, 0.01f);
		return true;
	}));

	// Crossing the threshold: level 2, a skill point, more max health, refilled, a banner
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondProgressionStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		UBeyondPartyComponent* Party = State->PC.IsValid() ? State->PC->PartyComponent.Get() : nullptr;
		if (!Angel || !JiWoong || !Party)
		{
			return true;
		}
		State->Numbers.Add(TEXT("AngelMaxHealth"), ReadStat(Angel, UCharacterAttributeSet::GetMaxHealthAttribute()));
		State->Numbers.Add(TEXT("AngelArcana"), ReadStat(Angel, UCharacterAttributeSet::GetArcanaAttribute()));
		State->Numbers.Add(TEXT("JiWoongStrength"), ReadStat(JiWoong, UCharacterAttributeSet::GetStrengthAttribute()));
		State->Numbers.Add(TEXT("AngelLevel"), static_cast<float>(Angel->GetCharacterLevel()));

		// Hurt Angel so the refill shows
		WriteBaseStat(Angel, UCharacterAttributeSet::GetCurrentHealthAttribute(), ReadStat(Angel, UCharacterAttributeSet::GetMaxHealthAttribute()) * 0.4f);

		const float Needed = Angel->GetExperienceToNextLevel() - Angel->GetExperience();
		State->Numbers.Add(TEXT("Leftover"), 5.0f);
		Party->AwardExperience(Needed + 5.0f);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondProgressionStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!Angel || !JiWoong)
		{
			return true;
		}
		const int32 OldLevel = FMath::RoundToInt(State->Numbers.FindRef(TEXT("AngelLevel")));
		T.TestEqual(TEXT("Angel levelled up once"), Angel->GetCharacterLevel(), OldLevel + 1);
		T.TestEqual(TEXT("Ji-Woong levelled up with him"), JiWoong->GetCharacterLevel(), OldLevel + 1);
		T.TestEqual(TEXT("The extra EXP carried into the new level"), Angel->GetExperience(), State->Numbers.FindRef(TEXT("Leftover")), 0.01f);
		const int32 PointsPerLevel = GetDefault<UBeyondProgressionSettings>()->SkillPointsPerLevel;
		T.TestEqual(TEXT("Angel got a skill point"), Angel->GetSkillPoints(), PointsPerLevel * OldLevel);
		T.TestEqual(TEXT("Ji-Woong got a skill point"), JiWoong->GetSkillPoints(), PointsPerLevel * OldLevel);

		const float MaxHealth = ReadStat(Angel, UCharacterAttributeSet::GetMaxHealthAttribute());
		T.TestEqual(TEXT("Angel's max health grew by his Stat Growth"), MaxHealth - State->Numbers.FindRef(TEXT("AngelMaxHealth")), Angel->GetStatGrowth().MaxHealth, 0.01f);
		T.TestEqual(TEXT("Angel's Arcana grew"), ReadStat(Angel, UCharacterAttributeSet::GetArcanaAttribute()) - State->Numbers.FindRef(TEXT("AngelArcana")), Angel->GetStatGrowth().Arcana, 0.01f);
		T.TestEqual(TEXT("Ji-Woong's Strength grew"), ReadStat(JiWoong, UCharacterAttributeSet::GetStrengthAttribute()) - State->Numbers.FindRef(TEXT("JiWoongStrength")), JiWoong->GetStatGrowth().Strength, 0.01f);
		T.TestEqual(TEXT("A level-up refills health"), UBeyondCombatLibrary::GetActorHealth(Angel), MaxHealth, 0.5f);

		if (const UBeyondProgressWidget* Progress = State->PC.IsValid() ? Cast<UBeyondProgressWidget>(State->PC->GetProgressWidget()) : nullptr)
		{
			T.TestTrue(TEXT("LEVEL UP banner is showing"), Progress->IsBannerShowing());
			T.TestTrue(TEXT("The banner names both demigods"),
				Progress->GetBannerSubtitle().Contains(Angel->GetCharacterDisplayName().ToString()) && Progress->GetBannerSubtitle().Contains(JiWoong->GetCharacterDisplayName().ToString()));
			T.TestEqual(TEXT("Level display shows the new level"), Progress->GetShownLevel(), Angel->GetCharacterLevel());
		}
		return true;
	}));

	// Strength scales melee damage; Defense reduces damage taken
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondProgressionStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		ABeyondCharacterBase* Target = State->Enemies.Num() > 1 ? State->Enemies[1].Get() : nullptr;
		if (!JiWoong || !T.TestNotNull(TEXT("A second enemy for damage checks"), Target))
		{
			return true;
		}
		const FGameplayAttribute Strength = UCharacterAttributeSet::GetStrengthAttribute();
		const FGameplayAttribute Defense = UCharacterAttributeSet::GetDefenseAttribute();
		const UAbilitySystemComponent* JiWoongASC = JiWoong->GetAbilitySystemComponent();
		const float StrengthBase = JiWoongASC->GetNumericAttributeBase(Strength);
		const float DefenseBase = Target->GetAbilitySystemComponent()->GetNumericAttributeBase(Defense);
		WriteBaseStat(Target, Defense, 0.0f);

		const float StrengthNow = ReadStat(JiWoong, Strength);
		const float Plain = MeasureHitDamage(JiWoong, Target, 100.0f, BeyondTags::DamageType_Melee);
		T.TestEqual(TEXT("Melee damage x (1 + Strength / 100)"), Plain, 100.0f * (1.0f + StrengthNow / 100.0f), 0.5f);

		WriteBaseStat(JiWoong, Strength, StrengthBase + 100.0f);
		const float Stronger = MeasureHitDamage(JiWoong, Target, 100.0f, BeyondTags::DamageType_Melee);
		T.TestEqual(TEXT("+100 Strength adds 100 % of the base hit"), Stronger - Plain, 100.0f, 0.5f);
		WriteBaseStat(JiWoong, Strength, StrengthBase);

		const float Environment = MeasureHitDamage(JiWoong, Target, 100.0f, BeyondTags::DamageType_Environment);
		T.TestEqual(TEXT("Environment damage ignores the attacker's stats"), Environment, 100.0f, 0.5f);

		WriteBaseStat(Target, Defense, 100.0f);
		const float Armored = MeasureHitDamage(JiWoong, Target, 100.0f, BeyondTags::DamageType_Environment);
		T.TestEqual(TEXT("100 Defense halves damage taken"), Armored, 50.0f, 0.5f);
		WriteBaseStat(Target, Defense, DefenseBase);
		return true;
	}));

	// Saves: the save object keeps its fields, and restoring a save sets level, EXP, points and stats
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondProgressionStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!JiWoong)
		{
			return true;
		}

		UBeyondSaveGame* Save = Cast<UBeyondSaveGame>(UGameplayStatics::CreateSaveGameObject(UBeyondSaveGame::StaticClass()));
		FBeyondMemberProgress& Entry = Save->Members.Add(FName(TEXT("BP_Test_C")));
		Entry.Level = 7;
		Entry.Experience = 123.0f;
		Entry.SkillPoints = 4;
		TArray<uint8> Bytes;
		T.TestTrue(TEXT("Save game serialises"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
		const UBeyondSaveGame* Loaded = Cast<UBeyondSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		const FBeyondMemberProgress* LoadedEntry = Loaded ? Loaded->Members.Find(FName(TEXT("BP_Test_C"))) : nullptr;
		T.TestTrue(TEXT("Save game round-trips a member's level, EXP and skill points"),
			LoadedEntry && LoadedEntry->Level == 7 && FMath::IsNearlyEqual(LoadedEntry->Experience, 123.0f) && LoadedEntry->SkillPoints == 4);

		const int32 Level = JiWoong->GetCharacterLevel();
		const float Experience = JiWoong->GetExperience();
		const int32 Points = JiWoong->GetSkillPoints();
		const float LevelOneMaxHealth = ReadStat(JiWoong, UCharacterAttributeSet::GetMaxHealthAttribute()) - JiWoong->GetStatGrowth().MaxHealth * (Level - 1);

		JiWoong->RestoreProgress(5, 10.0f, 3);
		T.TestEqual(TEXT("Restore: level"), JiWoong->GetCharacterLevel(), 5);
		T.TestEqual(TEXT("Restore: EXP"), JiWoong->GetExperience(), 10.0f, 0.01f);
		T.TestEqual(TEXT("Restore: skill points"), JiWoong->GetSkillPoints(), 3);
		T.TestEqual(TEXT("Restore: stats for level 5"), ReadStat(JiWoong, UCharacterAttributeSet::GetMaxHealthAttribute()),
			LevelOneMaxHealth + JiWoong->GetStatGrowth().MaxHealth * 4.0f, 0.5f);

		JiWoong->RestoreProgress(Level, Experience, Points);
		T.TestEqual(TEXT("Restore back"), JiWoong->GetCharacterLevel(), Level);

		// Hand Ji-Woong back to his companion AI
		if (AAIController* Holder = State->Holder.Get())
		{
			Holder->UnPossess();
			Holder->Destroy();
		}
		if (AController* Companion = State->CompanionController.Get())
		{
			Companion->Possess(JiWoong);
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondProgressionStep([State]()
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
