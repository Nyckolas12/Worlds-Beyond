// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AI/BeyondCompanionController.h"
#include "BeyondGameplayTags.h"
#include "Characters/BeyondCharacterBase.h"
#include "Characters/BeyondLegacyDamageBridge.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

/**
 * End-to-end check of the prototype in MAP_Demo_Main (Play In Editor, no rendering needed):
 * party + companion, swapping without duplicate abilities, GAS damage on enemies mirrored into the
 * old damage component, no friendly fire, attack tokens, death, and buddy revive.
 *
 * UnrealEditor-Cmd.exe WorldBeyond.uproject -ExecCmds="Automation RunTests WorldsBeyond.Prototype;Quit" -unattended -nullrhi -nosplash
 */

namespace BeyondSmokeTest
{
	struct FState
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ABeyondPlayerController> PC;
		TWeakObjectPtr<ABeyondCharacterBase> Enemy;
		TWeakObjectPtr<ABeyondCharacterBase> Companion;
		TMap<FString, int32> BlinkSpecsBefore;
	};

	UWorld* GetPlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	int32 CountSpecs(const ABeyondCharacterBase* Character, const TCHAR* AbilityNameFragment)
	{
		int32 Count = 0;
		if (const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr)
		{
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				Count += (Spec.Ability && Spec.Ability->GetClass()->GetName().Contains(AbilityNameFragment)) ? 1 : 0;
			}
		}
		return Count;
	}

	double ReadLegacyHealth(const ABeyondCharacterBase* Character)
	{
		const UActorComponent* Legacy = BeyondLegacyDamage::FindComponent(Character);
		const FNumericProperty* Prop = Legacy ? FindFProperty<FNumericProperty>(Legacy->GetClass(), TEXT("Health")) : nullptr;
		return Prop ? Prop->GetFloatingPointPropertyValue(Prop->ContainerPtrToValuePtr<void>(Legacy)) : -1.0;
	}
}

