// Fill out your copyright notice in the Description page of Project Settings.

#include "Tests/BeyondUICapture.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UI/BeyondUIStyle.h"
#include "UI/BeyondUISwatchWidget.h"

FString BeyondUICapture::GetCaptureFolder()
{
	return FPaths::ProjectSavedDir() / TEXT("UICaptures");
}

bool BeyondUICapture::CaptureWidget(UUserWidget* Widget, const FIntPoint& Size, const FString& Name)
{
	if (!Widget || !FApp::CanEverRender() || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		return false;
	}
	const FVector2D DrawSize(Size.X, Size.Y);
	UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false);
	if (!Target)
	{
		return false;
	}
	Target->AddToRoot();
	{
		FWidgetRenderer Renderer(false, true);
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		// Twice: the first pass lays the widget out and caches fonts
		Renderer.DrawWidget(Target, Slate, DrawSize, 0.016f);
		Renderer.DrawWidget(Target, Slate, DrawSize, 0.016f);
	}
	FlushRenderingCommands();

	TArray<FColor> Pixels;
	bool bWritten = false;
	if (FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource(); Resource && Resource->ReadPixels(Pixels) && Pixels.Num() == Size.X * Size.Y)
	{
		for (FColor& Pixel : Pixels)
		{
			Pixel.A = 255;
		}
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		IFileManager::Get().MakeDirectory(*GetCaptureFolder(), true);
		bWritten = FFileHelper::SaveArrayToFile(Png, *(GetCaptureFolder() / (Name + TEXT(".png"))));
	}
	Target->RemoveFromRoot();
	return bWritten;
}

/**
 * Plan 6: paints the UI swatch (every piece of the shared look) at 1920 x 1080 and 1280 x 720 into Saved/UICaptures.
 * Needs a renderer: run it without -nullrhi (e.g. -RenderOffscreen); skipped otherwise.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondUICaptureSwatchTest, "WorldsBeyond.UICapture.Swatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondUICaptureSwatchTest::RunTest(const FString& Parameters)
{
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("No renderer (-nullrhi): nothing captured"));
		return true;
	}
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor world"), World))
	{
		return false;
	}
	if (UBeyondUISubsystem* UI = UBeyondUISubsystem::Get())
	{
		UI->Reload();
	}
	UBeyondUISwatchWidget* Swatch = CreateWidget<UBeyondUISwatchWidget>(World, UBeyondUISwatchWidget::StaticClass());
	if (!TestNotNull(TEXT("Swatch widget"), Swatch))
	{
		return false;
	}
	TestTrue(TEXT("Swatch at 1920 x 1080"), BeyondUICapture::CaptureWidget(Swatch, FIntPoint(1920, 1080), TEXT("Swatch_1920x1080")));
	TestTrue(TEXT("Swatch at 1280 x 720"), BeyondUICapture::CaptureWidget(Swatch, FIntPoint(1280, 720), TEXT("Swatch_1280x720")));
	return true;
}

#endif
