// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondWorldBuilderLibrary.h"
#include "WorldBeyondEditor.h"
#include "AI/NavigationSystemConfig.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/ParallelFor.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Landscape.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeSubsystem.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "UObject/Package.h"
#include "World/BeyondHeightfieldGenerator.h"
#include "World/BeyondOpenWorldNavigationSystem.h"
#include "World/BeyondWorldLayout.h"
#include "World/BeyondWorldMapData.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionHelpers.h"

namespace BeyondWorldBuilderLocal
{
	TUniquePtr<FBeyondHeightfield> CachedField;
	TWeakObjectPtr<UBeyondWorldLayout> CachedLayout;
	TArray<FWorldPartitionReference> LoadedProxies;
	// Files of destroyed external actors; deleted after the save if the save left them behind
	TArray<FString> PendingDeletes;

	UWorld* ResolveWorld(UObject* WorldContext)
	{
		if (UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr)
		{
			return World;
		}
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	const FBeyondHeightfield* GetField(UBeyondWorldLayout* Layout)
	{
		if (!Layout)
		{
			return nullptr;
		}
		if (!CachedField || CachedLayout.Get() != Layout)
		{
			CachedField = MakeUnique<FBeyondHeightfield>();
			FBeyondHeightfieldGenerator::Generate(*Layout, *CachedField);
			CachedLayout = Layout;
		}
		return CachedField.Get();
	}

	bool SavePng(const FString& Path, const void* Data, int64 Bytes, int32 Width, int32 Height, ERGBFormat Format, int32 BitDepth)
	{
		IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(EImageFormat::PNG);
		if (!Wrapper.IsValid() || !Wrapper->SetRaw(Data, Bytes, Width, Height, Format, BitDepth))
		{
			return false;
		}
		const TArray64<uint8> Compressed = Wrapper->GetCompressed();
		return FFileHelper::SaveArrayToFile(Compressed, *Path);
	}

	uint32 HashCell(int32 X, int32 Y, int32 Seed)
	{
		uint32 H = static_cast<uint32>(X) * 73856093u ^ static_cast<uint32>(Y) * 19349663u ^ static_cast<uint32>(Seed) * 83492791u;
		H ^= H >> 13;
		H *= 0x5bd1e995u;
		H ^= H >> 15;
		return H;
	}

	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, float T)
	{
		return FLinearColor(FMath::Lerp(A.R, B.R, T), FMath::Lerp(A.G, B.G, T), FMath::Lerp(A.B, B.B, T), 1.0f);
	}

