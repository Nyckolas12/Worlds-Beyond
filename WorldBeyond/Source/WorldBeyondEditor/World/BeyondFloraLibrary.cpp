// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondFloraLibrary.h"
#include "WorldBeyondEditor.h"
#include "Algo/BinarySearch.h"
#include "Animation/TransformProviderData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "InstancedFoliage.h"
#include "InstancedFoliageActor.h"
#include "LandscapeProxy.h"
#include "Math/RandomStream.h"
#include "Misc/Crc.h"
#include "PhysicsEngine/BodySetup.h"
#include "World/BeyondFloraTile.h"
#include "World/BeyondHeightfieldGenerator.h"
#include "World/BeyondWorldBuilderInternal.h"
#include "World/BeyondWorldBuilderLibrary.h"
#include "World/BeyondWorldLayout.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionLHGrid.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"

namespace BeyondFloraLocal
{
	// The four regions in the order of a planting's Regions colour (R G B A)
	const FName RegionNames[4] = { TEXT("Forest"), TEXT("Frost"), TEXT("Molten"), TEXT("Blight") };

	int32 StyleSlot(EBeyondTerrainStyle Style)
	{
		switch (Style)
		{
		case EBeyondTerrainStyle::Frost: return 1;
		case EBeyondTerrainStyle::Molten: return 2;
		case EBeyondTerrainStyle::Blight: return 3;
		default: return 0;
		}
	}

	// Region weights by slot at a point (metres east / north)
	void RegionWeights(const FBeyondHeightfield& Field, const UBeyondWorldLayout& Layout, const FVector2D& P, float Out[4])
	{
		Out[0] = Out[1] = Out[2] = Out[3] = 0.0f;
		for (int32 Region = 0; Region < Layout.Regions.Num(); ++Region)
		{
			Out[StyleSlot(Layout.Regions[Region].Style)] += Field.SampleRegionWeight(Region, P);
		}
	}

	int32 Dominant(const float W[4])
	{
		int32 Best = 0;
		for (int32 Slot = 1; Slot < 4; ++Slot)
		{
			Best = W[Slot] > W[Best] ? Slot : Best;
		}
		return Best;
	}

	float Share(const FLinearColor& Regions, const float W[4])
	{
		return Regions.R * W[0] + Regions.G * W[1] + Regions.B * W[2] + Regions.A * W[3];
	}

	uint32 Hash(int32 X, int32 Y, uint32 Salt)
	{
		uint32 H = static_cast<uint32>(X) * 73856093u ^ static_cast<uint32>(Y) * 19349663u ^ Salt * 83492791u;
		H ^= H >> 13;
		H *= 0x5bd1e995u;
		H ^= H >> 15;
		return H;
	}

	// Three octaves of Perlin noise over the world: groves where it's low, clearings where it's high
	float GroveNoise(const FVector2D& P, float Scale, const FVector2D& Offset)
	{
		const FVector2D Q = P / FMath::Max(Scale, 10.0f) + Offset;
		return FMath::PerlinNoise2D(Q) + 0.5f * FMath::PerlinNoise2D(Q * 2.13 + FVector2D(17.3, -9.1))
			+ 0.25f * FMath::PerlinNoise2D(Q * 4.7 + FVector2D(-31.7, 5.3));
	}

	int32 VertexIndex(const FBeyondHeightfield& Field, const FVector2D& P)
	{
		const int32 X = FMath::Clamp(FMath::RoundToInt(P.X + Field.HalfExtent), 0, Field.Size - 1);
		const int32 Y = FMath::Clamp(FMath::RoundToInt(Field.HalfExtent - P.Y), 0, Field.Size - 1);
		return Field.Index(X, Y);
	}

	float LayerSum(const FBeyondHeightfield& Field, const TArray<int32>& Layers, int32 Vertex)
	{
		int32 Sum = 0;
		for (const int32 Layer : Layers)
		{
			Sum += Field.Weights[Layer][Vertex];
		}
		return Sum / 255.0f;
	}

	// The terrain's gradient (rise per metre east, north)
	FVector2D Gradient(const FBeyondHeightfield& Field, const FVector2D& P)
	{
		return FVector2D((Field.SampleHeight(P + FVector2D(1.0, 0.0)) - Field.SampleHeight(P - FVector2D(1.0, 0.0))) * 0.5f,
			(Field.SampleHeight(P + FVector2D(0.0, 1.0)) - Field.SampleHeight(P - FVector2D(0.0, 1.0))) * 0.5f);
	}

	TArray<int32> LayerIndices(const TArray<FName>& Names)
	{
		TArray<int32> Out;
		for (const FName& Name : Names)
		{
			for (int32 Layer = 0; Layer < BeyondTerrainLayers::Count; ++Layer)
			{
				if (BeyondTerrainLayers::GetName(Layer) == Name)
				{
					Out.Add(Layer);
				}
			}
		}
		return Out;
	}

	bool MeshBounds(const UObject* Mesh, FBoxSphereBounds& Out)
	{
		if (const UStaticMesh* Static = Cast<UStaticMesh>(Mesh))
		{
			Out = Static->GetBounds();
			return true;
		}
		if (const USkeletalMesh* Skeletal = Cast<USkeletalMesh>(Mesh))
		{
			Out = Skeletal->GetBounds();
			return true;
		}
		return false;
	}

	// Ground heights from the loaded landscape proxies (found by their bounds), the generated terrain where none is loaded
	// or where they disagree by more than 30 m (a proxy that isn't really there yet)
	struct FGround
	{
		const FBeyondHeightfield* Field = nullptr;
		TMap<FIntPoint, TArray<ALandscapeProxy*>> Cells;
		double CellSize = 12800.0;

		void Init(UWorld* World, const FBeyondHeightfield* InField)
		{
			Field = InField;
			if (!World)
			{
				return;
			}
			for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
			{
				const FBox Box = It->GetComponentsBoundingBox(true);
				if (!Box.IsValid)
				{
					continue;
				}
				for (int32 CY = FMath::FloorToInt(Box.Min.Y / CellSize); CY <= FMath::FloorToInt(Box.Max.Y / CellSize); ++CY)
				{
					for (int32 CX = FMath::FloorToInt(Box.Min.X / CellSize); CX <= FMath::FloorToInt(Box.Max.X / CellSize); ++CX)
					{
						Cells.FindOrAdd(FIntPoint(CX, CY)).Add(*It);
					}
				}
			}
		}

