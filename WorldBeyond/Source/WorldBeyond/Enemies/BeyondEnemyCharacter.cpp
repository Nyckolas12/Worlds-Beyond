// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemies/BeyondEnemyCharacter.h"
#include "AbilitySystem/Abilities/BeyondGA_HitReact.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AI/BeyondEnemyController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BeyondGameplayTags.h"
#include "CharacterAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Enemies/BeyondAffixComponent.h"
#include "Enemies/BeyondAffixDefinition.h"
#include "Enemies/BeyondEnemyAnimInstance.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Enemies/BeyondEnemySettings.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "BeyondEnemy"

ABeyondEnemyCharacter::ABeyondEnemyCharacter()
{
	AIControllerClass = ABeyondEnemyController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	TeamAffiliation = EBeyondTeam::Enemy;
	bUseLegacyMaxHealth = false;
	DestroyDelayAfterDeath = 8.0f;
	bUseControllerRotationYaw = false;

	AffixComponent = CreateDefaultSubobject<UBeyondAffixComponent>(TEXT("Affixes"));

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	Movement->bUseRVOAvoidance = true;
	Movement->AvoidanceConsiderationRadius = 350.0f;

	// Telegraph markers shouldn't paint the enemies standing in them
	GetMesh()->SetReceivesDecals(false);
}

void ABeyondEnemyCharacter::PostInitializeComponents()
{
	// Before Super: Super spawns the AI controller, and possession sets up abilities and stats from what we set here
	ApplyDefinition();
	Super::PostInitializeComponents();
}

void ABeyondEnemyCharacter::ApplyDefinition()
{
	if (bDefinitionApplied || !Definition)
	{
		return;
	}
	bDefinitionApplied = true;

	const UBeyondEnemySettings* Settings = GetDefault<UBeyondEnemySettings>();

	TeamAffiliation = EBeyondTeam::Enemy;
	DisplayName = Definition->DisplayName;
	Rank = Definition->Rank;
	if (IsElite() && Rank == EBeyondEnemyRank::Regular)
	{
		Rank = EBeyondEnemyRank::Elite;
	}
	// A boss's shadow copy is just a strong add
	if (bClone && (Rank == EBeyondEnemyRank::MiniBoss || Rank == EBeyondEnemyRank::Boss))
	{
		Rank = EBeyondEnemyRank::Elite;
	}
	ExperienceReward = (bSummoned || bClone) ? 0.0f : Definition->ExperienceReward;
	for (const TSoftObjectPtr<UBeyondItemDefinition>& Item : Definition->GuaranteedLoot)
	{
		GuaranteedLoot.AddUnique(Item);
	}
	AbilitySet = Definition->AbilitySet;
	bUseLegacyMaxHealth = false;

	// Stats: the definition, then elite status and affixes on top
	float Health = Definition->MaxHealth * HealthScale;
	float Strength = Definition->Strength;
	float Arcana = Definition->Arcana;
	float Defense = Definition->Defense;
	float Scale = Definition->Scale * SizeScale;
	float Speed = Definition->WalkSpeed;
	if (IsElite())
	{
		Health *= Settings->EliteHealthMultiplier;
		Strength += Settings->EliteDamageBonus;
		Arcana += Settings->EliteDamageBonus;
		Scale *= Settings->EliteScale;
	}
	for (const UBeyondAffixDefinition* Affix : Affixes)
	{
		if (Affix)
		{
			Health *= Affix->HealthMultiplier;
			Scale *= Affix->ScaleMultiplier;
			Speed *= Affix->SpeedMultiplier;
			Strength += Affix->BonusStrength;
			Arcana += Affix->BonusArcana;
			Defense += Affix->BonusDefense;
		}
	}
	DefaultMaxHealth = FMath::Max(Health, 1.0f);
	BaseStrength = Strength;
	BaseArcana = Arcana;
	BaseDefense = Defense;
	StatGrowth = Definition->Growth;
	// Per-level health grows by the same share as the base (elites stay elite at every level)
	StatGrowth.MaxHealth *= Definition->MaxHealth > 0.0f ? Health / Definition->MaxHealth : 1.0f;

	// Looks: a definition without a mesh keeps the class's own (boss Blueprints)
	USkeletalMeshComponent* MeshComponent = GetMesh();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (Definition->CapsuleRadius > 0.0f && Definition->CapsuleHalfHeight > 0.0f)
	{
		Capsule->SetCapsuleSize(Definition->CapsuleRadius, Definition->CapsuleHalfHeight);
	}
	if (Definition->Mesh)
	{
		// A class that already has a mesh (a boss Blueprint) only gets a skin: its anim Blueprint and placement stay
		const bool bClassHasMesh = MeshComponent->GetSkeletalMeshAsset() != nullptr;
		if (Definition->AnimClass || !bClassHasMesh)
		{
			MeshComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			MeshComponent->SetAnimInstanceClass(Definition->AnimClass ? Definition->AnimClass.Get() : UBeyondEnemyAnimInstance::StaticClass());
		}
		MeshComponent->SetSkeletalMeshAsset(Definition->Mesh);
		if (!bClassHasMesh)
		{
			MeshComponent->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -Capsule->GetUnscaledCapsuleHalfHeight()) + Definition->MeshOffset,
				FRotator(0.0f, Definition->MeshYaw, 0.0f));
		}
	}
	for (int32 Index = 0; Index < Definition->MaterialOverrides.Num(); ++Index)
	{
		if (UMaterialInterface* Material = Definition->MaterialOverrides[Index])
		{
			MeshComponent->SetMaterial(Index, Material);
		}
	}
	SetActorScale3D(FVector(FMath::Max(Scale, 0.1f)));

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = Speed;
}

