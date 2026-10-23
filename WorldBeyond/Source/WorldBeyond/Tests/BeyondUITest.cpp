// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/PointLight.h"
#include "Editor.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Progression/BeyondSkillTreeComponent.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/BeyondUICapture.h"
#include "UI/BeyondInventoryWidget.h"
#include "UI/BeyondMenuWidget.h"
#include "UI/BeyondOverviewWidget.h"
#include "UI/BeyondPauseMenuWidget.h"
#include "UI/BeyondQuestLogWidget.h"
#include "UI/BeyondSkillTreeWidget.h"

/**
 * Plan 6, the character menu in MAP_Demo_Main (Play In Editor, works under -nullrhi): every page opens paused through the
 * player controller and its key, with Angel / Ji-Woong / Duo tabs (the quest log's are filters); the picked demigod
 * carries from page to page by key and sidebar; the Armor page's Duo tab is the party view while it keeps two demigod
 * tabs; the Overview's numbers are the attributes; key hints end with Esc; header and sidebar hit tests; the pause
 * menu resumes and asks twice before quitting. WorldsBeyond.UICapture.Menus paints the pages to PNGs (needs a renderer).
 */
namespace BeyondUITest
{
	struct FState
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ABeyondPlayerController> PC;
		TWeakObjectPtr<ABeyondCharacterBase> Angel;
		TWeakObjectPtr<ABeyondCharacterBase> JiWoong;
		FString SavedSaveProgress;
		bool bCapture = false;
	};

	UWorld* GetPlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	UBeyondMenuWidget* GetMenu(const ABeyondPlayerController* PC, EBeyondMenuPage Page)
	{
		return PC ? Cast<UBeyondMenuWidget>(PC->GetMenuWidget(Page)) : nullptr;
	}

	FString PageName(EBeyondMenuPage Page)
	{
		return StaticEnum<EBeyondMenuPage>()->GetNameStringByValue(static_cast<int64>(Page));
	}

	bool Setup(const TSharedRef<FState>& State)
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* World = GetPlayWorld();
		ABeyondPlayerController* PC = World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!T.TestNotNull(TEXT("Beyond player controller"), PC))
		{
			return false;
		}
		State->PC = PC;
		for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
		{
			if (ULevelSequencePlayer* Player = It->GetSequencePlayer(); Player && Player->IsPlaying())
			{
				Player->GoToEndAndStop();
			}
		}
		for (ABeyondCharacterBase* Member : PC->PartyComponent->GetMembers())
		{
			if (Member->DuoRole == EBeyondDuoRole::Conduit) { State->Angel = Member; }
			if (Member->DuoRole == EBeyondDuoRole::Striker) { State->JiWoong = Member; }
		}
		if (!T.TestNotNull(TEXT("Angel"), State->Angel.Get()) || !T.TestNotNull(TEXT("Ji-Woong"), State->JiWoong.Get()))
		{
			return false;
		}
		if (PC->PartyComponent->GetLeader() != State->Angel.Get())
		{
			PC->PartyComponent->SwapLeader();
		}
		return true;
	}

	void Teardown(FAutomationTestBase& Test, const TSharedRef<FState>& State);
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondUIStep, TFunction<bool()>, Step);
bool FBeyondUIStep::Update()
{
	return Step();
}

void BeyondUITest::Teardown(FAutomationTestBase& Test, const TSharedRef<FState>& State)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondUIStep([State]()
	{
		// Only once PIE is gone: the party saves when play ends, and that save must still see saving switched off
		if (GEditor && GEditor->PlayWorld)
		{
			return false;
		}
		if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
		{
			SaveProgress->Set(State->SavedSaveProgress.IsEmpty() ? TEXT("1") : *State->SavedSaveProgress, ECVF_SetByCode);
		}
		return true;
	}));
}

