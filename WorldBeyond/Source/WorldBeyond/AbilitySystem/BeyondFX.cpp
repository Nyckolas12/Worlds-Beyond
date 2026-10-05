// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/BeyondFX.h"
#include "Camera/CameraShakeBase.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"

namespace BeyondFX
{
	namespace
	{
		void ApplyTint(UFXSystemComponent* Component, const FBeyondFX& FX)
		{
			if (!Component || !FX.bOverrideColor)
			{
				return;
			}
			if (UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(Component))
			{
				Niagara->SetVariableLinearColor(FX.ColorParameter, FX.Color);
			}
			else if (UParticleSystemComponent* Cascade = Cast<UParticleSystemComponent>(Component))
			{
				Cascade->SetColorParameter(FX.ColorParameter, FX.Color);
			}
		}

		void PlaySoundAndShake(const UObject* WorldContext, const FBeyondFX& FX, const FVector& Location)
		{
			if (FX.Sound)
			{
				UGameplayStatics::PlaySoundAtLocation(WorldContext, FX.Sound, Location);
			}
			if (FX.CameraShake)
			{
				UGameplayStatics::PlayWorldCameraShake(WorldContext, FX.CameraShake, Location, 0.0f, FX.CameraShakeRadius);
			}
		}
	}

	UFXSystemComponent* SpawnAtLocation(const UObject* WorldContext, const FBeyondFX& FX, const FVector& Location, const FRotator& Rotation)
	{
		if (!WorldContext || !WorldContext->GetWorld())
		{
			return nullptr;
		}

		const FVector SpawnLocation = Location + Rotation.RotateVector(FX.Offset);
		UFXSystemComponent* Spawned = nullptr;
		if (UNiagaraSystem* Niagara = Cast<UNiagaraSystem>(FX.System))
		{
			Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext, Niagara, SpawnLocation, Rotation, FX.Scale);
		}
		else if (UParticleSystem* Cascade = Cast<UParticleSystem>(FX.System))
		{
			Spawned = UGameplayStatics::SpawnEmitterAtLocation(WorldContext, Cascade, SpawnLocation, Rotation, FX.Scale);
		}

		ApplyTint(Spawned, FX);
		PlaySoundAndShake(WorldContext, FX, SpawnLocation);
		return Spawned;
	}

	UFXSystemComponent* SpawnAttached(const FBeyondFX& FX, USceneComponent* AttachTo, FName Socket)
	{
		if (!AttachTo)
		{
			return nullptr;
		}

		UFXSystemComponent* Spawned = nullptr;
		if (UNiagaraSystem* Niagara = Cast<UNiagaraSystem>(FX.System))
		{
			Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(Niagara, AttachTo, Socket, FX.Offset, FRotator::ZeroRotator, FX.Scale,
				EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::None);
		}
		else if (UParticleSystem* Cascade = Cast<UParticleSystem>(FX.System))
		{
			Spawned = UGameplayStatics::SpawnEmitterAttached(Cascade, AttachTo, Socket, FX.Offset, FRotator::ZeroRotator, FX.Scale);
		}

		ApplyTint(Spawned, FX);
		PlaySoundAndShake(AttachTo, FX, AttachTo->GetSocketLocation(Socket));
		return Spawned;
	}
}
