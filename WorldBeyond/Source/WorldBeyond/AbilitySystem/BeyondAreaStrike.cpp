// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/BeyondAreaStrike.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "BeyondGameplayTags.h"
#include "Characters/BeyondCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemies/BeyondEnemySettings.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystemComponent.h"

namespace
{
	constexpr float StrikeDecalDepth = 320.0f;
	constexpr float StrikeHeightTolerance = 420.0f;
	constexpr float StrikeLingerTickInterval = 0.5f;
	constexpr float StrikeFadeInTime = 0.15f;
	constexpr float StrikeFadeOutTime = 0.3f;

	float StrikeCapsuleTolerance(const AActor* Actor)
	{
		const ACharacter* Character = Cast<ACharacter>(Actor);
		return Character ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.5f : 20.0f;
	}
}

ABeyondAreaStrike::ABeyondAreaStrike()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetCanBeDamaged(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Marker"));
	Decal->SetupAttachment(Root);
	// Projects straight down; the decal's Z axis lies along the strike's facing
	Decal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	Decal->SetFadeScreenSize(0.0f);
	Decal->SortOrder = 10;
	Decal->SetVisibility(false);

	FallingMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallingMesh"));
	FallingMeshComponent->SetupAttachment(Root);
	FallingMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FallingMeshComponent->SetReceivesDecals(false);
	FallingMeshComponent->SetCastShadow(true);
	FallingMeshComponent->SetVisibility(false);
}

ABeyondAreaStrike* ABeyondAreaStrike::SpawnStrike(const UObject* WorldContext, AActor* Instigator, const FBeyondStrikeSettings& Settings, const FVector& Location,
	float Yaw, float StartDelay, float DamageScale)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	const FTransform Transform(FRotator(0.0f, Yaw, 0.0f), Location);
	ABeyondAreaStrike* Strike = World->SpawnActorDeferred<ABeyondAreaStrike>(StaticClass(), Transform, nullptr, Cast<APawn>(Instigator),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Strike)
	{
		return nullptr;
	}
	Strike->Configure(Instigator, Settings, Yaw, StartDelay, DamageScale);
	Strike->FinishSpawning(Transform);
	return Strike;
}

void ABeyondAreaStrike::Configure(AActor* InInstigator, const FBeyondStrikeSettings& InSettings, float InYaw, float StartDelay, float InDamageScale)
{
	StrikeInstigator = InInstigator;
	bHadInstigator = InInstigator != nullptr;
	Settings = InSettings;
	Yaw = InYaw;
	DamageScale = FMath::Max(InDamageScale, 0.0f);
	Elapsed = -FMath::Max(StartDelay, 0.0f);

	const UBeyondEnemySettings* EnemySettings = GetDefault<UBeyondEnemySettings>();
	const bool bHazard = Settings.Damage <= 0.0f && Settings.LingerDuration > 0.0f;
	MarkerColor = Settings.Color.A > 0.0f ? Settings.Color : (bHazard ? EnemySettings->HazardColor : EnemySettings->TelegraphColor);

	// Size the marker to the shape: lines start at the spawn point, everything else is centred on it
	const float Radius = FMath::Max(Settings.Radius, 10.0f);
	if (Settings.Shape == EBeyondStrikeShape::Line)
	{
		Decal->DecalSize = FVector(StrikeDecalDepth, Settings.Width * 0.5f, Settings.Length * 0.5f);
		Decal->SetRelativeLocation(FVector(Settings.Length * 0.5f, 0.0f, 0.0f));
	}
	else
	{
		Decal->DecalSize = FVector(StrikeDecalDepth, Radius, Radius);
	}

	if (UMaterialInterface* Material = EnemySettings->TelegraphMaterial.LoadSynchronous())
	{
		Decal->SetDecalMaterial(Material);
		DecalMaterial = Decal->CreateDynamicMaterialInstance();
	}
	if (DecalMaterial)
	{
		DecalMaterial->SetScalarParameterValue(TEXT("Shape"), static_cast<float>(static_cast<uint8>(Settings.Shape)));
		DecalMaterial->SetScalarParameterValue(TEXT("Inner"), Settings.Shape == EBeyondStrikeShape::Ring ? FMath::Clamp(Settings.InnerRadius / Radius, 0.0f, 0.95f) : 0.0f);
		DecalMaterial->SetScalarParameterValue(TEXT("HalfAngle"), FMath::DegreesToRadians(FMath::Clamp(Settings.ConeAngle, 5.0f, 360.0f) * 0.5f));
		DecalMaterial->SetVectorParameterValue(TEXT("Color"), MarkerColor);
	}

	if (Settings.FallingMesh)
	{
		FallingMeshComponent->SetStaticMesh(Settings.FallingMesh);
		FallingMeshComponent->SetWorldScale3D(FVector(Settings.FallingMeshScale));
	}
}