void ABeyondEnemyCharacter::BeginPlay()
{
	// Spawned by the subsystem: home already set; placed in a level: where it stands
	if (!bHomeSet)
	{
		SetHomeTransform(GetActorTransform());
	}

	Super::BeginPlay();

	if (HasAuthority() && Definition)
	{
		if (Definition->bUninterruptible)
		{
			AbilitySystemComponent->AddLooseGameplayTag(BeyondTags::State_Uninterruptible);
		}
		else
		{
			GrantAbilities({ TSubclassOf<UGameplayAbility>(UBeyondGA_HitReact::StaticClass()) });
		}
	}

	AffixComponent->ActivateAffixes(Affixes);
	ApplyTint();
	OnCharacterHitTaken.AddUniqueDynamic(this, &ThisClass::HandleSelfHitTaken);

	if (Definition)
	{
		BeyondFX::SpawnAtLocation(this, Definition->SpawnFX, GetActorLocation(), GetActorRotation());
		if (Definition->SpawnMontage)
		{
			PlayAnimMontage(Definition->SpawnMontage);
		}
	}

	if (UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(this))
	{
		Enemies->RegisterEnemy(this);
	}
}

void ABeyondEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(this))
	{
		Enemies->UnregisterEnemy(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ABeyondEnemyCharacter::ApplyTint()
{
	FLinearColor Tint = Definition ? Definition->Tint : FLinearColor::Transparent;
	for (const UBeyondAffixDefinition* Affix : Affixes)
	{
		if (Affix && Affix->Tint.A > 0.0f)
		{
			Tint = Affix->Tint;
			break;
		}
	}
	if (Tint.A <= 0.0f)
	{
		return;
	}

	UMaterialInterface* Overlay = GetDefault<UBeyondEnemySettings>()->TintMaterial.LoadSynchronous();
	if (!Overlay)
	{
		return;
	}
	TintMaterial = UMaterialInstanceDynamic::Create(Overlay, this);
	TintMaterial->SetVectorParameterValue(TEXT("Color"), Tint);
	GetMesh()->SetOverlayMaterial(TintMaterial);
}

void ABeyondEnemyCharacter::SetTintColor(const FLinearColor& Color)
{
	if (!TintMaterial)
	{
		UMaterialInterface* Overlay = GetDefault<UBeyondEnemySettings>()->TintMaterial.LoadSynchronous();
		if (!Overlay)
		{
			return;
		}
		TintMaterial = UMaterialInstanceDynamic::Create(Overlay, this);
	}
	TintMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	// Bosses keep their Blueprint's mesh, which may animate a child component
	if (USkeletalMeshComponent* TintedMesh = GetCombatMesh())
	{
		TintedMesh->SetOverlayMaterial(TintMaterial);
	}
}

FText ABeyondEnemyCharacter::GetEnemyName() const
{
	FText Name = GetCharacterDisplayName();
	// Two prefixes at most: "Molten Swift Raider"
	for (int32 Index = FMath::Min(Affixes.Num(), 2) - 1; Index >= 0; --Index)
	{
		if (const UBeyondAffixDefinition* Affix = Affixes[Index]; Affix && !Affix->Prefix.IsEmpty())
		{
			Name = FText::Format(LOCTEXT("AffixName", "{0} {1}"), Affix->Prefix, Name);
		}
	}
	return Name;
}

UAnimMontage* ABeyondEnemyCharacter::GetHitReactMontage(const FGameplayTag& Response) const
{
	if (!Definition)
	{
		return nullptr;
	}
	if (const TObjectPtr<UAnimMontage>* Montage = Definition->HitReactions.Find(Response); Montage && *Montage)
	{
		return *Montage;
	}
	const TObjectPtr<UAnimMontage>* Light = Definition->HitReactions.Find(BeyondTags::Event_Hit_Light);
	return Light ? Light->Get() : nullptr;
}

float ABeyondEnemyCharacter::GetTimeSinceDamaged() const
{
	return GetWorld()->GetTimeSeconds() - LastDamagedTime;
}

void ABeyondEnemyCharacter::HandleSelfHitTaken(ABeyondCharacterBase* HitCharacter, AActor* DamageInstigator, float Damage, FGameplayTag HitResponse)
{
	if (Damage > 0.0f)
	{
		LastDamagedTime = GetWorld()->GetTimeSeconds();
	}
}

void ABeyondEnemyCharacter::RestoreToFull()
{
	if (!AbilitySystemComponent || UBeyondCombatLibrary::IsActorDead(this))
	{
		return;
	}
	AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), UBeyondCombatLibrary::GetActorMaxHealth(this));
	LastDamagedTime = -1000.0f;
}

