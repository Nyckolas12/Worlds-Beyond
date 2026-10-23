// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "AI/BeyondEnemyController.h"
#include "BeyondGameplayTags.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "Editor.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Enemies/BeyondEnemySettings.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "EngineUtils.h"
#include "Game/BeyondCombatSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Items/BeyondItemLibrary.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/AutomationTest.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Progression/BeyondSkillTreeComponent.h"
#include "Progression/BeyondStatEffects.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "UI/BeyondMenuPanels.h"

/**
 * Plan 6, the new combat stats in MAP_Demo_Main (Play In Editor): every stat has a SetByCaller, an attribute and a menu
 * name; Beyond.CritMode 2 crits melee hits for 1 + Critical Damage and tags them, 0 never crits, procs never crit;
 * Angel's spells carry Storm and his staff swings nothing, a forest wolf's bites carry Verdant, a lightning proc Storm;
 * Elemental Mastery boosts his own element; Verdant affinity hits forest foes harder and resists their bites; percent
 * stats print with %. Skill node passives (a test tree swapped onto Angel): an Element Damage node raises his storm
 * spells, a real critical hit sets off an On Crit buff once per internal cooldown, a storm hit (not a frost one) cuts
 * running cooldowns, a kill heals. Saving is off for the run.
 */
namespace BeyondCombatStatsTest
{
	struct FState
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ABeyondCharacterBase> Angel;
		TWeakObjectPtr<ABeyondEnemyCharacter> Wolf;
		FString SavedSaveProgress;
		FString SavedCritMode;
		// The passives' test tree, and Angel's own to put back
		TStrongObjectPtr<UBeyondSkillTreeAsset> TestTree;
		TWeakObjectPtr<UBeyondSkillTreeAsset> OldTree;
		TMap<FName, int32> OldRanks;
		float BaseStrength = 0.0f;
	};

	UWorld* GetPlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	void SetCritMode(int32 Mode)
	{
		if (IConsoleVariable* CritMode = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.CritMode")))
		{
			CritMode->Set(Mode, ECVF_SetByCode);
		}
	}

	FBeyondSkillEffect MakePassive(EBeyondPassiveTrigger Trigger, EBeyondPassiveAction Action, float Value)
	{
		FBeyondSkillEffect Effect;
		Effect.Type = EBeyondSkillEffectType::Passive;
		Effect.Trigger = Trigger;
		Effect.Action = Action;
		Effect.Value = Value;
		return Effect;
	}

	// Storm's Embrace (+10 % storm damage), Raging Momentum (on a crit +5 Attack Power for 6 s, once per 30 s),
	// Elemental Resonance (a storm hit: cooldowns 1 s shorter), Feast (a kill heals 10 %); values per rank, 2 ranks each
	UBeyondSkillTreeAsset* MakePassiveTree()
	{
		UBeyondSkillTreeAsset* Tree = NewObject<UBeyondSkillTreeAsset>(GetTransientPackage());
		auto AddNode = [Tree](FName Id, const FBeyondSkillEffect& Effect)
		{
			FBeyondSkillNode& Node = Tree->Nodes.AddDefaulted_GetRef();
			Node.Id = Id;
			Node.MaxRank = 2;
			Node.Effects.Add(Effect);
		};
		FBeyondSkillEffect StormDamage;
		StormDamage.Type = EBeyondSkillEffectType::ElementDamage;
		StormDamage.Element = EBeyondElement::Storm;
		StormDamage.Value = 0.1f;
		AddNode(TEXT("StormsEmbrace"), StormDamage);
		FBeyondSkillEffect Momentum = MakePassive(EBeyondPassiveTrigger::OnCrit, EBeyondPassiveAction::BuffStat, 5.0f);
		Momentum.Stat = EBeyondSkillStat::Strength;
		Momentum.Duration = 6.0f;
		Momentum.InternalCooldown = 30.0f;
		AddNode(TEXT("RagingMomentum"), Momentum);
		FBeyondSkillEffect Resonance = MakePassive(EBeyondPassiveTrigger::OnElementalHit, EBeyondPassiveAction::ReduceCooldowns, 1.0f);
		Resonance.Element = EBeyondElement::Storm;
		AddNode(TEXT("ElementalResonance"), Resonance);
		AddNode(TEXT("Feast"), MakePassive(EBeyondPassiveTrigger::OnKill, EBeyondPassiveAction::Heal, 10.0f));
		return Tree;
	}

	TMap<FName, int32> AllRanks(const UBeyondSkillTreeAsset* Tree, FName Except = NAME_None)
	{
		TMap<FName, int32> Ranks;
		for (const FBeyondSkillNode& Node : Tree->Nodes)
		{
			if (Node.Id != Except)
			{
				Ranks.Add(Node.Id, Node.MaxRank);
			}
		}
		return Ranks;
	}

	int32 CountTimedStats(UAbilitySystemComponent* ASC)
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = UBeyondGE_TimedStats::StaticClass();
		return ASC->GetActiveEffects(Query).Num();
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondCombatStatsStep, TFunction<bool()>, Step);
bool FBeyondCombatStatsStep::Update()
{
	return Step();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondCombatStatsTest, "WorldsBeyond.Prototype.CombatStats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondCombatStatsTest::RunTest(const FString& Parameters)
{
	using namespace BeyondCombatStatsTest;
	TSharedRef<FState> State = MakeShared<FState>();
	State->Test = this;

	// Every stat is wired all the way through (no world needed)
	for (const EBeyondSkillStat Stat : BeyondStats::GetAllStats())
	{
		const FString Name = UBeyondSkillTreeAsset::GetStatName(Stat).ToString();
		TestTrue(FString::Printf(TEXT("Stat %d has a name"), static_cast<int32>(Stat)), !Name.IsEmpty());
		TestTrue(Name + TEXT(" has a SetByCaller tag"), BeyondStats::GetStatTag(Stat).IsValid());
		TestTrue(Name + TEXT(" has an attribute"), BeyondStats::GetStatAttribute(Stat).IsValid());
	}
	TestEqual(TEXT("Strength shows as Attack Power"), UBeyondSkillTreeAsset::GetStatName(EBeyondSkillStat::Strength).ToString(), FString(TEXT("Attack Power")));
	TestEqual(TEXT("Arcana shows as Magic Power"), UBeyondSkillTreeAsset::GetStatName(EBeyondSkillStat::Arcana).ToString(), FString(TEXT("Magic Power")));
	FBeyondItemStat Crit;
	Crit.Stat = EBeyondSkillStat::CritRate;
	Crit.Value = 4.0f;
	TestEqual(TEXT("Percent stats print with %"), UBeyondItemLibrary::FormatStat(Crit).ToString(), FString(TEXT("+4% Critical Rate")));

	if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
	{
		State->SavedSaveProgress = SaveProgress->GetString();
		SaveProgress->Set(TEXT("0"), ECVF_SetByCode);
	}
	if (IConsoleVariable* CritMode = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.CritMode")))
	{
		State->SavedCritMode = CritMode->GetString();
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/SICKA_PERSEPOLIS/MAPS/MAP_Demo_Main")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.0f));

	ADD_LATENT_AUTOMATION_COMMAND(FBeyondCombatStatsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* World = GetPlayWorld();
		ABeyondPlayerController* PC = World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!T.TestNotNull(TEXT("Beyond player controller"), PC))
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
		for (ABeyondCharacterBase* Member : PC->PartyComponent->GetMembers())
		{
			if (Member->DuoRole == EBeyondDuoRole::Conduit)
			{
				State->Angel = Member;
			}
		}
		ABeyondCharacterBase* Angel = State->Angel.Get();
		const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
		UBeyondEnemyDefinition* WolfDefinition = Roster ? Roster->FindEnemy(TEXT("forest_wolf")) : nullptr;
		UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(World);
		if (!T.TestNotNull(TEXT("Angel"), Angel) || !T.TestNotNull(TEXT("forest_wolf in the roster"), WolfDefinition) || !Enemies)
		{
			return true;
		}
		FBeyondEnemySpawnParams Params;
		Params.Level = 1;
		ABeyondEnemyCharacter* Wolf = Enemies->SpawnEnemy(WolfDefinition, FTransform(Angel->GetActorLocation() + Angel->GetActorForwardVector() * 800.0f), Params);
		if (!T.TestNotNull(TEXT("A wolf"), Wolf))
		{
			return true;
		}
		State->Wolf = Wolf;
		if (ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(Wolf->GetController()))
		{
			AI->SetBrainEnabled(false);
		}
		UAbilitySystemComponent* AngelASC = Angel->GetAbilitySystemComponent();
		const float Mastery = AngelASC->GetNumericAttributeBase(UCharacterAttributeSet::GetElementalMasteryAttribute());
		const float Verdant = AngelASC->GetNumericAttributeBase(UCharacterAttributeSet::GetVerdantAffinityAttribute());
		const float CritDamage = AngelASC->GetNumericAttribute(UCharacterAttributeSet::GetCritDamageAttribute());

		//~ Elements
		T.TestEqual(TEXT("Angel's own element is Storm"), Angel->GetNativeElement(), EBeyondElement::Storm);
		T.TestEqual(TEXT("A forest wolf is Verdant"), Wolf->GetNativeElement(), EBeyondElement::Verdant);
		T.TestEqual(TEXT("Angel's spells carry Storm"), UBeyondCombatLibrary::ResolveHitElement(Angel, BeyondTags::DamageType_Projectile, EBeyondElement::None),
			EBeyondElement::Storm);
		T.TestEqual(TEXT("His staff swings carry none"), UBeyondCombatLibrary::ResolveHitElement(Angel, BeyondTags::DamageType_Melee, EBeyondElement::None),
			EBeyondElement::None);
		T.TestEqual(TEXT("A wolf's bite carries Verdant"), UBeyondCombatLibrary::ResolveHitElement(Wolf, BeyondTags::DamageType_Melee, EBeyondElement::None),
			EBeyondElement::Verdant);
		T.TestEqual(TEXT("A lightning proc is Storm"), UBeyondCombatLibrary::ResolveHitElement(Angel, BeyondTags::DamageType_Proc_Lightning, EBeyondElement::None),
			EBeyondElement::Storm);
		T.TestEqual(TEXT("An explicit element wins"), UBeyondCombatLibrary::ResolveHitElement(Angel, BeyondTags::DamageType_Projectile, EBeyondElement::Frost),
			EBeyondElement::Frost);

		//~ Criticals
		SetCritMode(0);
		FGameplayTagContainer Tags;
		const float Plain = UBeyondCombatLibrary::ComputeOutgoingDamage(Angel, Wolf, 100.0f, BeyondTags::DamageType_Melee, EBeyondElement::None, Tags);
		T.TestFalse(TEXT("CritMode 0: no critical"), Tags.HasTagExact(BeyondTags::Damage_Critical));
		SetCritMode(2);
		Tags.Reset();
		const float Critical = UBeyondCombatLibrary::ComputeOutgoingDamage(Angel, Wolf, 100.0f, BeyondTags::DamageType_Melee, EBeyondElement::None, Tags);
		T.TestTrue(TEXT("CritMode 2: tagged critical"), Tags.HasTagExact(BeyondTags::Damage_Critical));
		T.TestEqual(TEXT("A critical deals 1 + Critical Damage"), Critical, Plain * (1.0f + CritDamage / 100.0f), 0.01f);
		Tags.Reset();
		UBeyondCombatLibrary::ComputeOutgoingDamage(Angel, Wolf, 100.0f, BeyondTags::DamageType_Proc_Lightning, EBeyondElement::None, Tags);
		T.TestFalse(TEXT("Procs never crit"), Tags.HasTagExact(BeyondTags::Damage_Critical));
		SetCritMode(0);

		//~ Mastery on his own element
		Tags.Reset();
		const float Spell = UBeyondCombatLibrary::ComputeOutgoingDamage(Angel, Wolf, 100.0f, BeyondTags::DamageType_Projectile, EBeyondElement::None, Tags);
		T.TestTrue(TEXT("The spell carries Element.Storm"), Tags.HasTagExact(BeyondTags::Element_Storm));
		AngelASC->SetNumericAttributeBase(UCharacterAttributeSet::GetElementalMasteryAttribute(), Mastery + 100.0f);
		Tags.Reset();
		const float Mastered = UBeyondCombatLibrary::ComputeOutgoingDamage(Angel, Wolf, 100.0f, BeyondTags::DamageType_Projectile, EBeyondElement::None, Tags);
		T.TestEqual(TEXT("+100 Mastery: storm spells x1.25"), Mastered, Spell * 1.25f, 0.01f);
		Tags.Reset();
		const float FrostSpell = UBeyondCombatLibrary::ComputeOutgoingDamage(Angel, Wolf, 100.0f, BeyondTags::DamageType_Projectile, EBeyondElement::Frost, Tags);
		T.TestEqual(TEXT("...but not another element's"), FrostSpell, Spell, 0.01f);
		AngelASC->SetNumericAttributeBase(UCharacterAttributeSet::GetElementalMasteryAttribute(), Mastery);

		//~ Affinity: offense against forest foes, defense against their bites
		AngelASC->SetNumericAttributeBase(UCharacterAttributeSet::GetVerdantAffinityAttribute(), Verdant + 20.0f);
		Tags.Reset();
		const float Favoured = UBeyondCombatLibrary::ComputeOutgoingDamage(Angel, Wolf, 100.0f, BeyondTags::DamageType_Melee, EBeyondElement::None, Tags);
		T.TestEqual(TEXT("+20% Verdant affinity: x1.2 against a wolf"), Favoured, Plain * 1.2f, 0.01f);
		if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(World))
		{
			FGameplayTagContainer Bite;
			Bite.AddTag(BeyondTags::DamageType_Melee);
			Bite.AddTag(BeyondTags::Element_Verdant);
			const float Resisted = Combat->ModifyIncomingDamage(*AngelASC, Wolf, Bite, 100.0f);
			AngelASC->SetNumericAttributeBase(UCharacterAttributeSet::GetVerdantAffinityAttribute(), Verdant);
			const float Unresisted = Combat->ModifyIncomingDamage(*AngelASC, Wolf, Bite, 100.0f);
			T.TestEqual(TEXT("...and resists a Verdant bite by 20%"), Resisted, Unresisted * 0.8f, 0.01f);
		}
		AngelASC->SetNumericAttributeBase(UCharacterAttributeSet::GetVerdantAffinityAttribute(), Verdant);

		//~ The menus list the new stats, critical damage as the whole hit
		const TArray<FBeyondStatRow> Rows = BeyondMenuPanels::GetCombatStats(Angel);
		const FBeyondStatRow* CritRow = Rows.FindByPredicate([](const FBeyondStatRow& Row) { return Row.Stat == EBeyondSkillStat::CritDamage; });
		T.TestTrue(TEXT("Combat stats list Critical Damage as a percent of the hit"), CritRow && CritRow->bPercent
			&& FMath::IsNearlyEqual(CritRow->Value, 100.0f + CritDamage, 0.01f));

		//~ Passives: a test tree on Angel (his own goes back at the end)
		UBeyondSkillTreeComponent* Skills = Angel->GetSkillTreeComponent();
		if (!T.TestNotNull(TEXT("Angel's skill tree component"), Skills))
		{
			Wolf->Destroy();
			return true;
		}
		UBeyondSkillTreeAsset* TestTree = MakePassiveTree();
		State->TestTree.Reset(TestTree);
		State->OldTree = Skills->Tree;
		State->OldRanks = Skills->GetRanks();
		T.TestTrue(TEXT("The passive test tree is valid"), TestTree->ValidateTree().IsEmpty());
		T.TestEqual(TEXT("Element damage describes itself"), UBeyondSkillTreeAsset::DescribeEffects(TestTree->Nodes[0], 2)[0].ToString(),
			FString(TEXT("+20% Storm damage")));
		T.TestEqual(TEXT("A passive describes itself"), UBeyondSkillTreeAsset::DescribeEffects(TestTree->Nodes[1], 2)[0].ToString(),
			FString(TEXT("On a critical hit: +10 Attack Power for 6s")));

		Skills->SetTree(TestTree);
		Skills->RestoreRanks(AllRanks(TestTree, TEXT("StormsEmbrace")));
		Tags.Reset();
		const float Unembraced = UBeyondCombatLibrary::ComputeOutgoingDamage(Angel, Wolf, 100.0f, BeyondTags::DamageType_Projectile, EBeyondElement::None, Tags);
		Skills->RestoreRanks(AllRanks(TestTree));
		T.TestEqual(TEXT("Storm's Embrace rank 2: +20% storm"), Skills->GetElementDamageBonus(EBeyondElement::Storm), 0.2f, 0.001f);
		T.TestEqual(TEXT("...and nothing for frost"), Skills->GetElementDamageBonus(EBeyondElement::Frost), 0.0f, 0.001f);
		Tags.Reset();
		const float Embraced = UBeyondCombatLibrary::ComputeOutgoingDamage(Angel, Wolf, 100.0f, BeyondTags::DamageType_Projectile, EBeyondElement::None, Tags);
		T.TestEqual(TEXT("Storm spells deal x1.2"), Embraced, Unembraced * 1.2f, 0.01f);

		// A real critical hit on the wolf; the hit feed runs next tick
		State->BaseStrength = AngelASC->GetNumericAttribute(UCharacterAttributeSet::GetStrengthAttribute());
		T.TestEqual(TEXT("No buff before the crit"), CountTimedStats(AngelASC), 0);
		SetCritMode(2);
		UBeyondCombatLibrary::ApplyDamage(Angel, Wolf, 1.0f, BeyondTags::DamageType_Melee, FGameplayTag());
		SetCritMode(0);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));

	ADD_LATENT_AUTOMATION_COMMAND(FBeyondCombatStatsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		UBeyondSkillTreeComponent* Skills = Angel ? Angel->GetSkillTreeComponent() : nullptr;
		UAbilitySystemComponent* AngelASC = Angel ? Angel->GetAbilitySystemComponent() : nullptr;
		UWorld* World = GetPlayWorld();
		if (!T.TestTrue(TEXT("Angel, his skill tree and the passive test tree are still there"), Skills && AngelASC && World && State->TestTree.IsValid()))
		{
			return true;
		}

		//~ On Crit: buff for its duration, once per internal cooldown
		T.TestEqual(TEXT("The crit set off Raging Momentum"), CountTimedStats(AngelASC), 1);
		T.TestEqual(TEXT("+10 Attack Power (2 ranks)"), AngelASC->GetNumericAttribute(UCharacterAttributeSet::GetStrengthAttribute()),
			State->BaseStrength + 10.0f, 0.01f);
		FGameplayEffectQuery TimedQuery;
		TimedQuery.EffectDefinition = UBeyondGE_TimedStats::StaticClass();
		const TArray<float> BuffLeft = AngelASC->GetActiveEffectsTimeRemaining(TimedQuery);
		T.TestTrue(TEXT("The buff lasts its 6 s"), BuffLeft.Num() == 1 && BuffLeft[0] > 5.0f && BuffLeft[0] <= 6.0f);
		FGameplayTagContainer CritTags;
		CritTags.AddTag(BeyondTags::DamageType_Melee);
		CritTags.AddTag(BeyondTags::Damage_Critical);
		Skills->FirePassives(EBeyondPassiveTrigger::OnCrit, CritTags);
		T.TestEqual(TEXT("A second crit inside the internal cooldown adds nothing"), CountTimedStats(AngelASC), 1);

		//~ On Elemental Hit: storm hits cut the running cooldowns, frost hits don't
		const FGameplayEffectSpecHandle Cooldown = AngelASC->MakeOutgoingSpec(UBeyondGE_Cooldown::StaticClass(), 1.0f, AngelASC->MakeEffectContext());
		Cooldown.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Duration, 10.0f);
		const FActiveGameplayEffectHandle CooldownHandle = AngelASC->ApplyGameplayEffectSpecToSelf(*Cooldown.Data);
		auto CooldownLeft = [AngelASC, World, CooldownHandle]()
		{
			const FActiveGameplayEffect* Active = AngelASC->GetActiveGameplayEffect(CooldownHandle);
			return Active ? Active->GetTimeRemaining(static_cast<float>(World->GetTimeSeconds())) : -1.0f;
		};
		const float CooldownBefore = CooldownLeft();
		T.TestEqual(TEXT("A 10 s test cooldown runs"), CooldownBefore, 10.0f, 0.05f);
		FGameplayTagContainer FrostHit;
		FrostHit.AddTag(BeyondTags::Element_Frost);
		Skills->FirePassives(EBeyondPassiveTrigger::OnElementalHit, FrostHit);
		T.TestEqual(TEXT("A frost hit leaves it"), CooldownLeft(), CooldownBefore, 0.05f);
		FGameplayTagContainer StormHit;
		StormHit.AddTag(BeyondTags::Element_Storm);
		Skills->FirePassives(EBeyondPassiveTrigger::OnElementalHit, StormHit);
		T.TestEqual(TEXT("A storm hit takes 2 s off (2 ranks)"), CooldownLeft(), CooldownBefore - 2.0f, 0.05f);
		AngelASC->RemoveActiveGameplayEffect(CooldownHandle);

		//~ On Kill: heal a share of max health
		const float MaxHealth = AngelASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());
		const float Health = AngelASC->GetNumericAttribute(UCharacterAttributeSet::GetCurrentHealthAttribute());
		AngelASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), MaxHealth * 0.5f);
		Skills->FirePassives(EBeyondPassiveTrigger::OnKill, FGameplayTagContainer());
		T.TestEqual(TEXT("A kill heals 20% of max health (2 ranks)"), AngelASC->GetNumericAttribute(UCharacterAttributeSet::GetCurrentHealthAttribute()),
			MaxHealth * 0.7f, 1.0f);
		AngelASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), Health);

		// Angel's own tree back
		AngelASC->RemoveActiveGameplayEffectBySourceEffect(UBeyondGE_TimedStats::StaticClass(), nullptr);
		T.TestEqual(TEXT("Attack Power back once the buff is gone"), AngelASC->GetNumericAttribute(UCharacterAttributeSet::GetStrengthAttribute()),
			State->BaseStrength, 0.01f);
		Skills->SetTree(State->OldTree.Get());
		Skills->RestoreRanks(State->OldRanks);
		if (ABeyondEnemyCharacter* Wolf = State->Wolf.Get())
		{
			Wolf->Destroy();
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondCombatStatsStep([State]()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			return false;
		}
		if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
		{
			SaveProgress->Set(State->SavedSaveProgress.IsEmpty() ? TEXT("1") : *State->SavedSaveProgress, ECVF_SetByCode);
		}
		if (IConsoleVariable* CritMode = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.CritMode")))
		{
			CritMode->Set(State->SavedCritMode.IsEmpty() ? TEXT("-1") : *State->SavedCritMode, ECVF_SetByCode);
		}
		return true;
	}));
	return true;
}

#endif
