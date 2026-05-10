// Copyright Project EXFIL. All Rights Reserved.

#include "UI/CraftingRecipeWidget.h"
#include "CoreMinimal.h"
#include "Internationalization/StringTableRegistry.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UCraftingRecipeWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (Button_Craft)
    {
        Button_Craft->OnClicked.AddDynamic(this, &UCraftingRecipeWidget::OnButtonClicked);
    }
}

void UCraftingRecipeWidget::SetRecipe(const FCraftingRecipeViewData& InRecipeViewData)
{
    RecipeID = InRecipeViewData.RecipeID;
    bIsCurrentCraftRecipe = InRecipeViewData.bIsCurrentRecipe;
    bPredictedCanCraft = InRecipeViewData.bPredictedCanCraft;

    if (TextBlock_RecipeName)
    {
        TextBlock_RecipeName->SetText(InRecipeViewData.RecipeName);
        TextBlock_RecipeName->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.75f));
    }

    if (TextBlock_Ingredients)
    {
        FString IngredientsStr;
        for (int32 i = 0; i < InRecipeViewData.Ingredients.Num(); ++i)
        {
            const FCraftingIngredientViewData& Ingredient =
                InRecipeViewData.Ingredients[i];
            IngredientsStr += FString::Printf(
                TEXT("%s x%d (%d)"),
                *Ingredient.DisplayName.ToString(),
                Ingredient.RequiredCount,
                Ingredient.OwnedCount);
            if (i < InRecipeViewData.Ingredients.Num() - 1)
            {
                IngredientsStr += TEXT(" · ");
            }
        }
        TextBlock_Ingredients->SetText(FText::FromString(IngredientsStr));
        TextBlock_Ingredients->SetColorAndOpacity(
            FLinearColor(1.f, 1.f, 1.f, 0.65f));
    }

    if (TextBlock_CraftTime)
    {
        TextBlock_CraftTime->SetText(FText::FromString(
            FString::Printf(TEXT("%.1fs"), InRecipeViewData.CraftDuration)));
    }

    if (Image_ResultIcon)
    {
        Image_ResultIcon->SetBrushFromTexture(
            InRecipeViewData.ResultItemIcon.Get(), true);
        Image_ResultIcon->SetVisibility(InRecipeViewData.ResultItemIcon
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
    }

    RefreshVisualState();
}

void UCraftingRecipeWidget::OnButtonClicked()
{
    OnRecipeActionRequested.ExecuteIfBound(RecipeID, bIsCurrentCraftRecipe);
}

void UCraftingRecipeWidget::RefreshVisualState()
{
    if (!Button_Craft)
    {
        return;
    }

    const bool bCanTriggerAction = bIsCurrentCraftRecipe || bPredictedCanCraft;
    Button_Craft->SetIsEnabled(bCanTriggerAction);
    SetRenderOpacity(bCanTriggerAction ? (bIsCurrentCraftRecipe ? 1.f : 0.85f) : 0.55f);

    UTextBlock* CraftLabel = nullptr;
    if (UWidget* Child = Button_Craft->GetChildAt(0))
    {
        CraftLabel = Cast<UTextBlock>(Child);
    }

    if (!CraftLabel)
    {
        return;
    }

    if (bIsCurrentCraftRecipe)
    {
        CraftLabel->SetText(LOCTABLE("/Game/Localization/ST_UI", "Crafting.Cancel"));
        CraftLabel->SetColorAndOpacity(FLinearColor(0.94f, 0.62f, 0.15f, 0.9f));
    }
    else
    {
        CraftLabel->SetText(LOCTABLE("/Game/Localization/ST_UI", "Crafting.Craft"));
        CraftLabel->SetColorAndOpacity(bPredictedCanCraft
            ? FLinearColor(0.12f, 0.63f, 0.43f, 0.9f)
            : FLinearColor(0.45f, 0.45f, 0.45f, 0.7f));
    }
}