namespace BeyondUITest
{
	void Start(FAutomationTestBase* Test, const TSharedRef<FState>& State)
	{
		State->Test = Test;
		if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
		{
			State->SavedSaveProgress = SaveProgress->GetString();
			SaveProgress->Set(TEXT("0"), ECVF_SetByCode);
		}
		ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/SICKA_PERSEPOLIS/MAPS/MAP_Demo_Main")));
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.0f));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondUITest, "WorldsBeyond.Prototype.UI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondUITest::RunTest(const FString& Parameters)
{
	using namespace BeyondUITest;
	TSharedRef<FState> State = MakeShared<FState>();
	Start(this, State);

	ADD_LATENT_AUTOMATION_COMMAND(FBeyondUIStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		if (!Setup(State))
		{
			return true;
		}
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();

		//~ Every page opens paused, one at a time, with its tabs
		const EBeyondMenuPage Pages[] = { EBeyondMenuPage::Overview, EBeyondMenuPage::SkillTree, EBeyondMenuPage::Armor, EBeyondMenuPage::Quests, EBeyondMenuPage::Pause };
		for (const EBeyondMenuPage Page : Pages)
		{
			const FString Name = PageName(Page);
			UBeyondMenuWidget* Menu = Cast<UBeyondMenuWidget>(PC->OpenMenuPage(Page));
			if (!T.TestNotNull(Name + TEXT(" opens"), Menu))
			{
				continue;
			}
			T.TestTrue(Name + TEXT(" is open"), PC->IsMenuPageOpen(Page) && PC->IsAnyMenuOpen());
			T.TestTrue(Name + TEXT(" pauses"), PC->IsPaused());
			T.TestEqual(Name + TEXT(" is the page it says"), Menu->GetPage(), Page);
			for (const EBeyondMenuPage Other : Pages)
			{
				if (Other != Page)
				{
					T.TestFalse(Name + TEXT(": only one menu at a time"), PC->IsMenuPageOpen(Other));
				}
			}
			const TArray<FBeyondKeyHint> Hints = Menu->GetKeyHints();
			T.TestTrue(Name + TEXT(": key hints end with Esc"), Hints.Num() > 0 && Hints.Last().Key == EKeys::Escape);
			if (Page == EBeyondMenuPage::Pause)
			{
				T.TestEqual(TEXT("Pause: no header tabs"), Menu->GetHeaderTabCount(), 0);
				continue;
			}
			T.TestEqual(Name + TEXT(": three header tabs"), Menu->GetHeaderTabCount(), 3);
			if (Page != EBeyondMenuPage::Quests)
			{
				T.TestEqual(Name + TEXT(": tabs are Angel, Ji-Woong, Duo"), Menu->GetHeaderTabTitle(0).ToString() + TEXT("|") + Menu->GetHeaderTabTitle(1).ToString()
					+ TEXT("|") + Menu->GetHeaderTabTitle(2).ToString(), Angel->GetCharacterDisplayName().ToString() + TEXT("|") + JiWoong->GetCharacterDisplayName().ToString()
					+ TEXT("|Duo"));
				T.TestEqual(Name + TEXT(": opens on the leader"), Menu->GetActiveHeaderTab(), 0);
			}
		}

		//~ The picked demigod carries from page to page (keys and sidebar)
		UBeyondMenuWidget* Overview = Cast<UBeyondMenuWidget>(PC->OpenMenuPage(EBeyondMenuPage::Overview));
		Overview->SelectHeaderTab(1);
		T.TestEqual(TEXT("Overview shows Ji-Woong"), Overview->GetActiveHeaderTab(), 1);
		Overview->PressKey(EKeys::K);
		UBeyondSkillTreeWidget* Tree = Cast<UBeyondSkillTreeWidget>(PC->GetMenuWidget(EBeyondMenuPage::SkillTree));
		T.TestTrue(TEXT("K went to the skill tree"), PC->IsSkillTreeOpen() && !PC->IsMenuPageOpen(EBeyondMenuPage::Overview));
		T.TestTrue(TEXT("...on Ji-Woong's tree"), Tree && Tree->GetActiveTree() == JiWoong->GetSkillTreeComponent());
		Tree->PressKey(EKeys::I);
		UBeyondInventoryWidget* Armor = Cast<UBeyondInventoryWidget>(PC->GetInventoryWidget());
		T.TestTrue(TEXT("I went to the Armor page showing Ji-Woong"), PC->IsInventoryOpen() && Armor && Armor->GetShownCharacter() == JiWoong);
		Armor->SelectHeaderTab(2);
		T.TestTrue(TEXT("Armor's Duo tab is the party view"), Armor->IsPartyView() && Armor->GetActiveHeaderTab() == 2);
		T.TestEqual(TEXT("...and the page still has two demigod tabs"), Armor->GetTabCount(), 2);
		Armor->PressKey(EKeys::J);
		UBeyondMenuWidget* Quests = GetMenu(PC, EBeyondMenuPage::Quests);
		T.TestTrue(TEXT("J went to the quest log"), PC->IsMenuPageOpen(EBeyondMenuPage::Quests) && Quests != nullptr);
		T.TestEqual(TEXT("The quest log keeps its own filter"), Quests ? Quests->GetActiveHeaderTab() : -1, 0);
		Quests->PressKey(EKeys::C);
		Overview = GetMenu(PC, EBeyondMenuPage::Overview);
		T.TestEqual(TEXT("C back to the Overview on the Duo (the last character tab)"), Overview ? Overview->GetActiveHeaderTab() : -1, 2);
		Overview->PressKey(EKeys::One);
		T.TestEqual(TEXT("1 picks Angel"), Overview->GetActiveHeaderTab(), 0);
		Overview->PressKey(EKeys::E);
		T.TestEqual(TEXT("E moves to the next tab"), Overview->GetActiveHeaderTab(), 1);

		//~ Hit tests on the 1920 x 1080 page
		const int32 SidebarArmor = UBeyondMenuWidget::GetSidebarPages().IndexOfByKey(EBeyondMenuPage::Armor);
		T.TestEqual(TEXT("Sidebar: Armor's entry"), Overview->HitTestSidebar(FVector2f(BeyondMenuLayout::SidebarLeft + 100.0f,
			BeyondMenuLayout::SidebarTop + SidebarArmor * (BeyondMenuLayout::SidebarEntryHeight + BeyondMenuLayout::SidebarGap) + 30.0f)), SidebarArmor);
		T.TestEqual(TEXT("Header: the middle tab"), Overview->HitTestHeaderTab(FVector2f(960.0f, BeyondMenuLayout::HeaderTabTop + 25.0f)), 1);
		T.TestEqual(TEXT("Header: nothing below the tabs"), Overview->HitTestHeaderTab(FVector2f(960.0f, 400.0f)), INDEX_NONE);

		//~ The Overview's numbers are the attributes
		Overview->SelectHeaderTab(0);
		const FBeyondOverviewModel Model = Cast<UBeyondOverviewWidget>(Overview)->BuildModel();
		const UAbilitySystemComponent* ASC = Angel->GetAbilitySystemComponent();
		T.TestEqual(TEXT("Overview: three party cards"), Model.Party.Num(), 3);
		T.TestEqual(TEXT("Overview: Angel's name"), Model.Name.ToString(), Angel->GetCharacterDisplayName().ToString());
		T.TestEqual(TEXT("Overview: max health"), Model.MaxHealth, ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute()), 0.01f);
		T.TestEqual(TEXT("Overview: level"), Model.Level.Level, Angel->GetCharacterLevel());
		T.TestTrue(TEXT("Overview: class title from the profile"), !Model.ClassTitle.IsEmpty());
		T.TestEqual(TEXT("Overview: five loadout slots"), Model.Loadout.Num(), 5);
		T.TestTrue(TEXT("Overview: stats listed"), Model.Stats.Num() >= 5);

		//~ The pause menu: resume, and quit asks twice
		UBeyondPauseMenuWidget* Pause = Cast<UBeyondPauseMenuWidget>(PC->OpenMenuPage(EBeyondMenuPage::Pause));
		if (T.TestNotNull(TEXT("Pause menu"), Pause))
		{
			Pause->ActivateEntry(EBeyondPauseEntry::Quit);
			T.TestTrue(TEXT("Quit once: still paused, asks again"), PC->IsMenuPageOpen(EBeyondMenuPage::Pause) && !Pause->GetMessage().IsEmpty());
			Pause->ActivateEntry(EBeyondPauseEntry::Resume);
			T.TestFalse(TEXT("Resume closes the pause menu"), PC->IsMenuPageOpen(EBeyondMenuPage::Pause));
			T.TestFalse(TEXT("...and unpauses"), PC->IsPaused());
		}
		T.TestFalse(TEXT("No menu left open"), PC->IsAnyMenuOpen());
		return true;
	}));

	Teardown(*this, State);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondUICaptureMenusTest, "WorldsBeyond.UICapture.Menus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondUICaptureMenusTest::RunTest(const FString& Parameters)
{
	using namespace BeyondUITest;
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("No renderer (-nullrhi): nothing captured"));
		return true;
	}
	TSharedRef<FState> State = MakeShared<FState>();
	Start(this, State);

	ADD_LATENT_AUTOMATION_COMMAND(FBeyondUIStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		if (!Setup(State))
		{
			return true;
		}
		ABeyondPlayerController* PC = State->PC.Get();

		// Fresh copies of each page, painted off screen (the live menus stay where they are)
		struct FShot { EBeyondMenuPage Page; TSubclassOf<UUserWidget> Class; int32 Tab; const TCHAR* Name; };
		const FShot Shots[] = {
			{ EBeyondMenuPage::SkillTree, PC->SkillTreeWidgetClass, 0, TEXT("SkillTree_Angel") },
			{ EBeyondMenuPage::SkillTree, PC->SkillTreeWidgetClass, 1, TEXT("SkillTree_JiWoong") },
			{ EBeyondMenuPage::SkillTree, PC->SkillTreeWidgetClass, 2, TEXT("SkillTree_Duo") },
			{ EBeyondMenuPage::Armor, PC->InventoryWidgetClass, 0, TEXT("Armor_Angel") },
			{ EBeyondMenuPage::Armor, PC->InventoryWidgetClass, 1, TEXT("Armor_JiWoong") },
			{ EBeyondMenuPage::Armor, PC->InventoryWidgetClass, 2, TEXT("Armor_Duo") },
			{ EBeyondMenuPage::Overview, PC->OverviewWidgetClass, 0, TEXT("Overview_Angel") },
			{ EBeyondMenuPage::Overview, PC->OverviewWidgetClass, 1, TEXT("Overview_JiWoong") },
			{ EBeyondMenuPage::Overview, PC->OverviewWidgetClass, 2, TEXT("Overview_Duo") },
			{ EBeyondMenuPage::Quests, PC->QuestLogWidgetClass, 0, TEXT("Quests") },
			{ EBeyondMenuPage::Pause, PC->PauseMenuWidgetClass, 0, TEXT("Pause") },
		};
		for (const FShot& Shot : Shots)
		{
			UBeyondMenuWidget* Menu = Shot.Class ? CreateWidget<UBeyondMenuWidget>(PC, Shot.Class) : nullptr;
			if (!T.TestNotNull(FString(Shot.Name) + TEXT(" widget"), Menu))
			{
				continue;
			}
			Menu->TakeWidget();
			Menu->NotifyOpened();
			Menu->SelectHeaderTab(Shot.Tab);
			Menu->AdvanceAnimation(1.0f);
			T.TestTrue(FString(Shot.Name) + TEXT(" captured"), BeyondUICapture::CaptureWidget(Menu, FIntPoint(1920, 1080), FString(TEXT("Menu_")) + Shot.Name));
		}
		// One page at other screen shapes
		if (UBeyondMenuWidget* Wide = CreateWidget<UBeyondMenuWidget>(PC, PC->OverviewWidgetClass))
		{
			Wide->TakeWidget();
			Wide->NotifyOpened();
			Wide->AdvanceAnimation(1.0f);
			T.TestTrue(TEXT("Overview at 2560 x 1080"), BeyondUICapture::CaptureWidget(Wide, FIntPoint(2560, 1080), TEXT("Menu_Overview_2560x1080")));
			T.TestTrue(TEXT("Overview at 1280 x 720"), BeyondUICapture::CaptureWidget(Wide, FIntPoint(1280, 720), TEXT("Menu_Overview_1280x720")));
		}
		return true;
	}));

	Teardown(*this, State);
	return true;
}

