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
class UItemDataSubsystem;

UCLASS(Abstract)
class PROJECT_EXFIL_API UCraftingRecipeWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    
    UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
    void SetRecipe(FName InRecipeID, UInventoryComponent* InInventory, bool bCanCraft);

    
    UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
    void SetCraftingInProgress(bool bInProgress, bool bIsCurrentRecipe);

    
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
    bool bIsCraftingCached = false;
    bool bIsCurrentCraftRecipe = false;

    UPROPERTY()
    TWeakObjectPtr<UItemDataSubsystem> CachedItemSub;

    UFUNCTION()
    void OnButtonClicked();

    
    void RefreshVisualState();
};
