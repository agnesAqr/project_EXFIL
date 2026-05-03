// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/CraftingViewModel.h"
#include "CraftingRecipeWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UTextBlock;

UCLASS(Abstract)
class PROJECT_EXFIL_API UCraftingRecipeWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    
    UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
    void SetRecipe(const FCraftingRecipeViewData& InRecipeViewData);

    
    DECLARE_DELEGATE_TwoParams(FOnRecipeActionRequested, FName, bool);
    FOnRecipeActionRequested OnRecipeActionRequested;

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
    bool bIsCurrentCraftRecipe = false;
    bool bPredictedCanCraft = false;

    UFUNCTION()
    void OnButtonClicked();

    
    void RefreshVisualState();
};