namespace BeyondUITest
{
	// A shot of one demigod alone (everything else hidden) for the painted portraits
	struct FPortraitShot
	{
		const TCHAR* Name;
		// Camera offset from the character: forward, right, up (cm), and the point it looks at above the actor's origin
		FVector Offset;
		float LookHeight;
		float FOV;
		FIntPoint Size;
	};

	const FPortraitShot PortraitShots[] = {
		{ TEXT("Card"), FVector(175.0f, 25.0f, 62.0f), 50.0f, 32.0f, FIntPoint(768, 1024) },
		{ TEXT("Medallion"), FVector(125.0f, 12.0f, 70.0f), 64.0f, 28.0f, FIntPoint(768, 768) },
		{ TEXT("FullFront"), FVector(300.0f, 0.0f, 10.0f), -4.0f, 34.0f, FIntPoint(1024, 1536) },
		{ TEXT("FullBack"), FVector(-290.0f, 55.0f, 30.0f), 0.0f, 34.0f, FIntPoint(1024, 1536) },
	};

	// A studio rig around the shot so the map's sun doesn't decide the look: warm key, cool fill, blue rim from behind
	TArray<AActor*> SpawnPortraitLights(UWorld* World, const FVector& Camera, const FVector& Subject)
	{
		const FVector ToCamera = (Camera - Subject).GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, ToCamera);
		struct FLight { FVector Offset; FLinearColor Color; float Intensity; };
		const FLight Lights[] = {
			{ ToCamera * 180.0f + Side * 140.0f + FVector(0.0f, 0.0f, 120.0f), FLinearColor(1.0f, 0.92f, 0.82f), 60.0f },
			{ ToCamera * 200.0f - Side * 180.0f + FVector(0.0f, 0.0f, 40.0f), FLinearColor(0.55f, 0.65f, 1.0f), 18.0f },
			{ -ToCamera * 150.0f + FVector(0.0f, 0.0f, 170.0f), FLinearColor(0.45f, 0.6f, 1.0f), 70.0f },
		};
		TArray<AActor*> Spawned;
		for (const FLight& Light : Lights)
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags = RF_Transient;
			if (APointLight* Point = World->SpawnActor<APointLight>(Subject + Light.Offset, FRotator::ZeroRotator, Params))
			{
				UPointLightComponent* Component = Cast<UPointLightComponent>(Point->GetLightComponent());
				Component->SetMobility(EComponentMobility::Movable);
				Component->SetIntensityUnits(ELightUnits::Candelas);
				Component->SetIntensity(Light.Intensity);
				Component->SetLightColor(Light.Color);
				Component->SetAttenuationRadius(900.0f);
				Component->SetCastShadows(false);
				Spawned.Add(Point);
			}
		}
		return Spawned;
	}

	bool CapturePortrait(UWorld* World, ABeyondCharacterBase* Character, const FPortraitShot& Shot, const FString& Name)
	{
		const FVector Origin = Character->GetActorLocation();
		const FVector Forward = Character->GetActorForwardVector();
		const FVector Right = Character->GetActorRightVector();
		const FVector Location = Origin + Forward * Shot.Offset.X + Right * Shot.Offset.Y + FVector(0.0f, 0.0f, Shot.Offset.Z);
		const FRotator Rotation = (Origin + FVector(0.0f, 0.0f, Shot.LookHeight) - Location).Rotation();

		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
		Target->RenderTargetFormat = RTF_RGBA8_SRGB;
		Target->ClearColor = FLinearColor::Black;
		Target->InitAutoFormat(Shot.Size.X, Shot.Size.Y);
		Target->UpdateResourceImmediate(true);

		FActorSpawnParameters Params;
		Params.ObjectFlags = RF_Transient;
		ASceneCapture2D* Camera = World->SpawnActor<ASceneCapture2D>(Location, Rotation, Params);
		if (!Camera)
		{
			return false;
		}
		USceneCaptureComponent2D* Capture = Camera->GetCaptureComponent2D();
		Capture->TextureTarget = Target;
		Capture->FOVAngle = Shot.FOV;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Capture->ShowOnlyActors.Add(Character);
		Capture->ShowFlags.SetTemporalAA(false);
		Capture->ShowFlags.SetAtmosphere(false);
		Capture->ShowFlags.SetFog(false);
		FPostProcessSettings& Post = Capture->PostProcessSettings;
		Post.bOverride_AutoExposureMethod = true;
		Post.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		Post.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Post.AutoExposureApplyPhysicalCameraExposure = false;
		Post.bOverride_AutoExposureBias = true;
		Post.AutoExposureBias = 1.0f;
		Post.bOverride_MotionBlurAmount = true;
		Post.MotionBlurAmount = 0.0f;
		const TArray<AActor*> Lights = SpawnPortraitLights(World, Location, Origin + FVector(0.0f, 0.0f, Shot.LookHeight));
		Capture->CaptureScene();
		FlushRenderingCommands();
		for (AActor* Light : Lights)
		{
			Light->Destroy();
		}

		TArray<FColor> Pixels;
		bool bWritten = false;
		if (FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource(); Resource && Resource->ReadPixels(Pixels))
		{
			for (FColor& Pixel : Pixels)
			{
				Pixel.A = 255;
			}
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(Shot.Size.X, Shot.Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
			const FString Folder = BeyondUICapture::GetCaptureFolder() / TEXT("Portraits");
			IFileManager::Get().MakeDirectory(*Folder, true);
			bWritten = FFileHelper::SaveArrayToFile(Png, *(Folder / (Name + TEXT(".png"))));
		}
		Camera->Destroy();
		return bWritten;
	}
}

