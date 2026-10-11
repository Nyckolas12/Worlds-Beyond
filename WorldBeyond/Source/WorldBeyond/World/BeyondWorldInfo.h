// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/BeyondRegionDefinition.h"
#include "BeyondWorldInfo.generated.h"

class UAudioComponent;
class UBeyondWorldMapData;
class UBillboardComponent;
class UFXSystemComponent;
class UPostProcessComponent;
class USoundBase;

/**
 * One per open-world map (Plan 5): turns on the world systems for it (start / resume hold, weather, music), holds the
 * world map's picture and the lights the region weather drives. Region weather blends over Weather Blend Time; the
 * region's music crossfades between two tracks; its ambience loops underneath; precipitation follows the camera.
 * Lights left empty are found in the level when play starts. Always loaded.
 */
UCLASS()
class WORLDBEYOND_API ABeyondWorldInfo : public AActor
{
	GENERATED_BODY()

public:
	ABeyondWorldInfo();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FText WorldName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	TObjectPtr<UBeyondWorldMapData> MapData;

	// The weather outside every region volume (and before the party is anywhere)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World|Weather")
	FBeyondWeatherPreset DefaultWeather;

	// Region weather drives the lights and the grading below
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World|Weather")
	bool bApplyWeather = true;

	// Directional light actor (empty: the first one in the level)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World|Weather")
	TObjectPtr<AActor> Sun;

	// Sky light actor (empty: the first one in the level)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World|Weather")
	TObjectPtr<AActor> SkyLight;

	// Exponential height fog actor (empty: the first one in the level)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World|Weather")
	TObjectPtr<AActor> HeightFog;

	// Region grading (saturation, tint, exposure), unbound
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|Weather")
	TObjectPtr<UPostProcessComponent> Grading;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|Sound")
	TObjectPtr<UAudioComponent> MusicA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|Sound")
	TObjectPtr<UAudioComponent> MusicB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|Sound")
	TObjectPtr<UAudioComponent> Ambience;

	// Blends to Weather over Blend Time seconds (0: at once)
	UFUNCTION(BlueprintCallable, Category = "World|Weather")
	void SetWeather(const FBeyondWeatherPreset& Weather, float BlendTime);

	UFUNCTION(BlueprintPure, Category = "World|Weather")
	FBeyondWeatherPreset GetTargetWeather() const { return TargetWeather; }

	UFUNCTION(BlueprintPure, Category = "World|Weather")
	FBeyondWeatherPreset GetCurrentWeather() const { return CurrentWeather; }

	// Crossfades to Track (null: fades the music out); the same track keeps playing
	UFUNCTION(BlueprintCallable, Category = "World|Sound")
	void SetMusic(USoundBase* Track, float FadeTime);

	UFUNCTION(BlueprintCallable, Category = "World|Sound")
	void SetAmbience(USoundBase* Loop, float FadeTime);

	UFUNCTION(BlueprintPure, Category = "World|Sound")
	USoundBase* GetCurrentMusic() const { return CurrentMusic.Get(); }

	UFUNCTION(BlueprintPure, Category = "World|Sound")
	USoundBase* GetCurrentAmbience() const { return CurrentAmbience.Get(); }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void FindLights();
	void ReadCurrentWeather();
	void ApplyWeather(const FBeyondWeatherPreset& Weather);
	void SetPrecipitation(const TSoftObjectPtr<UFXSystemAsset>& System);

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif

	FBeyondWeatherPreset FromWeather;
	FBeyondWeatherPreset TargetWeather;
	FBeyondWeatherPreset CurrentWeather;
	float BlendTime = 0.0f;
	float BlendElapsed = 0.0f;

	TWeakObjectPtr<USoundBase> CurrentMusic;
	TWeakObjectPtr<USoundBase> CurrentAmbience;
	bool bMusicOnA = false;

	TWeakObjectPtr<UFXSystemComponent> PrecipitationEffect;
	TSoftObjectPtr<UFXSystemAsset> CurrentPrecipitation;
};
