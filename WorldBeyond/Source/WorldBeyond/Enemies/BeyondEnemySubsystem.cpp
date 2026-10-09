// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemies/BeyondEnemySubsystem.h"
#include "WorldBeyond.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AI/BeyondEnemyController.h"
#include "BeyondGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/BeyondAffixDefinition.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Enemies/BeyondEnemySettings.h"
#include "Engine/World.h"
#include "Game/BeyondCombatSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "NavigationSystem.h"

namespace
{
	// Whoever is behind Actor (a projectile's or strike's instigator), as a character
	AActor* ResolveAttacker(AActor* Actor)
	{
		for (int32 Depth = 0; Actor && Depth < 4; ++Depth)
		{
			if (Cast<ABeyondCharacterBase>(Actor))
			{
				return Actor;
			}
			Actor = Actor->GetInstigator() && Actor->GetInstigator() != Actor ? Actor->GetInstigator() : Actor->GetOwner();
		}
		return Actor;
	}

	void SpawnFromConsole(const TArray<FString>& Args, UWorld* World)
	{
		UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(World);
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Enemies || !Pawn)
		{
			UE_LOG(LogBeyond, Warning, TEXT("Beyond.Spawn: needs a game world with a player pawn"));
			return;
		}
		if (Args.IsEmpty())
		{
			UE_LOG(LogBeyond, Display, TEXT("Beyond.Spawn <id> [level] [elite | <affix id>...] [x<count>]  (Beyond.ListEnemies lists the ids)"));
			return;
		}

		UBeyondEnemyDefinition* Definition = Enemies->FindDefinition(FName(*Args[0]));
		if (!Definition)
		{
			UE_LOG(LogBeyond, Warning, TEXT("Beyond.Spawn: no enemy '%s' in the roster (Beyond.ListEnemies)"), *Args[0]);
			return;
		}

		FBeyondEnemySpawnParams Params;
		int32 Count = Definition->PackSize;
		for (int32 Index = 1; Index < Args.Num(); ++Index)
		{
			const FString& Arg = Args[Index];
			if (Arg.IsNumeric())
			{
				Params.Level = FMath::Max(1, FCString::Atoi(*Arg));
			}
			else if (Arg.StartsWith(TEXT("x")) && Arg.RightChop(1).IsNumeric())
			{
				Count = FMath::Clamp(FCString::Atoi(*Arg.RightChop(1)), 1, 20);
			}
			else if (Arg.Equals(TEXT("elite"), ESearchCase::IgnoreCase))
			{
				Params.bElite = true;
			}
			else if (UBeyondAffixDefinition* Affix = Enemies->FindAffix(FName(*Arg)))
			{
				Params.bElite = true;
				Params.Affixes.Add(Affix);
			}
			else
			{
				UE_LOG(LogBeyond, Warning, TEXT("Beyond.Spawn: ignored '%s' (not a level, x<count>, elite or affix id)"), *Arg);
			}
		}