/**
 * Plan 6: renders Angel and Ji-Woong alone (card, medallion, full front, full back) into Saved/UICaptures/Portraits as
 * references for the painted portraits. Needs a renderer: run without -nullrhi (e.g. -RenderOffscreen).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondUICapturePortraitsTest, "WorldsBeyond.UICapture.Portraits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondUICapturePortraitsTest::RunTest(const FString& Parameters)
{
	using namespace BeyondUITest;
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("No renderer (-nullrhi): nothing captured"));
		return true;
	}
	TSharedRef<FState> State = MakeShared<FState>();
	Start(this, State);
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondUIStep([State]()
	{
		Setup(State);
		// Let the MetaHumans' textures stream in before the shots
		for (ABeyondCharacterBase* Character : { State->Angel.Get(), State->JiWoong.Get() })
		{
			if (Character)
			{
				Character->PrestreamTextures(10.0f, true);
			}
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondUIStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* World = GetPlayWorld();
		const TPair<const TCHAR*, ABeyondCharacterBase*> Who[] = { { TEXT("Angel"), State->Angel.Get() }, { TEXT("JiWoong"), State->JiWoong.Get() } };
		for (const TPair<const TCHAR*, ABeyondCharacterBase*>& Pair : Who)
		{
			if (!World || !Pair.Value)
			{
				continue;
			}
			for (const FPortraitShot& Shot : PortraitShots)
			{
				const FString Name = FString::Printf(TEXT("%s_%s"), Pair.Key, Shot.Name);
				T.TestTrue(Name + TEXT(" captured"), CapturePortrait(World, Pair.Value, Shot, Name));
			}
		}
		return true;
	}));
	Teardown(*this, State);
	return true;
}

#endif
