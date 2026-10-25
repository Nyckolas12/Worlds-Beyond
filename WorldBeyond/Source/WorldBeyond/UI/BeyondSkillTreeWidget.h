// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Progression/BeyondSkillTree.h"
#include "UI/BeyondMenuWidget.h"
#include "BeyondSkillTreeWidget.generated.h"

class ABeyondCharacterBase;
class UBeyondSkillTreeComponent;
class USoundBase;
class UTexture2D;

/**
 * The character menu's Skill Tree page (K): one tree per demigod plus the duo tree, one per header tab. The tree sits on
 * a rune circle beside the demigod's art; the right column shows level and combat stats and the selected node's card
 * (ranks, what each rank gives, cost, the UPGRADE button). Trees come from UBeyondSkillTreeAsset data, the rules from
 * UBeyondSkillTreeComponent.
 * Mouse: click a node to select it, hold the left button (or F) to unlock a rank, right-click a duo power to put it on G.
 * Keys: R twice resets the tree (refund); the frame's keys switch tab and page.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondSkillTreeWidget : public UBeyondMenuWidget
{
	GENERATED_BODY()

public:
	UBeyondSkillTreeWidget(const FObjectInitializer& ObjectInitializer);

	virtual EBeyondMenuPage GetPage() const override { return EBeyondMenuPage::SkillTree; }

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

	// The node the card shows: the one under the mouse, else the selected one, else the first that can be bought
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	FName GetDetailNode() const;

	UFUNCTION(BlueprintCallable, Category = "Skill Tree")
	void SelectNode(FName NodeId);

	// Where a node sits on the 1920 x 1080 page
	FVector2f GetNodeDesignCenter(FName NodeId) const;

	// Seconds to hold a node to unlock it (the pack's hold-to-acquire)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree", meta = (ClampMin = "0"))
	float HoldDuration = 0.6f;

	// Icons for stat nodes without their own
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Art")
	TMap<EBeyondSkillStat, TSoftObjectPtr<UTexture2D>> StatIcons;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Art")
	TSoftObjectPtr<USoundBase> UnlockSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Style", meta = (ClampMin = "12"))
	float NodeRadius = 34.0f;

protected:
	virtual void NativeConstruct() override;
	virtual void PaintPage(BeyondUI::FPainter& Paint) const override;
	virtual int32 GetPageTab() const override { return ActiveTab; }
	virtual void OnHeaderTabSelected(int32 Index) override { SelectTab(Index); }
	virtual void GetPageKeyHints(TArray<FBeyondKeyHint>& OutHints) const override;
	virtual bool GetCounter(FBeyondMenuCounter& OutCounter) const override;
	virtual bool HandlePageKey(const FKey& Key) override;
	virtual bool HandlePageKeyUp(const FKey& Key) override;
	virtual bool HandlePageClick(const FVector2f& Design, const FKey& Button) override;
	virtual bool HandlePageMouseUp(const FVector2f& Design, const FKey& Button) override;
	virtual void HandlePageHover(const FVector2f& Design) override;
	virtual void AdvancePage(float DeltaSeconds) override;
	virtual void CloseMenu() override;

private:
	struct FTreeTab
	{
		FText Title;
		TWeakObjectPtr<UBeyondSkillTreeComponent> Tree;
		TWeakObjectPtr<ABeyondCharacterBase> Character;
	};

	void LoadArt();
	const FBeyondSkillNode* FindActiveNode(FName NodeId) const;
	FName HitTestNode(const FVector2f& Design) const;
	UTexture2D* GetNodeIcon(const FBeyondSkillNode& Node) const;
	bool IsDuoLoadout(const FBeyondSkillNode& Node) const;
	void HandleResetKey();
	void PaintNode(const BeyondUI::FPainter& Paint, const UBeyondSkillTreeComponent& Tree, const FBeyondSkillNode& Node) const;
	void PaintSidePanels(const BeyondUI::FPainter& Paint, const UBeyondSkillTreeComponent& Tree) const;
	void PaintNodeCard(const BeyondUI::FPainter& Paint, const UBeyondSkillTreeComponent& Tree, const FBeyondSkillNode& Node, const FVector2f& TopLeft,
		const FVector2f& Size) const;

	TArray<FTreeTab> Tabs;
	int32 ActiveTab = 0;

	FName HoveredNode;
	FName SelectedNode;
	FName HoldNode;
	float HoldTime = 0.0f;
	FName FlashNode;
	float FlashAge = -1.0f;
	float ResetArmedAge = -1.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> LoadedTextures;
};