		// cm, Unreal X / Y
		float Z(double X, double Y) const
		{
			const float Layout = Field->SampleHeight(FVector2D(X / 100.0, -Y / 100.0)) * 100.0f;
			if (const TArray<ALandscapeProxy*>* Proxies = Cells.Find(FIntPoint(FMath::FloorToInt(X / CellSize), FMath::FloorToInt(Y / CellSize))))
			{
				for (ALandscapeProxy* Proxy : *Proxies)
				{
					const TOptional<float> Height = Proxy->GetHeightAtLocation(FVector(X, Y, 0.0), EHeightfieldSource::Editor);
					if (Height.IsSet())
					{
						return FMath::Abs(Height.GetValue() - Layout) <= 3000.0f ? Height.GetValue() : Layout;
					}
				}
			}
			return Layout;
		}

		FVector Normal(double X, double Y) const
		{
			const double Step = 150.0;
			const FVector DX(2.0 * Step, 0.0, Z(X + Step, Y) - Z(X - Step, Y));
			const FVector DY(0.0, 2.0 * Step, Z(X, Y + Step) - Z(X, Y - Step));
			const FVector N = FVector::CrossProduct(DX, DY).GetSafeNormal();
			return N.Z < 0.0 ? -N : N;
		}
	};

	// Footprints placed so far (metres east / north and radius), in 8 m cells
	struct FOccupancy
	{
		float Cell = 8.0f;
		float MaxRadius = 0.0f;
		TMap<FIntPoint, TArray<FVector3f>> Map;

		FIntPoint CellOf(const FVector2D& P) const
		{
			return FIntPoint(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell));
		}

		bool IsFree(const FVector2D& P, float Radius) const
		{
			const int32 Reach = FMath::CeilToInt((Radius + MaxRadius) / Cell);
			const FIntPoint Centre = CellOf(P);
			for (int32 DY = -Reach; DY <= Reach; ++DY)
			{
				for (int32 DX = -Reach; DX <= Reach; ++DX)
				{
					if (const TArray<FVector3f>* Entries = Map.Find(Centre + FIntPoint(DX, DY)))
					{
						for (const FVector3f& Entry : *Entries)
						{
							if (FVector2D::DistSquared(P, FVector2D(Entry.X, Entry.Y)) < FMath::Square(Radius + Entry.Z))
							{
								return false;
							}
						}
					}
				}
			}
			return true;
		}

		void Add(const FVector2D& P, float Radius)
		{
			Map.FindOrAdd(CellOf(P)).Add(FVector3f(P.X, P.Y, Radius));
			MaxRadius = FMath::Max(MaxRadius, Radius);
		}
	};

	struct FLake
	{
		FName Id;
		FVector2D Centre;
		float Radius = 0.0f;
		float Level = 0.0f;
		bool bFrozen = false;
	};

	struct FMeshInfo
	{
		UObject* Mesh = nullptr;
		float Weight = 1.0f;
		// Top above the pivot and half the wider side (cm)
		float Height = 100.0f;
		float HalfWidth = 50.0f;
	};

	// Quantile of a noise value among the world's grove noise (so a share means a share of the land)
	struct FQuantiles
	{
		TArray<float> Sorted;

		void Init(const FBeyondHeightfield& Field, float Scale, const FVector2D& Offset)
		{
			const float Step = 16.0f;
			for (float Y = -Field.HalfExtent; Y <= Field.HalfExtent; Y += Step)
			{
				for (float X = -Field.HalfExtent; X <= Field.HalfExtent; X += Step)
				{
					Sorted.Add(GroveNoise(FVector2D(X, Y), Scale, Offset));
				}
			}
			Sorted.Sort();
		}

		float Of(float Value) const
		{
			return Sorted.Num() ? Algo::LowerBound(Sorted, Value) / static_cast<float>(Sorted.Num()) : 0.5f;
		}
	};
}

