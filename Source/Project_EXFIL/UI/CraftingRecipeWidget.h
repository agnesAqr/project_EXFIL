// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CraftingRecipeWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UTextBlock;
class UInventoryComponent;

/**
 * UCraftingRecipeWidget — 크래프팅 레시피 엔트리 위젯 (하나의 레시피)
 *
 * WBP_CraftingRecipe 레이아웃:
 *   Border_Recipe (root)
 *   └── HorizontalBox_Content
 *       ├── Image_ResultIcon    (36×36, BindWidget)
 *       ├── VerticalBox_Info (Fill 1.0)
 *       │   ├── TextBlock_RecipeName  (BindWidget)
 *       │   └── TextBlock_Ingredients (BindWidget)
 *       ├── TextBlock_CraftTime (BindWidgetOptional)
 *       └── Button_Craft        (BindWidget)
 *
 * SetRecipe() 호출 후 UI 갱신.
 * Button 클릭 → OnRecipeClicked 델리게이트 실행.
 */
UCLASS(Abstract)
class PROJECT_EXFIL_API UCraftingRecipeWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * 레시피 데이터와 재료 보유 여부로 UI 갱신.
     * @param InRecipeID    레시피 RowName
     * @param InInventory   재료 보유량 조회용 인벤토리 (nullptr 허용)
     * @param bCanCraft     현재 크래프팅 가능 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
    void SetRecipe(FName InRecipeID, UInventoryComponent* InInventory, bool bCanCraft);

    /** 크래프팅 중 상태로 버튼 텍스트/스타일 변경 */
    UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
    void SetCraftingInProgress(bool bInProgress);

    /** 버튼 클릭 시 브로드캐스트 — UCraftingPanelWidget에서 바인딩 */
    DECLARE_DELEGATE_OneParam(FOnRecipeClicked, FName);
    FOnRecipeClicked OnRecipeClicked;

protected:
    virtual void NativeOnInitialized() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> Border_Recipe;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Craft;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_ResultIcon;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TextBlock_RecipeName;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TextBlock_Ingredients;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TextBlock_CraftTime;

private:
    FName RecipeID;
    bool bCanCraftCached = false;

    UFUNCTION()
    void OnButtonClicked();

    /** bCanCraft에 따른 버튼 스타일 적용 */
    void ApplyButtonStyle(bool bCanCraft, bool bInProgress);
};
