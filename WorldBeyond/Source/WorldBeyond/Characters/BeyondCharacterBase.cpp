// Fill out your copyright notice in the Description page of Project Settings.


#include "BeyondCharacterBase.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/BeyondAbilitySet.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "AbilitySystem/BeyondGameplayEffects.h"
#include "BeyondGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Game/BeyondCombatSubsystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Damage.h"
#include "Progression/BeyondProgressionAttributeSet.h"
#include "Progression/BeyondSkillTreeComponent.h"
#include "WorldBeyond.h"

// Sets default values
ABeyondCharacterBase::ABeyondCharacterBase()
{
	// Blueprints that implement Event Tick turn ticking back on automatically
	PrimaryActorTick.bCanEverTick = false;

	// Add the ability system component
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(ASCReplicationMode);

	AttributeSet = CreateDefaultSubobject<UCharacterAttributeSet>(TEXT("BasicAttributeSet"));

	EquipWeaponEventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Weapon.Equipped"), false);

}




// Called when the game starts or when spawned
void ABeyondCharacterBase::BeginPlay()
{
	MeshRelativeTransform = GetMesh()->GetRelativeTransform();
	MeshCollision = GetMesh()->GetCollisionEnabled();
	CapsuleCollision = GetCapsuleComponent()->GetCollisionEnabled();
	AvailableAttackTokens = MaxAttackTokens;

	// Before Super so Blueprint BeginPlay already sees attributes and abilities
	InitAbilitySystem();
	CreateAimComponent();
	Super::BeginPlay();

	// Blueprint BeginPlay may have granted abilities this character has replaced
	RemoveSuppressedAbilities();

	// Next tick, so abilities granted by Blueprint BeginPlay (GA_EquipWeapon) exist
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		RemoveSuppressedAbilities();
		EquipDefaultWeapon();
	}));
}

USkeletalMeshComponent* ABeyondCharacterBase::GetCombatMesh() const
{
	USkeletalMeshComponent* BaseMesh = GetMesh();
	if (BaseMesh && BaseMesh->GetSkeletalMeshAsset())
	{
		return BaseMesh;
	}
	if (CachedCombatMesh.IsValid())
	{
		return CachedCombatMesh.Get();
	}

	TInlineComponentArray<USkeletalMeshComponent*> Meshes(this);
	USkeletalMeshComponent* Fallback = nullptr;
	for (USkeletalMeshComponent* Candidate : Meshes)
	{
		if (Candidate == BaseMesh || !Candidate->GetSkeletalMeshAsset())
		{
			continue;
		}
		if (Candidate->GetFName() == CombatMeshName)
		{
			CachedCombatMesh = Candidate;
			return Candidate;
		}
		if (!Fallback && Candidate->GetAnimInstance())
		{
			Fallback = Candidate;
		}
	}

	if (Fallback)
	{
		CachedCombatMesh = Fallback;
		return Fallback;
	}
	return BaseMesh;
}

void ABeyondCharacterBase::StopAnimMontage(UAnimMontage* AnimMontage)
{
	const USkeletalMeshComponent* CombatMesh = GetCombatMesh();
	UAnimInstance* AnimInstance = CombatMesh ? CombatMesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		Super::StopAnimMontage(AnimMontage);
		return;
	}

	// None means "whatever is playing" (Blueprint combos end a missed window that way)
	if (!AnimMontage)
	{
		const UAnimMontage* Current = AnimInstance->GetCurrentActiveMontage();
		AnimInstance->Montage_Stop(Current ? Current->BlendOut.GetBlendTime() : 0.25f, nullptr);
		return;
	}
	if (!AnimInstance->Montage_GetIsStopped(AnimMontage))
	{
		AnimInstance->Montage_Stop(AnimMontage->BlendOut.GetBlendTime(), AnimMontage);
	}
}

