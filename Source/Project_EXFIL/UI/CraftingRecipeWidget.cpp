// Copyright Project EXFIL. All Rights Reserved.

#include "UI/CraftingRecipeWidget.h"
#include "CoreMinimal.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Data/ItemDataSubsystem.h"
#include "Data/EXFILItemTypes.h"
#include "Inventory/InventoryComponent.h"

void UCraftingRecipeWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (UGameInstance* GI = GetGameInstance())
    {
        CachedItemSub = GI->GetSubsystem<UItemDataSubsystem>();
    }

    if (Button_Craft)
    {
        Button_Craft->OnClicked.AddDynamic(this, &UCraftingRecipeWidget::OnButtonClicked);
    }
}

void UCraftingRecipeWidget::SetRecipe(FName InRecipeID, UInventoryComponent* InInventory,
                                       bool bCanCraft)
{
    RecipeID = InRecipeID;
    bCanCraftCached = bCanCraft;

    UItemDataSubsystem* Sub = CachedItemSub.Get();
    if (!Sub)
    {
        return;
    }

    const FCraftingRecipe* Recipe = Sub->GetCraftingRecipe(InRecipeID);
    if (!Recipe)
    {
        return;
    }
    if (TextBlock_RecipeName)
    {
        TextBlock_RecipeName->SetText(Recipe->RecipeName);
        TextBlock_RecipeName->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.75f));
    }
    if (TextBlock_Ingredients)
    {
        FString IngredientsStr;
        for (int32 i = 0; i < Recipe->Ingredients.Num(); ++i)
        {
            const FCraftingIngredient& Ing = Recipe->Ingredients[i];
            const int32 Owned = InInventory ? InInventory->GetItemCountByID_Cached(Ing.ItemDataID) : 0;
            IngredientsStr += FString::Printf(TEXT("%s x%d (%d)"),
                *Ing.ItemDataID.ToString(), Ing.RequiredCount, Owned);
            if (i < Recipe->Ingredients.Num() - 1)
            {
                IngredientsStr += TEXT(" · ");
            }
        }
        TextBlock_Ingredients->SetText(FText::FromString(IngredientsStr));
        const FLinearColor IngColor = bCanCraft
            ? FLinearColor(0.12f, 0.63f, 0.43f, 0.8f)
            : FLinearColor(0.87f, 0.18f, 0.18f, 0.8f);
        TextBlock_Ingredients->SetColorAndOpacity(IngColor);
    }
    if (TextBlock_CraftTime)
    {
        TextBlock_CraftTime->SetText(FText::FromString(
            FString::Printf(TEXT("%.1fs"), Recipe->CraftDuration)));
    }
    if (Image_ResultIcon)
    {
        const FItemData* ResultItem = Sub->GetItemData(Recipe->ResultItemID);
        if (ResultItem && !ResultItem->Icon.IsNull())
        {
            UTexture2D* IconTex = Sub->GetCachedTexture(ResultItem->Icon);
            if (IconTex)
            {
                Image_ResultIcon->SetBrushFromTexture(IconTex, true);
            }
        }
    }
    RefreshVisualState();
}

void UCraftingRecipeWidget::SetCraftingInProgress(bool bInProgress, bool bIsCurrentRecipe)
{
    bIsCraftingCached = bInProgress;
    bIsCurrentCraftRecipe = bInProgress && bIsCurrentRecipe;
    RefreshVisualState();
}

void UCraftingRecipeWidget::OnButtonClicked()
{
    OnRecipeClicked.ExecuteIfBound(RecipeID);
}

void UCraftingRecipeWidget::RefreshVisualState()
{
    if (!Button_Craft)
    {
        return;
    }

    const bool bShowCancel = bIsCurrentCraftRecipe;
    const bool bEnableCraft = !bIsCraftingCached && bCanCraftCached;
    const bool bEnableButton = bShowCancel || bEnableCraft;
    const float TargetOpacity = bEnableButton ? 1.f : 0.45f;

    SetRenderOpacity(TargetOpacity);
    Button_Craft->SetIsEnabled(bEnableButton);

    UTextBlock* CraftLabel = nullptr;
    if (UWidget* Child = Button_Craft->GetChildAt(0))
    {
        CraftLabel = Cast<UTextBlock>(Child);
    }

    if (bShowCancel)
    {
        if (CraftLabel)
        {
            CraftLabel->SetText(NSLOCTEXT("Crafting", "Cancel", "CANCEL"));
            CraftLabel->SetColorAndOpacity(FLinearColor(0.94f, 0.62f, 0.15f, 0.9f));
        }
    }
    else if (bEnableCraft)
    {
        if (CraftLabel)
        {
            CraftLabel->SetText(NSLOCTEXT("Crafting", "Craft", "CRAFT"));
            CraftLabel->SetColorAndOpacity(FLinearColor(0.12f, 0.63f, 0.43f, 0.9f));
        }
    }
    else
    {
        if (CraftLabel)
        {
            CraftLabel->SetText(NSLOCTEXT("Crafting", "Craft", "CRAFT"));
            CraftLabel->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.2f));
        }
    }
}
