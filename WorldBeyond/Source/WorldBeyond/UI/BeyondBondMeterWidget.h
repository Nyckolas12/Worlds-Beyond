// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondBondMeterWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UWidget;

/**
 * The party's Bond meter and the duo super move's slot, built in C++ (no Widget Blueprint needed).
 *
 * Arc style (the UI materials from migrate_pass6.py): a curved steel meter above the ability bar, Angel's medallion
 * on its left end and Ji-Woong's on its right. The fill flows blue and purple on the left into solid gold on the right
 * and crackles; every Bond gain flashes its front. When it is full the Heaven's Judgment medallion pops in above the
 * middle with blue / purple / gold flames circling it and "READY [G]"; when the move is used it all fades away.
 * Without those materials it falls back to a plain [icon] [bar] row.
 *
 * Make a Blueprint child (or set the Bond Widget Class on BP_PC to another widget with SetBond / SetDuoIcon) to restyle.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondBondMeterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Bond")
	void SetBond(float Bond, float MaxBond);

	// The duo ability's Icon (empty: the arc style draws a lightning bolt, the simple style shows Default Icon)
	UFUNCTION(BlueprintCallable, Category = "Bond")
	void SetDuoIcon(UTexture2D* Icon);

	// The animated arc is up (its materials loaded); false: the simple fallback row
	UFUNCTION(BlueprintPure, Category = "Bond")
	bool IsUsingArcStyle() const { return bArcStyle; }

	// 0 charging .. 1 the duo medallion and its flames fully shown
	UFUNCTION(BlueprintPure, Category = "Bond")
	float GetReadyBlend() const { return ReadyBlend; }

	// The fill the arc shows right now (it eases toward the real value)
	UFUNCTION(BlueprintPure, Category = "Bond")
	float GetDisplayedFill() const { return DisplayedFill; }

	// Runs the arc animation forward (NativeTick does this every frame; tests call it to skip ahead)
	void AdvanceAnimation(float DeltaTime);

	UImage* GetDuoImage() const { return DuoImage; }
	UImage* GetFlameImage() const { return FlameImage; }

	// ---------------------------------------------------------------- arc style

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc")
	TSoftObjectPtr<UMaterialInterface> ArcMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/UI/DuoMeter/M_UI_BondArc.M_UI_BondArc")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc")
	TSoftObjectPtr<UMaterialInterface> FlameMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/UI/DuoMeter/M_UI_DuoFlames.M_UI_DuoFlames")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc")
	TSoftObjectPtr<UMaterialInterface> MedallionMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/UI/DuoMeter/M_UI_DuoMedallion.M_UI_DuoMedallion")));

	// Angel's side
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Colors")
	FLinearColor ColorBlue = FLinearColor(0.10f, 0.45f, 1.0f);

	// Mixed in with the blue on the left
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Colors")
	FLinearColor ColorPurple = FLinearColor(0.55f, 0.15f, 1.0f);

	// Ji-Woong's side: solid on the right
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Colors")
	FLinearColor ColorGold = FLinearColor(1.0f, 0.72f, 0.12f);

	// How far along the arc (0..1) the purple reaches, and where the gold is solid from
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Colors", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float PurpleStop = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Colors", meta = (ClampMin = "0.05", ClampMax = "1"))
	float GoldStart = 0.62f;

	// Speed of the energy flowing along the arc
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Colors", meta = (ClampMin = "0"))
	float FlowSpeed = 0.6f;

	// Angel's medallion: his staff glyph (the ability bar's icon set) over a turning rune ring
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Medallions")
	TSoftObjectPtr<UTexture2D> AngelGlyph = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/Blueprints/Widgets/Images/crescent-staff.crescent-staff")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Medallions")
	TSoftObjectPtr<UTexture2D> RuneTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/FXVarietyPack/Textures/T_ky_magicCircle020.T_ky_magicCircle020")));

	// Ji-Woong's medallion (a drawn sun) and the duo medallion (the duo icon, or a drawn lightning bolt)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Medallions")
	FLinearColor JiWoongTint = FLinearColor(0.85f, 0.52f, 0.06f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Medallions")
	FLinearColor DuoTint = FLinearColor(1.0f, 0.68f, 0.12f);

	// Show the duo medallion (greyed) while the meter charges; off: it appears when the meter is full
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Medallions")
	bool bShowDuoIconWhileCharging = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc")
	FText ArcReadyText = NSLOCTEXT("Beyond", "BondArcReady", "READY");

	// Arc image size (px); the meter's own size adds the medallions around it
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Layout")
	FVector2D ArcSize = FVector2D(640.0f, 160.0f);

	// Bigger: flatter arc
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Layout", meta = (ClampMin = "100"))
	float ArcRadius = 600.0f;

	// Height of the arc's middle line at its top, from the top of the arc image
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Layout")
	float ArcApexY = 40.0f;

	// The arc's ends (where the medallions sit) this far in from the image's sides
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Layout")
	float ArcEndInset = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Layout", meta = (ClampMin = "8"))
	float BandThickness = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Layout", meta = (ClampMin = "0"))
	float FrameWidth = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Layout", meta = (ClampMin = "16"))
	float EmblemSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Layout", meta = (ClampMin = "16"))
	float DuoIconSize = 112.0f;

	// The flame ring around the duo medallion
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Layout", meta = (ClampMin = "16"))
	float FlameSize = 260.0f;

	// Height of the duo medallion's centre above the arc's top
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Layout")
	float DuoIconLift = 110.0f;

	// How fast the shown fill catches up with the real one
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Animation", meta = (ClampMin = "0.1"))
	float FillEaseSpeed = 5.0f;

	// Seconds for the duo medallion and flames to appear / disappear
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Arc|Animation", meta = (ClampMin = "0.05"))
	float ReadyBlendTime = 0.35f;

	// ---------------------------------------------------------------- simple style (fallback)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FLinearColor EmptyColor = FLinearColor(0.10f, 0.35f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FLinearColor HalfColor = FLinearColor(0.55f, 0.15f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FLinearColor FullColor = FLinearColor(1.0f, 0.78f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FText ChargingText = NSLOCTEXT("Beyond", "BondCharging", "BOND");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FText ReadyText = NSLOCTEXT("Beyond", "BondReady", "HEAVEN'S JUDGMENT READY");

	// Key shown on the duo icon
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FText KeyText = NSLOCTEXT("Beyond", "BondKey", "G");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FVector2D Size = FVector2D(380.0f, 22.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	float IconSize = 56.0f;

	// Shown when the duo ability has no Icon yet
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	TSoftObjectPtr<UTexture2D> DefaultIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/Blueprints/Widgets/Images/frameBackground.frameBackground")));

	// Icon tint while the meter is charging
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bond|Style")
	FLinearColor ChargingIconTint = FLinearColor(0.35f, 0.35f, 0.35f, 0.65f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Called after SetBond, e.g. to play a "ready" animation in a Blueprint child
	UFUNCTION(BlueprintImplementableEvent, Category = "Bond")
	void OnBondUpdated(float FillPercent, bool bIsFull);

private:
	bool BuildArcLayout();
	void BuildSimpleLayout();
	UImage* AddMaterialImage(UCanvasPanel* Canvas, FName Name, UMaterialInterface* Material, const FVector2D& Centre,
		float Width, float Height, TObjectPtr<UMaterialInstanceDynamic>& OutMaterial);
	void ApplyToWidgets();
	void ApplyDuoGlyph();

	// Simple style
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;

	UPROPERTY(Transient)
	TObjectPtr<UImage> IconImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> KeyLabel;

	// Arc style
	UPROPERTY(Transient)
	TObjectPtr<UImage> DuoImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> FlameImage;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> ReadyLabel;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ArcMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AngelMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> JiWoongMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DuoMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlameMID;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DuoIcon;

	float Percent = 0.0f;
	bool bFull = false;
	bool bArcStyle = false;

	float DisplayedFill = 0.0f;
	float Surge = 0.0f;
	float ReadyBlend = 0.0f;
	float AnimTime = 0.0f;

	// Arc geometry in arc-image pixels (centre x, centre y, radius) and its half opening angle, as sent to the material
	FVector ArcShape = FVector::ZeroVector;
	float ArcHalfAngle = 0.3f;
};