TArray<FBeyondFloraBatch> UBeyondFloraLibrary::ComputeFlora(UObject* WorldContext, UBeyondWorldLayout* Layout, const TArray<FBeyondFloraPlanting>& Plantings,
	const TArray<FBeyondFloraExclusion>& Exclusions, int32 Seed, const TArray<FName>& OnlyRegions, float GroveScale)
{
	using namespace BeyondFloraLocal;
	TArray<FBeyondFloraBatch> Batches;
	const FBeyondHeightfield* Field = BeyondWorldBuilderLocal::GetField(Layout);
	if (!Field || Field->Size < 3)
	{
		return Batches;
	}
	const double StartTime = FPlatformTime::Seconds();
	FGround Ground;
	Ground.Init(BeyondWorldBuilderLocal::ResolveWorld(WorldContext), Field);

	const FVector2D GroveOffset(FRandomStream(Seed).FRandRange(-500.0f, 500.0f), FRandomStream(Seed + 1).FRandRange(-500.0f, 500.0f));
	FQuantiles Quantiles;
	Quantiles.Init(*Field, GroveScale, GroveOffset);

	TArray<FLake> Lakes;
	for (const FBeyondLayoutLake& Lake : Layout->Lakes)
	{
		if (const float* Level = Field->LakeLevels.Find(Lake.Id))
		{
			Lakes.Add({ Lake.Id, Lake.Centre, Lake.Radius, *Level, Lake.bFrozen });
		}
	}

	TArray<bool> RegionAllowed;
	RegionAllowed.Init(OnlyRegions.Num() == 0, 4);
	for (int32 Slot = 0; Slot < 4; ++Slot)
	{
		RegionAllowed[Slot] |= OnlyRegions.Contains(RegionNames[Slot]);
	}

	const float Half = Field->HalfExtent;
	FOccupancy Occupancy;
	for (const FBeyondFloraPlanting& Planting : Plantings)
	{
		TArray<FMeshInfo> Meshes;
		float TotalWeight = 0.0f;
		for (const FBeyondFloraMesh& Entry : Planting.Meshes)
		{
			FBoxSphereBounds Bounds;
			if (Entry.Mesh && Entry.Weight > 0.0f && MeshBounds(Entry.Mesh, Bounds))
			{
				FMeshInfo& Info = Meshes.AddDefaulted_GetRef();
				Info.Mesh = Entry.Mesh;
				Info.Weight = Entry.Weight;
				Info.Height = FMath::Max(static_cast<float>(Bounds.Origin.Z + Bounds.BoxExtent.Z), 1.0f);
				Info.HalfWidth = FMath::Max(static_cast<float>(FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y)), 1.0f);
				TotalWeight += Entry.Weight;
			}
		}
		if (Meshes.Num() == 0 || Planting.Density <= 0.0f)
		{
			UE_LOG(LogBeyondEditor, Warning, TEXT("Flora: %s has no meshes or no density, skipped"), *Planting.Name.ToString());
			continue;
		}
		const int32 FirstBatch = Batches.Num();
		for (int32 Index = 0; Index < Meshes.Num(); ++Index)
		{
			FBeyondFloraBatch& Batch = Batches.AddDefaulted_GetRef();
			Batch.Planting = Planting.Name;
			Batch.MeshIndex = Index;
			Batch.Mesh = Meshes[Index].Mesh;
			Batch.RegionCounts.Init(0, 4);
		}

		const TArray<int32> Layers = LayerIndices(Planting.Layers);
		const TArray<int32> Avoid = LayerIndices(Planting.AvoidLayers);
		const float CellSize = FMath::Sqrt(10000.0f / Planting.Density);
		// A hash of the name's text (an FName's own hash is its table index, which changes from run to run)
		const uint32 Salt = FCrc::StrCrc32(*Planting.Name.ToString()) ^ static_cast<uint32>(Seed) * 2654435761u;
		const bool bGroves = Planting.GroveMin > 0.0f || Planting.GroveMax < 1.0f;
		const bool bShrub = Planting.Kind == EBeyondFloraKind::Shrub;

		// Shores only look round their lakes
		FBox2D Area(FVector2D(-Half, -Half), FVector2D(Half, Half));
		TArray<const FLake*> ShoreLakes;
		if (Planting.bShore)
		{
			Area = FBox2D(ForceInit);
			for (const FLake& Lake : Lakes)
			{
				if (!Lake.bFrozen && (Planting.Lakes.Num() == 0 || Planting.Lakes.Contains(Lake.Id)))
				{
					ShoreLakes.Add(&Lake);
					Area += FBox2D(Lake.Centre - FVector2D(Lake.Radius + 60.0f), Lake.Centre + FVector2D(Lake.Radius + 60.0f));
				}
			}
			if (ShoreLakes.Num() == 0)
			{
				continue;
			}
		}

		int32 Placed = 0;
		const int32 MinCX = FMath::FloorToInt(Area.Min.X / CellSize);
		const int32 MaxCX = FMath::FloorToInt(Area.Max.X / CellSize);
		const int32 MinCY = FMath::FloorToInt(Area.Min.Y / CellSize);
		const int32 MaxCY = FMath::FloorToInt(Area.Max.Y / CellSize);
		for (int32 CY = MinCY; CY <= MaxCY; ++CY)
		{
			for (int32 CX = MinCX; CX <= MaxCX; ++CX)
			{
				FRandomStream Rng(static_cast<int32>(Hash(CX, CY, Salt)));
				const FVector2D P((CX + Rng.FRand()) * CellSize, (CY + Rng.FRand()) * CellSize);
				if (FMath::Abs(P.X) > Half - 3.0f || FMath::Abs(P.Y) > Half - 3.0f)
				{
					continue;
				}
				float W[4];
				RegionWeights(*Field, *Layout, P, W);
				const float RegionShare = Share(Planting.Regions, W);
				if (RegionShare <= 0.001f || Rng.FRand() > RegionShare)
				{
					continue;
				}
				const int32 Region = Dominant(W);
				if (!RegionAllowed[Region])
				{
					continue;
				}
				if (bGroves)
				{
					const float Q = Quantiles.Of(GroveNoise(P, GroveScale, GroveOffset));
					const float Outside = FMath::Max3(Planting.GroveMin - Q, Q - Planting.GroveMax, 0.0f);
					// Thinning out over 5 % past the grove's edge
					if (Outside > 0.0f && Rng.FRand() > 1.0f - Outside / 0.05f)
					{
						continue;
					}
				}
				const int32 Vertex = VertexIndex(*Field, P);
				if ((Layers.Num() && LayerSum(*Field, Layers, Vertex) < Planting.MinLayerWeight)
					|| (Avoid.Num() && LayerSum(*Field, Avoid, Vertex) > Planting.MaxAvoidWeight))
				{
					continue;
				}
				const FVector2D Rise = Gradient(*Field, P);
				const float Slope = FMath::RadiansToDegrees(FMath::Atan(static_cast<float>(Rise.Size())));
				if (Slope < Planting.MinSlope || Slope > Planting.MaxSlope)
				{
					continue;
				}
				const float GroundMetres = Field->SampleHeight(P);
				if (Planting.Altitude.Y > Planting.Altitude.X && (GroundMetres < Planting.Altitude.X || GroundMetres > Planting.Altitude.Y))
				{
					continue;
				}

				// The mesh and its size
				float Pick = Rng.FRand() * TotalWeight;
				int32 MeshIndex = 0;
				while (MeshIndex < Meshes.Num() - 1 && Pick > Meshes[MeshIndex].Weight)
				{
					Pick -= Meshes[MeshIndex].Weight;
					++MeshIndex;
				}
				const FMeshInfo& Mesh = Meshes[MeshIndex];
				const float Scale = Planting.Size.Y > 0.0f
					? FMath::Lerp(Planting.Size.X, Planting.Size.Y, Rng.FRand()) * 100.0f / Mesh.Height
					: FMath::Lerp(Planting.Scale.X, Planting.Scale.Y, Rng.FRand());
				const float Footprint = Planting.Footprint > 0.0f ? Planting.Footprint : Mesh.HalfWidth * Scale / 100.0f * 0.8f;

				// Roads, gameplay sites, water, the crater
				float RoadEdge = TNumericLimits<float>::Max();
				for (const FBeyondLayoutRoad& Road : Layout->Roads)
				{
					RoadEdge = FMath::Min(RoadEdge, FBeyondHeightfieldGenerator::DistanceToPolyline(P, Road.Points) - Road.Width * 0.5f);
				}
				if (RoadEdge < Planting.RoadClearance + Footprint)
				{
					continue;
				}
				bool bBlocked = false;
				for (const FBeyondFloraExclusion& Exclusion : Exclusions)
				{
					if (!(bShrub && Exclusion.bShrubsAllowed) && FVector2D::DistSquared(P, Exclusion.Centre) < FMath::Square(Exclusion.Radius + Footprint))
					{
						bBlocked = true;
						break;
					}
				}
				if (bBlocked)
				{
					continue;
				}
				if (Planting.bShore)
				{
					bool bOnShore = false;
					for (const FLake* Lake : ShoreLakes)
					{
						if (FVector2D::Distance(P, Lake->Centre) < Lake->Radius + 60.0f
							&& GroundMetres >= Lake->Level + Planting.ShoreMin && GroundMetres <= Lake->Level + Planting.ShoreMax)
						{
							bOnShore = true;
							break;
						}
					}
					if (!bOnShore)
					{
						continue;
					}
				}
				else
				{
					for (const FLake& Lake : Lakes)
					{
						const float Distance = FVector2D::Distance(P, Lake.Centre);
						// The water's disc reaches 8 m past the lake; the ice is walked on
						if (Distance < Lake.Radius + 8.0f + Footprint && (Lake.bFrozen || Distance < Lake.Radius * 0.9f || GroundMetres < Lake.Level + 0.4f))
						{
							bBlocked = true;
							break;
						}
					}
					if (bBlocked)
					{
						continue;
					}
				}
				if (Layout->Volcano.bEnabled && FVector2D::Distance(P, Layout->Volcano.Centre) < Layout->Volcano.RimRadius * 0.92f)
				{
					continue;
				}
				if (!Occupancy.IsFree(P, Footprint))
				{
					continue;
				}

				// The pose
				const double X = P.X * 100.0;
				const double Y = -P.Y * 100.0;
				float Yaw = Rng.FRandRange(0.0f, 360.0f);
				if (Planting.bFaceDownhill && !Rise.IsNearlyZero())
				{
					// Downhill is -Rise (east, north); Unreal's Y runs south
					Yaw = FMath::RadiansToDegrees(FMath::Atan2(Rise.Y, -Rise.X)) + Planting.YawOffset;
				}
				FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(Yaw));
				if (Planting.MaxTilt > 0.0f)
				{
					const float Direction = Rng.FRandRange(0.0f, UE_TWO_PI);
					const FVector Axis(FMath::Cos(Direction), FMath::Sin(Direction), 0.0f);
					Rotation = FQuat(Axis, FMath::DegreesToRadians(Rng.FRandRange(0.0f, Planting.MaxTilt))) * Rotation;
				}
				if (Planting.AlignToSlope > 0.0f)
				{
					const FQuat Align = FQuat::FindBetweenNormals(FVector::UpVector, Ground.Normal(X, Y));
					Rotation = FQuat::Slerp(FQuat::Identity, Align, Planting.AlignToSlope) * Rotation;
				}
				const float Sink = Planting.Sink * Mesh.Height * Scale + Planting.SinkMetres * 100.0f;
				const FVector Location(X, Y, Ground.Z(X, Y) - Sink);

				Occupancy.Add(P, Footprint);
				FBeyondFloraBatch& Batch = Batches[FirstBatch + MeshIndex];
				Batch.Transforms.Add(FTransform(Rotation, Location, FVector(Scale)));
				++Batch.RegionCounts[Region];
				++Placed;
			}
		}
		UE_LOG(LogBeyondEditor, Display, TEXT("Flora: %s placed %d (%d meshes, a cell every %.1f m)"), *Planting.Name.ToString(), Placed, Meshes.Num(), CellSize);
	}
	Batches.RemoveAll([](const FBeyondFloraBatch& Batch) { return Batch.Transforms.Num() == 0; });
	UE_LOG(LogBeyondEditor, Display, TEXT("Flora: %d plantings scattered in %.1f s"), Plantings.Num(), FPlatformTime::Seconds() - StartTime);
	return Batches;
}

