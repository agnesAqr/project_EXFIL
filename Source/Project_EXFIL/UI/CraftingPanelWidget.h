// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/CraftingViewModel.h"
#include "CraftingPanelWidget.generated.h"

class UCraftingRecipeWidget;
class UScrollBox;
class UBorder;
class UTextBlock;
class UImage;

UCLASS(Abstract)
class PROJECT_EXFIL_API UCraftingPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    
    UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
    void SetViewModel(UCraftingViewModel* InViewModel);

    
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
    UPROPERTY()
    TWeakObjectPtr<UCraftingViewModel> ViewModel;

    
    UPROPERTY()
    TMap<FName, UCraftingRecipeWidget*> RecipeWidgetCache;

    
    bool bRecipesInitialized = false;
    bool bPanelVisible = false;
    bool bHasPendingRecipeDelta = false;

    FCraftingRecipeListDeltaViewData PendingRecipeDelta;
    FDelegateHandle RecipeListDeltaHandle;
    FDelegateHandle ProgressStartedHandle;
    FDelegateHandle ProgressStoppedHandle;
    FDelegateHandle CraftStartFailedHandle;

    void HandleRecipeListDeltaChanged(const FCraftingRecipeListDeltaViewData& Delta);
    void ApplyRecipeListDelta(const FCraftingRecipeListDeltaViewData& Delta);
    void FlushPendingRecipeDelta();
    void HandleCraftingProgressStarted(float Duration);
    void HandleCraftingProgressStopped();
    void HandleCraftStartFailed(FName RecipeID);

    
    void OnRecipeActionRequested(FName RecipeID, bool bIsCurrentRecipe);
    FTimerHandle ProgressTimerHandle;
    float CraftStartTime = 0.f;
    float CraftTotalDuration = 0.f;

    void StartProgressTimer(float Duration);
    void StopProgressTimer();
    void UpdateProgressBar();

    
    int32 CachedElapsedTenths = -1;
    int32 CachedDurationTenths = -1;
};