	// The painted map at Resolution px: north up, east right
	void PaintMap(const FBeyondHeightfield& Field, const UBeyondWorldLayout& Layout, int32 Resolution, TArray<FColor>& Out)
	{
		Out.SetNumUninitialized(Resolution * Resolution);
		const float Span = static_cast<float>(Field.Size - 1);
		const float MetresPerPixel = Span / Resolution;
		const FLinearColor WaterColour(0.30f, 0.45f, 0.56f);
		const FLinearColor IceColour(0.78f, 0.86f, 0.92f);
		const FLinearColor LavaColour(0.88f, 0.32f, 0.08f);
		const FLinearColor RoadColour(0.80f, 0.70f, 0.52f);
		const FLinearColor InkColour(0.18f, 0.13f, 0.08f);
		const FVector3f Light = FVector3f(-1.0f, 1.0f, 1.3f).GetSafeNormal();

		auto VertexAt = [&Field](float East, float North, int32& OutX, int32& OutY)
		{
			OutX = FMath::Clamp(FMath::RoundToInt(East + Field.HalfExtent), 0, Field.Size - 1);
			OutY = FMath::Clamp(FMath::RoundToInt(Field.HalfExtent - North), 0, Field.Size - 1);
		};

		ParallelFor(Resolution, [&](int32 PY)
		{
			for (int32 PX = 0; PX < Resolution; ++PX)
			{
				const float East = -Field.HalfExtent + (PX + 0.5f) * MetresPerPixel;
				const float North = Field.HalfExtent - (PY + 0.5f) * MetresPerPixel;
				const FVector2D P(East, North);
				const float H = Field.SampleHeight(P);
				int32 VX, VY;
				VertexAt(East, North, VX, VY);
				const int32 Vertex = Field.Index(VX, VY);

				// Ground colour by layer
				FLinearColor Colour(0.0f, 0.0f, 0.0f, 1.0f);
				for (int32 Layer = 0; Layer < BeyondTerrainLayers::Count; ++Layer)
				{
					const float W = Field.Weights[Layer][Vertex] / 255.0f;
					if (W > 0.0f)
					{
						Colour += BeyondTerrainLayers::GetMapColour(Layer) * W;
					}
				}
				// Region tint
				FLinearColor Tint(0.0f, 0.0f, 0.0f, 1.0f);
				float TintWeight = 0.0f;
				bool bWooded = false;
				for (int32 Region = 0; Region < Layout.Regions.Num(); ++Region)
				{
					const float W = Field.SampleRegionWeight(Region, P);
					Tint += Layout.Regions[Region].MapColour * W;
					TintWeight += W;
					const EBeyondTerrainStyle Style = Layout.Regions[Region].Style;
					bWooded |= W > 0.5f && (Style == EBeyondTerrainStyle::Forest || Style == EBeyondTerrainStyle::Frost || Style == EBeyondTerrainStyle::Blight);
				}
				if (TintWeight > 0.0f)
				{
					Colour = Mix(Colour, Colour * (Tint / TintWeight) * 1.7f, 0.28f);
				}

				// Hillshade from the north-west
				const float Step = FMath::Max(MetresPerPixel, 1.0f);
				const float DX = (Field.SampleHeight(P + FVector2D(Step, 0.0)) - Field.SampleHeight(P - FVector2D(Step, 0.0))) / (2.0f * Step);
				const float DY = (Field.SampleHeight(P + FVector2D(0.0, Step)) - Field.SampleHeight(P - FVector2D(0.0, Step))) / (2.0f * Step);
				const FVector3f Normal = FVector3f(-DX * 2.2f, -DY * 2.2f, 1.0f).GetSafeNormal();
				const float Shade = FMath::Clamp(FVector3f::DotProduct(Normal, Light), 0.0f, 1.0f);
				Colour *= 0.55f + 0.62f * Shade;

				// Contours every 25 m
				const float HRight = Field.SampleHeight(P + FVector2D(MetresPerPixel, 0.0));
				const float HDown = Field.SampleHeight(P - FVector2D(0.0, MetresPerPixel));
				if (FMath::FloorToInt(H / 25.0f) != FMath::FloorToInt(HRight / 25.0f) || FMath::FloorToInt(H / 25.0f) != FMath::FloorToInt(HDown / 25.0f))
				{
					Colour = Mix(Colour, InkColour, 0.18f);
				}

				// Forest stipple: small dark dots in wooded regions, off roads and rock
				const bool bOpenGround = Field.Weights[BeyondTerrainLayers::Rock][Vertex] < 100 && Field.RoadMask[Vertex] < 40
					&& Field.Weights[BeyondTerrainLayers::Dirt][Vertex] < 120 && Field.Weights[BeyondTerrainLayers::Cobble][Vertex] < 120;
				if (bWooded && bOpenGround)
				{
					const int32 Cell = 7;
					const int32 CX = PX / Cell;
					const int32 CY = PY / Cell;
					const uint32 Hash = HashCell(CX, CY, Layout.Seed);
					if ((Hash & 0xFF) < 110)
					{
						const float OX = CX * Cell + 1.5f + ((Hash >> 8) & 0x3);
						const float OY = CY * Cell + 1.5f + ((Hash >> 12) & 0x3);
						if (FMath::Square(PX - OX) + FMath::Square(PY - OY) <= 2.3f)
						{
							Colour *= 0.62f;
						}
					}
				}

				// Roads
				const uint8 Road = Field.RoadMask[Vertex];
				if (Road > 110)
				{
					Colour = Mix(Colour, RoadColour * (0.85f + 0.3f * Shade), 0.85f);
				}
				else if (Road > 20)
				{
					Colour = Mix(Colour, InkColour, 0.25f);
				}

				// Water and lava
				for (const FBeyondLayoutLake& Lake : Layout.Lakes)
				{
					const float* Level = Field.LakeLevels.Find(Lake.Id);
					if (Level && FVector2D::Distance(P, Lake.Centre) < Lake.Radius + 8.0f && H < *Level)
					{
						const float Depth = FMath::Clamp((*Level - H) / FMath::Max(Lake.Depth, 1.0f), 0.0f, 1.0f);
						const FLinearColor Surface = Lake.bFrozen ? IceColour : WaterColour;
						Colour = Mix(Surface, Surface * 0.7f, Depth);
						if (*Level - H < 0.6f)
						{
							Colour = Mix(Colour, InkColour, 0.45f);
						}
					}
				}
				if (Field.bHasVolcano && H < Field.LavaLevel && FVector2D::Distance(P, Layout.Volcano.Centre) < Layout.Volcano.RimRadius)
				{
					Colour = LavaColour * (0.85f + 0.3f * Shade);
				}

				// Paper: grain and a soft vignette
				const float Grain = ((HashCell(PX, PY, 7) & 0xFF) / 255.0f - 0.5f) * 0.05f;
				const float U = (PX + 0.5f) / Resolution - 0.5f;
				const float V = (PY + 0.5f) / Resolution - 0.5f;
				const float Vignette = 1.0f - 0.35f * FMath::Square(FMath::Clamp(FMath::Sqrt(U * U + V * V) * 1.45f, 0.0f, 1.0f));
				Colour = (Colour * (1.0f + Grain)) * Vignette;
				Colour.A = 1.0f;
				Out[PY * Resolution + PX] = Colour.ToFColor(false);
			}
		});
	}