float UBeyondFloraLibrary::GetMeshHeight(UObject* Mesh)
{
	FBoxSphereBounds Bounds;
	return BeyondFloraLocal::MeshBounds(Mesh, Bounds) ? static_cast<float>(Bounds.Origin.Z + Bounds.BoxExtent.Z) / 100.0f : 0.0f;
}

FName UBeyondFloraLibrary::GetRegionAt(UBeyondWorldLayout* Layout, float X, float Y)
{
	const FBeyondHeightfield* Field = BeyondWorldBuilderLocal::GetField(Layout);
	if (!Field)
	{
		return NAME_None;
	}
	float W[4];
	BeyondFloraLocal::RegionWeights(*Field, *Layout, FVector2D(X / 100.0f, -Y / 100.0f), W);
	return BeyondFloraLocal::RegionNames[BeyondFloraLocal::Dominant(W)];
}

UTransformProviderData* UBeyondFloraLibrary::CreateWindData(const FString& AssetPath)
{
	UClass* WindClass = FindObject<UClass>(nullptr, TEXT("/Script/DynamicWind.DynamicWindData"));
	if (!WindClass || !WindClass->IsChildOf(UTransformProviderData::StaticClass()))
	{
		UE_LOG(LogBeyondEditor, Warning, TEXT("Flora: the Dynamic Wind plugin isn't loaded (no DynamicWindData)"));
		return nullptr;
	}
	if (UTransformProviderData* Existing = BeyondWorldBuilderLocal::LoadExisting<UTransformProviderData>(AssetPath))
	{
		return Existing;
	}
	UPackage* Package = CreatePackage(*AssetPath);
	UTransformProviderData* Wind = NewObject<UTransformProviderData>(Package, WindClass, *FPackageName::GetShortName(AssetPath), RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Wind);
	Wind->MarkPackageDirty();
	return Wind;
}

