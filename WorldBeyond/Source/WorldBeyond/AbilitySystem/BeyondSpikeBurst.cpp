// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/BeyondSpikeBurst.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Spike bases start this far underground so they never float on uneven floors
	constexpr float SpikeBuriedDepth = 8.0f;
}

ABeyondSpikeBurst::ABeyondSpikeBurst()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->Mobility = EComponentMobility::Movable;
	RootComponent = SceneRoot;

	FlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlashLight"));
	FlashLight->SetupAttachment(SceneRoot);
	FlashLight->Mobility = EComponentMobility::Movable;
	FlashLight->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	FlashLight->IntensityUnits = ELightUnits::Candelas;
	FlashLight->Intensity = 0.0f;
	FlashLight->CastShadows = false;

	// Works before the migration script made the crystal mesh / material: an engine cone tinted through "Color"
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	SpikeMesh = ConeMesh.Object;
	SpikeMaterial = ShapeMaterial.Object;
}

void ABeyondSpikeBurst::BeginPlay()
{
	Super::BeginPlay();

	BuildSpikes();

	if (FlashLight)
	{
		FlashLight->SetLightColor(LightColor);
		FlashLight->SetAttenuationRadius(Radius * 2.5f + 300.0f);
		FlashLight->SetIntensity(0.0f);
	}

	BeyondFX::SpawnAtLocation(this, EruptFX, GetActorLocation());
	OnErupt();

	// Safety net in case ticking stops
	SetLifeSpan(GetTotalDuration() + 1.0f);
}

FLinearColor ABeyondSpikeBurst::GetSpikeColor(int32 Index) const
{
	return Spikes.IsValidIndex(Index) ? Spikes[Index].Color : FLinearColor::Black;
}

void ABeyondSpikeBurst::BuildSpikes()
{
	if (!SpikeMesh)
	{
		return;
	}

	const FBox Bounds = SpikeMesh->GetBoundingBox();
	MeshBottom = Bounds.Min.Z;
	MeshHeight = FMath::Max(Bounds.GetSize().Z, 1.0f);
	MeshHalfWidth = FMath::Max(FMath::Max(Bounds.GetExtent().X, Bounds.GetExtent().Y), 1.0f);

	// The tallest one in the middle, then rings that lean outward and rise a moment later
	AddSpike(FVector::ZeroVector, CenterHeight, 0.0f, 0.0f);
	for (int32 Ring = 1; Ring <= Rings; ++Ring)
	{
		const float RingFraction = static_cast<float>(Ring) / Rings;
		const float RingDistance = Radius * RingFraction * 0.85f;
		const int32 Count = SpikesInFirstRing + SpikesAddedPerRing * (Ring - 1);
		const float AngleStep = 360.0f / Count;
		const float AngleOffset = FMath::FRandRange(0.0f, 360.0f);

		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = FMath::DegreesToRadians(AngleOffset + AngleStep * (Index + FMath::FRandRange(-0.3f, 0.3f)));
			const float Distance = RingDistance * FMath::FRandRange(0.85f, 1.1f);
			const FVector Offset(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
			const float Height = FMath::Lerp(CenterHeight, EdgeHeight, RingFraction);
			const float Tilt = FMath::Clamp(FMath::Lerp(MinTilt, MaxTilt, RingFraction) + FMath::FRandRange(-5.0f, 5.0f), 0.0f, 80.0f);
			const float Delay = WaveTime * FMath::Clamp(Distance / FMath::Max(Radius, 1.0f), 0.0f, 1.0f);
			AddSpike(Offset, Height, Tilt, Delay);
		}
	}
}