	template <typename T>
	T* LoadExisting(const FString& AssetPath)
	{
		const FString ObjectPath = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
		return LoadObject<T>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

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

	UTexture2D* MakeTexture(const FString& AssetPath, int32 Resolution, const TArray<FColor>& Pixels)
	{
		bool bNew = false;
		UTexture2D* Texture = MakeAsset<UTexture2D>(AssetPath, bNew);
		Texture->PreEditChange(nullptr);
		Texture->Source.Init(Resolution, Resolution, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
		Texture->SRGB = true;
		Texture->CompressionSettings = TC_Default;
		Texture->LODGroup = TEXTUREGROUP_UI;
		Texture->MipGenSettings = TMGS_SimpleAverage;
		Texture->NeverStream = true;
		Texture->PostEditChange();
		if (bNew)
		{
			FAssetRegistryModule::AssetCreated(Texture);
		}
		Texture->MarkPackageDirty();
		return Texture;
	}
}

FString UBeyondWorldBuilderLibrary::WriteTerrainPreview(UBeyondWorldLayout* Layout)
{
	using namespace BeyondWorldBuilderLocal;
	const FBeyondHeightfield* Field = GetField(Layout);
	if (!Field)
	{
		return FString();
	}
	const FString Folder = FPaths::ProjectSavedDir() / TEXT("WorldBuilder");
	IFileManager::Get().MakeDirectory(*Folder, true);

	const TArray<uint16> Heights = Field->ToLandscapeHeights(Layout->HeightScaleZ);
	FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8*>(Heights.GetData()), Heights.Num() * sizeof(uint16)),
		*(Folder / TEXT("heightmap.r16")));
	SavePng(Folder / TEXT("heightmap.png"), Heights.GetData(), Heights.Num() * sizeof(uint16), Field->Size, Field->Size, ERGBFormat::Gray, 16);

