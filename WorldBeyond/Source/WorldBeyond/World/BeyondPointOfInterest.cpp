// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondPointOfInterest.h"
#include "Components/BillboardComponent.h"
#include "World/BeyondWorldSubsystem.h"

#define LOCTEXT_NAMESPACE "BeyondPlace"

ABeyondPointOfInterest::ABeyondPointOfInterest()
{
	PrimaryActorTick.bCanEverTick = false;
	bIsSpatiallyLoaded = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(GetRootComponent());
		Sprite->bIsScreenSizeScaled = true;
	}
#endif
}

void ABeyondPointOfInterest::BeginPlay()
{
	Super::BeginPlay();
	if (UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this))
	{
		WorldSubsystem->RegisterPointOfInterest(this);
	}
}

void ABeyondPointOfInterest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this))
	{
		WorldSubsystem->UnregisterPointOfInterest(this);
	}
	Super::EndPlay(EndPlayReason);
}

FText ABeyondPointOfInterest::GetDisplayNameOrId() const
{
	return DisplayName.IsEmpty() ? FText::FromName(PoiId) : DisplayName;
}

FText ABeyondPointOfInterest::GetKindName(EBeyondPoiKind InKind)
{
	switch (InKind)
	{
	case EBeyondPoiKind::Cave: return LOCTEXT("Cave", "Cave");
	case EBeyondPoiKind::Shrine: return LOCTEXT("Shrine", "Shrine");
	case EBeyondPoiKind::Ruins: return LOCTEXT("Ruins", "Ruins");
	case EBeyondPoiKind::Vista: return LOCTEXT("Vista", "Vista");
	case EBeyondPoiKind::Lake: return LOCTEXT("Lake", "Lake");
	case EBeyondPoiKind::Camp: return LOCTEXT("Camp", "Camp");
	case EBeyondPoiKind::ArenaSite: return LOCTEXT("ArenaSite", "Arena");
	default: return LOCTEXT("Landmark", "Landmark");
	}
}

#undef LOCTEXT_NAMESPACE