void ABeyondCharacterBase::UseCombatMeshForAbilities()
{
	// GAS picks the first skeletal mesh it finds; on MetaHumans that is the empty CharacterMesh0,
	// so PlayMontageAndWait in Blueprint abilities would have nothing to play on
	FGameplayAbilityActorInfo* ActorInfo = AbilitySystemComponent ? AbilitySystemComponent->AbilityActorInfo.Get() : nullptr;
	if (!ActorInfo)
	{
		return;
	}

	const USkeletalMeshComponent* Current = ActorInfo->SkeletalMeshComponent.Get();
	if (!Current || !Current->GetSkeletalMeshAsset())
	{
		ActorInfo->SkeletalMeshComponent = GetCombatMesh();
	}
}

bool ABeyondCharacterBase::IsAbilitySuppressed(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	if (!AbilityClass)
	{
		return false;
	}
	for (const TSubclassOf<UGameplayAbility>& Suppressed : SuppressedAbilities)
	{
		if (Suppressed && AbilityClass->IsChildOf(Suppressed))
		{
			return true;
		}
	}
	return false;
}

void ABeyondCharacterBase::RemoveSuppressedAbilities()
{
	if (!AbilitySystemComponent || !HasAuthority() || SuppressedAbilities.IsEmpty())
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> ToClear;
	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.Ability && IsAbilitySuppressed(Spec.Ability->GetClass()))
		{
			ToClear.Add(Spec.Handle);
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : ToClear)
	{
		AbilitySystemComponent->ClearAbility(Handle);
	}
	if (!ToClear.IsEmpty())
	{
		SendAbilitiesChangedEvent();
	}
}

void ABeyondCharacterBase::ConfigureAIPerception(AController* NewController) const
{
	if (!bAIPerceivesHostileTeams || !Cast<AAIController>(NewController))
	{
		return;
	}

	UAIPerceptionComponent* Perception = NewController->FindComponentByClass<UAIPerceptionComponent>();
	if (!Perception)
	{
		return;
	}

	// Characters are team agents now, so the demigods count as hostile to enemies and must be detected as such
	bool bChanged = false;
	for (auto It = Perception->GetSensesConfigIterator(); It; ++It)
	{
		if (UAISenseConfig_Sight* Sight = Cast<UAISenseConfig_Sight>(*It); Sight && !Sight->DetectionByAffiliation.bDetectEnemies)
		{
			Sight->DetectionByAffiliation.bDetectEnemies = true;
			bChanged = true;
		}
		else if (UAISenseConfig_Hearing* Hearing = Cast<UAISenseConfig_Hearing>(*It); Hearing && !Hearing->DetectionByAffiliation.bDetectEnemies)
		{
			Hearing->DetectionByAffiliation.bDetectEnemies = true;
			bChanged = true;
		}
	}

	if (bChanged)
	{
		Perception->RequestStimuliListenerUpdate();
	}
}

void ABeyondCharacterBase::EquipDefaultWeapon()
{
	if (!AbilitySystemComponent || !DefaultWeaponTag.IsValid() || !EquipWeaponEventTag.IsValid())
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EquipWeaponEventTag;
	Payload.Instigator = this;
	Payload.Target = this;
	Payload.TargetTags.AddTag(DefaultWeaponTag);
	// Spawning with the weapon already in hand: skip the draw animation
	Payload.EventMagnitude = 1.0f;
	AbilitySystemComponent->HandleGameplayEvent(EquipWeaponEventTag, &Payload);
}

void ABeyondCharacterBase::PawnClientRestart()
{
	Super::PawnClientRestart();

	// Blueprint key-event nodes are bound during Super; ability inputs replace them
	if (bDisableLegacyKeyInput && InputComponent)
	{
		if (LegacyKeysToDisable.IsEmpty())
		{
			InputComponent->KeyBindings.Reset();
		}
		else
		{
			InputComponent->KeyBindings.RemoveAll([this](const FInputKeyBinding& Binding)
			{
				return LegacyKeysToDisable.Contains(Binding.Chord.Key);
			});
		}
	}
}