	TArray<FColor> Preview;
	PaintMap(*Field, *Layout, 1024, Preview);
	SavePng(Folder / TEXT("preview.png"), Preview.GetData(), Preview.Num() * sizeof(FColor), 1024, 1024, ERGBFormat::BGRA, 8);
	for (int32 Layer = 0; Layer < BeyondTerrainLayers::Count; ++Layer)
	{
		SavePng(Folder / FString::Printf(TEXT("weight_%s.png"), *BeyondTerrainLayers::GetName(Layer).ToString()), Field->Weights[Layer].GetData(),
			Field->Weights[Layer].Num(), Field->Size, Field->Size, ERGBFormat::Gray, 8);
	}
	UE_LOG(LogBeyondEditor, Display, TEXT("World builder: wrote the terrain preview to %s"), *Folder);
	return Folder;
}

UMaterial* UBeyondWorldBuilderLibrary::CreateGreyboxLandscapeMaterial(const FString& AssetPath, bool bRebuild)
{
	using namespace BeyondWorldBuilderLocal;
	bool bNew = false;
	UMaterial* Material = MakeAsset<UMaterial>(AssetPath, bNew);
	if (!bNew && !bRebuild)
	{
		return Material;
	}
	UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);

	UMaterialExpressionLandscapeLayerBlend* Blend = Cast<UMaterialExpressionLandscapeLayerBlend>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionLandscapeLayerBlend::StaticClass(), -300, 0));
	for (int32 Layer = 0; Layer < BeyondTerrainLayers::Count; ++Layer)
	{
		UMaterialExpressionConstant3Vector* Colour = Cast<UMaterialExpressionConstant3Vector>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionConstant3Vector::StaticClass(), -700, Layer * 90 - 400));
		Colour->Constant = BeyondTerrainLayers::GetGreyboxColour(Layer);
		FLayerBlendInput& Input = Blend->Layers.AddDefaulted_GetRef();
		Input.LayerName = BeyondTerrainLayers::GetName(Layer);
		Input.BlendType = LB_WeightBlend;
		Input.PreviewWeight = Layer == 0 ? 1.0f : 0.0f;
		Input.LayerInput.Expression = Colour;
	}

	// Large soft blotches so flat colour reads as ground
	UMaterialExpressionNoise* Noise = Cast<UMaterialExpressionNoise>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionNoise::StaticClass(), -300, 300));
	Noise->Scale = 0.0025f;
	Noise->Levels = 3;
	Noise->OutputMin = 0.8f;
	Noise->OutputMax = 1.15f;
	UMaterialExpressionMultiply* Multiply = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionMultiply::StaticClass(), -100, 100));
	Multiply->A.Expression = Blend;
	Multiply->B.Expression = Noise;
	UMaterialEditingLibrary::ConnectMaterialProperty(Multiply, FString(), MP_BaseColor);

	UMaterialExpressionConstant* Roughness = Cast<UMaterialExpressionConstant>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionConstant::StaticClass(), -100, 300));
	Roughness->R = 0.92f;
	UMaterialEditingLibrary::ConnectMaterialProperty(Roughness, FString(), MP_Roughness);

	UMaterialEditingLibrary::RecompileMaterial(Material);
	if (bNew)
	{
		FAssetRegistryModule::AssetCreated(Material);
	}
	Material->MarkPackageDirty();
	UE_LOG(LogBeyondEditor, Display, TEXT("World builder: built %s (%d layers)"), *AssetPath, static_cast<int32>(BeyondTerrainLayers::Count));
	return Material;
}

TArray<ULandscapeLayerInfoObject*> UBeyondWorldBuilderLibrary::CreateLayerInfos(const FString& Folder)
{
	using namespace BeyondWorldBuilderLocal;
	TArray<ULandscapeLayerInfoObject*> Infos;
	for (int32 Layer = 0; Layer < BeyondTerrainLayers::Count; ++Layer)
	{
		const FName Name = BeyondTerrainLayers::GetName(Layer);
		bool bNew = false;
		ULandscapeLayerInfoObject* Info = MakeAsset<ULandscapeLayerInfoObject>(Folder / FString::Printf(TEXT("LI_%s"), *Name.ToString()), bNew);
		if (bNew)
		{
			Info->SetLayerName(Name, false);
			FAssetRegistryModule::AssetCreated(Info);
			Info->MarkPackageDirty();
		}
		Infos.Add(Info);
	}
	return Infos;
}

