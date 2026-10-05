// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Abilities/BeyondGA_EquipWeapon.h"
#include "AbilitySystem/Abilities/BeyondGA_GroundStrike.h"
#include "AbilitySystem/Abilities/BeyondGA_MeleeCombo.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondSpikeBurst.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "AI/BeyondCompanionController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "BeyondGameplayTags.h"
#include "Blueprint/UserWidget.h"
#include "BrainComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/BeyondAimComponent.h"
#include "Characters/BeyondCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Game/BeyondCombatSubsystem.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Particles/ParticleSystemComponent.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/BeyondCrosshairWidget.h"
#include "Weapons/BeyondWeapon.h"

/**
 * Passes 2-5 in MAP_Demo_Main (Play In Editor, no rendering needed): MetaHuman combat mesh, companion walk fix,
 * the buddy holding still during the intro, enemy perception, Ji-Woong's sword and his LMB sword combo (voice once,
 * upper body while moving), Gilded Step, Sunbrand, Angel's staff, aiming (crosshair, shoulder camera, target under
 * the crosshair, casts facing it) and his E crystal spikes, the Bond meter and the duo super move, the ability bar
 * refresh on swap, and the boss health bar.
 *
 * UnrealEditor-Cmd.exe WorldBeyond.uproject -ExecCmds="Automation RunTests WorldsBeyond.Prototype;Quit" -unattended -nullrhi -nosplash
 */

