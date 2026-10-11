// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondVillageBuilderLibrary.h"
#include "WorldBeyondEditor.h"
#include "AssetCompilingManager.h"
#include "AssetToolsModule.h"
#include "Components/SceneCaptureComponent2D.h"
#include "ContentStreaming.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Factories/BlueprintFactory.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PackedLevelActor/PackedLevelActor.h"
#include "PackedLevelActor/PackedLevelActorBuilder.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

namespace BeyondVillageBuilderLocal
{
	UWorld* ResolveWorld(UObject* WorldContext)
	{
		if (UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr)
		{
			return World;
		}
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	TSoftObjectPtr<UWorld> WorldAsset(const FString& LevelPackageName)
	{
		const FString Name = FPackageName::GetShortName(LevelPackageName);
		return TSoftObjectPtr<UWorld>(FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *LevelPackageName, *Name)));
	}

	TWeakObjectPtr<ASceneCapture2D> CaptureCamera;
	TStrongObjectPtr<UTextureRenderTarget2D> CaptureTarget;
}

bool UBeyondVillageBuilderLibrary::UseExternalActors(UObject* WorldContext)
{
	UWorld* World = BeyondVillageBuilderLocal::ResolveWorld(WorldContext);
	ULevel* Level = World ? World->PersistentLevel.Get() : nullptr;
	if (!Level)
	{
		return false;
	}
	if (!Level->IsUsingExternalActors())
	{
		Level->ConvertAllActorsToPackaging(true);
		Level->SetUseExternalActors(true);
		Level->MarkPackageDirty();
	}
	return Level->IsUsingExternalActors();
}

FString UBeyondVillageBuilderLibrary::GetPackedPrefabPath(const FString& LevelPackageName)
{
	return FPaths::GetPath(LevelPackageName) / (FPackedLevelActorBuilder::GetPackedBPPrefix() + FPackageName::GetShortName(LevelPackageName));
}

UBlueprint* UBeyondVillageBuilderLibrary::PackPrefab(const FString& LevelPackageName)
{
	if (!FPackageName::DoesPackageExist(LevelPackageName))
	{
		UE_LOG(LogBeyondEditor, Warning, TEXT("Village builder: no prefab level %s to pack"), *LevelPackageName);
		return nullptr;
	}
	const FString BlueprintPath = GetPackedPrefabPath(LevelPackageName);
	const FString BlueprintName = FPackageName::GetShortName(BlueprintPath);
	const TSoftObjectPtr<UWorld> World = BeyondVillageBuilderLocal::WorldAsset(LevelPackageName);

	// Made here when new: the engine's own maker shows it in the Content Browser, which a commandlet doesn't have
	UBlueprint* Blueprint = FPackageName::DoesPackageExist(BlueprintPath)
		? LoadObject<UBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *BlueprintPath, *BlueprintName))
		: nullptr;
	if (!Blueprint)
	{
		UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
		Factory->ParentClass = APackedLevelActor::StaticClass();
		Factory->bSkipClassPicker = true;
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		Blueprint = Cast<UBlueprint>(AssetTools.CreateAsset(BlueprintName, FPaths::GetPath(BlueprintPath), UBlueprint::StaticClass(), Factory));
		if (!Blueprint)
		{
			UE_LOG(LogBeyondEditor, Warning, TEXT("Village builder: could not make %s"), *BlueprintPath);
			return nullptr;
		}
		CastChecked<APackedLevelActor>(Blueprint->GeneratedClass->GetDefaultObject())->SetWorldAsset(World);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}

	TSharedPtr<FPackedLevelActorBuilder> Builder = FPackedLevelActorBuilder::CreateDefaultBuilder();
	if (!Builder.IsValid() || !Builder->CreateOrUpdateBlueprint(World, TSoftObjectPtr<UBlueprint>(Blueprint), false, false))
	{
		UE_LOG(LogBeyondEditor, Warning, TEXT("Village builder: could not pack %s"), *LevelPackageName);
		return nullptr;
	}
	return Blueprint;
}

AActor* UBeyondVillageBuilderLibrary::PlacePrefab(UObject* WorldContext, const FString& LevelPackageName, FTransform Transform, const FString& Label, bool bPacked)
{
	UWorld* World = BeyondVillageBuilderLocal::ResolveWorld(WorldContext);
	if (!World || !FPackageName::DoesPackageExist(LevelPackageName))
	{
		UE_LOG(LogBeyondEditor, Warning, TEXT("Village builder: no prefab level %s"), *LevelPackageName);
		return nullptr;
	}

	AActor* Actor = nullptr;
	const FString BlueprintPath = GetPackedPrefabPath(LevelPackageName);
	if (bPacked && FPackageName::DoesPackageExist(BlueprintPath))
	{
		const FString BlueprintName = FPackageName::GetShortName(BlueprintPath);
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *BlueprintPath, *BlueprintName));
		if (Blueprint && Blueprint->GeneratedClass)
		{
			Actor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, Transform);
		}
	}
	if (!Actor)
	{
		ALevelInstance* Instance = World->SpawnActor<ALevelInstance>(ALevelInstance::StaticClass(), Transform);
		if (Instance)
		{
			Instance->SetWorldAsset(BeyondVillageBuilderLocal::WorldAsset(LevelPackageName));
			// Loaded now, not on the next editor tick (a commandlet may capture or save before that)
			if (ULevelInstanceSubsystem* LevelInstances = World->GetSubsystem<ULevelInstanceSubsystem>())
			{
				LevelInstances->BlockLoadLevelInstance(Instance);
			}
		}
		Actor = Instance;
	}
	if (Actor && !Label.IsEmpty())
	{
		Actor->SetActorLabel(Label);
	}
	return Actor;
}