// Called to bind functionality to input
void ABeyondCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	for (const FBeyondInputBinding& Binding : AbilityInputBindings)
	{
		BindAbilityInput(EnhancedInput, Binding.InputAction, Binding.InputTag);
	}

	if (AbilitySet)
	{
		for (const FBeyondAbilitySet_Ability& Entry : AbilitySet->Abilities)
		{
			FGameplayTag InputTag = Entry.InputTag;
			if (!InputTag.IsValid() && Entry.Ability)
			{
				if (const UBeyondGameplayAbility* BeyondCDO = Cast<UBeyondGameplayAbility>(Entry.Ability->GetDefaultObject()))
				{
					InputTag = BeyondCDO->InputTag;
				}
			}
			BindAbilityInput(EnhancedInput, Entry.InputAction, InputTag);
		}
	}

	if (ConfirmTargetAction)
	{
		EnhancedInput->BindAction(ConfirmTargetAction.Get(), ETriggerEvent::Started, this, &ThisClass::Input_ConfirmTarget);
	}
	if (CancelTargetAction)
	{
		EnhancedInput->BindAction(CancelTargetAction.Get(), ETriggerEvent::Started, this, &ThisClass::Input_CancelTarget);
	}
	if (AimSettings.bEnabled && AimSettings.AimAction)
	{
		EnhancedInput->BindAction(AimSettings.AimAction.Get(), ETriggerEvent::Started, this, &ThisClass::Input_AimStarted);
		EnhancedInput->BindAction(AimSettings.AimAction.Get(), ETriggerEvent::Completed, this, &ThisClass::Input_AimStopped);
		EnhancedInput->BindAction(AimSettings.AimAction.Get(), ETriggerEvent::Canceled, this, &ThisClass::Input_AimStopped);
	}
}

void ABeyondCharacterBase::BindAbilityInput(UEnhancedInputComponent* EnhancedInput, const UInputAction* Action, const FGameplayTag& InputTag)
{
	if (Action && InputTag.IsValid())
	{
		EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::Input_AbilityPressed, InputTag);
		EnhancedInput->BindAction(Action, ETriggerEvent::Completed, this, &ThisClass::Input_AbilityReleased, InputTag);
	}
}

void ABeyondCharacterBase::Input_ConfirmTarget()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->LocalInputConfirm();
	}
}

void ABeyondCharacterBase::Input_CancelTarget()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->LocalInputCancel();
	}
}

void ABeyondCharacterBase::CreateAimComponent()
{
	if (!AimSettings.bEnabled || AimComponent)
	{
		return;
	}
	AimComponent = NewObject<UBeyondAimComponent>(this, TEXT("BeyondAimComponent"));
	AimComponent->RegisterComponent();
}

void ABeyondCharacterBase::Input_AimStarted()
{
	// Input can arrive before BeginPlay when the pawn is possessed early
	CreateAimComponent();
	if (AimComponent)
	{
		AimComponent->StartAim();
	}
}

void ABeyondCharacterBase::Input_AimStopped()
{
	if (AimComponent)
	{
		AimComponent->StopAim();
	}
}

void ABeyondCharacterBase::PossessedBy(AController* NewControl)
{
	Super::PossessedBy(NewControl);

	InitAbilitySystem();

	// AI controllers start with NoTeam; give them ours so perception affiliation works
	if (IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(NewControl))
	{
		TeamAgent->SetGenericTeamId(GetGenericTeamId());
	}
	ConfigureAIPerception(NewControl);
}

void ABeyondCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this,this);
		UseCombatMeshForAbilities();
	}
}

void ABeyondCharacterBase::InitAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	UseCombatMeshForAbilities();

	if (!bAbilitySystemBound)
	{
		bAbilitySystemBound = true;

		if (AttributeSet)
		{
			AttributeSet->OnHitTaken.AddUObject(this, &ThisClass::HandleAttributeHitTaken);
			AttributeSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);
		}

		AbilitySystemComponent->RegisterGameplayTagEvent(BeyondTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnDeadTagChanged);

		LegacyDamageComponent = BeyondLegacyDamage::FindComponent(this);
		if (LegacyDamageComponent.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetCurrentHealthAttribute())
				.AddUObject(this, &ThisClass::HandleHealthAttributeChanged);
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxHealthAttribute())
				.AddUObject(this, &ThisClass::HandleHealthAttributeChanged);
		}
	}

	// Grant everything exactly once, no matter how often the character is possessed (swapping)
	if (HasAuthority() && !bStartupGiven)
	{
		bStartupGiven = true;

		CreateProgressionSet();
		InitializeAttributeSet();

		if (AbilitySet)
		{
			AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, this, &AbilitySetHandles);
		}

		GrantAbilities(StartingAbilities);
	}
}