// Runs a step once; the step returns true when it is finished
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondStep, TFunction<bool()>, Step);
bool FBeyondStep::Update()
{
	return Step();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondPrototypeSmokeTest, "WorldsBeyond.Prototype.Smoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondPrototypeSmokeTest::RunTest(const FString& Parameters)
{
	using namespace BeyondSmokeTest;
	TSharedRef<FState> State = MakeShared<FState>();
	State->Test = this;

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/SICKA_PERSEPOLIS/MAPS/MAP_Demo_Main")));
	// Never load or write the player's saved progress (levels / EXP) during a test
	IConsoleVariable* SaveProgressVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress"));
	const FString SaveProgressBefore = SaveProgressVar ? SaveProgressVar->GetString() : FString();
	if (SaveProgressVar)
	{
		SaveProgressVar->Set(TEXT("0"), ECVF_SetByCode);
	}
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.0f));

	// Party formed, companion driven by the companion controller, no duplicate abilities
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* World = GetPlayWorld();
		if (!T.TestNotNull(TEXT("PIE world"), World))
		{
			return true;
		}

		ABeyondPlayerController* PC = Cast<ABeyondPlayerController>(World->GetFirstPlayerController());
		if (!T.TestNotNull(TEXT("Player controller is a BeyondPlayerController (GameMode uses BP_PC)"), PC))
		{
			return true;
		}
		State->PC = PC;

		UBeyondPartyComponent* Party = PC->PartyComponent;
		const TArray<ABeyondCharacterBase*> Members = Party->GetMembers();
		T.TestEqual(TEXT("Party members"), Members.Num(), 2);

		ABeyondCharacterBase* Leader = Party->GetLeader();
		ABeyondCharacterBase* Companion = Party->GetCompanion();
		T.TestNotNull(TEXT("Leader"), Leader);
		if (!T.TestNotNull(TEXT("Companion"), Companion))
		{
			return true;
		}
		State->Companion = Companion;
		T.TestTrue(TEXT("Companion is driven by BeyondCompanionController"), Companion->GetController() && Companion->GetController()->IsA<ABeyondCompanionController>());

		for (const ABeyondCharacterBase* Member : Members)
		{
			T.TestEqual(*FString::Printf(TEXT("%s team"), *Member->GetName()), Member->TeamAffiliation, EBeyondTeam::Player);
			T.TestTrue(*FString::Printf(TEXT("%s starts at full health"), *Member->GetName()),
				UBeyondCombatLibrary::GetActorHealth(Member) > 0.0f && FMath::IsNearlyEqual(UBeyondCombatLibrary::GetActorHealth(Member), UBeyondCombatLibrary::GetActorMaxHealth(Member)));
			State->BlinkSpecsBefore.Add(Member->GetName(), CountSpecs(Member, TEXT("GA_Blink")));
		}
		return true;
	}));

	// Swap back and forth: leader alternates, abilities are not granted again
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		if (!PC)
		{
			return true;
		}

		UBeyondPartyComponent* Party = PC->PartyComponent;
		Party->SwapCooldown = 0.0f;
		for (int32 i = 0; i < 4; ++i)
		{
			const ABeyondCharacterBase* Before = Party->GetLeader();
			T.TestTrue(*FString::Printf(TEXT("Swap %d succeeded"), i + 1), Party->SwapLeader());
			T.TestTrue(*FString::Printf(TEXT("Swap %d changed leader"), i + 1), Party->GetLeader() != Before);
			T.TestTrue(*FString::Printf(TEXT("Swap %d: other demigod follows as companion"), i + 1),
				Party->GetCompanion() && Party->GetCompanion()->GetController() && Party->GetCompanion()->GetController()->IsA<ABeyondCompanionController>());
		}

		for (const ABeyondCharacterBase* Member : Party->GetMembers())
		{
			T.TestEqual(*FString::Printf(TEXT("%s GA_Blink specs unchanged after swaps"), *Member->GetName()),
				CountSpecs(Member, TEXT("GA_Blink")), State->BlinkSpecsBefore.FindRef(Member->GetName()));
		}
		return true;
	}));

	// GAS damage on an enemy, mirrored into the old damage component; no friendly fire; attack tokens
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		UWorld* World = GetPlayWorld();
		if (!PC || !World)
		{
			return true;
		}
		ABeyondCharacterBase* Leader = PC->PartyComponent->GetLeader();

		ABeyondCharacterBase* Enemy = nullptr;
		for (TActorIterator<ABeyondCharacterBase> It(World); It; ++It)
		{
			if (It->TeamAffiliation == EBeyondTeam::Enemy && !UBeyondCombatLibrary::IsActorDead(*It))
			{
				Enemy = *It;
				break;
			}
		}
		if (!T.TestNotNull(TEXT("An enemy with an ability system (BP_Enemy_Base reparented)"), Enemy))
		{
			return true;
		}
		State->Enemy = Enemy;

		T.TestTrue(TEXT("Leader and enemy are hostile"), UBeyondCombatLibrary::AreHostile(Leader, Enemy));

		const float Before = UBeyondCombatLibrary::GetActorHealth(Enemy);
		UBeyondCombatLibrary::ApplyDamage(Leader, Enemy, 10.0f, BeyondTags::DamageType_Melee, BeyondTags::Event_Hit_Light);
		const float After = UBeyondCombatLibrary::GetActorHealth(Enemy);
		T.TestEqual(TEXT("Enemy lost 10 health"), After, Before - 10.0f, 0.01f);

		const double LegacyHealth = ReadLegacyHealth(Enemy);
		if (LegacyHealth >= 0.0)
		{
			T.TestEqual(TEXT("BPC_DamageSystem Health mirrors GAS"), static_cast<float>(LegacyHealth), After, 0.01f);
		}

		ABeyondCharacterBase* Companion = State->Companion.Get();
		if (Companion)
		{
			const float CompanionBefore = UBeyondCombatLibrary::GetActorHealth(Companion);
			UBeyondCombatLibrary::ApplyDamage(Leader, Companion, 10.0f, BeyondTags::DamageType_Melee, BeyondTags::Event_Hit_Light);
			T.TestEqual(TEXT("No friendly fire on the companion"), UBeyondCombatLibrary::GetActorHealth(Companion), CompanionBefore, 0.01f);
		}

		T.TestTrue(TEXT("Reserve 2 attack tokens"), Leader->TryReserveAttackTokens(2));
		T.TestFalse(TEXT("No tokens left"), Leader->TryReserveAttackTokens(1));
		Leader->ReleaseAttackTokens(2);
		T.TestTrue(TEXT("Tokens return"), Leader->TryReserveAttackTokens(1));
		Leader->ReleaseAttackTokens(1);

		UBeyondCombatLibrary::ApplyDamage(Leader, Enemy, 100000.0f, BeyondTags::DamageType_Melee, BeyondTags::Event_Hit_Light, true, nullptr, true, true);
		T.TestTrue(TEXT("Enemy dies at 0 health"), UBeyondCombatLibrary::IsActorDead(Enemy));
		return true;
	}));

	// Companion goes down; the leader stands next to the body and revives it
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondCharacterBase* Companion = State->Companion.Get();
		if (!PC || !Companion)
		{
			return true;
		}
		if (PC->PartyComponent->GetLeader() == Companion)
		{
			PC->PartyComponent->SwapLeader();
		}

		UBeyondCombatLibrary::ApplyDamage(nullptr, Companion, 100000.0f, BeyondTags::DamageType_Environment, FGameplayTag(), true, nullptr, true, true);
		T.TestTrue(TEXT("Companion is down"), UBeyondCombatLibrary::IsActorDead(Companion));
		T.TestFalse(TEXT("Dead characters can't use abilities"), Companion->TryActivateAbilityByInputTag(BeyondTags::Ability_Input_Q));
		return true;
	}));

	// The death ragdoll flings the body; walk over once it has landed
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondStep([State]()
	{
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondCharacterBase* Companion = State->Companion.Get();
		if (PC && Companion)
		{
			ABeyondCharacterBase* Leader = PC->PartyComponent->GetLeader();
			Leader->SetActorLocation(Companion->GetMesh()->GetComponentLocation() + FVector(80.0f, 0.0f, 90.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		if (ABeyondCharacterBase* Companion = State->Companion.Get())
		{
			T.TestFalse(TEXT("Companion was revived by standing next to it"), UBeyondCombatLibrary::IsActorDead(Companion));
			T.TestTrue(TEXT("Revived with some health"), UBeyondCombatLibrary::GetActorHealth(Companion) > 0.0f);
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondStep([SaveProgressBefore]()
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
		{
			Var->Set(SaveProgressBefore.IsEmpty() ? TEXT("1") : *SaveProgressBefore, ECVF_SetByCode);
		}
		return true;
	}));
	return true;
}

#endif