namespace BeyondPowersTest
{
	struct FState
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ABeyondPlayerController> PC;
		TWeakObjectPtr<ABeyondCharacterBase> Angel;
		TWeakObjectPtr<ABeyondCharacterBase> JiWoong;
		TArray<TWeakObjectPtr<ABeyondCharacterBase>> Enemies;
		TWeakObjectPtr<ABeyondCharacterBase> Boss;
		TWeakObjectPtr<ABeyondCharacterBase> StrikeTarget;
		TWeakObjectPtr<AAIController> Tester;
		TWeakObjectPtr<AController> SavedCompanionController;
		FVector StartLocation = FVector::ZeroVector;
		TMap<FString, float> Health;
		int32 AbilitiesChangedEvents = 0;
		int32 VoiceLinesBefore = 0;
		TWeakObjectPtr<ABeyondCharacterBase> AimEnemy;
		float AimYaw = 0.0f;
		TWeakObjectPtr<ABeyondSpikeBurst> SpikeBurst;
		FDelegateHandle SpawnHandle;
	};

	UWorld* GetPlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	FGameplayAbilitySpecHandle FindSpec(const ABeyondCharacterBase* Character, const TCHAR* NameFragment)
	{
		if (const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr)
		{
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->GetClass()->GetName().Contains(NameFragment))
				{
					return Spec.Handle;
				}
			}
		}
		return FGameplayAbilitySpecHandle();
	}

	bool HasTag(const ABeyondCharacterBase* Character, const FGameplayTag& Tag)
	{
		const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		return ASC && ASC->HasMatchingGameplayTag(Tag);
	}

	// Freeze an enemy's AI and park it relative to Anchor
	void PlaceEnemy(ABeyondCharacterBase* Enemy, const AActor* Anchor, float Forward, float Right = 0.0f)
	{
		if (!Enemy || !Anchor)
		{
			return;
		}
		if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
		{
			AI->StopMovement();
			if (UBrainComponent* Brain = AI->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Powers test"));
			}
		}
		if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
		{
			ASC->SetLooseGameplayTagCount(BeyondTags::State_Blocking, 0);
			ASC->SetLooseGameplayTagCount(BeyondTags::State_Parrying, 0);
		}
		const FVector Location = Anchor->GetActorLocation() + Anchor->GetActorForwardVector() * Forward + Anchor->GetActorRightVector() * Right;
		Enemy->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		Enemy->GetCharacterMovement()->StopMovementImmediately();
	}

	// Drive a demigod with a plain AI controller (no companion thinking) so an ability can be tested on its own
	AAIController* TakeOver(FState& State, ABeyondCharacterBase* Character)
	{
		UWorld* World = Character->GetWorld();
		State.SavedCompanionController = Character->GetController();
		AAIController* Tester = State.Tester.Get();
		if (!Tester)
		{
			Tester = World->SpawnActor<AAIController>();
			State.Tester = Tester;
		}
		Tester->Possess(Character);
		return Tester;
	}

	void GiveBack(FState& State, ABeyondCharacterBase* Character)
	{
		if (AAIController* Tester = State.Tester.Get())
		{
			Tester->ClearFocus(EAIFocusPriority::Gameplay);
			Tester->UnPossess();
		}
		if (AController* Companion = State.SavedCompanionController.Get())
		{
			Companion->Possess(Character);
		}
	}

	// Stand Character on open ground facing a clear lane (level geometry would stop a dash test)
	bool MoveToOpenGround(ACharacter* Character, const TArray<FVector>& Candidates, float Distance)
	{
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight() * 0.7f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondTestOpenGround), false, Character);
		for (const FVector& Candidate : Candidates)
		{
			const FVector Start = Candidate + FVector(0.0f, 0.0f, 40.0f);
			for (int32 Step = 0; Step < 16; ++Step)
			{
				const FRotator Facing(0.0f, Step * 22.5f, 0.0f);
				FHitResult Hit;
				if (!Character->GetWorld()->SweepSingleByObjectType(Hit, Start, Start + Facing.Vector() * Distance, FQuat::Identity,
					FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllStaticObjects), Shape, Params))
				{
					Character->SetActorLocationAndRotation(Candidate, Facing, false, nullptr, ETeleportType::TeleportPhysics);
					return true;
				}
			}
		}
		return false;
	}

	// The weapon a demigod carries (Ji-Woong's sword, Angel's staff)
	UBeyondGA_EquipWeapon* GetEquipAbility(const ABeyondCharacterBase* Character)
	{
		if (const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr)
		{
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (UBeyondGA_EquipWeapon* Equip = Cast<UBeyondGA_EquipWeapon>(Spec.GetPrimaryInstance()))
				{
					return Equip;
				}
			}
		}
		return nullptr;
	}

	UBeyondGA_MeleeCombo* GetSwordCombo(const ABeyondCharacterBase* Character)
	{
		const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(FindSpec(Character, TEXT("GA_JiWoong_SwordCombo"))) : nullptr;
		return Spec ? Cast<UBeyondGA_MeleeCombo>(Spec->GetPrimaryInstance() ? Spec->GetPrimaryInstance() : Spec->Ability.Get()) : nullptr;
	}

	// Montage weight on an anim Blueprint slot (DefaultSlot = full body, UpperBody = above spine_01)
	float SlotWeight(const ABeyondCharacterBase* Character, const TCHAR* Slot)
	{
		const UAnimInstance* Anim = Character && Character->GetCombatMesh() ? Character->GetCombatMesh()->GetAnimInstance() : nullptr;
		return Anim ? Anim->GetSlotMontageGlobalWeight(FName(Slot)) : 0.0f;
	}

	// The anim Blueprint's current idle (IdleAnimation variable)
	FString IdleName(const ABeyondCharacterBase* Character)
	{
		const UAnimInstance* Anim = Character && Character->GetCombatMesh() ? Character->GetCombatMesh()->GetAnimInstance() : nullptr;
		const FObjectProperty* Prop = Anim ? FindFProperty<FObjectProperty>(Anim->GetClass(), TEXT("IdleAnimation")) : nullptr;
		return Prop ? GetNameSafe(Prop->GetObjectPropertyValue_InContainer(Anim)) : FString();
	}

	bool IsHolstered(const UBeyondGA_EquipWeapon* Equip)
	{
		const ABeyondWeapon* Weapon = Equip ? Cast<ABeyondWeapon>(Equip->GetEquippedWeapon()) : nullptr;
		return Weapon && Weapon->bHolstered && !Equip->IsWeaponDrawn() && !Weapon->IsHidden();
	}

	TArray<FString> AbilityBar(const ABeyondCharacterBase* Character)
	{
		TArray<FString> Names;
		UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		TArray<FGameplayAbilitySpecHandle> Handles;
		UBeyondCombatLibrary::GetAbilityBarAbilities(ASC, Handles);
		for (const FGameplayAbilitySpecHandle& Handle : Handles)
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			Names.Add(Spec && Spec->Ability ? Spec->Ability->GetClass()->GetName() : FString());
		}
		return Names;
	}

	// Particle systems still running on a character's animated mesh
	int32 ActiveEffectsOn(const ABeyondCharacterBase* Character)
	{
		int32 Count = 0;
		if (const USkeletalMeshComponent* Mesh = Character ? Character->GetCombatMesh() : nullptr)
		{
			TArray<USceneComponent*> Children;
			Mesh->GetChildrenComponents(true, Children);
			for (const USceneComponent* Child : Children)
			{
				Count += (Child && Child->IsA<UFXSystemComponent>() && Child->IsActive()) ? 1 : 0;
			}
		}
		return Count;
	}

	float Health(const AActor* Actor)
	{
		return UBeyondCombatLibrary::GetActorHealth(Actor);
	}

	bool TookDamage(const AActor* Actor, float Before, float AtLeast)
	{
		return UBeyondCombatLibrary::IsActorDead(Actor) || Before - Health(Actor) >= AtLeast;
	}

	// Point the player's camera at the middle of Target (run it twice: the shoulder camera moves as it turns)
	void LookAt(ABeyondPlayerController* PC, const AActor* Target)
	{
		if (!PC || !Target || !PC->PlayerCameraManager)
		{
			return;
		}
		FVector Origin, Extent;
		Target->GetActorBounds(true, Origin, Extent);
		PC->SetControlRotation((Origin - PC->PlayerCameraManager->GetCameraLocation()).Rotation());
	}

	bool IsCrosshairShown(const ABeyondPlayerController* PC)
	{
		const UUserWidget* Crosshair = PC ? PC->GetCrosshairWidget() : nullptr;
		return Crosshair && Crosshair->GetVisibility() == ESlateVisibility::HitTestInvisible;
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondPowersStep, TFunction<bool()>, Step);
bool FBeyondPowersStep::Update()
{
	return Step();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondPowersTest, "WorldsBeyond.Prototype.Powers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondPowersTest::RunTest(const FString& Parameters)
{
	using namespace BeyondPowersTest;
	TSharedRef<FState> State = MakeShared<FState>();
	State->Test = this;

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/SICKA_PERSEPOLIS/MAPS/MAP_Demo_Main")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.0f));

	// Setup and configuration checks
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
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
		Party->SwapCooldown = 0.0f;

		// The intro cutscene is still running this early and drives the demigods. The buddy must stand still meanwhile
		// (it used to walk after the leader the sequence moves); then skip to the end
		bool bIntroHasParty = false;
		for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
		{
			if (ULevelSequencePlayer* Player = It->GetSequencePlayer(); Player && Player->IsPlaying())
			{
				for (ABeyondCharacterBase* Member : Party->GetMembers())
				{
					bIntroHasParty |= !Player->GetObjectBindings(Member).IsEmpty();
				}
			}
		}
		if (bIntroHasParty)
		{
			const ABeyondCharacterBase* Buddy = Party->GetCompanion();
			const ABeyondCompanionController* BuddyAI = Buddy ? Cast<ABeyondCompanionController>(Buddy->GetController()) : nullptr;
			T.TestTrue(TEXT("Buddy holds still during the intro cutscene"),
				BuddyAI && BuddyAI->IsHoldingForCutscene() && Buddy->GetVelocity().Size2D() < 10.0f);
		}
		else
		{
			T.AddWarning(TEXT("The intro cutscene wasn't playing 4 s in; the buddy's cutscene hold wasn't checked"));
		}
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
		if (!T.TestNotNull(TEXT("Angel is the duo Conduit"), Angel) || !T.TestNotNull(TEXT("Ji-Woong is the duo Striker"), JiWoong))
		{
			return true;
		}

		// Angel leads, Ji-Woong follows
		if (Party->GetLeader() != Angel)
		{
			Party->SwapLeader();
		}
		T.TestTrue(TEXT("Angel leads"), Party->GetLeader() == Angel);

		for (ABeyondCharacterBase* Member : { Angel, JiWoong })
		{
			USkeletalMeshComponent* CombatMesh = Member->GetCombatMesh();
			T.TestTrue(*FString::Printf(TEXT("%s animates a mesh with an asset (Body)"), *Member->GetName()),
				CombatMesh && CombatMesh->GetSkeletalMeshAsset() && CombatMesh->GetAnimInstance());
			const FGameplayAbilityActorInfo* Info = Member->GetAbilitySystemComponent()->AbilityActorInfo.Get();
			T.TestTrue(*FString::Printf(TEXT("%s: GAS plays montages on the combat mesh"), *Member->GetName()),
				Info && Info->SkeletalMeshComponent.Get() == CombatMesh);
			T.TestTrue(*FString::Printf(TEXT("%s has the duo move"), *Member->GetName()), FindSpec(Member, TEXT("GA_Duo_HeavensJudgment")).IsValid());
		}

		T.TestTrue(TEXT("Companion path following accelerates (walk animation)"),
			JiWoong->GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths);

		T.TestTrue(TEXT("Angel: Lightning Strike"), FindSpec(Angel, TEXT("GA_Angel_LightningStrike")).IsValid());
		T.TestFalse(TEXT("Angel: old GA_AOEAttack gone"), FindSpec(Angel, TEXT("GA_AOEAttack")).IsValid());
		T.TestTrue(TEXT("Ji-Woong: Gilded Step"), FindSpec(JiWoong, TEXT("GA_JiWoong_GildedStep")).IsValid());
		T.TestTrue(TEXT("Ji-Woong: Sunbrand"), FindSpec(JiWoong, TEXT("GA_JiWoong_Sunbrand")).IsValid());
		T.TestTrue(TEXT("Ji-Woong: sword equip"), FindSpec(JiWoong, TEXT("GA_JiWoong_EquipWeapon")).IsValid());
		T.TestFalse(TEXT("Ji-Woong: Blink suppressed"), FindSpec(JiWoong, TEXT("GA_Blink")).IsValid());
		T.TestFalse(TEXT("Ji-Woong: old GA_Dash gone"), FindSpec(JiWoong, TEXT("GA_Dash_C")).IsValid());

		// The sword lives on Ji-Woong: on the hip with a relaxed idle, or in hand with the guard idle (enemies near)
		const UBeyondGA_EquipWeapon* Equip = GetEquipAbility(JiWoong);
		const AActor* Sword = Equip ? Equip->GetEquippedWeapon() : nullptr;
		T.TestTrue(TEXT("Ji-Woong spawns with his sword attached to the Body mesh"),
			Sword && Sword->GetRootComponent()->GetAttachParent() == JiWoong->GetCombatMesh() && !Sword->IsHidden());
		if (Equip && Sword)
		{
			T.TestTrue(*FString::Printf(TEXT("Idle matches the sword (%s, drawn=%d)"), *IdleName(JiWoong), Equip->IsWeaponDrawn()),
				IdleName(JiWoong) == (Equip->IsWeaponDrawn() ? TEXT("sword-idle") : TEXT("MM_Idle1")));
			T.TestTrue(TEXT("A holstered sword doesn't count for hit detection"),
				Equip->IsWeaponDrawn() == (ABeyondWeapon::FindEquippedWeapon(JiWoong) != nullptr));
		}

		// LMB runs the GAS sword combo for Ji-Woong; his old Blueprint LMB event (voice line on every press) is off
		const UInputAction* PrimaryAction = nullptr;
		for (const FBeyondInputBinding& Binding : JiWoong->GetAbilityInputBindings())
		{
			if (Binding.InputTag == BeyondTags::Ability_Input_Primary)
			{
				PrimaryAction = Binding.InputAction;
			}
		}
		bool bLeftMouseMapped = false;
		if (const UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default")))
		{
			for (const FEnhancedActionKeyMapping& Mapping : IMC->GetMappings())
			{
				bLeftMouseMapped |= PrimaryAction && Mapping.Action == PrimaryAction && Mapping.Key == EKeys::LeftMouseButton;
			}
		}
		T.TestTrue(TEXT("Ji-Woong: LMB presses the GAS sword combo"), bLeftMouseMapped && JiWoong->IsLegacyKeyInputDisabled());
		T.TestTrue(TEXT("Ji-Woong: only his old LMB key event is switched off (\"1\" still draws the sword)"),
			JiWoong->GetLegacyKeysToDisable().Num() == 1 && JiWoong->GetLegacyKeysToDisable()[0] == EKeys::LeftMouseButton);
		if (const UBeyondGA_MeleeCombo* Combo = GetSwordCombo(JiWoong))
		{
			T.TestTrue(TEXT("Sword combo window is 15 % longer"), FMath::IsNearlyEqual(Combo->ComboWindowExtension, 0.15f, 0.01f));
			T.TestTrue(TEXT("Sword combo has a voice line"), Combo->ComboVoiceLine != nullptr);
			T.TestTrue(TEXT("Sword combo swings on the upper body while moving"), Combo->bLegsFollowMovement);
		}

		// Angel's staff: on his back (or in hand if enemies are close already), the old Blueprint equip is gone
		T.TestTrue(TEXT("Angel: staff equip ability"), FindSpec(Angel, TEXT("GA_Angel_EquipStaff")).IsValid());
		T.TestFalse(TEXT("Angel: old Blueprint GA_EquipWeapon suppressed"), FindSpec(Angel, TEXT("GA_EquipWeapon_C")).IsValid());
		const UBeyondGA_EquipWeapon* AngelEquip = GetEquipAbility(Angel);
		const AActor* Staff = AngelEquip ? AngelEquip->GetEquippedWeapon() : nullptr;
		T.TestTrue(TEXT("Angel spawns with his staff attached to the Body mesh"),
			Staff && Staff->GetRootComponent()->GetAttachParent() == Angel->GetCombatMesh() && !Staff->IsHidden());
		if (AngelEquip && Staff)
		{
			T.TestTrue(*FString::Printf(TEXT("Angel's idle matches the staff (%s, drawn=%d)"), *IdleName(Angel), AngelEquip->IsWeaponDrawn()),
				IdleName(Angel) == (AngelEquip->IsWeaponDrawn() ? TEXT("UE5_WZ_Idle_Seq") : TEXT("MM_Idle")));
		}

		T.TestTrue(TEXT("Sword and staff glide between holster and hand (no pop)"),
			Equip && Equip->HandoffBlendTime > 0.0f && AngelEquip && AngelEquip->HandoffBlendTime > 0.0f);

		// Aiming: Angel has a crosshair / shoulder camera / hold-RMB aim, Ji-Woong doesn't
		T.TestTrue(TEXT("Angel aims (Aim Settings on, IA_Aim, aim component)"),
			Angel->GetAimSettings().bEnabled && Angel->GetAimSettings().AimAction && Angel->GetAimComponent());
		T.TestTrue(TEXT("Angel's casts turn him to the crosshair"), Angel->GetAimSettings().FaceAimMontages.Num() >= 2);
		T.TestTrue(TEXT("Ji-Woong has no crosshair"), !JiWoong->GetAimSettings().bEnabled && !JiWoong->GetAimComponent());
		bool bAimMapped = false;
		if (const UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default")))
		{
			for (const FEnhancedActionKeyMapping& Mapping : IMC->GetMappings())
			{
				bAimMapped |= Mapping.Action == Angel->GetAimSettings().AimAction && Mapping.Key == EKeys::RightMouseButton;
			}
		}
		T.TestTrue(TEXT("Right mouse button aims"), bAimMapped);

		// Ability bar: only the keys each demigod can press, in Q / E / R order
		T.TestEqual(TEXT("Angel's ability bar"), FString::Join(AbilityBar(Angel), TEXT(",")), FString(TEXT("GA_Blink_C,GA_Angel_LightningStrike_C,GA_HealSpell_C")));
		T.TestEqual(TEXT("Ji-Woong's ability bar"), FString::Join(AbilityBar(JiWoong), TEXT(",")), FString(TEXT("GA_JiWoong_GildedStep_C,GA_JiWoong_Sunbrand_C,GA_HealSpell_C")));

		// Heal: "2" presses the heal slot (no more per-frame IA_Heal), Angel's heal plays AM_Heal through the ability
		if (const UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default")))
		{
			bool bTwoIsHealSlot = false;
			bool bTwoIsOldHeal = false;
			for (const FEnhancedActionKeyMapping& Mapping : IMC->GetMappings())
			{
				if (Mapping.Key == EKeys::Two)
				{
					bTwoIsHealSlot |= GetNameSafe(Mapping.Action) == TEXT("IA_AbilityR");
					bTwoIsOldHeal |= GetNameSafe(Mapping.Action) == TEXT("IA_Heal");
				}
			}
			T.TestTrue(TEXT("Key 2 presses the heal slot"), bTwoIsHealSlot && !bTwoIsOldHeal);
		}
		bool bAngelHealMontage = false;
		for (const TPair<TSubclassOf<UGameplayAbility>, TObjectPtr<UAnimMontage>>& Entry : Angel->AbilityMontages)
		{
			bAngelHealMontage |= Entry.Key && Entry.Key->GetName() == TEXT("GA_HealSpell_C") && GetNameSafe(Entry.Value) == TEXT("AM_Heal");
		}
		T.TestTrue(TEXT("Angel's heal ability plays AM_Heal"), bAngelHealMontage);

		// Sword combo notifies
		if (const UAnimMontage* Combo = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Animations/Melee/Montage_SwordCombo.Montage_SwordCombo")))
		{
			int32 HitScanNotifies = 0;
			for (const FAnimNotifyEvent& Notify : Combo->Notifies)
			{
				HitScanNotifies += (Notify.Notify && Notify.Notify->GetClass()->GetName().StartsWith(TEXT("AN_HitScan"))) ? 1 : 0;
			}
			T.TestEqual(TEXT("Montage_SwordCombo hit-scan notifies"), HitScanNotifies, 6);
		}

		// Enemies: perception now detects the (hostile) demigods
		for (TActorIterator<ABeyondCharacterBase> It(World); It; ++It)
		{
			if (It->TeamAffiliation != EBeyondTeam::Enemy || UBeyondCombatLibrary::IsActorDead(*It))
			{
				continue;
			}
			if (It->BossBarWidgetClass)
			{
				State->Boss = *It;
			}
			else
			{
				State->Enemies.Add(*It);
			}
		}
		T.TestTrue(TEXT("At least 3 regular enemies in the map"), State->Enemies.Num() >= 3);
		T.TestTrue(TEXT("A boss with a boss bar"), State->Boss.IsValid());

		if (State->Enemies.Num() > 0)
		{
			const AController* EnemyController = State->Enemies[0]->GetController();
			const UAIPerceptionComponent* Perception = EnemyController ? EnemyController->FindComponentByClass<UAIPerceptionComponent>() : nullptr;
			bool bSightDetectsEnemies = false;
			if (Perception)
			{
				for (auto It = Perception->GetSensesConfigIterator(); It; ++It)
				{
					if (const UAISenseConfig_Sight* Sight = Cast<UAISenseConfig_Sight>(*It))
					{
						bSightDetectsEnemies = Sight->DetectionByAffiliation.bDetectEnemies;
					}
				}
			}
			T.TestTrue(TEXT("Enemy sight detects hostile teams (enemies aggro on the demigods again)"), bSightDetectsEnemies);
		}

		// From here on, enemies stand still: their attacks would interrupt the abilities under test
		for (TActorIterator<ABeyondCharacterBase> It(World); It; ++It)
		{
			if (It->TeamAffiliation == EBeyondTeam::Enemy)
			{
				PlaceEnemy(*It, *It, 0.0f);
			}
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		const ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		const ABeyondCompanionController* BuddyAI = JiWoong ? Cast<ABeyondCompanionController>(JiWoong->GetController()) : nullptr;
		State->Test->TestTrue(TEXT("Buddy moves on once the cutscene is over"), BuddyAI && !BuddyAI->IsHoldingForCutscene());
		return true;
	}));

	// Gilded Step: Ji-Woong dashes through an enemy
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!JiWoong || State->Enemies.Num() < 3)
		{
			return true;
		}
		ABeyondCharacterBase* Enemy = State->Enemies[0].Get();
		AAIController* Tester = TakeOver(*State, JiWoong);
		TArray<FVector> Spots = { JiWoong->GetActorLocation() };
		if (const ABeyondCharacterBase* Angel = State->Angel.Get())
		{
			Spots.Add(Angel->GetActorLocation());
		}
		for (const TWeakObjectPtr<ABeyondCharacterBase>& Other : State->Enemies)
		{
			if (Other.IsValid())
			{
				Spots.Add(Other->GetActorLocation());
			}
		}
		T.TestTrue(TEXT("Found open ground for the dash"), MoveToOpenGround(JiWoong, Spots, 900.0f));
		PlaceEnemy(Enemy, JiWoong, 350.0f);
		Tester->SetFocus(Enemy, EAIFocusPriority::Gameplay);

		State->StartLocation = JiWoong->GetActorLocation();
		State->Health.Add(TEXT("Dash"), Health(Enemy));
		T.TestTrue(TEXT("Gilded Step activates"), JiWoong->GetAbilitySystemComponent()->TryActivateAbility(FindSpec(JiWoong, TEXT("GA_JiWoong_GildedStep"))));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!JiWoong || State->Enemies.Num() < 3)
		{
			return true;
		}
		const ABeyondCharacterBase* Enemy = State->Enemies[0].Get();
		T.TestTrue(TEXT("Gilded Step moved Ji-Woong"), FVector::Dist2D(JiWoong->GetActorLocation(), State->StartLocation) > 300.0f);
		T.TestTrue(TEXT("Gilded Step hit the enemy it crossed"), TookDamage(Enemy, State->Health.FindRef(TEXT("Dash")), 29.0f));
		T.TestFalse(TEXT("Dash invincibility is gone again"), HasTag(JiWoong, BeyondTags::State_Invincible));
		return true;
	}));

	// Sunbrand: brand an enemy, then a melee hit from Ji-Woong detonates it
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		AAIController* Tester = State->Tester.Get();
		if (!JiWoong || !Tester || State->Enemies.Num() < 3)
		{
			return true;
		}
		ABeyondCharacterBase* Enemy = State->Enemies[1].Get();
		PlaceEnemy(Enemy, JiWoong, 600.0f);
		Tester->SetFocus(Enemy, EAIFocusPriority::Gameplay);
		T.TestTrue(TEXT("Sunbrand activates"), JiWoong->GetAbilitySystemComponent()->TryActivateAbility(FindSpec(JiWoong, TEXT("GA_JiWoong_Sunbrand"))));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!JiWoong || State->Enemies.Num() < 3)
		{
			return true;
		}
		ABeyondCharacterBase* Enemy = State->Enemies[1].Get();
		UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(JiWoong);
		T.TestTrue(TEXT("Sunbrand branded the enemy"), Combat && Combat->IsBranded(Enemy) && HasTag(Enemy, BeyondTags::State_Branded));

		State->Health.Add(TEXT("Brand"), Health(Enemy));
		UBeyondCombatLibrary::ApplyDamage(JiWoong, Enemy, 10.0f, BeyondTags::DamageType_Melee, BeyondTags::Event_Hit_Light);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!JiWoong || State->Enemies.Num() < 3)
		{
			return true;
		}
		const ABeyondCharacterBase* Enemy = State->Enemies[1].Get();
		const UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(JiWoong);
		T.TestFalse(TEXT("Melee hit consumed the brand"), Combat && Combat->IsBranded(Enemy));
		// 10 x 1.2 amplified + 40 detonation
		T.TestTrue(TEXT("Brand amplified the hit and detonated"), TookDamage(Enemy, State->Health.FindRef(TEXT("Brand")), 51.0f));

		// Sword on the hip, automatic draw / sheathe off so the test drives it
		if (UBeyondGA_EquipWeapon* Equip = GetEquipAbility(JiWoong))
		{
			Equip->AutoDrawRadius = 0.0f;
			Equip->AutoSheatheDelay = 0.0f;
			Equip->RequestWeaponAction(BeyondTags::Weapon_Action_Sheathe, true);
			T.TestTrue(TEXT("Instant sheathe puts the sword on the hip"), IsHolstered(Equip) && IdleName(JiWoong) == TEXT("MM_Idle1"));
		}

		// Buddy sword combo plays on the Body mesh, and starting it quick-draws the sword
		ABeyondCharacterBase* Target = State->Enemies[2].Get();
		PlaceEnemy(Target, JiWoong, 150.0f);
		if (AAIController* Tester = State->Tester.Get())
		{
			Tester->SetFocus(Target, EAIFocusPriority::Gameplay);
		}
		const UBeyondGA_MeleeCombo* Combo = GetSwordCombo(JiWoong);
		State->VoiceLinesBefore = Combo ? Combo->GetVoiceLinesPlayed() : 0;
		T.TestTrue(TEXT("Sword combo activates"), JiWoong->GetAbilitySystemComponent()->TryActivateAbility(FindSpec(JiWoong, TEXT("GA_JiWoong_SwordCombo"))));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!JiWoong)
		{
			return true;
		}
		const UAnimInstance* Anim = JiWoong->GetCombatMesh() ? JiWoong->GetCombatMesh()->GetAnimInstance() : nullptr;
		T.TestTrue(TEXT("Buddy sword combo montage plays on the Body mesh"), Anim && Anim->IsAnyMontagePlaying());

		const UBeyondGA_EquipWeapon* Equip = GetEquipAbility(JiWoong);
		T.TestTrue(TEXT("Starting the sword combo quick-drew the sword"),
			Equip && Equip->IsWeaponDrawn() && ABeyondWeapon::FindEquippedWeapon(JiWoong) && IdleName(JiWoong) == TEXT("sword-idle"));
		T.TestTrue(*FString::Printf(TEXT("Standing still, the combo keeps its footwork (full-body weight %.2f)"), SlotWeight(JiWoong, TEXT("DefaultSlot"))),
			SlotWeight(JiWoong, TEXT("DefaultSlot")) > 0.5f);

		// The Blueprint combo ends a missed combo window with Stop Anim Montage (None): it has to reach Body
		JiWoong->StopAnimMontage(nullptr);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.6f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		UBeyondGA_EquipWeapon* Equip = GetEquipAbility(JiWoong);
		if (!JiWoong || !Equip)
		{
			return true;
		}
		const UAnimInstance* Anim = JiWoong->GetCombatMesh() ? JiWoong->GetCombatMesh()->GetAnimInstance() : nullptr;
		T.TestFalse(TEXT("Stop Anim Montage stopped the combo on the Body mesh"), Anim && Anim->IsAnyMontagePlaying());
		if (const UBeyondGA_MeleeCombo* Combo = GetSwordCombo(JiWoong))
		{
			T.TestEqual(TEXT("The combo's voice line played once"), Combo->GetVoiceLinesPlayed(), State->VoiceLinesBefore + 1);
		}

		Equip->RequestWeaponAction(BeyondTags::Weapon_Action_Sheathe, false);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.8f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		UBeyondGA_EquipWeapon* Equip = GetEquipAbility(JiWoong);
		if (!Equip)
		{
			return true;
		}
		T.TestTrue(TEXT("Animated sheathe returns the sword to the hip and relaxes the idle"), IsHolstered(Equip) && IdleName(JiWoong) == TEXT("MM_Idle1"));

		// Enemies (frozen, but present) count as close with a huge radius: he draws by himself
		Equip->AutoDrawRadius = 100000.0f;
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.6f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		UBeyondGA_EquipWeapon* Equip = GetEquipAbility(JiWoong);
		if (!Equip)
		{
			return true;
		}
		T.TestTrue(TEXT("Auto draw when enemies are close"), Equip->IsWeaponDrawn() && IdleName(JiWoong) == TEXT("sword-idle"));

		// Ignore enemies and calm down quickly: he puts it away by himself
		Equip->AutoDrawRadius = 0.0f;
		Equip->AutoSheatheRadius = 0.0f;
		Equip->AutoSheatheDelay = 0.5f;
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		UBeyondGA_EquipWeapon* Equip = GetEquipAbility(JiWoong);
		if (!JiWoong || !Equip)
		{
			return true;
		}
		T.TestTrue(TEXT("Auto sheathe after a quiet spell"), IsHolstered(Equip) && IdleName(JiWoong) == TEXT("MM_Idle1"));

		const UBeyondGA_EquipWeapon* Defaults = GetDefault<UBeyondGA_EquipWeapon>(Equip->GetClass());
		Equip->AutoDrawRadius = Defaults->AutoDrawRadius;
		Equip->AutoSheatheRadius = Defaults->AutoSheatheRadius;
		Equip->AutoSheatheDelay = Defaults->AutoSheatheDelay;

		// Sword combo on the move: start walking down a clear lane
		TArray<FVector> Spots = { JiWoong->GetActorLocation() };
		if (const ABeyondCharacterBase* Angel = State->Angel.Get())
		{
			Spots.Add(Angel->GetActorLocation());
		}
		for (const TWeakObjectPtr<ABeyondCharacterBase>& Enemy : State->Enemies)
		{
			if (Enemy.IsValid())
			{
				Spots.Add(Enemy->GetActorLocation());
			}
		}
		T.TestTrue(TEXT("Found open ground to walk"), MoveToOpenGround(JiWoong, Spots, 900.0f));
		if (AAIController* Tester = State->Tester.Get())
		{
			Tester->ClearFocus(EAIFocusPriority::Gameplay);
			Tester->MoveToLocation(JiWoong->GetActorLocation() + JiWoong->GetActorForwardVector() * 850.0f, 10.0f, false, false);
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!JiWoong)
		{
			return true;
		}
		T.TestTrue(*FString::Printf(TEXT("Ji-Woong is walking (%.0f cm/s)"), JiWoong->GetVelocity().Size2D()), JiWoong->GetVelocity().Size2D() > 100.0f);
		T.TestTrue(TEXT("Sword combo activates on the move"), JiWoong->GetAbilitySystemComponent()->TryActivateAbility(FindSpec(JiWoong, TEXT("GA_JiWoong_SwordCombo"))));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.4f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!JiWoong)
		{
			return true;
		}
		const float Upper = SlotWeight(JiWoong, TEXT("UpperBody"));
		const float Full = SlotWeight(JiWoong, TEXT("DefaultSlot"));
		T.TestTrue(*FString::Printf(TEXT("Moving, the combo swings on the upper body and the legs keep walking (upper %.2f, full body %.2f)"), Upper, Full),
			Upper > 0.5f && Full < 0.5f);

		if (AAIController* Tester = State->Tester.Get())
		{
			Tester->StopMovement();
		}
		JiWoong->StopAnimMontage(nullptr);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	// One click, no further presses (like the player): the combo ends after the first swing, not after all three
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		UBeyondGA_MeleeCombo* Combo = GetSwordCombo(JiWoong);
		if (!JiWoong || !Combo)
		{
			return true;
		}
		Combo->bAIChainsCombo = false;
		T.TestTrue(TEXT("Sword combo activates for a single swing"), JiWoong->GetAbilitySystemComponent()->TryActivateAbility(FindSpec(JiWoong, TEXT("GA_JiWoong_SwordCombo"))));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.8f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		UBeyondGA_MeleeCombo* Combo = GetSwordCombo(JiWoong);
		if (!JiWoong || !Combo)
		{
			return true;
		}
		const UAnimInstance* Anim = JiWoong->GetCombatMesh() ? JiWoong->GetCombatMesh()->GetAnimInstance() : nullptr;
		T.TestFalse(TEXT("Without another press the combo stops after the first swing (the montage holds three)"),
			Combo->IsActive() || (Anim && Anim->IsAnyMontagePlaying()));
		Combo->bAIChainsCombo = true;
		JiWoong->StopAnimMontage(nullptr);
		return true;
	}));
	// Angel's aiming (he leads): staff out -> crosshair and shoulder camera; the enemy under it is the target
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondPlayerController* PC = State->PC.Get();
		if (!Angel || !PC || !T.TestTrue(TEXT("Angel leads for the aiming checks"), PC->GetPawn() == Angel))
		{
			return true;
		}

		TArray<FVector> Spots = { Angel->GetActorLocation() };
		if (const ABeyondCharacterBase* JiWoong = State->JiWoong.Get())
		{
			Spots.Add(JiWoong->GetActorLocation());
		}
		T.TestTrue(TEXT("Found open ground to aim along"), MoveToOpenGround(Angel, Spots, 900.0f));
		if (UBeyondGA_EquipWeapon* Staff = GetEquipAbility(Angel))
		{
			Staff->RequestWeaponAction(BeyondTags::Weapon_Action_Draw, true);
		}

		for (TActorIterator<ABeyondCharacterBase> It(Angel->GetWorld()); It; ++It)
		{
			if (It->TeamAffiliation == EBeyondTeam::Enemy && !It->BossBarWidgetClass && !UBeyondCombatLibrary::IsActorDead(*It))
			{
				State->AimEnemy = *It;
				break;
			}
		}
		PlaceEnemy(State->AimEnemy.Get(), Angel, 700.0f);
		PC->SetControlRotation(FRotator(-5.0f, Angel->GetActorRotation().Yaw, 0.0f));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondPlayerController* PC = State->PC.Get();
		const UBeyondAimComponent* Aim = Angel ? Angel->GetAimComponent() : nullptr;
		if (!Aim || !PC || PC->GetPawn() != Angel)
		{
			return true;
		}
		T.TestTrue(TEXT("Staff out: the crosshair shows"), Aim->IsCombatReady() && IsCrosshairShown(PC));
		T.TestTrue(*FString::Printf(TEXT("Staff out: the camera moved over the right shoulder (offset Y %.0f)"), Aim->GetAppliedCameraOffset().Y),
			Aim->GetAppliedCameraOffset().Y > Angel->GetAimSettings().ReadyCameraOffset.Y * 0.5f);
		LookAt(PC, State->AimEnemy.Get());
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		LookAt(State->PC.Get(), State->AimEnemy.Get());
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondPlayerController* PC = State->PC.Get();
		const UBeyondAimComponent* Aim = Angel ? Angel->GetAimComponent() : nullptr;
		ABeyondCharacterBase* Enemy = State->AimEnemy.Get();
		if (!Aim || !PC || !Enemy || PC->GetPawn() != Angel)
		{
			return true;
		}
		T.TestTrue(*FString::Printf(TEXT("The enemy under the crosshair is the aim target (%s)"), *GetNameSafe(Aim->GetAimTarget())), Aim->GetAimTarget() == Enemy);
		const UBeyondCrosshairWidget* Crosshair = Cast<UBeyondCrosshairWidget>(PC->GetCrosshairWidget());
		T.TestTrue(TEXT("The crosshair turns purple over the enemy"), Crosshair && Crosshair->IsOverHostile());
		if (UMaterialInterface* Highlight = Angel->GetAimSettings().AimHighlightMaterial)
		{
			T.TestTrue(TEXT("The aimed-at enemy glows"), Enemy->GetCombatMesh() && Enemy->GetCombatMesh()->GetOverlayMaterial() == Highlight);
		}

		// Look 90 degrees to his right and cast: he turns to the crosshair
		State->AimYaw = FRotator::NormalizeAxis(Angel->GetActorRotation().Yaw + 90.0f);
		PC->SetControlRotation(FRotator(-5.0f, State->AimYaw, 0.0f));
		UAnimMontage* CastMontage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/EssentialAnimation/MagicStaff/Animation/UE5/Sequence/Attack/UE5_WZ_Attack_02_Seq_Montage.UE5_WZ_Attack_02_Seq_Montage"));
		UAnimInstance* Anim = Angel->GetCombatMesh() ? Angel->GetCombatMesh()->GetAnimInstance() : nullptr;
		T.TestTrue(TEXT("Angel's cast montage plays"), CastMontage && Anim && Anim->Montage_Play(CastMontage) > 0.0f);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.4f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondPlayerController* PC = State->PC.Get();
		if (!Angel || !PC || PC->GetPawn() != Angel)
		{
			return true;
		}
		const float Off = FMath::Abs(FMath::FindDeltaAngleDegrees(Angel->GetActorRotation().Yaw, State->AimYaw));
		T.TestTrue(*FString::Printf(TEXT("Casting turned Angel to the crosshair (%.0f degrees off)"), Off), Off < 10.0f);
		Angel->StopAnimMontage(nullptr);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!JiWoong)
		{
			return true;
		}

		GiveBack(*State, JiWoong);
		T.TestTrue(TEXT("Ji-Woong back with his companion controller"), JiWoong->GetController() && JiWoong->GetController()->IsA<ABeyondCompanionController>());

		// Swap: Ji-Woong leads, the HUD is rebuilt and the ability bar is told to refill
		ABeyondPlayerController* PC = State->PC.Get();
		if (UAbilitySystemComponent* ASC = JiWoong->GetAbilitySystemComponent())
		{
			ASC->GenericGameplayEventCallbacks.FindOrAdd(BeyondTags::Event_Abilities_Changed).AddLambda([State](const FGameplayEventData*)
			{
				++State->AbilitiesChangedEvents;
			});
		}
		if (PC)
		{
			PC->PartyComponent->SwapLeader();
			T.TestTrue(TEXT("Ji-Woong leads after the swap"), PC->PartyComponent->GetLeader() == JiWoong);
			T.TestTrue(TEXT("Swap refreshed the ability bar (Event.Abilities.Changed)"), State->AbilitiesChangedEvents > 0);

			// His Blueprint "1" (draw / sheathe) is still bound; the old LMB combo event is not
			bool bOneBound = false;
			bool bLeftMouseBound = false;
			if (const UInputComponent* Input = JiWoong->InputComponent)
			{
				for (const FInputKeyBinding& Binding : Input->KeyBindings)
				{
					bOneBound |= Binding.Chord.Key == EKeys::One;
					bLeftMouseBound |= Binding.Chord.Key == EKeys::LeftMouseButton;
				}
			}
			T.TestTrue(TEXT("Controlling Ji-Woong, \"1\" is bound and the old LMB event isn't"), bOneBound && !bLeftMouseBound);
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	// Angel's Lightning Strike (AI path) damages the enemy it targets
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		if (!Angel || State->Enemies.Num() < 3)
		{
			return true;
		}
		// An enemy still standing, preferably one the earlier steps didn't hit (hit enemies may raise their guard)
		ABeyondCharacterBase* Enemy = nullptr;
		for (TActorIterator<ABeyondCharacterBase> It(Angel->GetWorld()); It; ++It)
		{
			if (It->TeamAffiliation != EBeyondTeam::Enemy || It->BossBarWidgetClass || UBeyondCombatLibrary::IsActorDead(*It))
			{
				continue;
			}
			const bool bUsedEarlier = State->Enemies.Num() >= 3 && (State->Enemies[0] == *It || State->Enemies[1] == *It || State->Enemies[2] == *It);
			if (!Enemy || !bUsedEarlier)
			{
				Enemy = *It;
			}
			if (!bUsedEarlier)
			{
				break;
			}
		}
		if (!T.TestNotNull(TEXT("An enemy left for Lightning Strike"), Enemy))
		{
			return true;
		}
		State->StrikeTarget = Enemy;

		// As a buddy she may already be casting (or have used the strike herself): start clean
		AAIController* Tester = TakeOver(*State, Angel);
		UAbilitySystemComponent* ASC = Angel->GetAbilitySystemComponent();
		ASC->CancelAllAbilities();
		ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(FGameplayTag::RequestGameplayTag(TEXT("Cooldown")))));

		PlaceEnemy(Enemy, Angel, 700.0f);
		Tester->SetFocus(Enemy, EAIFocusPriority::Gameplay);
		State->Health.Add(TEXT("Strike"), Health(Enemy));

		// E: crystal spikes instead of the yellow lightning
		const FGameplayAbilitySpec* StrikeSpec = ASC->FindAbilitySpecFromHandle(FindSpec(Angel, TEXT("GA_Angel_LightningStrike")));
		const UBeyondGA_GroundStrike* Strike = StrikeSpec ? Cast<UBeyondGA_GroundStrike>(StrikeSpec->Ability) : nullptr;
		T.TestTrue(TEXT("Angel's E bursts spikes, without the yellow lightning cue, and has a purple reticle"),
			Strike && Strike->SpikeBurstClass && !Strike->StrikeCueTag.IsValid() && Strike->bOverrideTargetDecalColor);
		State->SpikeBurst.Reset();
		State->SpawnHandle = Angel->GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([State](AActor* Spawned)
		{
			if (ABeyondSpikeBurst* Burst = Cast<ABeyondSpikeBurst>(Spawned))
			{
				State->SpikeBurst = Burst;
			}
		}));

		T.TestTrue(TEXT("Lightning Strike activates"), Angel->GetAbilitySystemComponent()->TryActivateAbility(FindSpec(Angel, TEXT("GA_Angel_LightningStrike"))));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		if (!Angel || State->Enemies.Num() < 3)
		{
			return true;
		}
		ABeyondSpikeBurst* Burst = State->SpikeBurst.Get();
		for (TActorIterator<ABeyondSpikeBurst> It(Angel->GetWorld()); It && !Burst; ++It)
		{
			Burst = *It;
		}
		if (!T.TestNotNull(TEXT("Angel's E burst crystal spikes out of the ground"), Burst))
		{
			return true;
		}
		State->SpikeBurst = Burst;
		T.TestTrue(*FString::Printf(TEXT("A full ring of spikes (%d)"), Burst->GetSpikeCount()), Burst->GetSpikeCount() >= 10);
		bool bBlueOrPurple = Burst->GetSpikeCount() > 0;
		for (int32 Index = 0; Index < Burst->GetSpikeCount(); ++Index)
		{
			const FLinearColor Color = Burst->GetSpikeColor(Index);
			bBlueOrPurple &= Color.B > Color.R && Color.B > Color.G;
		}
		T.TestTrue(TEXT("Every spike is blue or purple (never yellow)"), bBlueOrPurple);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		if (!Angel || State->Enemies.Num() < 3)
		{
			return true;
		}
		const ABeyondCharacterBase* Target = State->StrikeTarget.Get();
		T.TestTrue(*FString::Printf(TEXT("Lightning Strike damaged the enemy (Angel's E): %s %.0f -> %.0f"), *GetNameSafe(Target), State->Health.FindRef(TEXT("Strike")), Health(Target)),
			TookDamage(Target, State->Health.FindRef(TEXT("Strike")), 99.0f));
		Angel->GetWorld()->RemoveOnActorSpawnedHandler(State->SpawnHandle);
		T.TestTrue(TEXT("The spikes sink back and clean up"), !State->SpikeBurst.IsValid() || State->SpikeBurst->IsActorBeingDestroyed());
		const UBeyondGA_EquipWeapon* Staff = GetEquipAbility(Angel);
		T.TestTrue(TEXT("Casting near enemies, Angel has the staff in hand and its idle"),
			Staff && Staff->IsWeaponDrawn() && IdleName(Angel) == TEXT("UE5_WZ_Idle_Seq"));

		// Heal slot: the ability plays Angel's AM_Heal (animation + voice) once
		UAbilitySystemComponent* AngelASC = Angel->GetAbilitySystemComponent();
		AngelASC->CancelAllAbilities();
		AngelASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(FGameplayTag::RequestGameplayTag(TEXT("Cooldown")))));
		T.TestTrue(TEXT("Heal slot activates"), Angel->TryActivateAbilityByInputTag(BeyondTags::Ability_Input_R));
		const UAnimInstance* AngelAnim = Angel->GetCombatMesh() ? Angel->GetCombatMesh()->GetAnimInstance() : nullptr;
		const UAnimMontage* CurrentMontage = AngelAnim ? AngelAnim->GetCurrentActiveMontage() : nullptr;
		T.TestEqual(TEXT("Heal plays AM_Heal on Angel"), GetNameSafe(CurrentMontage), FString(TEXT("AM_Heal")));

		GiveBack(*State, Angel);
		return true;
	}));

	// Bond meter fills from damage; a full meter fires Heaven's Judgment
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		ABeyondCharacterBase* Angel = State->Angel.Get();
		if (!PC || !JiWoong || !Angel)
		{
			return true;
		}
		UBeyondPartyComponent* Party = PC->PartyComponent;

		// Fresh targets for the duo: every living regular enemy, gathered around Ji-Woong
		TArray<ABeyondCharacterBase*> Targets;
		for (TActorIterator<ABeyondCharacterBase> It(JiWoong->GetWorld()); It; ++It)
		{
			if (It->TeamAffiliation == EBeyondTeam::Enemy && !It->BossBarWidgetClass && !UBeyondCombatLibrary::IsActorDead(*It) && Targets.Num() < 3)
			{
				Targets.Add(*It);
			}
		}
		if (!T.TestTrue(TEXT("Enemies left for the duo move"), Targets.Num() > 0))
		{
			return true;
		}

		const float BondBefore = Party->GetBond();
		UBeyondCombatLibrary::ApplyDamage(JiWoong, Targets[0], 20.0f, BeyondTags::DamageType_Melee, BeyondTags::Event_Hit_Light);
		T.TestTrue(TEXT("Damage dealt fills the Bond meter"), Party->GetBond() > BondBefore);

		T.TestFalse(TEXT("Duo needs a full Bond meter"), JiWoong->TryActivateAbilityByInputTag(BeyondTags::Ability_Input_Duo));

		Angel->SetActorLocation(JiWoong->GetActorLocation() - JiWoong->GetActorForwardVector() * 300.0f, false, nullptr, ETeleportType::TeleportPhysics);
		State->Health.Reset();
		for (int32 i = 0; i < Targets.Num(); ++i)
		{
			PlaceEnemy(Targets[i], JiWoong, 250.0f + 120.0f * i, (i - 1) * 150.0f);
			State->Health.Add(Targets[i]->GetName(), Health(Targets[i]));
		}

		Party->AddBond(Party->MaxBond);
		T.TestTrue(TEXT("Bond full"), Party->IsBondFull());
		T.TestTrue(TEXT("Heaven's Judgment fires on G"), JiWoong->TryActivateAbilityByInputTag(BeyondTags::Ability_Input_Duo));
		T.TestEqual(TEXT("Duo spent the Bond meter"), Party->GetBond(), 0.0f);
		T.TestTrue(TEXT("Both demigods are in the duo state"), HasTag(JiWoong, BeyondTags::State_Duo) && HasTag(Angel, BeyondTags::State_Duo));
		T.TestTrue(TEXT("Both demigods are invincible during the duo"), HasTag(JiWoong, BeyondTags::State_Invincible) && HasTag(Angel, BeyondTags::State_Invincible));
		T.TestFalse(TEXT("No swapping in the middle of the duo"), Party->SwapLeader());
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		ABeyondCharacterBase* Angel = State->Angel.Get();
		if (!JiWoong || !Angel)
		{
			return true;
		}
		for (TActorIterator<ABeyondCharacterBase> It(JiWoong->GetWorld()); It; ++It)
		{
			if (const float* Before = State->Health.Find(It->GetName()))
			{
				T.TestTrue(*FString::Printf(TEXT("Heaven's Judgment hit %s"), *It->GetName()), TookDamage(*It, *Before, 80.0f));
			}
		}
		T.TestFalse(TEXT("Duo state cleared on Ji-Woong"), HasTag(JiWoong, BeyondTags::State_Duo) || HasTag(JiWoong, BeyondTags::State_Invincible));
		T.TestFalse(TEXT("Duo state cleared on Angel"), HasTag(Angel, BeyondTags::State_Duo) || HasTag(Angel, BeyondTags::State_Invincible));
		T.TestEqual(TEXT("No effects left running on Angel after the duo"), ActiveEffectsOn(Angel), 0);
		return true;
	}));

	// Boss bar: appears near the boss, follows GAS health, goes away when far
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondCharacterBase* Boss = State->Boss.Get();
		if (!PC || !Boss)
		{
			return true;
		}
		PlaceEnemy(Boss, Boss, 0.0f);
		ABeyondCharacterBase* Leader = PC->PartyComponent->GetLeader();
		Leader->SetActorLocation(Boss->GetActorLocation() + Boss->GetActorForwardVector() * 900.0f, false, nullptr, ETeleportType::TeleportPhysics);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondCharacterBase* Boss = State->Boss.Get();
		if (!PC || !Boss)
		{
			return true;
		}
		T.TestTrue(TEXT("Boss bar shows near the boss"), PC->GetBossBarWidget() != nullptr && PC->GetShownBoss() == Boss);
		UBeyondCombatLibrary::ApplyDamage(PC->PartyComponent->GetLeader(), Boss, 10.0f, BeyondTags::DamageType_Melee, BeyondTags::Event_Hit_Light);

		ABeyondCharacterBase* Leader = PC->PartyComponent->GetLeader();
		Leader->SetActorLocation(Boss->GetActorLocation() + Boss->GetActorForwardVector() * (Boss->BossBarShowRadius * 2.0f), false, nullptr, ETeleportType::TeleportPhysics);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.8f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondPowersStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		if (ABeyondPlayerController* PC = State->PC.Get())
		{
			T.TestTrue(TEXT("Boss bar hides when the party walks away"), PC->GetBossBarWidget() == nullptr);
		}
		if (AAIController* Tester = State->Tester.Get())
		{
			Tester->Destroy();
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