void ABeyondCharacterBase::InitializeAttributeSet()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	if (DefaultAttributesEffect)
	{
		AbilitySystemComponent->ApplyGameplayEffectToSelf(DefaultAttributesEffect->GetDefaultObject<UGameplayEffect>(), 1.0f, AbilitySystemComponent->MakeEffectContext());
	}
	else
	{
		float MaxHealthValue = DefaultMaxHealth;
		const float LegacyMaxHealth = BeyondLegacyDamage::GetLegacyMaxHealth(LegacyDamageComponent.Get());
		if (bUseLegacyMaxHealth && LegacyMaxHealth > 0.0f)
		{
			MaxHealthValue = LegacyMaxHealth;
		}

		// Max before current so the clamp in the attribute set doesn't cut the current value
		AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), MaxHealthValue);
		AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), MaxHealthValue);
		AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), DefaultMaxStamina);
		AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentStaminaAttribute(), DefaultMaxStamina);
	}

	// Combat stats and level; growth for the levels above 1 goes on top through an effect
	AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetStrengthAttribute(), BaseStrength);
	AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetArcanaAttribute(), BaseArcana);
	AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetDefenseAttribute(), BaseDefense);
	const int32 MaxLevel = GetDefault<UBeyondProgressionSettings>()->MaxLevel;
	AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetLevelAttribute(), static_cast<float>(FMath::Clamp(StartingLevel, 1, MaxLevel)));
	ApplyLevelStats();
	RefillVitals();
}

FText ABeyondCharacterBase::GetCharacterDisplayName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	FString Name = GetClass()->GetName();
	Name.RemoveFromEnd(TEXT("_C"));
	Name.RemoveFromStart(TEXT("BP_"));
	return FText::FromString(Name);
}

void ABeyondCharacterBase::CreateProgressionSet()
{
	// Only the demigods earn EXP; enemies just have a Level
	if (ProgressionSet || !AbilitySystemComponent || TeamAffiliation != EBeyondTeam::Player)
	{
		return;
	}

	ProgressionSet = NewObject<UBeyondProgressionAttributeSet>(this, TEXT("ProgressionAttributeSet"));
	AbilitySystemComponent->AddSpawnedAttribute(ProgressionSet);
	ProgressionSet->OnLevelUp.AddUObject(this, &ThisClass::HandleLevelUp);

	SkillTreeComponent = NewObject<UBeyondSkillTreeComponent>(this, TEXT("SkillTree"));
	SkillTreeComponent->Tree = SkillTree;
	SkillTreeComponent->RegisterComponent();
}

bool ABeyondCharacterBase::SpendSkillPoints(int32 Amount)
{
	if (!ProgressionSet || !AbilitySystemComponent || Amount < 0 || GetSkillPoints() < Amount)
	{
		return false;
	}
	AbilitySystemComponent->SetNumericAttributeBase(UBeyondProgressionAttributeSet::GetSkillPointsAttribute(), static_cast<float>(GetSkillPoints() - Amount));
	return true;
}

void ABeyondCharacterBase::AddSkillPoints(int32 Amount)
{
	if (ProgressionSet && AbilitySystemComponent && Amount > 0)
	{
		AbilitySystemComponent->SetNumericAttributeBase(UBeyondProgressionAttributeSet::GetSkillPointsAttribute(), static_cast<float>(GetSkillPoints() + Amount));
	}
}

const UGameplayAbility* ABeyondCharacterBase::FindAbilityOnInput(const FGameplayTag& InputTag) const
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	CollectSpecsWithInputTag(InputTag, Handles);
	for (const FGameplayAbilitySpecHandle& Handle : Handles)
	{
		if (const FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle); Spec && Spec->Ability)
		{
			return Spec->Ability;
		}
	}
	return nullptr;
}

