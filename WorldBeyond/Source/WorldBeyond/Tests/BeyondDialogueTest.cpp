// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AI/BeyondEnemyController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BeyondGameplayTags.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "Components/ChildActorComponent.h"
#include "Components/WidgetComponent.h"
#include "Dialogue/BeyondBanter.h"
#include "Dialogue/BeyondBanterComponent.h"
#include "Dialogue/BeyondDialogueBridge.h"
#include "Dialogue/BeyondDialogueSettings.h"
#include "Dialogue/BeyondDialogueSubsystem.h"
#include "Dialogue/BeyondNPCCharacter.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Enemies/BeyondBossCharacter.h"
#include "Enemies/BeyondBossDefinition.h"
#include "Enemies/BeyondEnemySettings.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "EngineUtils.h"
#include "Game/BeyondSaveGame.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/BeyondInteractPromptWidget.h"
#include "UnrealClient.h"

/**
 * Plan 4 in the test arena (Play In Editor), on the village corner and banter volumes from migrate_pass13.py:
 * the dialogue pack is wired (every function / variable C++ calls), the content links up, talking with F (face to
 * face, a choice that sets a story flag through a row's Special Event, follow-up conversations picked by flags,
 * once-only lines), talking as Ji-Woong after a swap, villager chatter and greetings, banter (region volume, a talk
 * closing it, cooldowns, idle, a boss falling, low health), story flags in the save, PlayAnimMontage on the Body.
 * Saving is off for the run (Beyond.SaveProgress 0). When the run can render (no -nullrhi), it also saves screenshots
 * of a talk, the choices, chatter and banter to Saved/Screenshots/Dialogue/.
 *
 * UnrealEditor-Cmd.exe WorldBeyond.uproject -ExecCmds="Automation RunTests WorldsBeyond.Prototype.Dialogue;Quit" -unattended -nullrhi -nosplash
 */

