// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class ABeyondPlayerController;

namespace BeyondUIDemo
{
	/**
	 * A mid-game party for screenshots of the menus (Plan 6; console: Beyond.UI.DemoState): both demigods at level 24
	 * with skills learned, their armor sets, weapons, rings and charms upgraded, a mixed bag (some locked, some junk), a
	 * full wallet, Bond level 3, the first two chapters done and the third tracked, some places found. It overwrites the
	 * party's progress: set Beyond.SaveProgress 0 first unless the save may change.
	 */
	WORLDBEYOND_API bool ApplyDemoState(ABeyondPlayerController* PC);
}
