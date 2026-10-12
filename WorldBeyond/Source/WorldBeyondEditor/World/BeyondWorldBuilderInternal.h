// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

class FWorldPartitionActorDescInstance;
class UBeyondWorldLayout;
class UMaterial;
class UObject;
class UWorld;
struct FBeyondHeightfield;

/** Helpers the world builder's libraries share (defined in BeyondWorldBuilderLibrary.cpp) */
namespace BeyondWorldBuilderLocal
{
	// The world of a context object, else the editor world
	UWorld* ResolveWorld(UObject* WorldContext);

	// The layout's generated terrain (generated once per layout and kept)
	const FBeyondHeightfield* GetField(UBeyondWorldLayout* Layout);

	// Loads a World Partition actor and keeps it loaded until UBeyondWorldBuilderLibrary::SaveWorld
	void KeepLoaded(FWorldPartitionActorDescInstance* Desc);

	// Deletes every node of a material (UMaterialEditingLibrary::DeleteAllMaterialExpressions removes from the list it
	// walks, so it leaves every other node behind: a rebuild then had two grass or water outputs)
	void ClearMaterialExpressions(UMaterial* Material);

	template <typename T>
	T* LoadExisting(const FString& AssetPath)
	{
		const FString ObjectPath = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
		return LoadObject<T>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	// The asset at AssetPath, or a new one in a new package (bOutNew)
	template <typename T>
	T* MakeAsset(const FString& AssetPath, bool& bOutNew)
	{
		bOutNew = false;
		if (T* Existing = LoadExisting<T>(AssetPath))
		{
			return Existing;
		}
		UPackage* Package = CreatePackage(*AssetPath);
		T* Asset = NewObject<T>(Package, *FPackageName::GetShortName(AssetPath), RF_Public | RF_Standalone | RF_Transactional);
		bOutNew = true;
		return Asset;
	}
}