int32 ABeyondCharacterBase::GetCharacterLevel() const
{
	return AttributeSet ? FMath::Max(1, FMath::RoundToInt(AttributeSet->GetLevel())) : 1;
}

float ABeyondCharacterBase::GetExperience() const
{
	return ProgressionSet ? ProgressionSet->GetExperience() : 0.0f;
}

float ABeyondCharacterBase::GetExperienceToNextLevel() const
{
	return UBeyondProgressionSettings::GetExperienceToNextLevel(GetCharacterLevel());
}

int32 ABeyondCharacterBase::GetSkillPoints() const
{
	return ProgressionSet ? FMath::RoundToInt(ProgressionSet->GetSkillPoints()) : 0;
}

float ABeyondCharacterBase::GetExperienceRewardValue() const
{
	return ExperienceReward >= 0.0f ? ExperienceReward : UBeyondProgressionSettings::GetExperienceReward(Rank, GetCharacterLevel());
}

void ABeyondCharacterBase::GrantExperience(float Amount)
{
	if (!ProgressionSet || !AbilitySystemComponent || !HasAuthority() || Amount <= 0.0f)
	{
		return;
	}

	const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UBeyondGE_GrantExperience::StaticClass(), 1.0f, AbilitySystemComponent->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Experience, Amount);
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}
}

void ABeyondCharacterBase::RestoreProgress(int32 NewLevel, float NewExperience, int32 NewSkillPoints)
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return;
	}

	const int32 MaxLevel = GetDefault<UBeyondProgressionSettings>()->MaxLevel;
	NewLevel = FMath::Clamp(NewLevel, 1, MaxLevel);
	AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetLevelAttribute(), static_cast<float>(NewLevel));
	if (ProgressionSet)
	{
		const float Needed = UBeyondProgressionSettings::GetExperienceToNextLevel(NewLevel);
		AbilitySystemComponent->SetNumericAttributeBase(UBeyondProgressionAttributeSet::GetExperienceAttribute(),
			Needed > 0.0f ? FMath::Clamp(NewExperience, 0.0f, Needed - 1.0f) : 0.0f);
		AbilitySystemComponent->SetNumericAttributeBase(UBeyondProgressionAttributeSet::GetSkillPointsAttribute(), static_cast<float>(FMath::Max(NewSkillPoints, 0)));
	}
	ApplyLevelStats();
	RefillVitals();
}

void ABeyondCharacterBase::ApplyLevelStats()
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return;
	}

	// The new effect goes on before the old one comes off, so max health never dips and cuts current health
	const FActiveGameplayEffectHandle OldHandle = LevelStatsHandle;
	LevelStatsHandle.Invalidate();

	const int32 Levels = GetCharacterLevel() - 1;
	if (Levels > 0)
	{
		const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UBeyondGE_LevelStats::StaticClass(), 1.0f, AbilitySystemComponent->MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_MaxHealth, StatGrowth.MaxHealth * Levels);
			Spec.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_MaxStamina, StatGrowth.MaxStamina * Levels);
			Spec.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Strength, StatGrowth.Strength * Levels);
			Spec.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Arcana, StatGrowth.Arcana * Levels);
			Spec.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Defense, StatGrowth.Defense * Levels);
			LevelStatsHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}

	if (OldHandle.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(OldHandle);
	}
}

void ABeyondCharacterBase::RefillVitals()
{
	if (!AbilitySystemComponent || !AttributeSet || AbilitySystemComponent->HasMatchingGameplayTag(BeyondTags::State_Dead))
	{
		SyncLegacyHealth();
		return;
	}

	AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), AttributeSet->GetMaxHealth());
	AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentStaminaAttribute(), AttributeSet->GetMaxStamina());
	SyncLegacyHealth();
}