ALandscape* UBeyondWorldBuilderLibrary::CreateLandscape(UObject* WorldContext, UBeyondWorldLayout* Layout, UMaterialInterface* Material,
	const TArray<ULandscapeLayerInfoObject*>& LayerInfos, int32 GridSizeInComponents)
{
	using namespace BeyondWorldBuilderLocal;
	UWorld* World = ResolveWorld(WorldContext);
	const FBeyondHeightfield* Field = GetField(Layout);
	if (!World || !Field)
	{
		return nullptr;
	}
	const int32 Size = Field->Size;
	const FVector Location(-Field->HalfExtent * 100.0f, -Field->HalfExtent * 100.0f, 0.0f);

	FActorSpawnParameters Params;
	Params.Name = MakeUniqueObjectName(World->PersistentLevel, ALandscape::StaticClass(), TEXT("WB_Landscape"));
	ALandscape* Landscape = World->SpawnActor<ALandscape>(ALandscape::StaticClass(), FTransform(FRotator::ZeroRotator, Location), Params);
	if (!Landscape)
	{
		return nullptr;
	}
	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorRelativeScale3D(FVector(100.0f, 100.0f, Layout->HeightScaleZ));
	Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((Size * Size) / (2048 * 2048) + 1), static_cast<uint32>(2));

	TMap<FGuid, TArray<uint16>> HeightData;
	HeightData.Add(FGuid(), Field->ToLandscapeHeights(Layout->HeightScaleZ));
	TArray<FLandscapeImportLayerInfo> ImportLayers;
	for (int32 Layer = 0; Layer < BeyondTerrainLayers::Count && Layer < LayerInfos.Num(); ++Layer)
	{
		FLandscapeImportLayerInfo& Import = ImportLayers.Emplace_GetRef(BeyondTerrainLayers::GetName(Layer));
		Import.LayerInfo = LayerInfos[Layer];
		Import.LayerData = Field->Weights[Layer];
	}
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> LayerData;
	LayerData.Add(FGuid(), MoveTemp(ImportLayers));

	Landscape->Import(FGuid::NewGuid(), 0, 0, Size - 1, Size - 1, Layout->SectionsPerComponent, Layout->QuadsPerSection, HeightData, TEXT(""), LayerData,
		ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	Landscape->SetActorLabel(TEXT("WB_Landscape"));

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (Info)
	{
		Info->UpdateLayerInfoMap(Landscape);
		for (ULandscapeLayerInfoObject* LayerInfo : LayerInfos)
		{
			if (!LayerInfo)
			{
				continue;
			}
			Landscape->AddTargetLayer(LayerInfo->GetLayerName(), FLandscapeTargetLayerSettings(LayerInfo));
			const int32 Index = Info->GetLayerInfoIndex(LayerInfo->GetLayerName());
			if (Info->Layers.IsValidIndex(Index))
			{
				Info->Layers[Index].LayerInfoObj = LayerInfo;
			}
		}
	}

	// The import is the "Generated" edit layer; hand sculpting goes on "Sculpt" above it
	if (ULandscapeEditLayerBase* Generated = Landscape->GetEditLayer(0))
	{
		Generated->SetName(TEXT("Generated"), true);
	}
	Landscape->CreateLayer(TEXT("Sculpt"));

	if (Info && World->IsPartitionedWorld())
	{
		World->GetSubsystem<ULandscapeSubsystem>()->ChangeGridSize(Info, static_cast<uint32>(FMath::Max(GridSizeInComponents, 1)));
	}
	Landscape->ForceUpdateLayersContent();
	UE_LOG(LogBeyondEditor, Display, TEXT("World builder: landscape %d x %d (%d components, grid %d) made"), Size, Size,
		Layout->ComponentsPerSide * Layout->ComponentsPerSide, GridSizeInComponents);
	return Landscape;
}