		const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
		const FVector Centre = Pawn->GetActorLocation() + Forward * 700.0f;
		const TArray<ABeyondEnemyCharacter*> Spawned = Enemies->SpawnPack(Definition, Centre, (-Forward).Rotation().Yaw, Count, Params);
		for (const ABeyondEnemyCharacter* Enemy : Spawned)
		{
			UE_LOG(LogBeyond, Display, TEXT("Beyond.Spawn: %s, level %d, %.0f health"), *Enemy->GetEnemyName().ToString(), Enemy->GetCharacterLevel(),
				UBeyondCombatLibrary::GetActorMaxHealth(Enemy));
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GSpawnEnemyCommand(
		TEXT("Beyond.Spawn"),
		TEXT("Spawns roster enemies in front of the player: Beyond.Spawn <id> [level] [elite | <affix id>...] [x<count>]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SpawnFromConsole));

	FAutoConsoleCommandWithWorldAndArgs GKillEnemiesCommand(
		TEXT("Beyond.KillEnemies"),
		TEXT("Kills every roster enemy in the world (no EXP, no loot)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (const UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(World))
			{
				for (ABeyondEnemyCharacter* Enemy : Enemies->GetLiveEnemies())
				{
					UBeyondCombatLibrary::ApplyDamage(nullptr, Enemy, 1.0e7f, BeyondTags::DamageType_Environment, FGameplayTag(), true, nullptr, true, true);
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GListEnemiesCommand(
		TEXT("Beyond.ListEnemies"),
		TEXT("Lists the roster's enemy and affix ids and the living enemies"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster())
			{
				for (const UBeyondEnemyDefinition* Enemy : Roster->Enemies)
				{
					if (Enemy)
					{
						UE_LOG(LogBeyond, Display, TEXT("enemy %s: %s (%s)"), *Enemy->EnemyId.ToString(), *Enemy->DisplayName.ToString(), *Enemy->Region.ToString());
					}
				}
				for (const UBeyondAffixDefinition* Affix : Roster->Affixes)
				{
					if (Affix)
					{
						UE_LOG(LogBeyond, Display, TEXT("affix %s: %s"), *Affix->AffixId.ToString(), *Affix->Description.ToString());
					}
				}
			}
			if (const UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(World))
			{
				for (const ABeyondEnemyCharacter* Enemy : Enemies->GetLiveEnemies())
				{
					UE_LOG(LogBeyond, Display, TEXT("alive: %s level %d, %.0f / %.0f"), *Enemy->GetEnemyName().ToString(), Enemy->GetCharacterLevel(),
						UBeyondCombatLibrary::GetActorHealth(Enemy), UBeyondCombatLibrary::GetActorMaxHealth(Enemy));
				}
			}
		}));
}

UBeyondEnemySubsystem* UBeyondEnemySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBeyondEnemySubsystem>() : nullptr;
}

void UBeyondEnemySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(&InWorld))
	{
		PartyWipedHandle = Combat->OnPartyWiped.AddUObject(this, &ThisClass::ResetAllEnemies);
	}
}

void UBeyondEnemySubsystem::Deinitialize()
{
	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(GetWorld()))
	{
		Combat->OnPartyWiped.Remove(PartyWipedHandle);
	}
	Super::Deinitialize();
}

UBeyondEnemyDefinition* UBeyondEnemySubsystem::FindDefinition(FName EnemyId) const
{
	const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
	return Roster ? Roster->FindEnemy(EnemyId) : nullptr;
}

UBeyondAffixDefinition* UBeyondEnemySubsystem::FindAffix(FName AffixId) const
{
	const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
	return Roster ? Roster->FindAffix(AffixId) : nullptr;
}

TArray<UBeyondAffixDefinition*> UBeyondEnemySubsystem::RollAffixes(const UBeyondEnemyDefinition* Definition, int32 Level) const
{
	TArray<UBeyondAffixDefinition*> Pool;
	if (Definition && !Definition->AllowedAffixes.IsEmpty())
	{
		for (UBeyondAffixDefinition* Affix : Definition->AllowedAffixes)
		{
			if (Affix)
			{
				Pool.Add(Affix);
			}
		}
	}
	else if (const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster())
	{
		for (UBeyondAffixDefinition* Affix : Roster->Affixes)
		{
			if (Affix)
			{
				Pool.Add(Affix);
			}
		}
	}

	const UBeyondEnemySettings* Settings = GetDefault<UBeyondEnemySettings>();
	const int32 Wanted = Settings->AffixesPerElite + (Level >= Settings->ExtraAffixFromLevel ? 1 : 0);
	TArray<UBeyondAffixDefinition*> Result;
	while (Result.Num() < Wanted && !Pool.IsEmpty())
	{
		Result.Add(Pool[FMath::RandRange(0, Pool.Num() - 1)]);
		Pool.Remove(Result.Last());
	}
	return Result;
}

bool UBeyondEnemySubsystem::FindGroundPoint(const UWorld* World, const FVector& Desired, FVector& OutPoint)
{
	if (!World)
	{
		return false;
	}

	// The floor right under the point first: a wide search could snap to a level above or below (ledges, stairs)
	FHitResult Hit;
	const FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::AllStaticObjects);
	const bool bFloor = World->LineTraceSingleByObjectType(Hit, Desired + FVector(0.0f, 0.0f, 300.0f), Desired - FVector(0.0f, 0.0f, 800.0f), Objects);

	if (const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		FNavLocation NavLocation;
		const FVector Probe = bFloor ? FVector(Hit.ImpactPoint) : Desired;
		if (NavSystem->ProjectPointToNavigation(Probe, NavLocation, FVector(250.0f, 250.0f, 150.0f)))
		{
			OutPoint = NavLocation.Location;
			return true;
		}
	}

	if (bFloor)
	{
		OutPoint = Hit.ImpactPoint;
		return true;
	}
	return false;
}

