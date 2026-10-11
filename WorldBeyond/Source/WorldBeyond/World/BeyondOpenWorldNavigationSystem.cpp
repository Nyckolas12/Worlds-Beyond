// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondOpenWorldNavigationSystem.h"

UBeyondOpenWorldNavigationSystem::UBeyondOpenWorldNavigationSystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bGenerateNavigationOnlyAroundNavigationInvokers = true;
}

void UBeyondOpenWorldNavigationSystem::PostInitProperties()
{
	Super::PostInitProperties();
	// After the config is read, so a project-wide setting can't switch it off for the open world
	bGenerateNavigationOnlyAroundNavigationInvokers = true;
}
