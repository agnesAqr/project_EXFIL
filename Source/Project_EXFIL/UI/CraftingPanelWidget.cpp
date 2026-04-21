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
#include "Project_EXFIL.h"

namespace CraftingPanelDebug
{
    static FString FormatItemIDs(const TSet<FName>& ItemIDs)
    {
        TArray<FName> SortedIDs = ItemIDs.Array();
        SortedIDs.Sort(FNameLexicalLess());

        TArray<FString> Parts;
        const int32 PreviewCount = FMath::Min(SortedIDs.Num(), 16);
        Parts.Reserve(PreviewCount + 1);

        for (int32 i = 0; i < PreviewCount; ++i)
        {
            Parts.Add(SortedIDs[i].ToString());
        }

        if (SortedIDs.Num() > PreviewCount)
        {
            Parts.Add(TEXT("..."));
        }

        return FString::Printf(TEXT("[%s]"), *FString::Join(Parts, TEXT(",")));
    }
}

void UCraftingPanelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

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

    UE_LOG(LogProject_EXFIL, Log,
        TEXT("[CraftingPanel] SetupCrafting BoundInventory=%s BoundCrafting=%s"),
        InInventoryComp ? TEXT("true") : TEXT("false"),
        InCraftingComp ? TEXT("true") : TEXT("false"));
}

void UCraftingPanelWidget::NotifyPanelShown()
{
    bPanelVisible = true;

    UE_LOG(LogProject_EXFIL, Log,
        TEXT("[CraftingPanel] NotifyPanelShown RecipesInitialized=%s PendingDirtyCount=%d"),
        bRecipesInitialized ? TEXT("true") : TEXT("false"),
        PendingDirtyItemIDs.Num());

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
    UE_LOG(LogProject_EXFIL, Log, TEXT("[CraftingPanel] NotifyPanelHidden"));
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

        UE_LOG(LogProject_EXFIL, Log,
            TEXT("[CraftingPanel] QueueItemCountChange Visible=false Incoming=%d Pending=%d IDs=%s"),
            ChangedItemDataIDs.Num(),
            PendingDirtyItemIDs.Num(),
            *CraftingPanelDebug::FormatItemIDs(PendingDirtyItemIDs));
        return;
    }

    UE_LOG(LogProject_EXFIL, Log,
        TEXT("[CraftingPanel] HandleItemCountChange Visible=true Changed=%d IDs=%s"),
        ChangedItemDataIDs.Num(),
        *CraftingPanelDebug::FormatItemIDs(ChangedItemDataIDs));

    RefreshRecipesByItemChanges(ChangedItemDataIDs);
}

void UCraftingPanelWidget::RefreshRecipeList()
{
    if (bRecipesInitialized || !ScrollBox_Recipes || !RecipeWidgetClass)
    {
        return;
    }

    UItemDataSubsystem* Subsystem = nullptr;
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            Subsystem = GI->GetSubsystem<UItemDataSubsystem>();
        }
    }

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
        RecipeWidget->SetRecipe(RecipeID, Inventory, bCanCraft);
        RecipeWidget->SetCraftingInProgress(Crafting ? Crafting->IsCrafting() : false);
        RecipeWidget->OnRecipeClicked.BindUObject(
            this, &UCraftingPanelWidget::OnRecipeSelected);

        ScrollBox_Recipes->AddChild(RecipeWidget);
        RecipeWidgetCache.Add(RecipeID, RecipeWidget);
    }

    UE_LOG(LogProject_EXFIL, Log,
        TEXT("[CraftingPanel] RefreshRecipeList InitialBuild RecipeWidgetCount=%d DependencyKeys=%d"),
        RecipeWidgetCache.Num(),
        IngredientToRecipeIDs.Num());
}

void UCraftingPanelWidget::BuildRecipeDependencyIndex()
{
    IngredientToRecipeIDs.Empty();

    UItemDataSubsystem* Subsystem = nullptr;
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            Subsystem = GI->GetSubsystem<UItemDataSubsystem>();
        }
    }

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

    UE_LOG(LogProject_EXFIL, Log,
        TEXT("[CraftingPanel] BuildRecipeDependencyIndex IngredientKeyCount=%d"),
        IngredientToRecipeIDs.Num());
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

    UE_LOG(LogProject_EXFIL, Log,
        TEXT("[CraftingPanel] RefreshRecipesByItemChanges Changed=%d AffectedRecipes=%d IDs=%s"),
        ChangedItemDataIDs.Num(),
        AffectedRecipeIDs.Num(),
        *CraftingPanelDebug::FormatItemIDs(ChangedItemDataIDs));

    for (const FName& RecipeID : AffectedRecipeIDs)
    {
        UCraftingRecipeWidget* const* FoundWidget = RecipeWidgetCache.Find(RecipeID);
        if (!FoundWidget || !*FoundWidget)
        {
            continue;
        }

        const bool bCanCraft = Crafting->CanCraft(RecipeID);
        (*FoundWidget)->SetRecipe(RecipeID, Inventory, bCanCraft);
        (*FoundWidget)->SetCraftingInProgress(Crafting->IsCrafting());
    }
}

void UCraftingPanelWidget::FlushPendingRecipeRefresh()
{
    if (!bPanelVisible || !bHasPendingRecipeRefresh || PendingDirtyItemIDs.Num() == 0)
    {
        return;
    }

    UE_LOG(LogProject_EXFIL, Log,
        TEXT("[CraftingPanel] FlushPendingRecipeRefresh Pending=%d IDs=%s"),
        PendingDirtyItemIDs.Num(),
        *CraftingPanelDebug::FormatItemIDs(PendingDirtyItemIDs));

    RefreshRecipesByItemChanges(PendingDirtyItemIDs);
    PendingDirtyItemIDs.Reset();
    bHasPendingRecipeRefresh = false;
}

void UCraftingPanelWidget::OnCraftingStateChanged(bool bIsCrafting, float RemainingTime)
{
    UE_LOG(LogProject_EXFIL, Log,
        TEXT("[CraftingPanel] OnCraftingStateChanged bIsCrafting=%s Remaining=%.2f"),
        bIsCrafting ? TEXT("true") : TEXT("false"),
        RemainingTime);

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
            Pair.Value->SetCraftingInProgress(bIsCrafting);
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
