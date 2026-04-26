// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "CraftingViewModel.generated.h"

class UCraftingComponent;
class UInventoryComponent;
class UItemDataSubsystem;
class UTexture2D;

USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FCraftingIngredientViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly)
    int32 RequiredCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 OwnedCount = 0;
};

USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FCraftingRecipeViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName RecipeID;

    UPROPERTY(BlueprintReadOnly)
    FText RecipeName;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UTexture2D> ResultItemIcon = nullptr;

    UPROPERTY(BlueprintReadOnly)
    float CraftDuration = 0.f;

    UPROPERTY(BlueprintReadOnly)
    bool bIsCurrentRecipe = false;

    UPROPERTY(BlueprintReadOnly)
    TArray<FCraftingIngredientViewData> Ingredients;
};

USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FCraftingRecipeListDeltaViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bReset = false;

    UPROPERTY(BlueprintReadOnly)
    TArray<FCraftingRecipeViewData> UpsertRecipes;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FOnCraftingRecipeListDeltaChanged, const FCraftingRecipeListDeltaViewData&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCraftingProgressStarted, float);
DECLARE_MULTICAST_DELEGATE(FOnCraftingProgressStopped);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCraftStartFailedViewModel, FName);

UCLASS()
class PROJECT_EXFIL_API UCraftingViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
    void Initialize(UCraftingComponent* InCraftingComponent,
                    UInventoryComponent* InInventoryComponent);

    virtual void BeginDestroy() override;

    UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
    void RequestStartCraft(FName RecipeID);

    UFUNCTION(BlueprintCallable, Category = "Crafting|ViewModel")
    void RequestCancelCraft();

    bool BuildInitialRecipeListDelta(FCraftingRecipeListDeltaViewData& OutDelta);

    FOnCraftingRecipeListDeltaChanged OnRecipeListDeltaChanged;
    FOnCraftingProgressStarted OnCraftingProgressStarted;
    FOnCraftingProgressStopped OnCraftingProgressStopped;
    FOnCraftStartFailedViewModel OnCraftStartFailed;

private:
    UPROPERTY()
    TWeakObjectPtr<UCraftingComponent> CraftingComp;

    UPROPERTY()
    TWeakObjectPtr<UInventoryComponent> InventoryComp;

    UPROPERTY()
    TWeakObjectPtr<UItemDataSubsystem> CachedItemSub;

    TMap<FName, TSet<FName>> IngredientToRecipeIDs;
    FDelegateHandle InventoryItemCountsChangedHandle;
    FName LastCurrentRecipeID;

    UFUNCTION()
    void HandleCraftingStateChanged(bool bIsCrafting, float Duration);

    UFUNCTION()
    void HandleCraftStartFailed(FName RecipeID);

    void HandleInventoryItemCountsChanged(const TSet<FName>& ChangedItemDataIDs);
    void BuildRecipeDependencyIndex();
    bool BuildRecipeViewData(FName RecipeID, FCraftingRecipeViewData& OutViewData) const;
    void BroadcastRecipeDeltaForIDs(const TSet<FName>& RecipeIDs, bool bReset);
    void UnbindModelDelegates();
};