void ABeyondCharacterBase::HandleLevelUp(int32 OldLevel, int32 NewLevel)
{
	UE_LOG(LogBeyond, Log, TEXT("%s reached level %d (from %d), %d skill points"), *GetName(), NewLevel, OldLevel, GetSkillPoints());

	ApplyLevelStats();
	RefillVitals();
	// This runs inside the EXP effect's execution; refill again once the new maximums have settled
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		RefillVitals();
	}));
	BeyondFX::SpawnAttached(LevelUpFX, GetRootComponent());

	if (AbilitySystemComponent)
	{
		FGameplayEventData Payload;
		Payload.EventTag = BeyondTags::Event_Progression_LevelUp;
		Payload.Instigator = this;
		Payload.Target = this;
		Payload.EventMagnitude = static_cast<float>(NewLevel);
		AbilitySystemComponent->HandleGameplayEvent(BeyondTags::Event_Progression_LevelUp, &Payload);
	}

	OnLevelUp(NewLevel);
	OnCharacterLevelUp.Broadcast(this, NewLevel);
}

void ABeyondCharacterBase::HandleHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	SyncLegacyHealth();
}

void ABeyondCharacterBase::SyncLegacyHealth()
{
	if (AttributeSet && AbilitySystemComponent)
	{
		BeyondLegacyDamage::SyncHealth(LegacyDamageComponent.Get(), AttributeSet->GetCurrentHealth(), AttributeSet->GetMaxHealth(),
			AbilitySystemComponent->HasMatchingGameplayTag(BeyondTags::State_Dead));
	}
}

void ABeyondCharacterBase::SyncLegacyDamageState()
{
	BeyondLegacyDamage::SyncStateTags(LegacyDamageComponent.Get(), AbilitySystemComponent, LegacyStateMirror);
}

bool ABeyondCharacterBase::TryReserveAttackTokens(int32 Amount)
{
	if (Amount > AvailableAttackTokens)
	{
		return false;
	}
	AvailableAttackTokens -= Amount;
	return true;
}

void ABeyondCharacterBase::ReleaseAttackTokens(int32 Amount)
{
	AvailableAttackTokens = FMath::Min(AvailableAttackTokens + FMath::Max(Amount, 0), MaxAttackTokens);
}

void ABeyondCharacterBase::HandleAttributeHitTaken(AActor* DamageInstigator, AActor* Causer, float Damage, FGameplayTag HitResponse)
{
	if (Damage > 0.0f)
	{
		LastDamageInstigator = DamageInstigator;

		// Keeps AI damage-sense reactions (turn to / chase the attacker) working with GAS damage
		if (DamageInstigator)
		{
			UAISense_Damage::ReportDamageEvent(this, this, DamageInstigator, Damage, DamageInstigator->GetActorLocation(), GetActorLocation());
		}

		// World-wide damage feed (the party's Bond meter listens to it)
		if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
		{
			Combat->OnDamageDealt.Broadcast(DamageInstigator, this, Damage);
		}
	}

	// Drive the old Blueprint handlers bound to BPC_DamageSystem (hit reactions, block reactions)
	if (UActorComponent* Legacy = LegacyDamageComponent.Get())
	{
		if (HitResponse == BeyondTags::Event_Hit_Blocked)
		{
			BeyondLegacyDamage::BroadcastBlocked(Legacy, true, DamageInstigator);
		}
		else if (Damage > 0.0f && HitResponse.IsValid() && !AbilitySystemComponent->HasMatchingGameplayTag(BeyondTags::State_Dead))
		{
			BeyondLegacyDamage::BroadcastDamageResponse(Legacy, BeyondLegacyDamage::HitResponseToLegacy(HitResponse), DamageInstigator);
		}
	}

	OnHitTaken(DamageInstigator, Damage, HitResponse);
	OnCharacterHitTaken.Broadcast(this, DamageInstigator, Damage, HitResponse);
}

void ABeyondCharacterBase::HandleOutOfHealth(AActor* DamageInstigator, AActor* Causer, float Damage, FGameplayTag HitResponse)
{
	if (DamageInstigator)
	{
		LastDamageInstigator = DamageInstigator;
	}

	// The tag change drives the rest of the death flow (OnDeadTagChanged)
	if (AbilitySystemComponent && !AbilitySystemComponent->HasMatchingGameplayTag(BeyondTags::State_Dead))
	{
		AbilitySystemComponent->AddLooseGameplayTag(BeyondTags::State_Dead);
	}
}

