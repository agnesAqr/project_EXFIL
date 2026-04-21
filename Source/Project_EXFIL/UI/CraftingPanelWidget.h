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
class UItemDataSubsystem;

UCLASS(Abstract)
class PROJECT_EXFIL_API UCraftingPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    
    UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
    void SetupCrafting(UCraftingComponent* InCraftingComp, UInventoryComponent* InInventoryComp);

    
    UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
    void RefreshRecipeList();

    void NotifyPanelShown();
    void NotifyPanelHidden();

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

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

    
    UPROPERTY(EditDefaultsOnly, Category = "Crafting|UI")
    TSubclassOf<UCraftingRecipeWidget> RecipeWidgetClass;

private:
    TWeakObjectPtr<UCraftingComponent> CraftingComp;
    TWeakObjectPtr<UInventoryComponent> InventoryComp;

    UPROPERTY()
    TWeakObjectPtr<UItemDataSubsystem> CachedItemSub;

    
    UPROPERTY()
    TMap<FName, UCraftingRecipeWidget*> RecipeWidgetCache;

    
    bool bRecipesInitialized = false;
    bool bRecipeDependencyIndexBuilt = false;
    bool bPanelVisible = false;
    bool bHasPendingRecipeRefresh = false;

    TMap<FName, TSet<FName>> IngredientToRecipeIDs;
    TSet<FName> PendingDirtyItemIDs;
    FDelegateHandle InventoryItemCountsChangedHandle;

    
    UFUNCTION()
    void OnCraftingStateChanged(bool bIsCrafting, float RemainingTime);

    
    void OnInventoryItemCountsChanged(const TSet<FName>& ChangedItemDataIDs);

    void BuildRecipeDependencyIndex();
    void RefreshRecipesByItemChanges(const TSet<FName>& ChangedItemDataIDs);
    void FlushPendingRecipeRefresh();

    
    void OnRecipeSelected(FName ClickedRecipeID);
    FTimerHandle ProgressTimerHandle;
    float CraftStartTime = 0.f;
    float CraftTotalDuration = 0.f;

    void StartProgressTimer(float Duration);
    void StopProgressTimer();
    void UpdateProgressBar();

    
    int32 CachedElapsedTenths = -1;
    int32 CachedDurationTenths = -1;
};