void ABeyondAreaStrike::BeginPlay()
{
	Super::BeginPlay();
	UpdateMarker();
}

void ABeyondAreaStrike::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFXSystemComponent* Linger = LingerComponent.Get())
	{
		Linger->DestroyComponent();
	}
	if (UFXSystemComponent* Trail = FallingTrail.Get())
	{
		Trail->DestroyComponent();
	}
	Super::EndPlay(EndPlayReason);
}

float ABeyondAreaStrike::GetFill() const
{
	if (bStruck)
	{
		return 1.0f;
	}
	return Settings.WindUp > 0.0f ? FMath::Clamp(Elapsed / Settings.WindUp, 0.0f, 1.0f) : 1.0f;
}

FVector ABeyondAreaStrike::GetShapeCentre() const
{
	return Settings.Shape == EBeyondStrikeShape::Line
		? GetActorLocation() + GetActorForwardVector() * Settings.Length * 0.5f
		: GetActorLocation();
}

void ABeyondAreaStrike::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The caster died before it landed: no hit from beyond the grave (lingering hazards are already down)
	if (!bStruck && bCancelWithInstigator && bHadInstigator && (!StrikeInstigator.IsValid() || UBeyondCombatLibrary::IsActorDead(StrikeInstigator.Get())))
	{
		Destroy();
		return;
	}

	Elapsed += DeltaSeconds;
	if (Elapsed < 0.0f)
	{
		return;
	}

	if (!bStruck)
	{
		if (Settings.FallingMesh)
		{
			// Falls from high and slightly behind, speeding up, lands exactly as the marker fills
			const float Fill = GetFill();
			const FVector Start = GetShapeCentre() + FVector(0.0f, 0.0f, Settings.FallHeight) - GetActorForwardVector() * Settings.FallHeight * 0.35f;
			FallingMeshComponent->SetWorldLocation(FMath::Lerp(Start, GetShapeCentre(), Fill * Fill));
			if (!FallingMeshComponent->IsVisible())
			{
				FallingMeshComponent->SetVisibility(true);
				FallingTrail = BeyondFX::SpawnAttached(Settings.FallingFX, FallingMeshComponent);
			}
		}

		if (Elapsed >= Settings.WindUp)
		{
			Strike();
		}
	}
	else if (Settings.LingerDuration > 0.0f)
	{
		LingerElapsed += DeltaSeconds;
		if (LingerElapsed >= NextLingerTick && Settings.LingerDamagePerSecond > 0.0f)
		{
			NextLingerTick += StrikeLingerTickInterval;
			DamageInside(Settings.LingerDamagePerSecond * StrikeLingerTickInterval,
				Settings.LingerDamageType.IsValid() ? Settings.LingerDamageType : BeyondTags::DamageType_Environment, FGameplayTag(), true);
		}
		if (LingerElapsed >= Settings.LingerDuration)
		{
			Destroy();
			return;
		}
	}
	else if (Elapsed - Settings.WindUp >= StrikeFadeOutTime)
	{
		Destroy();
		return;
	}

	UpdateMarker();
}

void ABeyondAreaStrike::UpdateMarker()
{
	const bool bShown = Elapsed >= 0.0f;
	Decal->SetVisibility(bShown);
	if (!bShown || !DecalMaterial)
	{
		return;
	}

	float Opacity = FMath::Clamp(Elapsed / StrikeFadeInTime, 0.0f, 1.0f);
	if (bStruck)
	{
		if (Settings.LingerDuration > 0.0f)
		{
			// Fades out over its last half second
			Opacity = 0.85f * FMath::Clamp((Settings.LingerDuration - LingerElapsed) / 0.5f, 0.0f, 1.0f);
		}
		else
		{
			Opacity = 1.0f - FMath::Clamp((Elapsed - Settings.WindUp) / StrikeFadeOutTime, 0.0f, 1.0f);
		}
	}

	DecalMaterial->SetScalarParameterValue(TEXT("Fill"), GetFill());
	DecalMaterial->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	DecalMaterial->SetScalarParameterValue(TEXT("Linger"), bStruck && Settings.LingerDuration > 0.0f ? 1.0f : 0.0f);
}

