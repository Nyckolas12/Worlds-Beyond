// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/BeyondEditorLibrary.h"
#include "WorldBeyond.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"

#if WITH_EDITOR
#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
#endif

bool UBeyondEditorLibrary::SetMontageSlot(UAnimMontage* Montage, FName SlotName)
{
#if WITH_EDITOR
	if (!Montage || SlotName.IsNone() || Montage->SlotAnimTracks.IsEmpty())
	{
		return false;
	}
	Montage->Modify();
	for (FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
	{
		Track.SlotName = SlotName;
	}
	Montage->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

FName UBeyondEditorLibrary::GetMontageSlot(const UAnimMontage* Montage)
{
	return Montage && !Montage->SlotAnimTracks.IsEmpty() ? Montage->SlotAnimTracks[0].SlotName : NAME_None;
}

AActor* UBeyondEditorLibrary::CreateNavMeshBounds(UObject* WorldContext, FVector Centre, FVector HalfExtent)
{
#if WITH_EDITOR
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANavMeshBoundsVolume* Volume = World->SpawnActor<ANavMeshBoundsVolume>(Centre, FRotator::ZeroRotator, Params);
	if (!Volume)
	{
		return nullptr;
	}

	UCubeBuilder* Builder = NewObject<UCubeBuilder>();
	Builder->X = HalfExtent.X * 2.0f;
	Builder->Y = HalfExtent.Y * 2.0f;
	Builder->Z = HalfExtent.Z * 2.0f;
	UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
	Volume->PostEditChange();

	if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		NavSystem->OnNavigationBoundsUpdated(Volume);
	}
	return Volume;
#else
	return nullptr;
#endif
}

bool UBeyondEditorLibrary::BuildNavigation(UObject* WorldContext)
{
#if WITH_EDITOR
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	// A commandlet's editor world may have no navigation system yet (ResavePackages -BuildNavigationData does this too)
	if (!FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::EditorMode);
	}
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSystem)
	{
		UE_LOG(LogBeyond, Warning, TEXT("BuildNavigation: %s has no navigation system"), *World->GetName());
		return false;
	}

	// Editor worlds opened by a script keep build locks (async map load, material updates); lift them for the build
	uint8 HeldLocks = 0;
	for (int32 Bit = 0; Bit < 8; ++Bit)
	{
		const uint8 Flag = static_cast<uint8>(1 << Bit);
		if (NavSystem->IsNavigationBuildingLocked(Flag))
		{
			HeldLocks |= Flag;
		}
	}
	if (HeldLocks)
	{
		NavSystem->RemoveNavigationBuildLock(HeldLocks, UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
	}

	NavSystem->Build();
	if (ANavigationData* NavData = NavSystem->GetDefaultNavDataInstance())
	{
		NavData->EnsureBuildCompletion();
	}

	if (HeldLocks)
	{
		NavSystem->AddNavigationBuildLock(HeldLocks);
	}

	const bool bBuilt = NavSystem->GetDefaultNavDataInstance() != nullptr;
	UE_LOG(LogBeyond, Log, TEXT("BuildNavigation: %s the navmesh of %s (lifted locks 0x%x)"), bBuilt ? TEXT("built") : TEXT("could not build"),
		*World->GetName(), HeldLocks);
	return bBuilt;
#else
	return false;
#endif
}
