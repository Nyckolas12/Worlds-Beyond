// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondRegionDefinition.h"

#define LOCTEXT_NAMESPACE "BeyondRegion"

FBeyondWeatherPreset FBeyondWeatherPreset::Lerp(const FBeyondWeatherPreset& To, float Alpha) const
{
	FBeyondWeatherPreset Result = Alpha < 0.5f ? *this : To;
	Result.SunIntensity = FMath::Lerp(SunIntensity, To.SunIntensity, Alpha);
	Result.SunColor = FMath::Lerp(SunColor, To.SunColor, Alpha);
	Result.SkyLightIntensity = FMath::Lerp(SkyLightIntensity, To.SkyLightIntensity, Alpha);
	Result.SkyLightColor = FMath::Lerp(SkyLightColor, To.SkyLightColor, Alpha);
	Result.FogDensity = FMath::Lerp(FogDensity, To.FogDensity, Alpha);
	Result.FogHeightFalloff = FMath::Lerp(FogHeightFalloff, To.FogHeightFalloff, Alpha);
	Result.FogColor = FMath::Lerp(FogColor, To.FogColor, Alpha);
	Result.Saturation = FMath::Lerp(Saturation, To.Saturation, Alpha);
	Result.Tint = FMath::Lerp(Tint, To.Tint, Alpha);
	Result.ExposureBias = FMath::Lerp(ExposureBias, To.ExposureBias, Alpha);
	return Result;
}

FText UBeyondRegionDefinition::GetLevelText() const
{
	const int32 Low = FMath::Max(LevelMin, 1);
	const int32 High = FMath::Max(LevelMax, Low);
	return Low == High
		? FText::Format(LOCTEXT("LevelOne", "Lv {0}"), Low)
		: FText::Format(LOCTEXT("LevelBand", "Lv {0}–{1}"), Low, High);
}

FText UBeyondRegionDefinition::GetDisplayNameOrId() const
{
	return DisplayName.IsEmpty() ? FText::FromName(RegionId) : DisplayName;
}

#undef LOCTEXT_NAMESPACE
