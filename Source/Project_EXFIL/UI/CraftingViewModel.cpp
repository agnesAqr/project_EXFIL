// Copyright Project EXFIL. All Rights Reserved.

#include "UI/CraftingViewModel.h"
#include "CoreMinimal.h"
#include "Crafting/CraftingComponent.h"
#include "Data/EXFILItemTypes.h"
#include "Data/ItemDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Inventory/InventoryComponent.h"

void UCraftingViewModel::Initialize(
    UCraftingComponent* InCraftingComponent, UInventoryComponent* InInventoryComponent)
{
    UnbindModelDelegates();

    CraftingComp = InCraftingComponent;
    InventoryComp = InInventoryComponent;

    UObject* ContextObject = InCraftingComponent
        ? static_cast<UObject*>(InCraftingComponent)
        : static_cast<UObject*>(InInventoryComponent);
    UWorld* World = ContextObject ? ContextObject->GetWorld() : nullptr;
    if (World)
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            CachedItemSub = GI->GetSubsystem<UItemDataSubsystem>();
        }
    }

    if (InCraftingComponent)
    {
        InCraftingComponent->OnCraftingStateChanged.AddDynamic(
            this, &UCraftingViewModel::HandleCraftingStateChanged);
        InCraftingComponent->OnCraftStartFailed.AddDynamic(
            this, &UCraftingViewModel::HandleCraftStartFailed);
    }

    if (InInventoryComponent)
    {
        InventoryItemCountsChangedHandle =
            InInventoryComponent->OnInventoryItemCountsChanged.AddUObject(
                this, &UCraftingViewModel::HandleInventoryItemCountsChanged);
    }

    BuildRecipeDependencyIndex();
}

void UCraftingViewModel::BeginDestroy()
{
    UnbindModelDelegates();
    Super::BeginDestroy();
}

void UCraftingViewModel::RequestStartCraft(FName RecipeID)
{
    if (CraftingComp.IsValid())
    {
        CraftingComp->RequestStartCraft(RecipeID);
    }
}

void UCraftingViewModel::RequestCancelCraft()
{
    if (CraftingComp.IsValid())
    {
        CraftingComp->RequestCancelCraft();
    }
}

bool UCraftingViewModel::BuildInitialRecipeListDelta(
    FCraftingRecipeListDeltaViewData& OutDelta)
{
    OutDelta = FCraftingRecipeListDeltaViewData();
    OutDelta.bReset = true;

    UItemDataSubsystem* ItemSub = CachedItemSub.Get();
    if (!ItemSub)
    {
        return false;
    }

    if (InventoryComp.IsValid())
    {
        InventoryComp->EnsureReplicatedCachesReady();
    }

    for (const FName& RecipeID : ItemSub->GetAllRecipeIDs())
    {
        FCraftingRecipeViewData RecipeViewData;
        if (BuildRecipeViewData(RecipeID, RecipeViewData))
        {
            OutDelta.UpsertRecipes.Add(RecipeViewData);
        }
    }

    return true;
}

void UCraftingViewModel::HandleCraftingStateChanged(
    bool bIsCrafting, float Duration)
{
    const FName CurrentRecipeID =
        bIsCrafting && CraftingComp.IsValid()
            ? CraftingComp->GetCurrentRecipeID()
            : NAME_None;

    TSet<FName> ChangedRecipeIDs;
    if (!LastCurrentRecipeID.IsNone())
    {
        ChangedRecipeIDs.Add(LastCurrentRecipeID);
    }
    if (!CurrentRecipeID.IsNone())
    {
        ChangedRecipeIDs.Add(CurrentRecipeID);
    }

    LastCurrentRecipeID = CurrentRecipeID;

    if (ChangedRecipeIDs.Num() > 0)
    {
        BroadcastRecipeDeltaForIDs(ChangedRecipeIDs, false);
    }

    if (bIsCrafting)
    {
        OnCraftingProgressStarted.Broadcast(Duration);
    }
    else
    {
        OnCraftingProgressStopped.Broadcast();
    }
}

void UCraftingViewModel::HandleCraftStartFailed(FName RecipeID)
{
    OnCraftStartFailed.Broadcast(RecipeID);
}