void ABeyondAreaStrike::Strike()
{
	bStruck = true;
	FallingMeshComponent->SetVisibility(false);
	if (UFXSystemComponent* Trail = FallingTrail.Get())
	{
		Trail->DestroyComponent();
	}

	BeyondFX::SpawnAtLocation(this, Settings.ImpactFX, GetShapeCentre(), GetActorRotation());

	if (Settings.Damage > 0.0f)
	{
		DamageInside(Settings.Damage * DamageScale,
			Settings.DamageType.IsValid() ? Settings.DamageType : BeyondTags::DamageType_Explosion, Settings.HitResponse, Settings.bUnblockable);
	}

	if (Settings.LingerDuration > 0.0f)
	{
		LingerComponent = BeyondFX::SpawnAttached(Settings.LingerFX, GetRootComponent());
		NextLingerTick = StrikeLingerTickInterval;
	}
}

bool ABeyondAreaStrike::IsTarget(const AActor* Actor) const
{
	if (!Actor || UBeyondCombatLibrary::IsActorDead(Actor))
	{
		return false;
	}
	if (const AActor* Source = StrikeInstigator.Get())
	{
		return UBeyondCombatLibrary::AreHostile(Source, Actor);
	}
	// No instigator (arena hazards) or it's gone (a dead elite's lava): the demigods are the targets
	const ABeyondCharacterBase* Character = Cast<ABeyondCharacterBase>(Actor);
	return Character && Character->TeamAffiliation == EBeyondTeam::Player;
}

bool ABeyondAreaStrike::IsInside(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	const FVector Origin = GetActorLocation();
	const FVector ToActor = Actor->GetActorLocation() - Origin;
	if (FMath::Abs(ToActor.Z) > StrikeHeightTolerance)
	{
		return false;
	}

	const float Tolerance = StrikeCapsuleTolerance(Actor);
	const FVector Flat(ToActor.X, ToActor.Y, 0.0f);
	const float Distance = Flat.Size();
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();

	switch (Settings.Shape)
	{
	case EBeyondStrikeShape::Circle:
		return Distance <= Settings.Radius + Tolerance;
	case EBeyondStrikeShape::Ring:
		return Distance <= Settings.Radius + Tolerance && Distance >= Settings.InnerRadius - Tolerance;
	case EBeyondStrikeShape::Cone:
	{
		if (Distance > Settings.Radius + Tolerance)
		{
			return false;
		}
		if (Distance <= Tolerance * 2.0f || Settings.ConeAngle >= 359.0f)
		{
			return true;
		}
		const float CosAngle = FVector::DotProduct(Flat / Distance, Forward);
		return CosAngle >= FMath::Cos(FMath::DegreesToRadians(Settings.ConeAngle * 0.5f));
	}
	case EBeyondStrikeShape::Line:
	{
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
		const float Along = FVector::DotProduct(Flat, Forward);
		const float Across = FMath::Abs(FVector::DotProduct(Flat, Right));
		return Along >= -Tolerance && Along <= Settings.Length + Tolerance && Across <= Settings.Width * 0.5f + Tolerance;
	}
	}
	return false;
}

void ABeyondAreaStrike::DamageInside(float Amount, const FGameplayTag& DamageTypeTag, const FGameplayTag& Response, bool bUnblockableHit)
{
	UWorld* World = GetWorld();
	if (!World || Amount <= 0.0f)
	{
		return;
	}

	const float QueryRadius = (Settings.Shape == EBeyondStrikeShape::Line ? Settings.Length * 0.5f + Settings.Width : Settings.Radius) + 150.0f;
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondAreaStrike), false, this);
	World->OverlapMultiByObjectType(Overlaps, GetShapeCentre(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(QueryRadius), Params);

	TSet<AActor*> Hit;
	AActor* Source = StrikeInstigator.Get();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!Actor || Hit.Contains(Actor) || !IsTarget(Actor) || !IsInside(Actor))
		{
			continue;
		}
		Hit.Add(Actor);
		// Unparryable: you step out of a telegraph, you don't parry the floor
		UBeyondCombatLibrary::ApplyDamage(Source, Actor, Amount, DamageTypeTag, Response, bUnblockableHit, this, true);
	}
}
