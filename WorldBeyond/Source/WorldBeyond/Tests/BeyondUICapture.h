// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

class UUserWidget;

namespace BeyondUICapture
{
	/** Saved/UICaptures */
	FString GetCaptureFolder();

	/**
	 * Paints Widget off screen at Size and writes it to Saved/UICaptures/<Name>.png (paint only runs with a renderer:
	 * run the capture tests without -nullrhi). False when there is no renderer or the file couldn't be written.
	 */
	bool CaptureWidget(UUserWidget* Widget, const FIntPoint& Size, const FString& Name);
}

#endif