ABeyondEnemyCharacter* UBeyondEnemySubsystem::SpawnEnemy(UBeyondEnemyDefinition* Definition, const FTransform& Transform, const FBeyondEnemySpawnParams& Params)
{
	UWorld* World = GetWorld();
	if (!World || !Definition)
	{
		return nullptr;
	}

	UClass* Class = Definition->CharacterClass.IsNull() ? nullptr : Definition->CharacterClass.LoadSynchronous();
	if (!Class)
	{
		Class = ABeyondEnemyCharacter::StaticClass();
	}

	// Feet on the ground: lift the capsule by its (scaled) half height
	const float Scale = Definition->Scale * Params.SizeScale * (Params.bElite ? GetDefault<UBeyondEnemySettings>()->EliteScale : 1.0f);
	const float HalfHeight = (Definition->CapsuleHalfHeight > 0.0f
		? Definition->CapsuleHalfHeight
		: Class->GetDefaultObject<ABeyondEnemyCharacter>()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()) * Scale;
	FVector Location = Transform.GetLocation();
	FVector Ground;
	if (FindGroundPoint(World, Location, Ground))
	{
		Location = Ground + FVector(0.0f, 0.0f, HalfHeight + 5.0f);
	}
	const FTransform SpawnTransform(FRotator(0.0f, Transform.Rotator().Yaw, 0.0f), Location);

	ABeyondEnemyCharacter* Enemy = World->SpawnActorDeferred<ABeyondEnemyCharacter>(Class, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Enemy)
	{
		return nullptr;
	}

	Enemy->Definition = Definition;
	Enemy->StartingLevel = FMath::Max(Params.Level, 1);
	Enemy->bElite = Params.bElite && Definition->bCanBeElite;
	Enemy->Affixes = Params.Affixes;
	if (Enemy->bElite && Enemy->Affixes.IsEmpty())
	{
		for (UBeyondAffixDefinition* Affix : RollAffixes(Definition, Params.Level))
		{
			Enemy->Affixes.Add(Affix);
		}
	}
	Enemy->bSummoned = Params.bSummoned;
	Enemy->HealthScale = Params.HealthScale;
	Enemy->SizeScale = Params.SizeScale;
	Enemy->OutgoingDamageScale = Params.OutgoingDamageScale;
	Enemy->bClone = Params.bClone;
	Enemy->SetHomeTransform(SpawnTransform);
	Enemy->FinishSpawning(SpawnTransform);
	return Enemy;
}

TArray<ABeyondEnemyCharacter*> UBeyondEnemySubsystem::SpawnPack(UBeyondEnemyDefinition* Definition, FVector Centre, float Yaw, int32 Count, const FBeyondEnemySpawnParams& Params)
{
	TArray<ABeyondEnemyCharacter*> Spawned;
	Count = FMath::Max(Count, 1);
	const float Spacing = Definition ? FMath::Max(Definition->CapsuleRadius * Definition->Scale * 3.0f, 160.0f) : 160.0f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FVector Location = Centre;
		if (Count > 1)
		{
			const float Angle = 2.0f * PI * Index / Count;
			Location += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Spacing;
		}
		if (ABeyondEnemyCharacter* Enemy = SpawnEnemy(Definition, FTransform(FRotator(0.0f, Yaw, 0.0f), Location), Params))
		{
			Spawned.Add(Enemy);
		}
	}
	return Spawned;
}

TArray<ABeyondEnemyCharacter*> UBeyondEnemySubsystem::GetLiveEnemies() const
{
	TArray<ABeyondEnemyCharacter*> Result;
	for (const TWeakObjectPtr<ABeyondEnemyCharacter>& Weak : LiveEnemies)
	{
		ABeyondEnemyCharacter* Enemy = Weak.Get();
		if (Enemy && !UBeyondCombatLibrary::IsActorDead(Enemy))
		{
			Result.Add(Enemy);
		}
	}
	return Result;
}

void UBeyondEnemySubsystem::RegisterEnemy(ABeyondEnemyCharacter* Enemy)
{
	LiveEnemies.RemoveAll([](const TWeakObjectPtr<ABeyondEnemyCharacter>& Weak) { return !Weak.IsValid(); });
	LiveEnemies.AddUnique(Enemy);
}

void UBeyondEnemySubsystem::UnregisterEnemy(ABeyondEnemyCharacter* Enemy)
{
	LiveEnemies.Remove(Enemy);
}

void UBeyondEnemySubsystem::AlertEnemy(ABeyondEnemyCharacter* Enemy, AActor* Target)
{
	Target = ResolveAttacker(Target);
	if (ABeyondEnemyController* AI = Enemy ? Cast<ABeyondEnemyController>(Enemy->GetController()) : nullptr; AI && Target)
	{
		AI->EngageTarget(Target, false);
	}
}

void UBeyondEnemySubsystem::AlertNearby(const ABeyondEnemyCharacter* Source, AActor* Target, float Radius)
{
	if (!Source || !Target || Radius <= 0.0f)
	{
		return;
	}
	for (ABeyondEnemyCharacter* Enemy : GetLiveEnemies())
	{
		ABeyondEnemyController* AI = Enemy != Source ? Cast<ABeyondEnemyController>(Enemy->GetController()) : nullptr;
		if (AI && AI->GetAIState() == EBeyondEnemyAIState::Idle && FVector::Dist(Enemy->GetActorLocation(), Source->GetActorLocation()) <= Radius)
		{
			AI->EngageTarget(Target, false);
		}
	}
}

void UBeyondEnemySubsystem::ResetAllEnemies()
{
	for (ABeyondEnemyCharacter* Enemy : GetLiveEnemies())
	{
		if (Enemy->bSummoned)
		{
			Enemy->Destroy();
			continue;
		}
		if (ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(Enemy->GetController()))
		{
			AI->ReturnHome(true);
		}
		else
		{
			Enemy->RestoreToFull();
		}
	}
}