FString UBeyondVillageBuilderLibrary::GetPrefabLevel(AActor* Actor)
{
	const ILevelInstanceInterface* Instance = Cast<ILevelInstanceInterface>(Actor);
	return Instance ? Instance->GetWorldAsset().GetLongPackageName() : FString();
}

bool UBeyondVillageBuilderLibrary::CaptureView(UObject* WorldContext, FVector Location, FRotator Rotation, const FString& FilePath, float FOV,
	int32 Width, int32 Height, float ExposureEV, float OrthoWidth)
{
	using namespace BeyondVillageBuilderLocal;
	UWorld* World = ResolveWorld(WorldContext);
	if (!World || !FApp::CanEverRender())
	{
		UE_LOG(LogBeyondEditor, Warning, TEXT("Village builder: nothing to capture without rendering (run with -AllowCommandletRendering, no -NullRHI)"));
		return false;
	}
	Width = FMath::Clamp(Width, 16, 4096);
	Height = FMath::Clamp(Height, 16, 4096);

	// Materials still compiling render with the default material; streamed textures start blurry
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		It->PrestreamTextures(30.0f, true);
	}

	// One camera and target, reused: every new capture view keeps its own shadow and history memory
	UTextureRenderTarget2D* Target = CaptureTarget.Get();
	if (!Target || Target->SizeX != Width || Target->SizeY != Height)
	{
		Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
		Target->RenderTargetFormat = RTF_RGBA8_SRGB;
		Target->ClearColor = FLinearColor::Black;
		Target->InitAutoFormat(Width, Height);
		Target->UpdateResourceImmediate(true);
		CaptureTarget.Reset(Target);
	}
	ASceneCapture2D* Camera = CaptureCamera.Get();
	if (!Camera || Camera->GetWorld() != World)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags = RF_Transient;
		Camera = World->SpawnActor<ASceneCapture2D>(Location, Rotation, Params);
		if (!Camera)
		{
			return false;
		}
		Camera->SetActorLabel(TEXT("BeyondCaptureCamera"));
		CaptureCamera = Camera;
		USceneCaptureComponent2D* Capture = Camera->GetCaptureComponent2D();
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->bAlwaysPersistRenderingState = true;
		Capture->ShowFlags.SetTemporalAA(false);
		Capture->PostProcessBlendWeight = 1.0f;
		FPostProcessSettings& Post = Capture->PostProcessSettings;
		Post.bOverride_AutoExposureMethod = true;
		Post.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		Post.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Post.AutoExposureApplyPhysicalCameraExposure = false;
		Post.bOverride_DynamicGlobalIlluminationMethod = true;
		Post.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
		Post.bOverride_ReflectionMethod = true;
		Post.ReflectionMethod = EReflectionMethod::None;
		Post.bOverride_MotionBlurAmount = true;
		Post.MotionBlurAmount = 0.0f;
	}
	Camera->SetActorLocationAndRotation(Location, Rotation);
	USceneCaptureComponent2D* Capture = Camera->GetCaptureComponent2D();
	Capture->TextureTarget = Target;
	Capture->FOVAngle = FOV;
	Capture->ProjectionType = OrthoWidth > 0.0f ? ECameraProjectionMode::Orthographic : ECameraProjectionMode::Perspective;
	Capture->OrthoWidth = FMath::Max(OrthoWidth, 1.0f);
	Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
	Capture->PostProcessSettings.AutoExposureBias = ExposureEV;

	// A few frames: shadows and streaming settle
	for (int32 Frame = 0; Frame < 4; ++Frame)
	{
		IStreamingManager::Get().Tick(0.1f, true);
		IStreamingManager::Get().StreamAllResources(5.0f);
		Capture->CaptureScene();
		FlushRenderingCommands();
	}

	TArray<FColor> Pixels;
	FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
	if (!Resource || !Resource->ReadPixels(Pixels) || Pixels.Num() != Width * Height)
	{
		UE_LOG(LogBeyondEditor, Warning, TEXT("Village builder: could not read the capture back"));
		return false;
	}
	for (FColor& Pixel : Pixels)
	{
		Pixel.A = 255;
	}

	IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(EImageFormat::PNG);
	if (!Wrapper.IsValid() || !Wrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
	{
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
	const bool bSaved = FFileHelper::SaveArrayToFile(Wrapper->GetCompressed(), *FilePath);
	UE_LOG(LogBeyondEditor, Display, TEXT("Village builder: captured %s"), *FilePath);
	return bSaved;
}

void UBeyondVillageBuilderLibrary::ReleaseCapture()
{
	using namespace BeyondVillageBuilderLocal;
	if (ASceneCapture2D* Camera = CaptureCamera.Get())
	{
		if (UWorld* World = Camera->GetWorld())
		{
			World->EditorDestroyActor(Camera, false);
		}
	}
	CaptureCamera.Reset();
	CaptureTarget.Reset();
}