UFoliageType_InstancedStaticMesh* UBeyondFloraLibrary::CreateFoliageType(const FString& AssetPath, UStaticMesh* Mesh, const FBeyondFoliageSettings& Settings)
{
	if (!Mesh)
	{
		return nullptr;
	}
	bool bNew = false;
	UFoliageType_InstancedStaticMesh* Type = BeyondWorldBuilderLocal::MakeAsset<UFoliageType_InstancedStaticMesh>(AssetPath, bNew);
	Type->Modify();
	Type->SetStaticMesh(Mesh);
	Type->OverrideMaterials = Settings.OverrideMaterials;
	Type->Mobility = EComponentMobility::Static;
	Type->CullDistance = FInt32Interval(FMath::RoundToInt(Settings.CullStart * 100.0f), FMath::RoundToInt(Settings.CullEnd * 100.0f));
	Type->CastShadow = Settings.bCastShadow;
	Type->bCastDynamicShadow = Settings.bCastShadow;
	Type->bCastStaticShadow = false;
	Type->BodyInstance.SetCollisionProfileName(Settings.bBlocks ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Type->CustomNavigableGeometry = Settings.bBlocks ? EHasCustomNavigableGeometry::Yes : EHasCustomNavigableGeometry::DontExport;
	// Never thinned by the scalability settings: what blocks must stay drawn
	Type->bEnableDensityScaling = false;
	// The brush, for painting more by hand
	Type->Density = Settings.PaintDensity;
	Type->Scaling = EFoliageScaling::Uniform;
	Type->ScaleX = FFloatInterval(Settings.PaintScale.X, Settings.PaintScale.Y);
	Type->ScaleY = Type->ScaleX;
	Type->ScaleZ = Type->ScaleX;
	Type->ZOffset = FFloatInterval(-Settings.PaintSink * 100.0f, -Settings.PaintSink * 50.0f);
	Type->AlignToNormal = Settings.bPaintAlignToNormal;
	Type->RandomYaw = true;
	Type->GroundSlopeAngle = FFloatInterval(0.0f, Settings.PaintMaxSlope);
	Type->PostEditChange();
	if (bNew)
	{
		FAssetRegistryModule::AssetCreated(Type);
	}
	Type->MarkPackageDirty();
	return Type;
}

bool UBeyondFloraLibrary::SetTrunkCollision(UStaticMesh* Mesh, float Radius, float Height)
{
	if (!Mesh || Radius <= 0.0f || Height <= 0.0f)
	{
		return false;
	}
	UBodySetup* Body = Mesh->GetBodySetup();
	if (!Body)
	{
		Mesh->CreateBodySetup();
		Body = Mesh->GetBodySetup();
	}
	if (!Body)
	{
		return false;
	}
	Mesh->Modify();
	Body->Modify();
	Body->RemoveSimpleCollision();
	const float RadiusCm = Radius * 100.0f;
	const float HeightCm = FMath::Max(Height * 100.0f, RadiusCm * 2.0f + 1.0f);
	FKSphylElem Capsule(RadiusCm, HeightCm - RadiusCm * 2.0f);
	Capsule.Center = FVector(0.0, 0.0, HeightCm * 0.5f);
	Body->AggGeom.SphylElems.Add(Capsule);
	Body->CollisionTraceFlag = CTF_UseDefault;
	Body->InvalidatePhysicsData();
	Body->CreatePhysicsMeshes();
	Mesh->SetCustomizedCollision(true);
	Mesh->CreateNavCollision(true);
	Mesh->MarkPackageDirty();
	return true;
}

bool UBeyondFloraLibrary::UseComplexAsSimple(UStaticMesh* Mesh)
{
	UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
	if (!Body)
	{
		return false;
	}
	if (Body->CollisionTraceFlag == CTF_UseComplexAsSimple)
	{
		return true;
	}
	Mesh->Modify();
	Body->Modify();
	Body->CollisionTraceFlag = CTF_UseComplexAsSimple;
	Body->InvalidatePhysicsData();
	Body->CreatePhysicsMeshes();
	Mesh->CreateNavCollision(true);
	Mesh->MarkPackageDirty();
	return true;
}

int32 UBeyondFloraLibrary::GetSimpleCollisionCount(UStaticMesh* Mesh)
{
	const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
	return Body ? Body->AggGeom.GetElementCount() : 0;
}

int32 UBeyondFloraLibrary::PlaceSkinnedFlora(UObject* WorldContext, USkeletalMesh* Mesh, const TArray<FTransform>& Transforms, const FBeyondSkinnedFloraSettings& Settings)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	if (!World || !Mesh || Transforms.Num() == 0)
	{
		return 0;
	}
	const double TileCm = FMath::Max(Settings.TileSize, 16.0f) * 100.0;

	TMap<FString, ABeyondFloraTile*> Tiles;
	for (TActorIterator<ABeyondFloraTile> It(World); It; ++It)
	{
		Tiles.Add(It->GetActorLabel(), *It);
	}
	TMap<FIntPoint, TArray<FTransform>> ByTile;
	for (const FTransform& Transform : Transforms)
	{
		const FVector Location = Transform.GetLocation();
		ByTile.FindOrAdd(FIntPoint(FMath::FloorToInt(Location.X / TileCm), FMath::FloorToInt(Location.Y / TileCm))).Add(Transform);
	}

	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	const FName PlantName(*FString::Printf(TEXT("Plant_%s"), *Mesh->GetName()));
	int32 Touched = 0;
	for (const TPair<FIntPoint, TArray<FTransform>>& Pair : ByTile)
	{
		const FString Label = FString::Printf(TEXT("%s%d_%d"), *Settings.LabelPrefix, Pair.Key.X, Pair.Key.Y);
		ABeyondFloraTile* Tile = Tiles.FindRef(Label);
		if (!Tile)
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags = RF_Transactional;
			const FVector Centre((Pair.Key.X + 0.5) * TileCm, (Pair.Key.Y + 0.5) * TileCm, Pair.Value[0].GetLocation().Z);
			Tile = World->SpawnActor<ABeyondFloraTile>(ABeyondFloraTile::StaticClass(), FTransform(Centre), Params);
			if (!Tile)
			{
				UE_LOG(LogBeyondEditor, Warning, TEXT("Flora: could not spawn the tile %s"), *Label);
				continue;
			}
			Tile->SetActorLabel(Label);
			Tile->SetFolderPath(TEXT("Flora"));
			Tiles.Add(Label, Tile);
		}
		Tile->Modify();
		if (!Settings.RuntimeGrid.IsNone())
		{
			Tile->SetRuntimeGrid(Settings.RuntimeGrid);
		}

		// The plant's component (one per mesh)
		UInstancedSkinnedMeshComponent* Plants = nullptr;
		TArray<UInstancedSkinnedMeshComponent*> Existing;
		Tile->GetPlantComponents(Existing);
		for (UInstancedSkinnedMeshComponent* Component : Existing)
		{
			if (Component->GetFName() == PlantName)
			{
				Plants = Component;
				break;
			}
		}
		if (!Plants)
		{
			// (Not through FSkinnedMeshComponentDescriptor: it sets the mesh without telling the component, which ensures
			// on the next SetMaterial)
			Plants = NewObject<UInstancedSkinnedMeshComponent>(Tile, PlantName, RF_Transactional);
			Plants->SetSkinnedAssetAndUpdate(Mesh, false);
			Plants->SetTransformProvider(Settings.Wind);
			Plants->SetCullDistances(FMath::RoundToInt(Settings.CullStart * 100.0f), FMath::RoundToInt(Settings.CullEnd * 100.0f));
			Plants->CastShadow = Settings.bCastShadow;
			Plants->bCastDynamicShadow = Settings.bCastShadow;
			Plants->bCastStaticShadow = false;
			Plants->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			Plants->SetCanEverAffectNavigation(false);
			for (int32 Slot = 0; Slot < Settings.OverrideMaterials.Num(); ++Slot)
			{
				if (Settings.OverrideMaterials[Slot])
				{
					Plants->SetMaterial(Slot, Settings.OverrideMaterials[Slot]);
				}
			}
			Plants->SetupAttachment(Tile->GetRootComponent());
			Tile->AddInstanceComponent(Plants);
			Plants->RegisterComponent();
		}
		Plants->Modify();
		TArray<int32> Animations;
		Animations.Init(0, Pair.Value.Num());
		Plants->AddInstances(Pair.Value, Animations, false, true);

		// Trunks that block
		if (Settings.TrunkRadius > 0.0f && Settings.TrunkHeight > 0.0f && Cylinder)
		{
			UInstancedStaticMeshComponent* Colliders = Tile->GetColliders();
			if (!Colliders)
			{
				Colliders = NewObject<UInstancedStaticMeshComponent>(Tile, TEXT("TrunkColliders"), RF_Transactional);
				Colliders->SetStaticMesh(Cylinder);
				Colliders->SetMobility(EComponentMobility::Static);
				Colliders->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
				Colliders->SetCanEverAffectNavigation(true);
				Colliders->SetVisibility(false);
				Colliders->SetHiddenInGame(true);
				Colliders->CastShadow = false;
				Colliders->bAffectDistanceFieldLighting = false;
				Colliders->ComponentTags.Add(ABeyondFloraTile::ColliderTag);
				Colliders->SetupAttachment(Tile->GetRootComponent());
				Tile->AddInstanceComponent(Colliders);
				Colliders->RegisterComponent();
			}
			Colliders->Modify();
			TArray<FTransform> Trunks;
			for (const FTransform& Transform : Pair.Value)
			{
				const float Scale = Transform.GetScale3D().X;
				const float Radius = Settings.TrunkRadius * 100.0f * Scale;
				const float Height = Settings.TrunkHeight * 100.0f * Scale;
				// The engine cylinder: radius 50, 100 tall, pivot in the middle
				Trunks.Add(FTransform(FQuat::Identity, Transform.GetLocation() + FVector(0.0, 0.0, Height * 0.5f), FVector(Radius / 50.0f, Radius / 50.0f, Height / 100.0f)));
			}
			Colliders->AddInstances(Trunks, false, true);
		}
		++Touched;
	}
	return Touched;
}