void ABeyondCharacterBase::OnDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (NewCount > 0)
	{
		Die();
	}
	else
	{
		OnRevived();
	}
}

void ABeyondCharacterBase::Die()
{
	AActor* Killer = LastDamageInstigator.Get();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();

		// Lets a GA_Death with an Event.Death trigger play
		FGameplayEventData Payload;
		Payload.EventTag = BeyondTags::Event_Death;
		Payload.Instigator = Killer;
		Payload.Target = this;
		AbilitySystemComponent->HandleGameplayEvent(BeyondTags::Event_Death, &Payload);
	}

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Dead"));
		}
	}

	HandleDeath();

	SyncLegacyHealth();
	BeyondLegacyDamage::BroadcastDeath(LegacyDamageComponent.Get());

	OnKilled(Killer);
	OnCharacterKilled.Broadcast(this, Killer);

	// World-wide kill feed (the party hands out EXP from it)
	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		Combat->OnCharacterKilled.Broadcast(this, Killer);
	}

	if (DestroyDelayAfterDeath > 0.0f)
	{
		SetLifeSpan(DestroyDelayAfterDeath);
	}
}

void ABeyondCharacterBase::HandleDeath_Implementation()
{
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();

	FVector Impulse = GetActorForwardVector() * -20000;
	Impulse.Z = 15000;
	GetMesh()->AddImpulseAtLocation(Impulse, GetActorLocation());
}

void ABeyondCharacterBase::Revive(float HealthFraction)
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->HasMatchingGameplayTag(BeyondTags::State_Dead))
	{
		return;
	}

	// Undo the ragdoll: bring the capsule to where the body ended up and snap the mesh back
	USkeletalMeshComponent* MeshComp = GetMesh();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (MeshComp->IsSimulatingPhysics())
	{
		const FVector BodyLocation = MeshComp->GetComponentLocation() + FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight());
		MeshComp->SetSimulatePhysics(false);
		SetActorLocation(BodyLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	MeshComp->AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	MeshComp->SetRelativeTransform(MeshRelativeTransform);
	MeshComp->SetCollisionEnabled(MeshCollision);
	Capsule->SetCollisionEnabled(CapsuleCollision);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	SetLifeSpan(0.0f);

	// The old Blueprint death handler disables the pawn's input
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		EnableInput(PC);
	}

	// Removing the tag calls OnRevived; heal afterwards because dead characters can't be healed
	AbilitySystemComponent->SetLooseGameplayTagCount(BeyondTags::State_Dead, 0);
	UBeyondCombatLibrary::ApplyHeal(this, this, FMath::Max(1.0f, AttributeSet->GetMaxHealth() * FMath::Clamp(HealthFraction, 0.0f, 1.0f)));
	SyncLegacyHealth();
}

UAbilitySystemComponent* ABeyondCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

FGenericTeamId ABeyondCharacterBase::GetGenericTeamId() const
{
	return TeamAffiliation == EBeyondTeam::Neutral ? FGenericTeamId::NoTeam : FGenericTeamId(static_cast<uint8>(TeamAffiliation));
}

void ABeyondCharacterBase::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	TeamAffiliation = NewTeamID == FGenericTeamId::NoTeam ? EBeyondTeam::Neutral : static_cast<EBeyondTeam>(NewTeamID.GetId());
}

TArray<FGameplayAbilitySpecHandle> ABeyondCharacterBase::GrantAbilities(
	TArray<TSubclassOf<UGameplayAbility>> AbilitiesToGrant)
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return TArray<FGameplayAbilitySpecHandle>();
	}

	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	for (TSubclassOf<UGameplayAbility> Ability : AbilitiesToGrant)
	{
		if (!Ability || IsAbilitySuppressed(Ability))
		{
			continue;
		}

		// Granted already (ability set, or a second Blueprint call): hand back the existing spec
		if (const FGameplayAbilitySpec* Existing = AbilitySystemComponent->FindAbilitySpecFromClass(Ability))
		{
			AbilityHandles.Add(Existing->Handle);
			continue;
		}

		FGameplayAbilitySpecHandle SpecHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(
			Ability, 1, -1, this
		));
		AbilityHandles.Add(SpecHandle);
	}

	SendAbilitiesChangedEvent();
	return AbilityHandles;
}