void UCraftingViewModel::HandleInventoryItemCountsChanged(
    const TSet<FName>& ChangedItemDataIDs)
{
    if (ChangedItemDataIDs.Num() == 0)
    {
        return;
    }

    if (InventoryComp.IsValid())
    {
        InventoryComp->EnsureReplicatedCachesReady();
    }

    TSet<FName> AffectedRecipeIDs;
    for (const FName& ItemDataID : ChangedItemDataIDs)
    {
        if (const TSet<FName>* RecipeIDs = IngredientToRecipeIDs.Find(ItemDataID))
        {
            AffectedRecipeIDs.Append(*RecipeIDs);
        }
    }

    BroadcastRecipeDeltaForIDs(AffectedRecipeIDs, false);
}

void UCraftingViewModel::BuildRecipeDependencyIndex()
{
    IngredientToRecipeIDs.Empty();

    UItemDataSubsystem* ItemSub = CachedItemSub.Get();
    if (!ItemSub)
    {
        return;
    }

    for (const FName& RecipeID : ItemSub->GetAllRecipeIDs())
    {
        const FCraftingRecipe* Recipe = ItemSub->GetCraftingRecipe(RecipeID);
        if (!Recipe)
        {
            continue;
        }

        for (const FCraftingIngredient& Ingredient : Recipe->Ingredients)
        {
            IngredientToRecipeIDs.FindOrAdd(Ingredient.ItemDataID).Add(RecipeID);
        }
    }
}

bool UCraftingViewModel::BuildRecipeViewData(
    FName RecipeID, FCraftingRecipeViewData& OutViewData) const
{
    UItemDataSubsystem* ItemSub = CachedItemSub.Get();
    if (!ItemSub)
    {
        return false;
    }

    const FCraftingRecipe* Recipe = ItemSub->GetCraftingRecipe(RecipeID);
    if (!Recipe)
    {
        return false;
    }

    OutViewData = FCraftingRecipeViewData();
    OutViewData.RecipeID = RecipeID;
    OutViewData.RecipeName = Recipe->RecipeName;
    OutViewData.CraftDuration = Recipe->CraftDuration;
    OutViewData.bIsCurrentRecipe =
        CraftingComp.IsValid() &&
        CraftingComp->IsCrafting() &&
        CraftingComp->GetCurrentRecipeID() == RecipeID;

    const FItemData* ResultItem = ItemSub->GetItemData(Recipe->ResultItemID);
    if (ResultItem && !ResultItem->Icon.IsNull())
    {
        OutViewData.ResultItemIcon = ItemSub->GetCachedTexture(ResultItem->Icon);
    }

    UInventoryComponent* Inventory = InventoryComp.Get();
    for (const FCraftingIngredient& Ingredient : Recipe->Ingredients)
    {
        FCraftingIngredientViewData IngredientViewData;
        const FItemData* IngredientItem = ItemSub->GetItemData(Ingredient.ItemDataID);
        IngredientViewData.DisplayName = IngredientItem
            ? IngredientItem->DisplayName
            : FText::FromName(Ingredient.ItemDataID);
        IngredientViewData.RequiredCount = Ingredient.RequiredCount;
        IngredientViewData.OwnedCount = Inventory
            ? Inventory->GetItemCountByID_Cached(Ingredient.ItemDataID)
            : 0;
        OutViewData.Ingredients.Add(IngredientViewData);
    }

    return true;
}

void UCraftingViewModel::BroadcastRecipeDeltaForIDs(
    const TSet<FName>& RecipeIDs, bool bReset)
{
    if (RecipeIDs.Num() == 0)
    {
        return;
    }

    FCraftingRecipeListDeltaViewData Delta;
    Delta.bReset = bReset;
    for (const FName& RecipeID : RecipeIDs)
    {
        FCraftingRecipeViewData RecipeViewData;
        if (BuildRecipeViewData(RecipeID, RecipeViewData))
        {
            Delta.UpsertRecipes.Add(RecipeViewData);
        }
    }

    if (Delta.UpsertRecipes.Num() > 0 || Delta.bReset)
    {
        OnRecipeListDeltaChanged.Broadcast(Delta);
    }
}

void UCraftingViewModel::UnbindModelDelegates()
{
    if (CraftingComp.IsValid())
    {
        CraftingComp->OnCraftingStateChanged.RemoveDynamic(
            this, &UCraftingViewModel::HandleCraftingStateChanged);
        CraftingComp->OnCraftStartFailed.RemoveDynamic(
            this, &UCraftingViewModel::HandleCraftStartFailed);
        CraftingComp.Reset();
    }

    if (InventoryComp.IsValid() && InventoryItemCountsChangedHandle.IsValid())
    {
        InventoryComp->OnInventoryItemCountsChanged.Remove(InventoryItemCountsChangedHandle);
        InventoryItemCountsChangedHandle.Reset();
    }
    InventoryComp.Reset();
}