float ABeyondEnemyCharacter::ModifyDamageTaken(float Damage, AActor* DamageInstigator, const FGameplayTagContainer& DamageTags) const
{
	return AffixComponent ? AffixComponent->ModifyDamageTaken(Damage, DamageInstigator, DamageTags) : Damage;
}

void ABeyondEnemyCharacter::HandleDeath_Implementation()
{
	if (Definition)
	{
		BeyondFX::SpawnAtLocation(this, Definition->DeathFX, GetActorLocation(), GetActorRotation());
	}

	UAnimMontage* DeathMontage = Definition && !Definition->DeathMontages.IsEmpty()
		? Definition->DeathMontages[FMath::RandRange(0, Definition->DeathMontages.Num() - 1)].Get() : nullptr;
	USkeletalMeshComponent* MeshComponent = GetMesh();

	// Without a death animation, ragdoll when the mesh can (Lane minions have physics assets)
	if (!DeathMontage && MeshComponent->GetPhysicsAsset())
	{
		Super::HandleDeath_Implementation();
		return;
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
	if (!DeathMontage || !AnimInstance)
	{
		return;
	}

	AnimInstance->Montage_Stop(0.1f);
	const float Length = AnimInstance->Montage_Play(DeathMontage);
	if (Length > 0.0f)
	{
		// Hold the last frame (in case the montage blends out on its own)
		TWeakObjectPtr<UAnimInstance> WeakAnim(AnimInstance);
		TWeakObjectPtr<UAnimMontage> WeakMontage(DeathMontage);
		FTimerHandle Hold;
		GetWorldTimerManager().SetTimer(Hold, FTimerDelegate::CreateWeakLambda(this, [WeakAnim, WeakMontage]()
		{
			if (UAnimInstance* Anim = WeakAnim.Get())
			{
				Anim->Montage_Pause(WeakMontage.Get());
			}
		}), FMath::Max(Length - 0.08f, 0.05f), false);
	}
}

#undef LOCTEXT_NAMESPACE
