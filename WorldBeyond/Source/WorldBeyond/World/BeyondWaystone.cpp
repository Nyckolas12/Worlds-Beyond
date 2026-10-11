// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondWaystone.h"
#include "Components/ArrowComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "NiagaraComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "World/BeyondWorldSubsystem.h"

ABeyondWaystone::ABeyondWaystone()
{
	PrimaryActorTick.bCanEverTick = false;
	bIsSpatiallyLoaded = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(GetRootComponent());
	// A stand-in obelisk until the pass gives it a real mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Mesh->SetStaticMesh(Cylinder.Object);
		Mesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 2.4f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	}
	// Seen from the far side of a valley; not drawn beyond that
	Mesh->LDMaxDrawDistance = 15000.0f;

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(GetRootComponent());
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f));
	Glow->SetIntensity(0.0f);
	Glow->SetAttenuationRadius(900.0f);
	Glow->SetCastShadows(false);

	ArrivalPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrivalPoint"));
	ArrivalPoint->SetupAttachment(GetRootComponent());
	ArrivalPoint->SetRelativeLocation(FVector(300.0f, 0.0f, 0.0f));
	ArrivalPoint->ArrowColor = FColor(80, 140, 255);
}

void ABeyondWaystone::BeginPlay()
{
	Super::BeginPlay();
	if (UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this))
	{
		WorldSubsystem->RegisterWaystone(this);
	}
}

void ABeyondWaystone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this))
	{
		WorldSubsystem->UnregisterWaystone(this);
	}
	if (UFXSystemComponent* Effect = AttunedEffect.Get())
	{
		Effect->DestroyComponent();
	}
	Super::EndPlay(EndPlayReason);
}

FTransform ABeyondWaystone::GetArrivalTransform() const
{
	const FTransform Arrival = ArrivalPoint->GetComponentTransform();
	return FTransform(FRotator(0.0f, Arrival.Rotator().Yaw, 0.0f), Arrival.GetLocation());
}

FText ABeyondWaystone::GetDisplayNameOrId() const
{
	return DisplayName.IsEmpty() ? FText::FromName(WaystoneId) : DisplayName;
}

void ABeyondWaystone::RefreshAttunedLook(bool bAttuned)
{
	if (bShownAttuned == bAttuned)
	{
		return;
	}
	bShownAttuned = bAttuned;
	Glow->SetLightColor(GlowColour);
	Glow->SetIntensity(bAttuned ? 4000.0f : 0.0f);
	if (bAttuned && AttunedFX.IsSet() && !AttunedEffect.IsValid())
	{
		AttunedEffect = BeyondFX::SpawnAttached(AttunedFX, Mesh);
	}
	else if (!bAttuned)
	{
		if (UFXSystemComponent* Effect = AttunedEffect.Get())
		{
			Effect->DestroyComponent();
		}
		AttunedEffect.Reset();
	}
}

void ABeyondWaystone::PlayActivateFX()
{
	if (ActivateFX.IsSet())
	{
		BeyondFX::SpawnAtLocation(this, ActivateFX, GetActorLocation() + FVector(0.0f, 0.0f, 120.0f));
	}
}
