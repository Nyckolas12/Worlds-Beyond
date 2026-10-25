// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BeyondUIEditorLibrary.generated.h"

class UFont;
class UFontFace;

/**
 * Editor helpers for the UI overhaul (Plan 6), called by migrate_pass20.py: font faces from TTF files and the composite
 * fonts the menus use (Cinzel, EB Garamond). The script saves what these make.
 */
UCLASS()
class WORLDBEYONDEDITOR_API UBeyondUIEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** A font face asset at AssetPath holding the TTF / OTF file's data (inline, so it needs no file at runtime); null if the file can't be read. */
	UFUNCTION(BlueprintCallable, Category = "Beyond|UI")
	static UFontFace* ImportFontFace(const FString& FontFile, const FString& AssetPath);

	/** A runtime composite font at AssetPath whose default typeface lists Faces under TypefaceNames ("Regular", "Bold", ...). */
	UFUNCTION(BlueprintCallable, Category = "Beyond|UI")
	static UFont* MakeCompositeFont(const FString& AssetPath, const TArray<FName>& TypefaceNames, const TArray<UFontFace*>& Faces);
};
