// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CraftingPanelWidget.generated.h"

class UCraftingComponent;
class UInventoryComponent;
class UCraftingRecipeWidget;
class UScrollBox;
class UBorder;
class UTextBlock;
class UImage;

/**
 * UCraftingPanelWidget — 크래프팅 패널 UI
 *
 * WBP_CraftingPanel 레이아웃:
 *   Border_CraftingPanel (root)
 *   └── VerticalBox_Content
 *       ├── Border_CraftProgress (BindWidget, Hidden by default)
 *       │   └── VerticalBox
 *       │       ├── HBox_ProgressInfo
 *       │       │   ├── TextBlock_CraftingLabel (BindWidget)
 *       │       │   └── TextBlock_CraftingTime  (BindWidget)
 *       │       └── SizeBox_ProgressTrack (H:4)
 *       │           └── Overlay
 *       │               ├── Image_ProgressBg
 *       │               └── Image_ProgressFill (BindWidget)
 *       └── ScrollBox_Recipes (BindWidget)
 *           └── WBP_CraftingRecipe (동적 생성)
 *
 * Initialize() 호출 후 RefreshRecipeList()로 레시피 목록 표시.
 */
UCLASS(Abstract)
class PROJECT_EXFIL_API UCraftingPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * 컴포넌트 참조 연결 + 델리게이트 바인딩.
     * 이름: UUserWidget::Initialize()와 충돌 방지를 위해 SetupCrafting 사용.
     */
    UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
    void SetupCrafting(UCraftingComponent* InCraftingComp, UInventoryComponent* InInventoryComp);

    /** ScrollBox_Recipes 갱신 */
    UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
    void RefreshRecipeList();

protected:
    virtual void NativeOnInitialized() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UScrollBox> ScrollBox_Recipes;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> Border_CraftProgress;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TextBlock_CraftingLabel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TextBlock_CraftingTime;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_ProgressFill;

    /** RecipeWidget 클래스 — BP에서 WBP_CraftingRecipe 지정 */
    UPROPERTY(EditDefaultsOnly, Category = "Crafting|UI")
    TSubclassOf<UCraftingRecipeWidget> RecipeWidgetClass;

private:
    TWeakObjectPtr<UCraftingComponent> CraftingComp;
    TWeakObjectPtr<UInventoryComponent> InventoryComp;

    /** 크래프팅 상태 변경 콜백 */
    UFUNCTION()
    void OnCraftingStateChanged(bool bIsCrafting, float RemainingTime);

    /** 크래프팅 완료 콜백 */
    UFUNCTION()
    void OnCraftingCompleted(FName CompletedRecipeID);

    /** 레시피 버튼 클릭 콜백 */
    void OnRecipeSelected(FName ClickedRecipeID);

    // ─── 프로그레스 바 ─────────────────────────────────────────────────────────
    FTimerHandle ProgressTimerHandle;
    float CraftStartTime = 0.f;
    float CraftTotalDuration = 0.f;

    void StartProgressTimer(float Duration);
    void StopProgressTimer();
    void UpdateProgressBar();
};
