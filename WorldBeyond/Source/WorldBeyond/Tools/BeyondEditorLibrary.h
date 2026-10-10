// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BeyondEditorLibrary.generated.h"

class AActor;
class UAnimMontage;

/**
 * Editor-only helpers for the migration scripts (Scripts/Migration), for things Python can't reach:
 * montage slots, navigation building, volumes with a real box brush. They do nothing outside the editor.
 */
UCLASS()
class WORLDBEYOND_API UBeyondEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Puts every track of Montage on SlotName (the Paragon hero anim Blueprints only have FullBody / UpperBody)
	UFUNCTION(BlueprintCallable, Category = "Beyond|Editor")
	static bool SetMontageSlot(UAnimMontage* Montage, FName SlotName);

	// The slot of Montage's first track (None without tracks)
	UFUNCTION(BlueprintPure, Category = "Beyond|Editor")
	static FName GetMontageSlot(const UAnimMontage* Montage);

	// A Nav Mesh Bounds Volume with a box brush of HalfExtent around Centre in WorldContext's world
	UFUNCTION(BlueprintCallable, Category = "Beyond|Editor", meta = (WorldContext = "WorldContext"))
	static AActor* CreateNavMeshBounds(UObject* WorldContext, FVector Centre, FVector HalfExtent);

	// Builds the navmesh of WorldContext's world (also inside a commandlet); false if there is nothing to build
	UFUNCTION(BlueprintCallable, Category = "Beyond|Editor", meta = (WorldContext = "WorldContext"))
	static bool BuildNavigation(UObject* WorldContext);
};