int32 UBeyondFloraLibrary::LoadFloraActors(UObject* WorldContext)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
	if (!WorldPartition)
	{
		return 0;
	}
	TArray<FWorldPartitionActorDescInstance*> Descs;
	FWorldPartitionHelpers::ForEachActorDescInstance<AInstancedFoliageActor>(WorldPartition, [&Descs](const FWorldPartitionActorDescInstance* Desc)
	{
		Descs.Add(const_cast<FWorldPartitionActorDescInstance*>(Desc));
		return true;
	});
	FWorldPartitionHelpers::ForEachActorDescInstance<ABeyondFloraTile>(WorldPartition, [&Descs](const FWorldPartitionActorDescInstance* Desc)
	{
		Descs.Add(const_cast<FWorldPartitionActorDescInstance*>(Desc));
		return true;
	});
	for (FWorldPartitionActorDescInstance* Desc : Descs)
	{
		BeyondWorldBuilderLocal::KeepLoaded(Desc);
	}
	UE_LOG(LogBeyondEditor, Display, TEXT("Flora: loaded %d foliage actors and flora tiles"), Descs.Num());
	return Descs.Num();
}

int32 UBeyondFloraLibrary::ClearFlora(UObject* WorldContext, UBeyondWorldLayout* Layout, const TArray<UFoliageType*>& FoliageTypes, const TArray<FName>& OnlyRegions)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	if (!World)
	{
		return 0;
	}
	auto InRegion = [Layout, &OnlyRegions](const FVector& Location)
	{
		return OnlyRegions.Num() == 0 || OnlyRegions.Contains(GetRegionAt(Layout, Location.X, Location.Y));
	};

	int32 Removed = 0;
	TArray<AActor*> Emptied;
	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* Foliage = *It;
		for (UFoliageType* Type : FoliageTypes)
		{
			FFoliageInfo* Info = Type ? Foliage->FindInfo(Type) : nullptr;
			if (!Info)
			{
				continue;
			}
			Foliage->Modify();
			if (OnlyRegions.Num() == 0)
			{
				Removed += Info->Instances.Num();
				UFoliageType* ToRemove = Type;
				Foliage->RemoveFoliageType(&ToRemove, 1);
				continue;
			}
			TArray<int32> Indices;
			for (int32 Index = 0; Index < Info->Instances.Num(); ++Index)
			{
				if (InRegion(Info->Instances[Index].Location))
				{
					Indices.Add(Index);
				}
			}
			if (Indices.Num())
			{
				Info->RemoveInstances(Indices, true);
				Removed += Indices.Num();
			}
		}
		// An emptied foliage actor is kept: the scatter fills its grid cell again (a new one would get its name and file)
	}

	for (TActorIterator<ABeyondFloraTile> It(World); It; ++It)
	{
		ABeyondFloraTile* Tile = *It;
		if (OnlyRegions.Num() == 0)
		{
			Removed += Tile->GetPlantCount();
			Emptied.Add(Tile);
			continue;
		}
		Tile->Modify();
		TArray<UInstancedSkinnedMeshComponent*> Plants;
		Tile->GetPlantComponents(Plants);
		for (UInstancedSkinnedMeshComponent* Component : Plants)
		{
			TArray<FPrimitiveInstanceId> Ids;
			for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
			{
				const FPrimitiveInstanceId Id = Component->GetInstanceId(Index);
				FTransform Transform;
				if (Component->GetInstanceTransform(Id, Transform, true) && InRegion(Transform.GetLocation()))
				{
					Ids.Add(Id);
				}
			}
			if (Ids.Num())
			{
				Component->Modify();
				Component->RemoveInstances(Ids);
				Removed += Ids.Num();
			}
		}
		if (UInstancedStaticMeshComponent* Colliders = Tile->GetColliders())
		{
			TArray<int32> Indices;
			for (int32 Index = 0; Index < Colliders->GetInstanceCount(); ++Index)
			{
				FTransform Transform;
				if (Colliders->GetInstanceTransform(Index, Transform, true) && InRegion(Transform.GetLocation()))
				{
					Indices.Add(Index);
				}
			}
			if (Indices.Num())
			{
				Colliders->Modify();
				Colliders->RemoveInstances(Indices);
			}
		}
		if (Tile->GetPlantCount() == 0)
		{
			Emptied.Add(Tile);
		}
	}
	const int32 Destroyed = UBeyondWorldBuilderLibrary::DestroyActors(Emptied);
	UE_LOG(LogBeyondEditor, Display, TEXT("Flora: cleared %d instances (%d empty actors removed)"), Removed, Destroyed);
	return Removed;
}