namespace BeyondDialogueTest
{
	struct FState
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ABeyondPlayerController> PC;
		TWeakObjectPtr<ABeyondCharacterBase> Angel;
		TWeakObjectPtr<ABeyondCharacterBase> JiWoong;
		TMap<FName, TWeakObjectPtr<ABeyondNPCCharacter>> NPCs;
		TWeakObjectPtr<ABeyondBossCharacter> Boss;
		TArray<TWeakObjectPtr<AActor>> Cleanup;
		FString SavedSaveProgress;
		float SavedIdleSeconds = 40.0f;
		float SavedBossDelay = 3.0f;
		FVector Start = FVector::ZeroVector;
	};

	UWorld* PlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	UBeyondDialogueSubsystem* Dialogue()
	{
		return UBeyondDialogueSubsystem::Get(PlayWorld());
	}

	ABeyondCharacterBase* Leader(const FState& State)
	{
		const ABeyondPlayerController* PC = State.PC.Get();
		return PC ? PC->PartyComponent->GetLeader() : nullptr;
	}

	UActorComponent* LeaderDialogue(const FState& State)
	{
		return BeyondDialogue::FindDialogueComponent(Leader(State));
	}

	ABeyondNPCCharacter* NPC(const FState& State, const TCHAR* Id)
	{
		return State.NPCs.FindRef(FName(Id)).Get();
	}

	bool HasFlag(const FState& State, const TCHAR* Flag)
	{
		const ABeyondPlayerController* PC = State.PC.Get();
		return PC && PC->PartyComponent->HasStoryFlag(FName(Flag));
	}

	// Who stands Distance in front of Target, facing it
	void PlaceBefore(ACharacter* Who, const AActor* Target, float Distance)
	{
		if (!Who || !Target)
		{
			return;
		}
		const FVector Forward = Target->GetActorForwardVector().GetSafeNormal2D();
		const FVector Where = Target->GetActorLocation() + Forward * Distance;
		Who->SetActorLocationAndRotation(Where, (-Forward).Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
		if (AController* Controller = Who->GetController())
		{
			Controller->SetControlRotation((-Forward).Rotation());
		}
	}

	void PlaceAt(ACharacter* Who, const FVector& Where)
	{
		if (Who)
		{
			Who->SetActorLocation(Where + FVector(0.0f, 0.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	// A step that presses the skip key every 0.12 s until Done (fails after Timeout seconds)
	TFunction<bool()> SkipUntil(TSharedRef<FState> State, TFunction<bool()> Done, float Timeout, FString What)
	{
		TSharedRef<double> Started = MakeShared<double>(-1.0);
		TSharedRef<double> LastSkip = MakeShared<double>(-1.0);
		return [State, Done, Timeout, What, Started, LastSkip]() -> bool
		{
			const UWorld* World = PlayWorld();
			if (!World)
			{
				return true;
			}
			const double Now = World->GetTimeSeconds();
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
				State->Test->AddError(FString::Printf(TEXT("Timed out after %.0f s: %s (row on screen: %s)"), Timeout, *What,
					*BeyondDialogue::GetCurrentRow(LeaderDialogue(*State)).ToString()));
				return true;
			}
			if (Now - *LastSkip >= 0.12)
			{
				*LastSkip = Now;
				BeyondDialogue::Skip(LeaderDialogue(*State));
			}
			return false;
		};
	}

	// A step that waits until Done (fails after Timeout seconds)
	TFunction<bool()> WaitUntil(TSharedRef<FState> State, TFunction<bool()> Done, float Timeout, FString What)
	{
		TSharedRef<double> Started = MakeShared<double>(-1.0);
		return [State, Done, Timeout, What, Started]() -> bool
		{
			const UWorld* World = PlayWorld();
			if (!World)
			{
				return true;
			}
			if (*Started < 0.0)
			{
				*Started = World->GetTimeSeconds();
			}
			if (Done())
			{
				return true;
			}
			if (World->GetTimeSeconds() - *Started > Timeout)
			{
				State->Test->AddError(FString::Printf(TEXT("Timed out after %.0f s: %s"), Timeout, *What));
				return true;
			}
			return false;
		};
	}

	bool OptionsReady(const FState& State)
	{
		bool bReady = false;
		BeyondDialogue::GetOptionCount(LeaderDialogue(State), &bReady);
		return bReady;
	}

	// A screenshot (with the UI) when this run renders
	void Shot(const TCHAR* Name)
	{
		if (FApp::CanEverRender())
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/Dialogue") / FString(Name) + TEXT(".png"), true, false);
		}
	}

	bool ConversationOver()
	{
		const UBeyondDialogueSubsystem* Subsystem = Dialogue();
		return Subsystem && !Subsystem->IsConversationActive();
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondDialogueStep, TFunction<bool()>, Step);
bool FBeyondDialogueStep::Update() { return Step(); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondDialogueTest, "WorldsBeyond.Prototype.Dialogue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondDialogueTest::RunTest(const FString& Parameters)
{
	using namespace BeyondDialogueTest;
	TSharedRef<FState> State = MakeShared<FState>();
	State->Test = this;

	if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
	{
		State->SavedSaveProgress = SaveProgress->GetString();
		SaveProgress->Set(TEXT("0"), ECVF_SetByCode);
	}
	UBeyondDialogueSettings* Settings = GetMutableDefault<UBeyondDialogueSettings>();
	State->SavedIdleSeconds = Settings->IdleSeconds;
	State->SavedBossDelay = Settings->BossBanterDelay;

	// The pack is wired up and the content links up (no PIE needed)
	{
		const TArray<FString> Missing = BeyondDialogue::CheckPackBindings();
		TestTrue(*FString::Printf(TEXT("The dialogue pack has everything C++ calls (missing: %s)"), *FString::Join(Missing, TEXT(", "))), Missing.IsEmpty());

		const UDataTable* Rows = BeyondDialogue::GetDialogueTable();
		const UDataTable* OverHead = BeyondDialogue::GetOverHeadTable();
		const UDataTable* Speakers = BeyondDialogue::GetSpeakersTable();
		const UBeyondBanterSet* Banter = Settings->BanterSet.LoadSynchronous();
		if (TestNotNull(TEXT("DT_Dialogue"), Rows) && TestNotNull(TEXT("DT_TextOverHead"), OverHead) && TestNotNull(TEXT("DT_Speakers"), Speakers)
			&& TestNotNull(TEXT("migrate_pass13: DA_Banter"), Banter))
		{
			TArray<FString> Broken;
			for (const FName& Row : Rows->GetRowNames())
			{
				FBeyondDialogueRowView View;
				BeyondDialogue::ReadDialogueRow(Row, View);
				TArray<FName> Targets = { View.NextRow };
				for (const TPair<FText, FName>& Option : View.Options)
				{
					Targets.Add(Option.Value);
				}
				for (const FName& Target : Targets)
				{
					// The pack's demo uses "Quit" as an option target (it quits the game)
					if (!Target.IsNone() && Target != TEXT("Quit") && !Rows->FindRowUnchecked(Target))
					{
						Broken.Add(FString::Printf(TEXT("%s -> %s"), *Row.ToString(), *Target.ToString()));
					}
				}
				if (!View.SpeakerRow.IsNone() && !Speakers->FindRowUnchecked(View.SpeakerRow))
				{
					Broken.Add(FString::Printf(TEXT("%s: speaker %s"), *Row.ToString(), *View.SpeakerRow.ToString()));
				}
			}
			for (const FBeyondBanterLine& Line : Banter->Lines)
			{
				FBeyondDialogueRowView View;
				if (!BeyondDialogue::ReadDialogueRow(Line.StartRow, View))
				{
					Broken.Add(FString::Printf(TEXT("banter %s: no row %s"), *Line.Id.ToString(), *Line.StartRow.ToString()));
				}
				else if (!View.bFreeMovement)
				{
					Broken.Add(FString::Printf(TEXT("banter %s: %s isn't free movement"), *Line.Id.ToString(), *Line.StartRow.ToString()));
				}
			}
			TestTrue(*FString::Printf(TEXT("Every next row, choice, speaker and banter row exists (%s)"), *FString::Join(Broken, TEXT("; "))), Broken.IsEmpty());
			TestTrue(TEXT("DA_Banter has lines for every trigger but Scripted"), Banter->Lines.Num() >= 6);

			FBeyondDialogueRowView Accept;
			TestTrue(TEXT("Elder_Accept sets Quest_Gorehide"), BeyondDialogue::ReadDialogueRow(TEXT("Elder_Accept"), Accept)
				&& Accept.SpecialEvent == TEXT("Flag.Quest_Gorehide") && !Accept.bFreeMovement);
			FBeyondDialogueRowView Choices;
			TestTrue(TEXT("Elder_Intro.5 offers three choices"), BeyondDialogue::ReadDialogueRow(TEXT("Elder_Intro.5"), Choices)
				&& Choices.bOptions && Choices.Options.Num() == 3);
		}

		// Story flags are part of the save
		UBeyondSaveGame* Save = NewObject<UBeyondSaveGame>();
		Save->StoryFlags = { TEXT("Quest_Gorehide"), TEXT("Boss.gorehide") };
		TArray<uint8> Bytes;
		const UBeyondSaveGame* Loaded = UGameplayStatics::SaveGameToMemory(Save, Bytes)
			? Cast<UBeyondSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)) : nullptr;
		TestTrue(TEXT("Save v5+ keeps story flags"), Loaded && Loaded->Version >= 5 && Loaded->StoryFlags.Num() == 2 && Loaded->StoryFlags.Contains(TEXT("Boss.gorehide")));
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/WorldsBeyond/Maps/TestArena")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.0f));

	// Setup: the party has the pack's parts, the village is there
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* World = PlayWorld();
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
		State->Start = Angel->GetActorLocation();
		Party->ResetStoryFlags();
		PC->BanterComponent->bBanterEnabled = false;

		const FName Tag = GetDefault<UBeyondDialogueSettings>()->ComponentTag;
		for (ABeyondCharacterBase* Member : { Angel, JiWoong })
		{
			bool bText = false;
			bool bCamera = false;
			for (const UActorComponent* Component : Member->GetComponents())
			{
				bText |= Component->IsA<UWidgetComponent>() && Component->ComponentHasTag(Tag);
				bCamera |= Component->IsA<UChildActorComponent>() && Component->ComponentHasTag(Tag)
					&& Cast<UChildActorComponent>(Component)->GetChildActor() != nullptr;
			}
			T.TestTrue(*FString::Printf(TEXT("%s got the pack's parts (conversations, over-head dialogue, text %d, camera %d)"), *Member->GetName(), bText, bCamera),
				BeyondDialogue::FindDialogueComponent(Member) && BeyondDialogue::FindOverHeadComponent(Member) && bText && bCamera);
		}

		for (TActorIterator<ABeyondNPCCharacter> It(World); It; ++It)
		{
			State->NPCs.Add(It->NPCId, *It);
		}
		for (const TCHAR* Id : { TEXT("elder_maren"), TEXT("merchant_bram"), TEXT("guard_ysolde"), TEXT("villager_tilda"), TEXT("villager_osk"),
			TEXT("villager_pell"), TEXT("villager_wren") })
		{
			const ABeyondNPCCharacter* Villager = NPC(*State, Id);
			if (T.TestNotNull(*FString::Printf(TEXT("migrate_pass13: %s stands in the village"), Id), Villager))
			{
				T.TestTrue(*FString::Printf(TEXT("%s wears its mesh and has over-head dialogue"), Id),
					Villager->GetMesh()->GetSkeletalMeshAsset() != nullptr && BeyondDialogue::FindOverHeadComponent(Villager) != nullptr);
				T.TestFalse(*FString::Printf(TEXT("%s is nobody's enemy"), Id), UBeyondCombatLibrary::AreHostile(Villager, Angel));
			}
		}
		const ABeyondNPCCharacter* Tilda = NPC(*State, TEXT("villager_tilda"));
		T.TestTrue(TEXT("Tilda chats with Osk"), Tilda && Tilda->ChatterPartners.Num() == 1 && Tilda->ChatterPartners[0] == NPC(*State, TEXT("villager_osk")));
		return true;
	}));

	// F in front of the elder: face to face, Elder_Intro
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		PlaceBefore(State->Angel.Get(), NPC(*State, TEXT("elder_maren")), 170.0f);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.4f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondNPCCharacter* Elder = NPC(*State, TEXT("elder_maren"));
		ABeyondCharacterBase* Angel = State->Angel.Get();
		if (!PC || !Elder || !Angel)
		{
			return true;
		}
		T.TestTrue(TEXT("F would talk to the elder"), PC->FindTalkTarget() == Elder);
		T.TestEqual(TEXT("The elder starts with Elder_Intro"), Elder->PickConversation(), FName(TEXT("Elder_Intro")));
		PC->UpdateInteractPrompt();
		const UBeyondInteractPromptWidget* Prompt = Cast<UBeyondInteractPromptWidget>(PC->GetInteractPromptWidget());
		T.TestTrue(TEXT("The prompt says Talk - Elder Maren"), Prompt && Prompt->GetTarget().ToString() == TEXT("Elder Maren") && Prompt->HasPrompt());

		T.TestTrue(TEXT("Talking starts"), PC->TalkTo());
		const UBeyondDialogueSubsystem* Subsystem = Dialogue();
		T.TestTrue(TEXT("Face to face with the elder"), Subsystem && Subsystem->IsTalking() && Subsystem->GetPartner() == Elder
			&& Subsystem->GetStartRow() == TEXT("Elder_Intro"));
		T.TestTrue(TEXT("Angel's BP_AC_Dialogue runs it"), BeyondDialogue::IsInDialogue(BeyondDialogue::FindDialogueComponent(Angel)));
		T.TestTrue(TEXT("The elder knows it is in a dialogue (BP_I_Dialogue)"), BeyondDialogue::IsActorInDialogue(Elder));
		T.TestFalse(TEXT("Angel takes no game input while talking"), Angel->InputEnabled());
		T.TestFalse(TEXT("Nobody else to talk to while talking"), PC->FindTalkTarget() != nullptr);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		const ABeyondPlayerController* PC = State->PC.Get();
		const UUserWidget* HUD = PC ? PC->GetHUDWidget() : nullptr;
		State->Test->TestTrue(TEXT("The HUD hides while talking"), !HUD || !HUD->IsVisible());
		// The first screenshot of a run can come from the editor's own viewport; this one takes it
		Shot(TEXT("0_WarmUp"));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([]() { Shot(TEXT("1_Talk")); return true; }));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));

	// Skip to the choices, pick "We'll hunt the Pale Alpha"
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep(SkipUntil(State, [State]() { return OptionsReady(*State); }, 20.0f, TEXT("the elder's choices"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([]() { Shot(TEXT("2_Choices")); return true; }));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UActorComponent* Component = LeaderDialogue(*State);
		T.TestEqual(TEXT("The choices are Elder_Intro.5"), BeyondDialogue::GetCurrentRow(Component), FName(TEXT("Elder_Intro.5")));
		T.TestEqual(TEXT("Three choices on screen"), BeyondDialogue::GetOptionCount(Component), 3);
		T.TestTrue(TEXT("Choosing the first one"), BeyondDialogue::ChooseOption(Component, 0));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		T.TestEqual(TEXT("The choice leads to Elder_Accept"), BeyondDialogue::GetCurrentRow(LeaderDialogue(*State)), FName(TEXT("Elder_Accept")));
		T.TestTrue(TEXT("Its Special Event set the story flag Quest_Gorehide"), HasFlag(*State, TEXT("Quest_Gorehide")));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep(SkipUntil(State, []() { return ConversationOver(); }, 20.0f, TEXT("the end of Elder_Accept"))));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondNPCCharacter* Elder = NPC(*State, TEXT("elder_maren"));
		if (!PC || !Angel || !Elder)
		{
			return true;
		}
		T.TestFalse(TEXT("The conversation is over"), BeyondDialogue::IsInDialogue(BeyondDialogue::FindDialogueComponent(Angel)));
		T.TestFalse(TEXT("The elder is free again"), BeyondDialogue::IsActorInDialogue(Elder));
		T.TestTrue(TEXT("Angel takes game input again"), Angel->InputEnabled());
		T.TestTrue(*FString::Printf(TEXT("The camera is back on Angel (%s)"), *GetNameSafe(PC->GetViewTarget())), PC->GetViewTarget() == Angel);
		T.TestTrue(TEXT("The HUD is back"), !PC->GetHUDWidget() || PC->GetHUDWidget()->IsVisible());

		// The next talk follows the flag
		T.TestEqual(TEXT("With Quest_Gorehide the elder says Elder_Waiting"), Elder->PickConversation(), FName(TEXT("Elder_Waiting")));
		T.TestTrue(TEXT("Talking again"), PC->TalkTo(Elder));
		T.TestEqual(TEXT("It is Elder_Waiting"), Dialogue() ? Dialogue()->GetStartRow() : NAME_None, FName(TEXT("Elder_Waiting")));
		Dialogue()->EndConversation();
		T.TestFalse(TEXT("EndConversation closes it"), BeyondDialogue::IsInDialogue(BeyondDialogue::FindDialogueComponent(Angel)) || Dialogue()->IsConversationActive());

		// Gorehide falls (the flag a boss death raises)
		Dialogue()->SetFlag(TEXT("Boss.gorehide"));
		T.TestEqual(TEXT("After Gorehide the elder says Elder_GorehideDone"), Elder->PickConversation(), FName(TEXT("Elder_GorehideDone")));
		T.TestTrue(TEXT("Talking about the alpha"), PC->TalkTo(Elder));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep(SkipUntil(State, []() { return ConversationOver(); }, 25.0f, TEXT("the end of Elder_GorehideDone"))));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondNPCCharacter* Elder = NPC(*State, TEXT("elder_maren"));
		T.TestTrue(TEXT("Elder_GorehideDone set Quest_Veyla mid-way"), HasFlag(*State, TEXT("Quest_Veyla")));
		T.TestTrue(TEXT("Elder_GorehideDone is remembered as said"), HasFlag(*State, TEXT("Talked.elder_maren.Elder_GorehideDone")));
		T.TestEqual(TEXT("It is said once: the elder moves on to Elder_Default"), Elder ? Elder->PickConversation() : NAME_None, FName(TEXT("Elder_Default")));
		return true;
	}));

	// Ji-Woong leads and talks to Bram (his own BP_AC_Dialogue runs it)
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.1f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		if (!PC)
		{
			return true;
		}
		T.TestTrue(TEXT("Swap to Ji-Woong"), PC->PartyComponent->SwapLeader() && Leader(*State) == State->JiWoong.Get());
		PlaceBefore(State->JiWoong.Get(), NPC(*State, TEXT("merchant_bram")), 170.0f);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondNPCCharacter* Bram = NPC(*State, TEXT("merchant_bram"));
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!PC || !Bram || !JiWoong)
		{
			return true;
		}
		T.TestTrue(TEXT("F would talk to Bram"), PC->FindTalkTarget() == Bram);
		T.TestTrue(TEXT("Ji-Woong talks to Bram"), PC->TalkTo());
		UActorComponent* Component = BeyondDialogue::FindDialogueComponent(JiWoong);
		T.TestTrue(TEXT("Ji-Woong's own BP_AC_Dialogue runs it"), BeyondDialogue::IsInDialogue(Component) && Dialogue()->GetActiveComponent() == Component);
		T.TestTrue(TEXT("Bram_Intro set Met_Bram"), HasFlag(*State, TEXT("Met_Bram")));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep(SkipUntil(State, []() { return ConversationOver(); }, 25.0f, TEXT("the end of Bram_Intro"))));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.1f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		const ABeyondNPCCharacter* Bram = NPC(*State, TEXT("merchant_bram"));
		if (!PC || !JiWoong)
		{
			return true;
		}
		T.TestTrue(TEXT("Ji-Woong takes game input again"), JiWoong->InputEnabled());
		T.TestEqual(TEXT("Bram's intro was once: after Gorehide he says Bram_AfterGorehide"), Bram ? Bram->PickConversation() : NAME_None,
			FName(TEXT("Bram_AfterGorehide")));
		T.TestTrue(TEXT("Swap back to Angel"), PC->PartyComponent->SwapLeader() && Leader(*State) == State->Angel.Get());
		return true;
	}));

	// Village life: a greeting, Tilda and Osk's chatter (picked by flags), Pell starting a chat by himself
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		ABeyondNPCCharacter* Guard = NPC(*State, TEXT("guard_ysolde"));
		PlaceBefore(State->Angel.Get(), Guard, 300.0f);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep(WaitUntil(State, [State]()
	{
		const ABeyondNPCCharacter* Guard = NPC(*State, TEXT("guard_ysolde"));
		return Guard && BeyondDialogue::IsInDialogue(BeyondDialogue::FindOverHeadComponent(Guard));
	}, 3.0f, TEXT("the guard greeting the party"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondNPCCharacter* Tilda = NPC(*State, TEXT("villager_tilda"));
		const ABeyondNPCCharacter* Osk = NPC(*State, TEXT("villager_osk"));
		if (!Tilda || !Osk)
		{
			return true;
		}
		BeyondDialogue::StopOverHead(BeyondDialogue::FindOverHeadComponent(Tilda));
		Tilda->ResetChatter();
		T.TestEqual(TEXT("With Gorehide dead Tilda talks about it (Chat_Wolves is blocked)"), Tilda->PickChatter(), FName(TEXT("Chat_AlphaDead")));
		T.TestTrue(TEXT("Tilda starts chatting"), Tilda->StartChatter());
		T.TestTrue(TEXT("Both are busy chatting"), Tilda->IsBusy() && Osk->IsBusy());
		PlaceBefore(State->Angel.Get(), Tilda, 450.0f);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([]() { Shot(TEXT("3_Chatter")); return true; }));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep(WaitUntil(State, [State]()
	{
		const ABeyondNPCCharacter* Tilda = NPC(*State, TEXT("villager_tilda"));
		const ABeyondNPCCharacter* Osk = NPC(*State, TEXT("villager_osk"));
		return Tilda && Osk && !Tilda->IsBusy() && !Osk->IsBusy();
	}, 12.0f, TEXT("Tilda and Osk's chat ending"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		ABeyondNPCCharacter* Pell = NPC(*State, TEXT("villager_pell"));
		ACharacter* Angel = State->Angel.Get();
		if (Pell && Angel)
		{
			// Quiet, then the party inside chatter range (outside greeting range): he starts on his own
			BeyondDialogue::StopOverHead(BeyondDialogue::FindOverHeadComponent(Pell));
			Pell->ResetChatter();
			PlaceAt(Angel, Pell->GetActorLocation() + FVector(0.0f, -900.0f, -100.0f));
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep(WaitUntil(State, [State]()
	{
		const ABeyondNPCCharacter* Pell = NPC(*State, TEXT("villager_pell"));
		return Pell && Pell->IsBusy();
	}, 8.0f, TEXT("Pell starting a chat on his own while the party is near"))));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		const ABeyondNPCCharacter* Wren = NPC(*State, TEXT("villager_wren"));
		State->Test->TestTrue(TEXT("Wren is in Pell's chat"), Wren && Wren->IsBusy());
		return true;
	}));

	// Banter: walking into the village volume
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		ABeyondPlayerController* PC = State->PC.Get();
		if (PC)
		{
			PlaceAt(State->Angel.Get(), State->Start - FVector(0.0f, 0.0f, 100.0f));
			PC->BanterComponent->bBanterEnabled = true;
			PC->BanterComponent->ResetCooldowns();
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		const ABeyondNPCCharacter* Elder = NPC(*State, TEXT("elder_maren"));
		if (Elder)
		{
			PlaceAt(State->Angel.Get(), Elder->GetActorLocation() + FVector(0.0f, -500.0f, -100.0f));
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([]() { Shot(TEXT("4_Banter")); return true; }));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondCharacterBase* Angel = State->Angel.Get();
		const UBeyondDialogueSubsystem* Subsystem = Dialogue();
		if (!PC || !Angel || !Subsystem)
		{
			return true;
		}
		T.TestTrue(*FString::Printf(TEXT("Entering the village plays its banter (%s)"), *Subsystem->GetStartRow().ToString()),
			Subsystem->IsBanterActive() && Subsystem->GetStartRow() == TEXT("Banter_Village_1"));
		T.TestTrue(TEXT("Once-only banter is remembered"), HasFlag(*State, TEXT("Banter.Village_1")));
		T.TestTrue(TEXT("Banter lets you keep playing"), Angel->InputEnabled());

		// Talking to someone ends it
		ABeyondNPCCharacter* Elder = NPC(*State, TEXT("elder_maren"));
		PlaceBefore(Angel, Elder, 170.0f);
		T.TestTrue(TEXT("Talking to the elder during banter"), PC->TalkTo(Elder));
		T.TestTrue(TEXT("The talk replaced the banter"), Subsystem->IsTalking() && Subsystem->GetStartRow() == TEXT("Elder_Default"));
		Dialogue()->EndConversation();

		// The next banter waits for the cooldown (regions and bosses don't)
		T.TestFalse(TEXT("Idle banter waits for the banter cooldown"), PC->BanterComponent->TriggerBanter(EBeyondBanterTrigger::Idle));
		T.TestTrue(TEXT("A region still plays (the forest's first look, picked by context)"),
			PC->BanterComponent->TriggerBanter(EBeyondBanterTrigger::RegionEntered, TEXT("region_forest"))
			&& PC->BanterComponent->GetLastBanterId() == TEXT("Forest_1"));
		Dialogue()->EndConversation();
		T.TestTrue(TEXT("Forest_1 is once only: the forest's next banter is Forest_2"),
			PC->BanterComponent->TriggerBanter(EBeyondBanterTrigger::RegionEntered, TEXT("region_forest"))
			&& PC->BanterComponent->GetLastBanterId() == TEXT("Forest_2"));
		Dialogue()->EndConversation();

		// Idle: nobody moving for Idle Seconds
		PC->BanterComponent->ResetCooldowns();
		GetMutableDefault<UBeyondDialogueSettings>()->IdleSeconds = 1.5f;
		PC->BanterComponent->NoteActivity();
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep(WaitUntil(State, [State]()
	{
		const ABeyondPlayerController* PC = State->PC.Get();
		return PC && Dialogue() && Dialogue()->IsBanterActive() && PC->BanterComponent->GetLastBanterId().ToString().StartsWith(TEXT("Idle_"));
	}, 6.0f, TEXT("idle banter"))));

	// A boss falls: Boss.gorehide again (cleared first) and its banter
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondCharacterBase* Angel = State->Angel.Get();
		UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(PlayWorld());
		const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
		UBeyondBossDefinition* Definition = Roster ? Cast<UBeyondBossDefinition>(Roster->FindEnemy(TEXT("boss_gorehide"))) : nullptr;
		if (!PC || !Angel || !Enemies || !T.TestNotNull(TEXT("Gorehide in the roster"), Definition))
		{
			return true;
		}
		Dialogue()->EndConversation();
		GetMutableDefault<UBeyondDialogueSettings>()->IdleSeconds = 1000.0f;
		GetMutableDefault<UBeyondDialogueSettings>()->BossBanterDelay = 0.5f;
		PC->PartyComponent->SetStoryFlag(TEXT("Boss.gorehide"), false);
		for (ABeyondCharacterBase* Member : PC->PartyComponent->GetMembers())
		{
			if (UAbilitySystemComponent* ASC = Member->GetAbilitySystemComponent())
			{
				ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 1000000.0f);
				ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), 1000000.0f);
			}
		}
		PlaceAt(Angel, State->Start - FVector(0.0f, 0.0f, 100.0f));
		FBeyondEnemySpawnParams Params;
		Params.Level = 1;
		const FVector Where = State->Start + FVector(-1600.0f, 0.0f, 0.0f);
		ABeyondBossCharacter* Boss = Cast<ABeyondBossCharacter>(Enemies->SpawnEnemy(Definition, FTransform(Where), Params));
		if (T.TestNotNull(TEXT("Gorehide spawns"), Boss))
		{
			if (ABeyondEnemyController* Brain = Cast<ABeyondEnemyController>(Boss->GetController()))
			{
				Brain->SetBrainEnabled(false);
			}
			Boss->ForcePhase(Boss->GetPhaseCount() - 1);
			State->Boss = Boss;
			State->Cleanup.Add(Boss);
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondBossCharacter* Boss = State->Boss.Get();
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondPlayerController* PC = State->PC.Get();
		if (!Boss || !Angel || !PC)
		{
			return true;
		}
		Dialogue()->EndConversation();
		PC->BanterComponent->ResetCooldowns();
		UBeyondCombatLibrary::ApplyDamage(Angel, Boss, 1.0e8f, BeyondTags::DamageType_Environment, FGameplayTag(), true, nullptr, true, true);
		T.TestTrue(TEXT("Gorehide is dead"), UBeyondCombatLibrary::IsActorDead(Boss));
		T.TestTrue(TEXT("Its death raised Boss.gorehide"), HasFlag(*State, TEXT("Boss.gorehide")));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep(WaitUntil(State, [State]()
	{
		const ABeyondPlayerController* PC = State->PC.Get();
		return PC && PC->BanterComponent->GetLastBanterId() == TEXT("Boss_Gorehide") && Dialogue()->IsBanterActive();
	}, 4.0f, TEXT("Gorehide's victory banter"))));

	// Low health in a fight
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondCharacterBase* Angel = State->Angel.Get();
		UAbilitySystemComponent* ASC = Angel ? Angel->GetAbilitySystemComponent() : nullptr;
		if (!PC || !ASC)
		{
			return true;
		}
		Dialogue()->EndConversation();
		PC->BanterComponent->ResetCooldowns();
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 1000.0f);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), 1000.0f);
		// A hit puts the party in a fight, then health drops low
		UBeyondCombatLibrary::ApplyDamage(nullptr, Angel, 10.0f, BeyondTags::DamageType_Environment, FGameplayTag(), true, nullptr, true, true);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), 150.0f);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep(WaitUntil(State, [State]()
	{
		const ABeyondPlayerController* PC = State->PC.Get();
		const FString Id = PC ? PC->BanterComponent->GetLastBanterId().ToString() : FString();
		return Dialogue()->IsBanterActive() && (Id == TEXT("Low_Angel") || Id == TEXT("Low_Any"));
	}, 3.0f, TEXT("low-health banter while Angel leads"))));

	// PlayAnimMontage plays on the MetaHuman's Body
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		Dialogue()->EndConversation();
		if (!Angel)
		{
			return true;
		}
		UAnimMontage* Montage = nullptr;
		for (const TPair<TSubclassOf<UGameplayAbility>, TObjectPtr<UAnimMontage>>& Pair : Angel->AbilityMontages)
		{
			Montage = Pair.Value ? Pair.Value.Get() : Montage;
		}
		if (T.TestNotNull(TEXT("Angel has an ability montage to play"), Montage))
		{
			const USkeletalMeshComponent* Body = Angel->GetCombatMesh();
			const float Length = Angel->PlayAnimMontage(Montage);
			T.TestTrue(TEXT("PlayAnimMontage plays it on the Body"), Length > 0.0f && Body && Body->GetAnimInstance()
				&& Body->GetAnimInstance()->Montage_IsPlaying(Montage));
			Angel->StopAnimMontage(Montage);
		}

		// Story flags: set, cleared, Boss.* answering for story bosses beaten in old saves
		UBeyondPartyComponent* Party = State->PC.IsValid() ? State->PC->PartyComponent.Get() : nullptr;
		if (Party)
		{
			Party->SetStoryFlag(TEXT("Test_Flag"));
			T.TestTrue(TEXT("A story flag sets"), Party->HasStoryFlag(TEXT("Test_Flag")));
			Party->SetStoryFlag(TEXT("Test_Flag"), false);
			T.TestFalse(TEXT("...and clears"), Party->HasStoryFlag(TEXT("Test_Flag")));
			Party->MarkBossDefeated(TEXT("test_boss"));
			T.TestTrue(TEXT("Boss.<id> answers for a beaten story boss"), Party->HasStoryFlag(TEXT("Boss.test_boss")));
			Party->ResetDefeatedBosses();
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		for (const TWeakObjectPtr<AActor>& Actor : State->Cleanup)
		{
			if (AActor* Alive = Actor.Get())
			{
				Alive->Destroy();
			}
		}
		if (const ABeyondPlayerController* PC = State->PC.Get())
		{
			PC->PartyComponent->ResetStoryFlags();
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondDialogueStep([State]()
	{
		// Only once PIE is gone: the party saves when play ends, and that save must still see saving switched off
		if (GEditor && GEditor->PlayWorld)
		{
			return false;
		}
		UBeyondDialogueSettings* Settings = GetMutableDefault<UBeyondDialogueSettings>();
		Settings->IdleSeconds = State->SavedIdleSeconds;
		Settings->BossBanterDelay = State->SavedBossDelay;
		if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
		{
			SaveProgress->Set(State->SavedSaveProgress.IsEmpty() ? TEXT("1") : *State->SavedSaveProgress, ECVF_SetByCode);
		}
		return true;
	}));
	return true;
}

#endif