void ABeyondSpikeBurst::AddSpike(const FVector& LocalOffset, float Height, float Tilt, float Delay)
{
	FSpike Spike;

	FVector Outward = LocalOffset.GetSafeNormal2D();
	if (Outward.IsNearlyZero())
	{
		Outward = FVector::ForwardVector;
	}
	const float TiltRadians = FMath::DegreesToRadians(Tilt);
	Spike.Axis = (FVector::UpVector * FMath::Cos(TiltRadians) + Outward * FMath::Sin(TiltRadians)).GetSafeNormal();

	// Random spin around its own axis so the facets catch the light differently
	FVector SpinReference(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f);
	if (!SpinReference.Normalize())
	{
		SpinReference = FVector::ForwardVector;
	}
	Spike.Rotation = FRotationMatrix::MakeFromZX(Spike.Axis, SpinReference).ToQuat();

	const float SizeScale = 1.0f + FMath::FRandRange(-SizeJitter, SizeJitter);
	Spike.Height = Height * SizeScale;
	const float Width = SpikeWidth * SizeScale * FMath::Sqrt(FMath::Clamp(Height / CenterHeight, 0.2f, 1.0f));
	Spike.Scale = FVector(Width / (2.0f * MeshHalfWidth), Width / (2.0f * MeshHalfWidth), Spike.Height / MeshHeight);
	Spike.Base = FindGround(GetActorLocation() + LocalOffset);
	Spike.Delay = Delay;
	Spike.Color = FMath::Lerp(ColorA, ColorB, FMath::FRand());

	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(SpikeMesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(false);
	Mesh->SetAbsolute(true, true, true);
	Mesh->SetVisibility(false);
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetWorldScale3D(Spike.Scale);
	Mesh->RegisterComponent();

	if (SpikeMaterial)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(SpikeMaterial, Mesh);
		Material->SetVectorParameterValue(ColorParameter, Spike.Color);
		Material->SetScalarParameterValue(GlowParameter, RiseGlow);
		for (int32 Slot = 0; Slot < FMath::Max(Mesh->GetNumMaterials(), 1); ++Slot)
		{
			Mesh->SetMaterial(Slot, Material);
		}
		Spike.Material = Material;
	}

	Spike.Mesh = Mesh;
	SpikeComponents.Add(Mesh);
	Spikes.Add(Spike);
}

FVector ABeyondSpikeBurst::FindGround(const FVector& Location) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondSpikeGround), false, this);
	const FVector Start = Location + FVector(0.0f, 0.0f, 150.0f);
	const FVector End = Location - FVector(0.0f, 0.0f, 300.0f);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return Hit.ImpactPoint;
	}
	return Location;
}

void ABeyondSpikeBurst::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	for (FSpike& Spike : Spikes)
	{
		UpdateSpike(Spike);
	}

	if (FlashLight && LightIntensity > 0.0f)
	{
		// Peaks as the first spikes break the surface, then fades over the hold
		const float FadeTime = FMath::Max(WaveTime + HoldTime * 0.6f, 0.05f);
		const float Brightness = Age < RiseTime ? Age / RiseTime : FMath::Max(0.0f, 1.0f - (Age - RiseTime) / FadeTime);
		FlashLight->SetIntensity(LightIntensity * Brightness);
	}

	if (Age > GetTotalDuration() + 0.05f)
	{
		Destroy();
	}
}

void ABeyondSpikeBurst::UpdateSpike(FSpike& Spike) const
{
	UStaticMeshComponent* Mesh = Spike.Mesh.Get();
	if (!Mesh)
	{
		return;
	}

	const float Local = Age - Spike.Delay;
	float Emerge = 0.0f;
	float GlowValue = Glow;
	if (Local <= 0.0f || Local >= RiseTime + HoldTime + SinkTime)
	{
		Mesh->SetVisibility(false);
		return;
	}
	if (Local < RiseTime)
	{
		// Shoots up, a little past its height, and settles
		const float Alpha = Local / RiseTime;
		Emerge = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 3.0f) + Overshoot * FMath::Sin(PI * Alpha);
		GlowValue = RiseGlow;
	}
	else if (Local < RiseTime + HoldTime)
	{
		Emerge = 1.0f;
		const float Settle = (Local - RiseTime) / FMath::Max(HoldTime * 0.5f, 0.01f);
		GlowValue = FMath::Lerp(RiseGlow, Glow, FMath::Clamp(Settle, 0.0f, 1.0f));
	}
	else
	{
		const float Alpha = (Local - RiseTime - HoldTime) / SinkTime;
		Emerge = 1.0f - FMath::InterpEaseIn(0.0f, 1.0f, Alpha, 2.0f);
		GlowValue = Glow * (1.0f - 0.7f * Alpha);
	}

	// Slide along its own axis: fully underground at Emerge 0, base just below the floor at 1
	const float Depth = SpikeBuriedDepth + (1.0f - Emerge) * Spike.Height + MeshBottom * Spike.Scale.Z;
	Mesh->SetWorldLocationAndRotation(Spike.Base - Spike.Axis * Depth, Spike.Rotation);
	if (!Mesh->IsVisible())
	{
		Mesh->SetVisibility(true);
	}

	if (UMaterialInstanceDynamic* Material = Spike.Material.Get(); Material && FMath::Abs(GlowValue - Spike.LastGlow) > 0.05f)
	{
		Material->SetScalarParameterValue(GlowParameter, GlowValue);
		Spike.LastGlow = GlowValue;
	}
}
