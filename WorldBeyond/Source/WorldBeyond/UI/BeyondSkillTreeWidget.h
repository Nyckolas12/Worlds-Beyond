// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Progression/BeyondSkillTree.h"
#include "Styling/SlateBrush.h"
#include "BeyondSkillTreeWidget.generated.h"

class ABeyondCharacterBase;
class UBeyondSkillTreeComponent;
class USoundBase;
class UTexture2D;

/**
 * The skill tree screen (K): one tab per demigod plus the duo tree. Drawn in C++ in the SkillTreeSystem pack's style
 * (its background, slot frame, lock and glow textures); the trees come from UBeyondSkillTreeAsset data and the rules
 * from UBeyondSkillTreeComponent.
 * Mouse: hover for details, hold the left button on a node to unlock it, right-click a duo power to put it on G.
 * Keys: Q / E (or arrows) switch trees, R twice resets the tree (refund), I goes to the equipment screen, K / Esc close.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondSkillTreeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBeyondSkillTreeWidget(const FObjectInitializer& ObjectInitializer);

	// Rebuilds the tabs from the party (call when opening)
	UFUNCTION(BlueprintCallable, Category = "Skill Tree")
	void RefreshTabs();

	UFUNCTION(BlueprintCallable, Category = "Skill Tree")
	void SelectTab(int32 Index);

	// The tab of a demigod's tree; 0 if it has none
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	int32 FindTabFor(const AActor* Character) const;

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	int32 GetTabCount() const { return Tabs.Num(); }

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	int32 GetActiveTab() const { return ActiveTab; }

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	UBeyondSkillTreeComponent* GetActiveTree() const;

	// Starts holding on a node, as pressing the mouse on it does; false (and the reason shown) if it can't be unlocked
	UFUNCTION(BlueprintCallable, Category = "Skill Tree")
	bool BeginHold(FName NodeId);

	UFUNCTION(BlueprintCallable, Category = "Skill Tree")
	void CancelHold();

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	bool IsHolding() const { return !HoldNode.IsNone(); }

	// Right-click on a duo-tree node: put its duo power on G
	UFUNCTION(BlueprintCallable, Category = "Skill Tree")
	bool EquipDuoPowerFrom(FName NodeId);

	// The last feedback line ("Requires level 8", "Storm Affinity rank 2")
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	FText GetMessage() const { return Message; }

	// Advance hold / flash / message timers (NativeTick calls it; tests call it directly)
	void AdvanceAnimation(float DeltaSeconds);

	// Seconds to hold a node to unlock it (the pack's hold-to-acquire)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree", meta = (ClampMin = "0"))
	float HoldDuration = 0.6f;

	//~ Art (SkillTreeSystem pack); anything missing falls back to plain shapes

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Art")
	TSoftObjectPtr<UTexture2D> BackgroundTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Art")
	TSoftObjectPtr<UTexture2D> SlotTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Art")
	TSoftObjectPtr<UTexture2D> LockTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Art")
	TSoftObjectPtr<UTexture2D> GlowTexture;

	// Icons for stat nodes without their own
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Art")
	TMap<EBeyondSkillStat, TSoftObjectPtr<UTexture2D>> StatIcons;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Art")
	TSoftObjectPtr<USoundBase> UnlockSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Art")
	TSoftObjectPtr<USoundBase> DeniedSound;

	//~ Layout and colours

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Style", meta = (ClampMin = "12"))
	float NodeRadius = 34.0f;

	// Pixels per grid cell (node Position)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Style")
	FVector2D CellSize = FVector2D(150.0f, 118.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Style")
	FLinearColor GoldColor = FLinearColor(1.0f, 0.72f, 0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Style")
	FLinearColor LockedColor = FLinearColor(0.32f, 0.33f, 0.38f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Style")
	FLinearColor PanelColor = FLinearColor(0.02f, 0.03f, 0.07f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Style")
	FLinearColor TextColor = FLinearColor(0.95f, 0.95f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Style")
	FLinearColor WarningColor = FLinearColor(1.0f, 0.42f, 0.35f, 1.0f);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	struct FTreeTab
	{
		FText Title;
		TWeakObjectPtr<UBeyondSkillTreeComponent> Tree;
		TWeakObjectPtr<ABeyondCharacterBase> Character;
	};

	void LoadArt();
	const FBeyondSkillNode* FindActiveNode(FName NodeId) const;
	FVector2f GetTreeScale(const FVector2f& LocalSize) const;
	FVector2f GetNodeCenter(const FBeyondSkillNode& Node, const FVector2f& LocalSize) const;
	FName HitTestNode(const FVector2f& Local, const FVector2f& LocalSize) const;
	int32 HitTestTab(const FVector2f& Local, const FVector2f& LocalSize) const;
	FVector2f GetTabCenter(int32 Index, const FVector2f& LocalSize) const;
	UTexture2D* GetNodeIcon(const FBeyondSkillNode& Node) const;
	const FSlateBrush* GetTextureBrush(UTexture2D* Texture) const;
	FLinearColor GetAccent() const;
	FLinearColor GetNodeColor(const UBeyondSkillTreeComponent& Tree, const FBeyondSkillNode& Node) const;
	bool IsDuoLoadout(const FBeyondSkillNode& Node) const;
	void ShowMessage(const FText& Text, bool bWarning = false);
	void PlayUISound(const TSoftObjectPtr<USoundBase>& Sound) const;
	void CycleTab(int32 Direction);
	void HandleResetKey();
	void Close();

	TArray<FTreeTab> Tabs;
	int32 ActiveTab = 0;

	FName HoveredNode;
	FName HoldNode;
	float HoldTime = 0.0f;
	FName FlashNode;
	float FlashAge = -1.0f;

	FText Message;
	bool bMessageWarning = false;
	float MessageAge = -1.0f;
	float ResetArmedAge = -1.0f;
	float AnimTime = 0.0f;

	// Size the last mouse event saw (hit tests use the same layout as painting)
	FVector2f LastLocalSize = FVector2f(1920.0f, 1080.0f);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> LoadedTextures;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Background;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> SlotFrame;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Lock;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Glow;

	// Rounded shapes (half-height corners) for discs, pills and panels without art
	FSlateBrush PillBrush;
	FSlateBrush PanelBrush;
	// Stable addresses: paint keeps brush pointers while it adds more
	mutable TMap<const UTexture2D*, TUniquePtr<FSlateBrush>> TextureBrushes;
};
