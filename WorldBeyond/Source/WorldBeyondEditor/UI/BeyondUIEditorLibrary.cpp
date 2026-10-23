// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondUIEditorLibrary.h"
#include "WorldBeyondEditor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "World/BeyondWorldBuilderInternal.h"

UFontFace* UBeyondUIEditorLibrary::ImportFontFace(const FString& FontFile, const FString& AssetPath)
{
	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *FontFile) || Data.Num() == 0)
	{
		UE_LOG(LogBeyondEditor, Warning, TEXT("UI: could not read the font file %s"), *FontFile);
		return nullptr;
	}

	bool bNew = false;
	UFontFace* Face = BeyondWorldBuilderLocal::MakeAsset<UFontFace>(AssetPath, bNew);
	Face->InitializeFromBulkData(FPaths::ConvertRelativePathToFull(FontFile), EFontHinting::Default, Data.GetData(), Data.Num());
	Face->LoadingPolicy = EFontLoadingPolicy::Inline;
	// PostEditChange flushes Slate's font cache, which a commandlet doesn't have
	if (FSlateApplication::IsInitialized())
	{
		Face->PostEditChange();
	}
	if (bNew)
	{
		FAssetRegistryModule::AssetCreated(Face);
	}
	Face->MarkPackageDirty();
	UE_LOG(LogBeyondEditor, Display, TEXT("UI: font face %s from %s (%d bytes)"), *AssetPath, *FPaths::GetCleanFilename(FontFile), Data.Num());
	return Face;
}

UFont* UBeyondUIEditorLibrary::MakeCompositeFont(const FString& AssetPath, const TArray<FName>& TypefaceNames, const TArray<UFontFace*>& Faces)
{
	if (TypefaceNames.Num() != Faces.Num() || Faces.Num() == 0)
	{
		UE_LOG(LogBeyondEditor, Warning, TEXT("UI: %s needs one face per typeface name (%d names, %d faces)"), *AssetPath, TypefaceNames.Num(), Faces.Num());
		return nullptr;
	}

	bool bNew = false;
	UFont* Font = BeyondWorldBuilderLocal::MakeAsset<UFont>(AssetPath, bNew);
	Font->FontCacheType = EFontCacheType::Runtime;
	FCompositeFont& Composite = Font->GetMutableInternalCompositeFont();
	Composite = FCompositeFont();
	for (int32 Index = 0; Index < Faces.Num(); ++Index)
	{
		if (!Faces[Index])
		{
			continue;
		}
		FTypefaceEntry& Entry = Composite.DefaultTypeface.Fonts.Emplace_GetRef(TypefaceNames[Index]);
		Entry.Font = FFontData(Faces[Index]);
	}
	Font->LegacyFontName = TypefaceNames[0];
	if (FSlateApplication::IsInitialized())
	{
		Font->PostEditChange();
	}
	if (bNew)
	{
		FAssetRegistryModule::AssetCreated(Font);
	}
	Font->MarkPackageDirty();
	UE_LOG(LogBeyondEditor, Display, TEXT("UI: composite font %s with %d typefaces"), *AssetPath, Composite.DefaultTypeface.Fonts.Num());
	return Font;
}