int32 UBeyondWorldBuilderLibrary::DestroyActors(const TArray<AActor*>& Actors)
{
	int32 Destroyed = 0;
	for (AActor* Actor : Actors)
	{
		UWorld* World = Actor ? Actor->GetWorld() : nullptr;
		if (!World)
		{
			continue;
		}
		if (const UPackage* Package = Actor->GetExternalPackage())
		{
			BeyondWorldBuilderLocal::PendingDeletes.Add(FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()));
		}
		World->EditorDestroyActor(Actor, true);
		++Destroyed;
	}
	return Destroyed;
}

int32 UBeyondWorldBuilderLibrary::DeleteLandscapes(UObject* WorldContext)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	if (!World)
	{
		return 0;
	}
	TArray<ALandscapeProxy*> Proxies;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		Proxies.Add(*It);
	}
	// Streaming proxies first, the landscape last. The loading references stay until the world is saved: releasing them
	// now would unload the packages and lose the deletion
	Proxies.Sort([](const ALandscapeProxy& A, const ALandscapeProxy& B) { return !A.IsA<ALandscape>() && B.IsA<ALandscape>(); });
	for (ALandscapeProxy* Proxy : Proxies)
	{
		if (const UPackage* Package = Proxy->GetExternalPackage())
		{
			BeyondWorldBuilderLocal::PendingDeletes.Add(FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()));
		}
		World->EditorDestroyActor(Proxy, true);
	}
	return Proxies.Num();
}

int32 UBeyondWorldBuilderLibrary::LoadActorsWithLabelPrefix(UObject* WorldContext, const FString& Prefix)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
	if (!WorldPartition)
	{
		return 0;
	}
	TArray<FWorldPartitionActorDescInstance*> Descs;
	FWorldPartitionHelpers::ForEachActorDescInstance<AActor>(WorldPartition, [&Descs, &Prefix](const FWorldPartitionActorDescInstance* Desc)
	{
		if (Desc->GetActorLabelString().StartsWith(Prefix))
		{
			Descs.Add(const_cast<FWorldPartitionActorDescInstance*>(Desc));
		}
		return true;
	});
	for (FWorldPartitionActorDescInstance* Desc : Descs)
	{
		BeyondWorldBuilderLocal::LoadedProxies.Emplace(Desc);
	}
	UE_LOG(LogBeyondEditor, Display, TEXT("World builder: loaded %d actors labelled %s*"), Descs.Num(), *Prefix);
	return Descs.Num();
}

int32 UBeyondWorldBuilderLibrary::LoadLandscapeProxies(UObject* WorldContext)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
	if (!WorldPartition)
	{
		int32 Count = 0;
		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			++Count;
		}
		return Count;
	}
	TArray<FWorldPartitionActorDescInstance*> Descs;
	FWorldPartitionHelpers::ForEachActorDescInstance<ALandscapeProxy>(WorldPartition, [&Descs](const FWorldPartitionActorDescInstance* Desc)
	{
		Descs.Add(const_cast<FWorldPartitionActorDescInstance*>(Desc));
		return true;
	});
	for (FWorldPartitionActorDescInstance* Desc : Descs)
	{
		// The reference keeps the proxy loaded until the cache is cleared
		BeyondWorldBuilderLocal::LoadedProxies.Emplace(Desc);
	}
	int32 Loaded = 0;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		++Loaded;
	}
	UE_LOG(LogBeyondEditor, Display, TEXT("World builder: %d landscape proxies loaded (%d known)"), Loaded, Descs.Num());
	return Loaded;
}

bool UBeyondWorldBuilderLibrary::GetGroundHeight(UObject* WorldContext, UBeyondWorldLayout* Layout, float X, float Y, float& OutZ)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	if (World)
	{
		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			const TOptional<float> Height = It->GetHeightAtLocation(FVector(X, Y, 0.0f), EHeightfieldSource::Editor);
			if (Height.IsSet())
			{
				OutZ = Height.GetValue();
				return true;
			}
		}
	}
	if (const FBeyondHeightfield* Field = BeyondWorldBuilderLocal::GetField(Layout))
	{
		OutZ = Field->SampleHeight(FVector2D(X / 100.0f, -Y / 100.0f)) * 100.0f;
		return true;
	}
	return false;
}

