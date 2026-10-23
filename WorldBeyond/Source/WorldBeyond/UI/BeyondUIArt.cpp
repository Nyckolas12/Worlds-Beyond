// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondUIArt.h"
#include "Engine/Texture2D.h"

UBeyondUIArtSet::UBeyondUIArtSet()
{
	// A pack texture until the generated art is in (the rune circle is drawn in code without one)
	Glow = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SkillTreeSystem/Textures/T_RadialGradient.T_RadialGradient")));
}
