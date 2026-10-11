// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BeyondVillageBuilderLibrary.generated.h"

class UBlueprint;

/**
 * The village builder (Plan 5C), called by migrate_pass17.py. House prefabs are small levels (external actors) that the
 * script fills with kit pieces; they go into the open world as Level Instances, or as Packed Level Actors (one actor of
 * instanced meshes) once packed. Editing a prefab level updates every village. CaptureView renders the editor world to
 * a PNG so a headless run can be looked at (needs -AllowCommandletRendering).
 */
UCLASS()
class WORLDBEYONDEDITOR_API UBeyondVillageBuilderLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Makes the current editor level keep its actors in their own files (needed for a prefab to stream in a partitioned world). */
	UFUNCTION(BlueprintCallable, Category = "Beyond|Village Builder", meta = (WorldContext = "WorldContext"))
	static bool UseExternalActors(UObject* WorldContext);

	/**
	 * Packs the prefab level into a Packed Level Actor Blueprint next to it (BPP_<level>), made or updated in memory (the
	 * script saves it). Call it with another level open (the prefab is instanced into the current world to be packed).
	 */
	UFUNCTION(BlueprintCallable, Category = "Beyond|Village Builder")
	static UBlueprint* PackPrefab(const FString& LevelPackageName);

	/** Package name of the prefab level's packed Blueprint. */
	UFUNCTION(BlueprintPure, Category = "Beyond|Village Builder")
	static FString GetPackedPrefabPath(const FString& LevelPackageName);

	/**
	 * An instance of the prefab, labelled Label: its packed Blueprint when bPacked and there is one, else a Level Instance
	 * of the level.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beyond|Village Builder", meta = (WorldContext = "WorldContext"))
	static AActor* PlacePrefab(UObject* WorldContext, const FString& LevelPackageName, FTransform Transform, const FString& Label, bool bPacked);

	/** The prefab level (package name) an instance shows; empty for other actors. */
	UFUNCTION(BlueprintPure, Category = "Beyond|Village Builder")
	static FString GetPrefabLevel(AActor* Actor);

	/**
	 * Renders the editor world from Location / Rotation into a Width x Height PNG at FilePath (lit, manual exposure at
	 * ExposureEV, no dynamic GI; orthographic OrthoWidth wide when above 0). Shaders and textures are finished first, so
	 * the first capture can take a while. The camera stays for the next capture until ReleaseCapture.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beyond|Village Builder", meta = (WorldContext = "WorldContext"))
	static bool CaptureView(UObject* WorldContext, FVector Location, FRotator Rotation, const FString& FilePath, float FOV = 60.0f,
		int32 Width = 1280, int32 Height = 720, float ExposureEV = 0.0f, float OrthoWidth = 0.0f);

	/** Removes the capture camera (call before saving a level that was captured). */
	UFUNCTION(BlueprintCallable, Category = "Beyond|Village Builder")
	static void ReleaseCapture();
};