int32 UBeyondFloraLibrary::CountFoliageInstances(UObject* WorldContext, UFoliageType* FoliageType)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	int32 Count = 0;
	if (World && FoliageType)
	{
		for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
		{
			if (const FFoliageInfo* Info = It->FindInfo(FoliageType))
			{
				Count += Info->Instances.Num();
			}
		}
	}
	return Count;
}

bool UBeyondFloraLibrary::ConfigureFloraStreaming(UObject* WorldContext, FName PartitionName, float CellSize, float LoadingRange)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
	UObject* Hash = WorldPartition ? WorldPartition->RuntimeHash.Get() : nullptr;
	// UWorldPartitionRuntimeHashSet isn't exported: its partitions are reached by reflection
	FArrayProperty* Partitions = Hash ? FindFProperty<FArrayProperty>(Hash->GetClass(), TEXT("RuntimePartitions")) : nullptr;
	FStructProperty* DescProperty = Partitions ? CastField<FStructProperty>(Partitions->Inner) : nullptr;
	FNameProperty* DescName = DescProperty ? FindFProperty<FNameProperty>(DescProperty->Struct, TEXT("Name")) : nullptr;
	FObjectProperty* DescLayer = DescProperty ? FindFProperty<FObjectProperty>(DescProperty->Struct, TEXT("MainLayer")) : nullptr;
	if (!DescName || !DescLayer)
	{
		UE_LOG(LogBeyondEditor, Warning, TEXT("Flora: the world's runtime hash (%s) has no runtime partitions; the flora streams with the main grid"),
			Hash ? *Hash->GetClass()->GetName() : TEXT("none"));
		return false;
	}
	Hash->Modify();
	FScriptArrayHelper Array(Partitions, Partitions->ContainerPtrToValuePtr<void>(Hash));
	int32 Found = INDEX_NONE;
	for (int32 Index = 0; Index < Array.Num(); ++Index)
	{
		if (DescName->GetPropertyValue_InContainer(Array.GetRawPtr(Index)) == PartitionName)
		{
			Found = Index;
			break;
		}
	}
	if (Found == INDEX_NONE)
	{
		Found = Array.AddValue();
		DescName->SetPropertyValue_InContainer(Array.GetRawPtr(Found), PartitionName);
	}
	URuntimePartition* Layer = Cast<URuntimePartition>(DescLayer->GetObjectPropertyValue_InContainer(Array.GetRawPtr(Found)));
	if (!Layer || !Layer->IsA<URuntimePartitionLHGrid>())
	{
		Layer = NewObject<URuntimePartitionLHGrid>(Hash, NAME_None, RF_Transactional);
		Layer->SetDefaultValues();
		DescLayer->SetObjectPropertyValue_InContainer(Array.GetRawPtr(Found), Layer);
	}
	Layer->Modify();
	auto SetValue = [Layer](const TCHAR* Name, int64 Value)
	{
		if (FNumericProperty* Property = FindFProperty<FNumericProperty>(Layer->GetClass(), Name))
		{
			Property->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Layer), Value);
			return true;
		}
		return false;
	};
	if (FNameProperty* LayerName = FindFProperty<FNameProperty>(Layer->GetClass(), TEXT("Name")))
	{
		LayerName->SetPropertyValue_InContainer(Layer, PartitionName);
	}
	const bool bSet = SetValue(TEXT("CellSize"), FMath::RoundToInt64(CellSize * 100.0f)) & SetValue(TEXT("LoadingRange"), FMath::RoundToInt64(LoadingRange * 100.0f));

	// The main partition's HLOD setups (the same HLOD layers, on grids twice as coarse): the flora's actors carry the
	// world's default HLOD layer, which must be known to the partition they stream on
	FArrayProperty* Setups = FindFProperty<FArrayProperty>(DescProperty->Struct, TEXT("HLODSetups"));
	FStructProperty* SetupProperty = Setups ? CastField<FStructProperty>(Setups->Inner) : nullptr;
	FNameProperty* SetupName = SetupProperty ? FindFProperty<FNameProperty>(SetupProperty->Struct, TEXT("Name")) : nullptr;
	FArrayProperty* SetupLayers = SetupProperty ? FindFProperty<FArrayProperty>(SetupProperty->Struct, TEXT("HLODLayers")) : nullptr;
	FObjectProperty* SetupPartition = SetupProperty ? FindFProperty<FObjectProperty>(SetupProperty->Struct, TEXT("PartitionLayer")) : nullptr;
	FBoolProperty* SetupLoaded = SetupProperty ? FindFProperty<FBoolProperty>(SetupProperty->Struct, TEXT("bIsSpatiallyLoaded")) : nullptr;
	int32 HLODSetups = 0;
	if (Found > 0 && SetupName && SetupLayers && SetupPartition)
	{
		FScriptArrayHelper Main(Setups, Setups->ContainerPtrToValuePtr<void>(Array.GetRawPtr(0)));
		FScriptArrayHelper Flora(Setups, Setups->ContainerPtrToValuePtr<void>(Array.GetRawPtr(Found)));
		for (int32 Index = 0; Index < Main.Num(); ++Index)
		{
			void* Source = Main.GetRawPtr(Index);
			const FName Name = SetupName->GetPropertyValue_InContainer(Source);
			FScriptArrayHelper SourceLayers(SetupLayers, SetupLayers->ContainerPtrToValuePtr<void>(Source));
			bool bHas = SourceLayers.Num() == 0;
			for (int32 Existing = 0; Existing < Flora.Num() && !bHas; ++Existing)
			{
				bHas = SetupName->GetPropertyValue_InContainer(Flora.GetRawPtr(Existing)) == Name;
			}
			if (bHas)
			{
				continue;
			}
			const int32 Added = Flora.AddValue();
			void* Target = Flora.GetRawPtr(Added);
			SetupName->SetPropertyValue_InContainer(Target, Name);
			SetupLayers->CopyCompleteValue_InContainer(Target, Source);
			if (SetupLoaded)
			{
				SetupLoaded->SetPropertyValue_InContainer(Target, SetupLoaded->GetPropertyValue_InContainer(Source));
			}
			URuntimePartitionLHGrid* Grid = NewObject<URuntimePartitionLHGrid>(Hash, NAME_None, RF_Transactional);
			auto SetGrid = [Grid](const TCHAR* Property, int64 Value)
			{
				if (FNumericProperty* Numeric = FindFProperty<FNumericProperty>(Grid->GetClass(), Property))
				{
					Numeric->SetIntPropertyValue(Numeric->ContainerPtrToValuePtr<void>(Grid), Value);
				}
			};
			SetGrid(TEXT("CellSize"), FMath::RoundToInt64(CellSize * 100.0f) * (2 << Index));
			SetGrid(TEXT("LoadingRange"), FMath::RoundToInt64(LoadingRange * 100.0f) * (2 << Index));
			SetGrid(TEXT("HLODIndex"), Index);
			if (FNameProperty* GridName = FindFProperty<FNameProperty>(Grid->GetClass(), TEXT("Name")))
			{
				GridName->SetPropertyValue_InContainer(Grid, Name);
			}
			if (FBoolProperty* ClientOnly = FindFProperty<FBoolProperty>(Grid->GetClass(), TEXT("bClientOnlyVisible")))
			{
				ClientOnly->SetPropertyValue_InContainer(Grid, true);
			}
			SetupPartition->SetObjectPropertyValue_InContainer(Target, Grid);
		}
		// Cell names are <partition layer name>_<cell>: the flora's HLOD grids must not share the main partition's names,
		// or the streaming generation (HLOD builder, cook, PIE) asserts on duplicate cells. Renames grids saved earlier too.
		for (int32 Index = 0; Index < Flora.Num(); ++Index)
		{
			void* Setup = Flora.GetRawPtr(Index);
			UObject* Grid = SetupPartition->GetObjectPropertyValue_InContainer(Setup);
			FNameProperty* GridName = Grid ? FindFProperty<FNameProperty>(Grid->GetClass(), TEXT("Name")) : nullptr;
			if (GridName)
			{
				const FName Unique(*FString::Printf(TEXT("%s_%s"), *PartitionName.ToString(), *SetupName->GetPropertyValue_InContainer(Setup).ToString()));
				if (GridName->GetPropertyValue_InContainer(Grid) != Unique)
				{
					Grid->Modify();
					GridName->SetPropertyValue_InContainer(Grid, Unique);
				}
			}
		}
		HLODSetups = Flora.Num();
	}
	Hash->MarkPackageDirty();
	UE_LOG(LogBeyondEditor, Display, TEXT("Flora: %s has %d HLOD setups"), *PartitionName.ToString(), HLODSetups);
	UE_LOG(LogBeyondEditor, Display, TEXT("Flora: runtime partition %s, %.0f m cells, loaded within %.0f m%s"), *PartitionName.ToString(), CellSize, LoadingRange,
		bSet ? TEXT("") : TEXT(" (could not set its sizes)"));
	return bSet;
}

int32 UBeyondFloraLibrary::AssignFloraGrid(UObject* WorldContext, FName PartitionName)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	int32 Changed = 0;
	if (!World)
	{
		return 0;
	}
	auto Assign = [&Changed, PartitionName](AActor* Actor)
	{
		if (Actor->GetRuntimeGrid() != PartitionName)
		{
			Actor->Modify();
			Actor->SetRuntimeGrid(PartitionName);
			++Changed;
		}
	};
	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		Assign(*It);
	}
	for (TActorIterator<ABeyondFloraTile> It(World); It; ++It)
	{
		Assign(*It);
	}
	return Changed;
}
