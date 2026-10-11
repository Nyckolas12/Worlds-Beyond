// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondWorldInfo.h"
#include "WorldBeyond.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "AbilitySystem/BeyondFX.h"
#include "World/BeyondWorldSubsystem.h"

namespace BeyondWorldInfoLocal
{
	template <typename TComponent>
	TComponent* FindOn(AActor* Actor)
	{
		return Actor ? Actor->FindComponentByClass<TComponent>() : nullptr;
	}
}

ABeyondWorldInfo::ABeyondWorldInfo()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bIsSpatiallyLoaded = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	Grading = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Grading"));
	Grading->SetupAttachment(GetRootComponent());
	Grading->bUnbound = true;
	Grading->Priority = 1.0f;
	Grading->BlendWeight = 1.0f;

	auto MakeAudio = [this](const TCHAR* Name)
	{
		UAudioComponent* Audio = CreateDefaultSubobject<UAudioComponent>(Name);
		Audio->SetupAttachment(GetRootComponent());
		Audio->bAutoActivate = false;
		Audio->bAllowSpatialization = false;
		// Keeps playing while a menu pauses the game
		Audio->bIsUISound = true;
		return Audio;
	};
	MusicA = MakeAudio(TEXT("MusicA"));
	MusicB = MakeAudio(TEXT("MusicB"));
	Ambience = MakeAudio(TEXT("Ambience"));

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(GetRootComponent());
		Sprite->bIsScreenSizeScaled = true;
	}
#endif
}

void ABeyondWorldInfo::BeginPlay()
{
	Super::BeginPlay();
	FindLights();
	ReadCurrentWeather();
	TargetWeather = CurrentWeather;
	FromWeather = CurrentWeather;
	if (UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this))
	{
		WorldSubsystem->RegisterWorldInfo(this);
	}
}

void ABeyondWorldInfo::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this))
	{
		WorldSubsystem->UnregisterWorldInfo(this);
	}
	if (UFXSystemComponent* Effect = PrecipitationEffect.Get())
	{
		Effect->DestroyComponent();
	}
	Super::EndPlay(EndPlayReason);
}

void ABeyondWorldInfo::FindLights()
{
	UWorld* World = GetWorld();
	if (!Sun)
	{
		TActorIterator<ADirectionalLight> It(World);
		Sun = It ? *It : nullptr;
	}
	if (!SkyLight)
	{
		TActorIterator<ASkyLight> It(World);
		SkyLight = It ? *It : nullptr;
	}
	if (!HeightFog)
	{
		TActorIterator<AExponentialHeightFog> It(World);
		HeightFog = It ? *It : nullptr;
	}
}

void ABeyondWorldInfo::ReadCurrentWeather()
{
	using namespace BeyondWorldInfoLocal;
	CurrentWeather = DefaultWeather;
	if (const UDirectionalLightComponent* SunLight = FindOn<UDirectionalLightComponent>(Sun))
	{
		CurrentWeather.SunIntensity = SunLight->Intensity;
		CurrentWeather.SunColor = SunLight->GetLightColor();
	}
	if (const USkyLightComponent* Sky = FindOn<USkyLightComponent>(SkyLight))
	{
		CurrentWeather.SkyLightIntensity = Sky->Intensity;
		CurrentWeather.SkyLightColor = Sky->GetLightColor();
	}
	if (const UExponentialHeightFogComponent* Fog = FindOn<UExponentialHeightFogComponent>(HeightFog))
	{
		CurrentWeather.FogDensity = Fog->FogDensity;
		CurrentWeather.FogHeightFalloff = Fog->FogHeightFalloff;
		CurrentWeather.FogColor = Fog->FogInscatteringLuminance;
	}
}

void ABeyondWorldInfo::SetWeather(const FBeyondWeatherPreset& Weather, float InBlendTime)
{
	FromWeather = CurrentWeather;
	TargetWeather = Weather;
	BlendTime = FMath::Max(InBlendTime, 0.0f);
	BlendElapsed = 0.0f;
	SetPrecipitation(Weather.Precipitation);
	if (BlendTime <= 0.0f)
	{
		ApplyWeather(TargetWeather);
		SetActorTickEnabled(false);
	}
	else
	{
		SetActorTickEnabled(true);
	}
}