void ABeyondCharacterBase::RemoveAbilities(TArray<FGameplayAbilitySpecHandle> AbilityHandlesToRemove)
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return;
	}

	for (FGameplayAbilitySpecHandle AbilityHandle : AbilityHandlesToRemove)
	{
		AbilitySystemComponent->ClearAbility(AbilityHandle);
	}

	SendAbilitiesChangedEvent();
}

void ABeyondCharacterBase::SendAbilitiesChangedEvent()
{
	FGameplayEventData EventData;
	EventData.EventTag = BeyondTags::Event_Abilities_Changed;
	EventData.Instigator = this;
	EventData.Target = this;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, EventData.EventTag, EventData);
}

void ABeyondCharacterBase::ServerSendGameplayEventToSelf_Implementation(FGameplayEventData EventData)
{
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, EventData.EventTag, EventData);
}

void ABeyondCharacterBase::CollectSpecsWithInputTag(const FGameplayTag& InputTag, TArray<FGameplayAbilitySpecHandle>& OutHandles) const
{
	if (!AbilitySystemComponent || !InputTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (!Spec.Ability)
		{
			continue;
		}

		// Specs granted through an ability set carry their slot; ones granted elsewhere (Blueprint) use the ability's own Input Tag
		bool bMatches = Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag);
		if (!bMatches && !Spec.GetDynamicSpecSourceTags().HasTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Input"))))
		{
			const UBeyondGameplayAbility* BeyondAbility = Cast<UBeyondGameplayAbility>(Spec.Ability);
			bMatches = BeyondAbility && BeyondAbility->InputTag == InputTag;
		}

		if (bMatches)
		{
			OutHandles.Add(Spec.Handle);
		}
	}
}

bool ABeyondCharacterBase::TryActivateAbilityByInputTag(FGameplayTag InputTag)
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	CollectSpecsWithInputTag(InputTag, Handles);

	bool bActivated = false;
	for (const FGameplayAbilitySpecHandle& Handle : Handles)
	{
		bActivated |= AbilitySystemComponent->TryActivateAbility(Handle);
	}
	return bActivated;
}

void ABeyondCharacterBase::Input_AbilityPressed(FGameplayTag InputTag)
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	CollectSpecsWithInputTag(InputTag, Handles);

	for (const FGameplayAbilitySpecHandle& Handle : Handles)
	{
		FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
		if (!Spec)
		{
			continue;
		}

		if (Spec->IsActive())
		{
			// Already running (combos, charge-ups): forward the press so WaitInputPress tasks fire
			AbilitySystemComponent->AbilitySpecInputPressed(*Spec);
			if (UGameplayAbility* Instance = Spec->GetPrimaryInstance())
			{
				AbilitySystemComponent->InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Handle,
					Instance->GetCurrentActivationInfoRef().GetActivationPredictionKey());
			}
		}
		else
		{
			AbilitySystemComponent->TryActivateAbility(Handle);
		}
	}
}

void ABeyondCharacterBase::Input_AbilityReleased(FGameplayTag InputTag)
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	CollectSpecsWithInputTag(InputTag, Handles);

	for (const FGameplayAbilitySpecHandle& Handle : Handles)
	{
		FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->IsActive())
		{
			continue;
		}

		AbilitySystemComponent->AbilitySpecInputReleased(*Spec);
		if (UGameplayAbility* Instance = Spec->GetPrimaryInstance())
		{
			AbilitySystemComponent->InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Handle,
				Instance->GetCurrentActivationInfoRef().GetActivationPredictionKey());
		}

		// Only abilities waiting on target data listen to this
		if (bConfirmTargetingOnRelease)
		{
			AbilitySystemComponent->LocalInputConfirm();
		}
	}
}