float UBeyondWorldBuilderLibrary::GetLayoutHeight(UBeyondWorldLayout* Layout, float X, float Y)
{
	const FBeyondHeightfield* Field = BeyondWorldBuilderLocal::GetField(Layout);
	return Field ? Field->SampleHeight(FVector2D(X / 100.0f, -Y / 100.0f)) * 100.0f : 0.0f;
}

bool UBeyondWorldBuilderLibrary::GetLakeLevel(UBeyondWorldLayout* Layout, FName LakeId, float& OutZ)
{
	const FBeyondHeightfield* Field = BeyondWorldBuilderLocal::GetField(Layout);
	const float* Level = Field ? Field->LakeLevels.Find(LakeId) : nullptr;
	if (!Level)
	{
		return false;
	}
	OutZ = *Level * 100.0f;
	return true;
}

UBeyondWorldMapData* UBeyondWorldBuilderLibrary::BakeWorldMap(UBeyondWorldLayout* Layout, const FString& Folder, int32 Resolution)
{
	using namespace BeyondWorldBuilderLocal;
	const FBeyondHeightfield* Field = GetField(Layout);
	if (!Field)
	{
		return nullptr;
	}
	Resolution = FMath::Clamp(Resolution, 256, 4096);
	TArray<FColor> Pixels;
	PaintMap(*Field, *Layout, Resolution, Pixels);
	UTexture2D* MapTexture = MakeTexture(Folder / TEXT("T_WorldMap"), Resolution, Pixels);

	bool bNew = false;
	UBeyondWorldMapData* Data = MakeAsset<UBeyondWorldMapData>(Folder / TEXT("DA_WorldMap"), bNew);
	Data->MapTexture = MapTexture;
	const double Half = Field->HalfExtent * 100.0;
	Data->WorldMin = FVector2D(-Half, -Half);
	Data->WorldMax = FVector2D(Half, Half);
	Data->RegionFogMasks.Reset();

	// A soft white-with-alpha mask per region (the map darkens it until the region is found)
	constexpr int32 MaskResolution = 512;
	for (int32 Region = 0; Region < Layout->Regions.Num(); ++Region)
	{
		TArray<FColor> Mask;
		Mask.SetNumUninitialized(MaskResolution * MaskResolution);
		const float MetresPerPixel = (Field->Size - 1) / static_cast<float>(MaskResolution);
		for (int32 PY = 0; PY < MaskResolution; ++PY)
		{
			for (int32 PX = 0; PX < MaskResolution; ++PX)
			{
				const FVector2D P(-Field->HalfExtent + (PX + 0.5f) * MetresPerPixel, Field->HalfExtent - (PY + 0.5f) * MetresPerPixel);
				const float W = Field->SampleRegionWeight(Region, P);
				Mask[PY * MaskResolution + PX] = FColor(255, 255, 255, static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(W * 255.0f), 0, 255)));
			}
		}
		const FName RegionId = Layout->Regions[Region].RegionId;
		Data->RegionFogMasks.Add(RegionId, MakeTexture(Folder / FString::Printf(TEXT("T_WorldMapFog_%s"), *RegionId.ToString()), MaskResolution, Mask));
	}
	if (bNew)
	{
		FAssetRegistryModule::AssetCreated(Data);
	}
	Data->MarkPackageDirty();
	UE_LOG(LogBeyondEditor, Display, TEXT("World builder: painted the world map (%d px, %d fog masks)"), Resolution, Data->RegionFogMasks.Num());
	return Data;
}

