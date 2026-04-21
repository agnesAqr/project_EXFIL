// Copyright Project EXFIL. All Rights Reserved.

#include "UI/CraftingPanelWidget.h"
#include "CoreMinimal.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Crafting/CraftingComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Data/ItemDataSubsystem.h"
#include "UI/CraftingRecipeWidget.h"
#include "Engine/GameInstance.h"

void UCraftingPanelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            CachedItemSub = GI->GetSubsystem<UItemDataSubsystem>();
        }
    }

    if (Border_CraftProgress)
    {
        Border_CraftProgress->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UCraftingPanelWidget::NativeDestruct()
{
    if (CraftingComp.IsValid())
    {
        CraftingComp->OnCraftingStateChanged.RemoveDynamic(
            this, &UCraftingPanelWidget::OnCraftingStateChanged);
    }

    if (InventoryComp.IsValid() && InventoryItemCountsChangedHandle.IsValid())
    {
        InventoryComp->OnInventoryItemCountsChanged.Remove(InventoryItemCountsChangedHandle);
        InventoryItemCountsChangedHandle.Reset();
    }

    Super::NativeDestruct();
}

void UCraftingPanelWidget::SetupCrafting(UCraftingComponent* InCraftingComp,
    UInventoryComponent* InInventoryComp)
{
    if (CraftingComp.IsValid())
    {
        CraftingComp->OnCraftingStateChanged.RemoveDynamic(
            this, &UCraftingPanelWidget::OnCraftingStateChanged);
    }

    if (InventoryComp.IsValid() && InventoryItemCountsChangedHandle.IsValid())
    {
        InventoryComp->OnInventoryItemCountsChanged.Remove(InventoryItemCountsChangedHandle);
        InventoryItemCountsChangedHandle.Reset();
    }

    CraftingComp = InCraftingComp;
    InventoryComp = InInventoryComp;

    if (InCraftingComp)
    {
        InCraftingComp->OnCraftingStateChanged.AddDynamic(
            this, &UCraftingPanelWidget::OnCraftingStateChanged);
    }

    if (InInventoryComp)
    {
        InventoryItemCountsChangedHandle =
            InInventoryComp->OnInventoryItemCountsChanged.AddUObject(
                this, &UCraftingPanelWidget::OnInventoryItemCountsChanged);
    }
}

void UCraftingPanelWidget::NotifyPanelShown()
{
    bPanelVisible = true;

    if (!bRecipesInitialized)
    {
        RefreshRecipeList();
        PendingDirtyItemIDs.Reset();
        bHasPendingRecipeRefresh = false;
        return;
    }

    FlushPendingRecipeRefresh();
}

void UCraftingPanelWidget::NotifyPanelHidden()
{
    bPanelVisible = false;
}

void UCraftingPanelWidget::OnInventoryItemCountsChanged(
    const TSet<FName>& ChangedItemDataIDs)
{
    if (ChangedItemDataIDs.Num() == 0)
    {
        return;
    }

    if (!bPanelVisible)
    {
        PendingDirtyItemIDs.Append(ChangedItemDataIDs);
        bHasPendingRecipeRefresh = true;
        return;
    }

    RefreshRecipesByItemChanges(ChangedItemDataIDs);
}

void UCraftingPanelWidget::RefreshRecipeList()
{
    if (bRecipesInitialized || !ScrollBox_Recipes || !RecipeWidgetClass)
    {
        return;
    }

    UItemDataSubsystem* Subsystem = CachedItemSub.Get();
    if (!Subsystem)
    {
        return;
    }

    UCraftingComponent* Crafting = CraftingComp.Get();
    UInventoryComponent* Inventory = InventoryComp.Get();
    if (Inventory)
    {
        Inventory->EnsureReplicatedCachesReady();
    }

    BuildRecipeDependencyIndex();
    bRecipesInitialized = true;
    ScrollBox_Recipes->ClearChildren();
    RecipeWidgetCache.Empty();

    for (const FName& RecipeID : Subsystem->GetAllRecipeIDs())
    {
        UCraftingRecipeWidget* RecipeWidget =
            CreateWidget<UCraftingRecipeWidget>(this, RecipeWidgetClass);
        if (!RecipeWidget)
        {
            continue;
        }

        const bool bCanCraft = Crafting ? Crafting->CanCraft(RecipeID) : false;
        const bool bIsCrafting = Crafting ? Crafting->IsCrafting() : false;
        const FName ActiveRecipeID =
            bIsCrafting && Crafting ? Crafting->GetCurrentRecipeID() : NAME_None;
        RecipeWidget->SetRecipe(RecipeID, Inventory, bCanCraft);
        RecipeWidget->SetCraftingInProgress(
            bIsCrafting, RecipeID == ActiveRecipeID);
        RecipeWidget->OnRecipeClicked.BindUObject(
            this, &UCraftingPanelWidget::OnRecipeSelected);

        ScrollBox_Recipes->AddChild(RecipeWidget);
        RecipeWidgetCache.Add(RecipeID, RecipeWidget);
    }
}

void UCraftingPanelWidget::BuildRecipeDependencyIndex()
{
    IngredientToRecipeIDs.Empty();

    UItemDataSubsystem* Subsystem = CachedItemSub.Get();
    if (!Subsystem)
    {
        return;
    }

    for (const FName& RecipeID : Subsystem->GetAllRecipeIDs())
    {
        const FCraftingRecipe* Recipe = Subsystem->GetCraftingRecipe(RecipeID);
        if (!Recipe)
        {
            continue;
        }

        for (const FCraftingIngredient& Ingredient : Recipe->Ingredients)
        {
            IngredientToRecipeIDs.FindOrAdd(Ingredient.ItemDataID).Add(RecipeID);
        }
    }

    bRecipeDependencyIndexBuilt = true;
}

void UCraftingPanelWidget::RefreshRecipesByItemChanges(
    const TSet<FName>& ChangedItemDataIDs)
{
    if (!bRecipesInitialized || ChangedItemDataIDs.Num() == 0)
    {
        return;
    }

    if (!bRecipeDependencyIndexBuilt)
    {
        BuildRecipeDependencyIndex();
    }

    UCraftingComponent* Crafting = CraftingComp.Get();
    UInventoryComponent* Inventory = InventoryComp.Get();
    if (!Crafting || !Inventory)
    {
        return;
    }

    Inventory->EnsureReplicatedCachesReady();

    TSet<FName> AffectedRecipeIDs;
    for (const FName& ItemDataID : ChangedItemDataIDs)
    {
        if (const TSet<FName>* RecipeIDs = IngredientToRecipeIDs.Find(ItemDataID))
        {
            AffectedRecipeIDs.Append(*RecipeIDs);
        }
    }

    const bool bIsCrafting = Crafting->IsCrafting();
    const FName ActiveRecipeID =
        bIsCrafting ? Crafting->GetCurrentRecipeID() : NAME_None;
    for (const FName& RecipeID : AffectedRecipeIDs)
    {
        UCraftingRecipeWidget* const* FoundWidget = RecipeWidgetCache.Find(RecipeID);
        if (!FoundWidget || !*FoundWidget)
        {
            continue;
        }

        const bool bCanCraft = Crafting->CanCraft(RecipeID);
        (*FoundWidget)->SetRecipe(RecipeID, Inventory, bCanCraft);
        (*FoundWidget)->SetCraftingInProgress(
            bIsCrafting, RecipeID == ActiveRecipeID);
    }
}

void UCraftingPanelWidget::FlushPendingRecipeRefresh()
{
    if (!bPanelVisible || !bHasPendingRecipeRefresh || PendingDirtyItemIDs.Num() == 0)
    {
        return;
    }

    RefreshRecipesByItemChanges(PendingDirtyItemIDs);
    PendingDirtyItemIDs.Reset();
    bHasPendingRecipeRefresh = false;
}

void UCraftingPanelWidget::OnCraftingStateChanged(bool bIsCrafting, float RemainingTime)
{
    const FName ActiveRecipeID =
        bIsCrafting && CraftingComp.IsValid()
            ? CraftingComp->GetCurrentRecipeID()
            : NAME_None;

    if (bIsCrafting)
    {
        if (Image_ProgressFill)
        {
            Image_ProgressFill->SetRenderScale(FVector2D(0.f, 1.f));
        }

        if (TextBlock_CraftingTime)
        {
            TextBlock_CraftingTime->SetText(FText::FromString(
                FString::Printf(TEXT("0.0s / %.1fs"), RemainingTime)));
        }

        if (Border_CraftProgress)
        {
            Border_CraftProgress->SetVisibility(ESlateVisibility::Visible);
        }

        if (TextBlock_CraftingLabel)
        {
            UCraftingComponent* Crafting = CraftingComp.Get();
            const FName RecipeID = Crafting ? Crafting->GetCurrentRecipeID() : NAME_None;
            TextBlock_CraftingLabel->SetText(FText::FromString(
                FString::Printf(TEXT("Crafting: %s"), *RecipeID.ToString())));
        }

        StartProgressTimer(RemainingTime);
    }
    else
    {
        StopProgressTimer();

        if (Border_CraftProgress)
        {
            Border_CraftProgress->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    for (TPair<FName, UCraftingRecipeWidget*>& Pair : RecipeWidgetCache)
    {
        if (Pair.Value)
        {
            Pair.Value->SetCraftingInProgress(
                bIsCrafting, Pair.Key == ActiveRecipeID);
        }
    }
}

void UCraftingPanelWidget::OnRecipeSelected(FName ClickedRecipeID)
{
    UCraftingComponent* Crafting = CraftingComp.Get();
    if (!Crafting)
    {
        return;
    }

    if (Crafting->IsCrafting())
    {
        Crafting->RequestCancelCraft();
    }
    else
    {
        Crafting->RequestStartCraft(ClickedRecipeID);
    }
}

void UCraftingPanelWidget::StartProgressTimer(float Duration)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    CraftStartTime = World->GetTimeSeconds();
    CraftTotalDuration = Duration;
    CachedElapsedTenths = -1;
    CachedDurationTenths = -1;

    World->GetTimerManager().SetTimer(
        ProgressTimerHandle,
        this,
        &UCraftingPanelWidget::UpdateProgressBar,
        0.05f,
        true);
}

void UCraftingPanelWidget::StopProgressTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ProgressTimerHandle);
    }

    if (Image_ProgressFill)
    {
        Image_ProgressFill->SetRenderScale(FVector2D(0.f, 1.f));
    }

    if (TextBlock_CraftingTime)
    {
        TextBlock_CraftingTime->SetText(FText::FromString(TEXT("0.0s / 0.0s")));
    }
}

void UCraftingPanelWidget::UpdateProgressBar()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const float Elapsed = World->GetTimeSeconds() - CraftStartTime;
    const float Percent = FMath::Clamp(Elapsed / CraftTotalDuration, 0.f, 1.f);

    if (Image_ProgressFill)
    {
        Image_ProgressFill->SetRenderScale(FVector2D(Percent, 1.f));
    }

    if (TextBlock_CraftingTime)
    {
        const int32 ElapsedTenths = FMath::RoundToInt(Elapsed * 10.f);
        const int32 DurationTenths = FMath::RoundToInt(CraftTotalDuration * 10.f);
        if (ElapsedTenths != CachedElapsedTenths || DurationTenths != CachedDurationTenths)
        {
            CachedElapsedTenths = ElapsedTenths;
            CachedDurationTenths = DurationTenths;
            TextBlock_CraftingTime->SetText(FText::FromString(
                FString::Printf(TEXT("%.1fs / %.1fs"), Elapsed, CraftTotalDuration)));
        }
    }
}