void ABeyondWorldInfo::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	BlendElapsed += DeltaSeconds;
	const float Alpha = BlendTime > 0.0f ? FMath::Clamp(BlendElapsed / BlendTime, 0.0f, 1.0f) : 1.0f;
	ApplyWeather(FromWeather.Lerp(TargetWeather, FMath::SmoothStep(0.0f, 1.0f, Alpha)));
	if (Alpha >= 1.0f)
	{
		SetActorTickEnabled(false);
	}
}

void ABeyondWorldInfo::ApplyWeather(const FBeyondWeatherPreset& Weather)
{
	using namespace BeyondWorldInfoLocal;
	CurrentWeather = Weather;
	if (!bApplyWeather)
	{
		return;
	}
	if (UDirectionalLightComponent* SunLight = FindOn<UDirectionalLightComponent>(Sun))
	{
		SunLight->SetIntensity(Weather.SunIntensity);
		SunLight->SetLightColor(Weather.SunColor);
	}
	if (USkyLightComponent* Sky = FindOn<USkyLightComponent>(SkyLight))
	{
		Sky->SetIntensity(Weather.SkyLightIntensity);
		Sky->SetLightColor(Weather.SkyLightColor);
	}
	if (UExponentialHeightFogComponent* Fog = FindOn<UExponentialHeightFogComponent>(HeightFog))
	{
		Fog->SetFogDensity(Weather.FogDensity);
		Fog->SetFogHeightFalloff(Weather.FogHeightFalloff);
		Fog->SetFogInscatteringColor(Weather.FogColor);
	}

	FPostProcessSettings& Settings = Grading->Settings;
	Settings.bOverride_ColorSaturation = true;
	Settings.ColorSaturation = FVector4(1.0f, 1.0f, 1.0f, Weather.Saturation);
	Settings.bOverride_ColorGain = true;
	Settings.ColorGain = FVector4(Weather.Tint.R, Weather.Tint.G, Weather.Tint.B, 1.0f);
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = Weather.ExposureBias;
}

void ABeyondWorldInfo::SetPrecipitation(const TSoftObjectPtr<UFXSystemAsset>& System)
{
	if (System == CurrentPrecipitation && (PrecipitationEffect.IsValid() || System.IsNull()))
	{
		return;
	}
	CurrentPrecipitation = System;
	// The old one stops emitting and goes once its last flakes have fallen
	if (UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(PrecipitationEffect.Get()))
	{
		Niagara->SetAutoDestroy(true);
		Niagara->Deactivate();
	}
	else if (UParticleSystemComponent* Cascade = Cast<UParticleSystemComponent>(PrecipitationEffect.Get()))
	{
		Cascade->bAutoDestroy = true;
		Cascade->Deactivate();
	}
	else if (UFXSystemComponent* Effect = PrecipitationEffect.Get())
	{
		Effect->DestroyComponent();
	}
	PrecipitationEffect.Reset();

	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	USceneComponent* Camera = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetRootComponent() : nullptr;
	if (System.IsNull() || !Camera)
	{
		return;
	}
	FBeyondFX FX;
	FX.System = System.LoadSynchronous();
	FX.MaxLifetime = 0.0f;
	FX.Offset = FVector(0.0f, 0.0f, 400.0f);
	PrecipitationEffect = BeyondFX::SpawnAttached(FX, Camera);
}

void ABeyondWorldInfo::SetMusic(USoundBase* Track, float FadeTime)
{
	if (Track == CurrentMusic.Get())
	{
		return;
	}
	CurrentMusic = Track;
	UAudioComponent* Old = bMusicOnA ? MusicA : MusicB;
	UAudioComponent* New = bMusicOnA ? MusicB : MusicA;
	if (Old->IsPlaying())
	{
		Old->FadeOut(FMath::Max(FadeTime, 0.01f), 0.0f);
	}
	if (Track)
	{
		New->SetSound(Track);
		New->FadeIn(FMath::Max(FadeTime, 0.01f), 1.0f);
		bMusicOnA = !bMusicOnA;
	}
	UE_LOG(LogBeyond, Log, TEXT("World: music %s"), *GetNameSafe(Track));
}

void ABeyondWorldInfo::SetAmbience(USoundBase* Loop, float FadeTime)
{
	if (Loop == CurrentAmbience.Get())
	{
		return;
	}
	CurrentAmbience = Loop;
	if (Ambience->IsPlaying())
	{
		Ambience->FadeOut(FMath::Max(FadeTime, 0.01f), 0.0f);
	}
	if (Loop)
	{
		Ambience->SetSound(Loop);
		Ambience->FadeIn(FMath::Max(FadeTime, 0.01f), 1.0f);
	}
}
