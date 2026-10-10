// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/BeyondLootDrop.h"
#include "AbilitySystem/BeyondFX.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Items/BeyondItemLibrary.h"
#include "Items/BeyondLootSettings.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"

#define LOCTEXT_NAMESPACE "BeyondItems"

ABeyondLootDrop::ABeyondLootDrop()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->Mobility = EComponentMobility::Movable;
	RootComponent = SceneRoot;

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(SceneRoot);
	Light->Mobility = EComponentMobility::Movable;
	Light->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	Light->IntensityUnits = ELightUnits::Candelas;
	Light->Intensity = BaseIntensity;
	Light->AttenuationRadius = 320.0f;
	Light->CastShadows = false;
}

void ABeyondLootDrop::BeginPlay()
{
	Super::BeginPlay();
	if (UBeyondLootSubsystem* Loot = UBeyondLootSubsystem::Get(this))
	{
		Loot->RegisterDrop(this);
	}
	RefreshLook();
}

void ABeyondLootDrop::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBeyondLootSubsystem* Loot = UBeyondLootSubsystem::Get(this))
	{
		Loot->UnregisterDrop(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ABeyondLootDrop::SetItem(const FBeyondItemInstance& InItem)
{
	Item = InItem;
	if (HasActorBegunPlay())
	{
		RefreshLook();
	}
}

FText ABeyondLootDrop::GetLabel() const
{
	return FText::Format(LOCTEXT("DropLabel", "{0} ({1})"), UBeyondItemLibrary::GetItemName(Item), UBeyondItemLibrary::GetTierName(Item.Tier));
}

void ABeyondLootDrop::RefreshLook()
{
	if (UFXSystemComponent* Old = Glow.Get())
	{
		Old->DestroyComponent();
	}
	Glow.Reset();

	const FLinearColor Color = UBeyondItemLibrary::GetTierColor(Item.Tier);
	Light->SetLightColor(Color);
	// Better loot shines brighter
	BaseIntensity = 20.0f + 12.0f * static_cast<int32>(Item.Tier);
	Light->SetIntensity(BaseIntensity);

	const UBeyondLootSettings* Settings = GetDefault<UBeyondLootSettings>();
	const int32 TierIndex = static_cast<int32>(Item.Tier);
	if (Settings->DropGlow.IsValidIndex(TierIndex))
	{
		FBeyondFX GlowFX;
		GlowFX.System = Settings->DropGlow[TierIndex].LoadSynchronous();
		GlowFX.MaxLifetime = 0.0f;
		Glow = BeyondFX::SpawnAttached(GlowFX, SceneRoot);
	}
}

void ABeyondLootDrop::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	Light->SetIntensity(BaseIntensity * (0.8f + 0.2f * FMath::Sin(Age * 3.0f)));
}

//~ Subsystem

UBeyondLootSubsystem* UBeyondLootSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBeyondLootSubsystem>() : nullptr;
}

void UBeyondLootSubsystem::RegisterDrop(ABeyondLootDrop* Drop)
{
	Drops.AddUnique(Drop);
}

void UBeyondLootSubsystem::UnregisterDrop(ABeyondLootDrop* Drop)
{
	Drops.Remove(Drop);
}

int32 UBeyondLootSubsystem::GetDropCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ABeyondLootDrop>& Drop : Drops)
	{
		Count += Drop.IsValid() ? 1 : 0;
	}
	return Count;
}

ABeyondLootDrop* UBeyondLootSubsystem::FindNearestDrop(const FVector& Location, float MaxDistance) const
{
	ABeyondLootDrop* Best = nullptr;
	float BestDistance = MaxDistance * MaxDistance;
	for (const TWeakObjectPtr<ABeyondLootDrop>& Weak : Drops)
	{
		ABeyondLootDrop* Drop = Weak.Get();
		if (!Drop)
		{
			continue;
		}
		const float Distance = FVector::DistSquared(Location, Drop->GetActorLocation());
		if (Distance <= BestDistance)
		{
			BestDistance = Distance;
			Best = Drop;
		}
	}
	return Best;
}

TArray<ABeyondLootDrop*> UBeyondLootSubsystem::SpawnDrops(const TArray<FBeyondItemInstance>& Items, const FVector& Location)
{
	TArray<ABeyondLootDrop*> Spawned;
	UWorld* World = GetWorld();
	if (!World || Items.IsEmpty())
	{
		return Spawned;
	}

	const float StartAngle = FMath::FRandRange(0.0f, 360.0f);
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		// A small ring around the body (one item: right where it fell)
		const float Angle = FMath::DegreesToRadians(StartAngle + 360.0f * Index / Items.Num());
		const float Distance = Items.Num() > 1 ? 110.0f : 40.0f;
		FVector Spot = Location + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;

		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondLootGround), false);
		if (World->LineTraceSingleByObjectType(Hit, Spot + FVector(0.0f, 0.0f, 150.0f), Spot - FVector(0.0f, 0.0f, 600.0f),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Spot = Hit.ImpactPoint;
		}
		Spot.Z += 20.0f;

		if (ABeyondLootDrop* Drop = World->SpawnActorDeferred<ABeyondLootDrop>(ABeyondLootDrop::StaticClass(), FTransform(Spot), nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Drop->SetItem(Items[Index]);
			Drop->FinishSpawning(FTransform(Spot));
			Spawned.Add(Drop);
		}
	}
	return Spawned;
}

#undef LOCTEXT_NAMESPACE