bool UBeyondWorldBuilderLibrary::ConfigureOpenWorld(UObject* WorldContext)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	AWorldSettings* Settings = World ? World->GetWorldSettings() : nullptr;
	if (!Settings)
	{
		return false;
	}
	Settings->Modify();
	UNavigationSystemConfig* Config = Settings->GetNavigationSystemConfig();
	if (!Config)
	{
		Config = NewObject<UNavigationSystemModuleConfig>(Settings);
		if (FObjectProperty* Property = FindFProperty<FObjectProperty>(AWorldSettings::StaticClass(), TEXT("NavigationSystemConfig")))
		{
			Property->SetObjectPropertyValue_InContainer(Settings, Config);
		}
	}
	Config->NavigationSystemClass = FSoftClassPath(UBeyondOpenWorldNavigationSystem::StaticClass());

	ARecastNavMesh* NavMesh = nullptr;
	for (TActorIterator<ARecastNavMesh> It(World); It; ++It)
	{
		NavMesh = *It;
		break;
	}
	if (!NavMesh)
	{
		NavMesh = World->SpawnActor<ARecastNavMesh>();
	}
	if (NavMesh)
	{
		NavMesh->Modify();
		if (FEnumProperty* Generation = FindFProperty<FEnumProperty>(ANavigationData::StaticClass(), TEXT("RuntimeGeneration")))
		{
			Generation->GetUnderlyingProperty()->SetIntPropertyValue(Generation->ContainerPtrToValuePtr<void>(NavMesh), static_cast<int64>(ERuntimeGenerationType::Dynamic));
		}
		else if (FByteProperty* GenerationByte = FindFProperty<FByteProperty>(ANavigationData::StaticClass(), TEXT("RuntimeGeneration")))
		{
			GenerationByte->SetPropertyValue_InContainer(NavMesh, static_cast<uint8>(ERuntimeGenerationType::Dynamic));
		}
		if (FBoolProperty* FixedPool = FindFProperty<FBoolProperty>(ARecastNavMesh::StaticClass(), TEXT("bFixedTilePoolSize")))
		{
			FixedPool->SetPropertyValue_InContainer(NavMesh, true);
		}
		if (FIntProperty* PoolSize = FindFProperty<FIntProperty>(ARecastNavMesh::StaticClass(), TEXT("TilePoolSize")))
		{
			PoolSize->SetPropertyValue_InContainer(NavMesh, 4096);
		}
		if (NavMesh->CanChangeIsSpatiallyLoadedFlag())
		{
			NavMesh->SetIsSpatiallyLoaded(false);
		}
	}
	UE_LOG(LogBeyondEditor, Display, TEXT("World builder: navigation around the demigods (BeyondOpenWorldNavigationSystem, dynamic RecastNavMesh)"));
	return true;
}

bool UBeyondWorldBuilderLibrary::SaveWorld(UObject* WorldContext)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	if (!World)
	{
		return false;
	}
	const bool bSaved = UEditorLoadingAndSavingUtils::SaveDirtyPackages(true, true);

	// Destroyed actors whose files are still on disk (their package had nothing left to save)
	int32 Deleted = 0;
	for (const FString& File : BeyondWorldBuilderLocal::PendingDeletes)
	{
		const FString Full = FPaths::ConvertRelativePathToFull(File);
		if (IFileManager::Get().FileExists(*Full) && IFileManager::Get().Delete(*Full, false, true))
		{
			++Deleted;
		}
	}
	BeyondWorldBuilderLocal::PendingDeletes.Reset();
	BeyondWorldBuilderLocal::LoadedProxies.Reset();
	UE_LOG(LogBeyondEditor, Display, TEXT("World builder: saved %s and its dirty packages (%s), %d deleted actor files removed"), *World->GetName(),
		bSaved ? TEXT("ok") : TEXT("some failed"), Deleted);
	return bSaved;
}

void UBeyondWorldBuilderLibrary::SetSpatiallyLoaded(AActor* Actor, bool bSpatiallyLoaded)
{
	if (Actor && Actor->CanChangeIsSpatiallyLoadedFlag())
	{
		Actor->SetIsSpatiallyLoaded(bSpatiallyLoaded);
	}
}

void UBeyondWorldBuilderLibrary::ClearTerrainCache()
{
	BeyondWorldBuilderLocal::CachedField.Reset();
	BeyondWorldBuilderLocal::CachedLayout.Reset();
	BeyondWorldBuilderLocal::LoadedProxies.Reset();
}
