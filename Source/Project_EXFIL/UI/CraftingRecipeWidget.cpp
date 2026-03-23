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

    UItemDataSubsystem* Sub = nullptr;
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            Sub = GI->GetSubsystem<UItemDataSubsystem>();
        }
    }

    if (!Sub)
    {
        return;
    }

    const FCraftingRecipe* Recipe = Sub->GetCraftingRecipe(InRecipeID);
    if (!Recipe)
    {
        return;
    }

    // 레시피 이름
    if (TextBlock_RecipeName)
    {
        TextBlock_RecipeName->SetText(Recipe->RecipeName);
        TextBlock_RecipeName->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.75f));
    }

    // 재료 텍스트 조립
    if (TextBlock_Ingredients)
    {
        FString IngredientsStr;
        for (int32 i = 0; i < Recipe->Ingredients.Num(); ++i)
        {
            const FCraftingIngredient& Ing = Recipe->Ingredients[i];
            const int32 Owned = InInventory ? InInventory->GetItemCountByID(Ing.ItemDataID) : 0;
            IngredientsStr += FString::Printf(TEXT("%s x%d (%d)"),
                *Ing.ItemDataID.ToString(), Ing.RequiredCount, Owned);
            if (i < Recipe->Ingredients.Num() - 1)
            {
                IngredientsStr += TEXT(" · ");
            }
        }
        TextBlock_Ingredients->SetText(FText::FromString(IngredientsStr));

        // 단순 구현: 전체 텍스트를 bCanCraft에 따라 초록/빨강
        const FLinearColor IngColor = bCanCraft
            ? FLinearColor(0.12f, 0.63f, 0.43f, 0.8f)  // 초록
            : FLinearColor(0.87f, 0.18f, 0.18f, 0.8f); // 빨강
        TextBlock_Ingredients->SetColorAndOpacity(IngColor);
    }

    // 크래프팅 시간
    if (TextBlock_CraftTime)
    {
        TextBlock_CraftTime->SetText(FText::FromString(
            FString::Printf(TEXT("%.1fs"), Recipe->CraftDuration)));
    }

    // 결과물 아이콘 (텍스처가 없으면 배경 색만 표시)
    if (Image_ResultIcon)
    {
        const FItemData* ResultItem = Sub->GetItemData(Recipe->ResultItemID);
        if (ResultItem && !ResultItem->Icon.IsNull())
        {
            UTexture2D* IconTex = ResultItem->Icon.LoadSynchronous();
            if (IconTex)
            {
                Image_ResultIcon->SetBrushFromTexture(IconTex);
            }
        }
    }

    // 크래프팅 불가 시 전체 위젯 반투명 + 버튼 비활성화
    if (!bCanCraft)
    {
        SetRenderOpacity(0.45f);
        if (Button_Craft)
        {
            Button_Craft->SetIsEnabled(false);
        }
    }
    else
    {
        SetRenderOpacity(1.f);
        if (Button_Craft)
        {
            Button_Craft->SetIsEnabled(true);
        }
    }

    ApplyButtonStyle(bCanCraft, false);
}

void UCraftingRecipeWidget::SetCraftingInProgress(bool bInProgress)
{
    ApplyButtonStyle(bCanCraftCached, bInProgress);
}

void UCraftingRecipeWidget::OnButtonClicked()
{
    // bCanCraftCached와 무관하게 클릭 자체는 허용 (CraftingPanel에서 Cancel 처리)
    OnRecipeClicked.ExecuteIfBound(RecipeID);
}

void UCraftingRecipeWidget::ApplyButtonStyle(bool bCanCraft, bool bInProgress)
{
    if (!Button_Craft)
    {
        return;
    }

    // 버튼 내부 TextBlock 탐색
    UTextBlock* CraftLabel = nullptr;
    if (UWidget* Child = Button_Craft->GetChildAt(0))
    {
        CraftLabel = Cast<UTextBlock>(Child);
    }

    if (bInProgress)
    {
        // Crafting in progress: Amber
        if (CraftLabel)
        {
            CraftLabel->SetText(NSLOCTEXT("Crafting", "Cancel", "CANCEL"));
            CraftLabel->SetColorAndOpacity(FLinearColor(0.94f, 0.62f, 0.15f, 0.9f));
        }
    }
    else if (bCanCraft)
    {
        // Can craft: 초록
        if (CraftLabel)
        {
            CraftLabel->SetText(NSLOCTEXT("Crafting", "Craft", "CRAFT"));
            CraftLabel->SetColorAndOpacity(FLinearColor(0.12f, 0.63f, 0.43f, 0.9f));
        }
    }
    else
    {
        // Cannot craft: 비활성
        if (CraftLabel)
        {
            CraftLabel->SetText(NSLOCTEXT("Crafting", "Craft", "CRAFT"));
            CraftLabel->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.2f));
        }
    }
}
