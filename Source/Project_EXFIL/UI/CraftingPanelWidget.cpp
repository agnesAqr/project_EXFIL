// Copyright Project EXFIL. All Rights Reserved.

#include "UI/CraftingPanelWidget.h"
#include "CoreMinimal.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "UI/CraftingRecipeWidget.h"

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
    if (ViewModel.IsValid())
    {
        if (RecipeListDeltaHandle.IsValid())
        {
            ViewModel->OnRecipeListDeltaChanged.Remove(RecipeListDeltaHandle);
        }
        if (ProgressStartedHandle.IsValid())
        {
            ViewModel->OnCraftingProgressStarted.Remove(ProgressStartedHandle);
        }
        if (ProgressStoppedHandle.IsValid())
        {
            ViewModel->OnCraftingProgressStopped.Remove(ProgressStoppedHandle);
        }
        if (CraftStartFailedHandle.IsValid())
        {
            ViewModel->OnCraftStartFailed.Remove(CraftStartFailedHandle);
        }
    }

    Super::NativeDestruct();
}

void UCraftingPanelWidget::SetViewModel(UCraftingViewModel* InViewModel)
{
    if (ViewModel.IsValid())
    {
        if (RecipeListDeltaHandle.IsValid())
        {
            ViewModel->OnRecipeListDeltaChanged.Remove(RecipeListDeltaHandle);
            RecipeListDeltaHandle.Reset();
        }
        if (ProgressStartedHandle.IsValid())
        {
            ViewModel->OnCraftingProgressStarted.Remove(ProgressStartedHandle);
            ProgressStartedHandle.Reset();
        }
        if (ProgressStoppedHandle.IsValid())
        {
            ViewModel->OnCraftingProgressStopped.Remove(ProgressStoppedHandle);
            ProgressStoppedHandle.Reset();
        }
        if (CraftStartFailedHandle.IsValid())
        {
            ViewModel->OnCraftStartFailed.Remove(CraftStartFailedHandle);
            CraftStartFailedHandle.Reset();
        }
    }

    ViewModel = InViewModel;
    bRecipesInitialized = false;
    bHasPendingRecipeDelta = false;
    PendingRecipeDelta = FCraftingRecipeListDeltaViewData();

    if (!InViewModel)
    {
        if (ScrollBox_Recipes)
        {
            ScrollBox_Recipes->ClearChildren();
        }
        RecipeWidgetCache.Empty();
        return;
    }

    RecipeListDeltaHandle = InViewModel->OnRecipeListDeltaChanged.AddUObject(
        this, &UCraftingPanelWidget::HandleRecipeListDeltaChanged);
    ProgressStartedHandle = InViewModel->OnCraftingProgressStarted.AddUObject(
        this, &UCraftingPanelWidget::HandleCraftingProgressStarted);
    ProgressStoppedHandle = InViewModel->OnCraftingProgressStopped.AddUObject(
        this, &UCraftingPanelWidget::HandleCraftingProgressStopped);
    CraftStartFailedHandle = InViewModel->OnCraftStartFailed.AddUObject(
        this, &UCraftingPanelWidget::HandleCraftStartFailed);
}

void UCraftingPanelWidget::NotifyPanelShown()
{
    bPanelVisible = true;

    if (!bRecipesInitialized)
    {
        RefreshRecipeList();
        return;
    }

    FlushPendingRecipeDelta();
}

void UCraftingPanelWidget::NotifyPanelHidden()
{
    bPanelVisible = false;
}

void UCraftingPanelWidget::RefreshRecipeList()
{
    if (!ViewModel.IsValid())
    {
        return;
    }

    FCraftingRecipeListDeltaViewData InitialDelta;
    if (ViewModel->BuildInitialRecipeListDelta(InitialDelta))
    {
        ApplyRecipeListDelta(InitialDelta);
        bRecipesInitialized = true;
        bHasPendingRecipeDelta = false;
        PendingRecipeDelta = FCraftingRecipeListDeltaViewData();
    }
}

void UCraftingPanelWidget::HandleRecipeListDeltaChanged(
    const FCraftingRecipeListDeltaViewData& Delta)
{
    if (!bPanelVisible || !bRecipesInitialized)
    {
        PendingRecipeDelta.bReset = PendingRecipeDelta.bReset || Delta.bReset;
        PendingRecipeDelta.UpsertRecipes.Append(Delta.UpsertRecipes);
        bHasPendingRecipeDelta = true;
        return;
    }

    ApplyRecipeListDelta(Delta);
}

void UCraftingPanelWidget::ApplyRecipeListDelta(
    const FCraftingRecipeListDeltaViewData& Delta)
{
    if (!ScrollBox_Recipes || !RecipeWidgetClass)
    {
        return;
    }

    if (Delta.bReset)
    {
        ScrollBox_Recipes->ClearChildren();
        RecipeWidgetCache.Empty();
    }

    for (const FCraftingRecipeViewData& RecipeViewData : Delta.UpsertRecipes)
    {
        UCraftingRecipeWidget* RecipeWidget = nullptr;
        if (UCraftingRecipeWidget** FoundWidget =
                RecipeWidgetCache.Find(RecipeViewData.RecipeID))
        {
            RecipeWidget = *FoundWidget;
        }

        if (!RecipeWidget)
        {
            RecipeWidget = CreateWidget<UCraftingRecipeWidget>(this, RecipeWidgetClass);
            if (!RecipeWidget)
            {
                continue;
            }

            RecipeWidget->OnRecipeActionRequested.BindUObject(
                this, &UCraftingPanelWidget::OnRecipeActionRequested);
            ScrollBox_Recipes->AddChild(RecipeWidget);
            RecipeWidgetCache.Add(RecipeViewData.RecipeID, RecipeWidget);
        }

        RecipeWidget->SetRecipe(RecipeViewData);
    }
}

void UCraftingPanelWidget::FlushPendingRecipeDelta()
{
    if (!bPanelVisible || !bHasPendingRecipeDelta)
    {
        return;
    }

    ApplyRecipeListDelta(PendingRecipeDelta);
    PendingRecipeDelta = FCraftingRecipeListDeltaViewData();
    bHasPendingRecipeDelta = false;
}

void UCraftingPanelWidget::HandleCraftingProgressStarted(float Duration)
{
    if (Image_ProgressFill)
    {
        Image_ProgressFill->SetRenderScale(FVector2D(0.f, 1.f));
    }

    if (TextBlock_CraftingTime)
    {
        TextBlock_CraftingTime->SetText(FText::FromString(
            FString::Printf(TEXT("0.0s / %.1fs"), Duration)));
    }

    if (Border_CraftProgress)
    {
        Border_CraftProgress->SetVisibility(ESlateVisibility::Visible);
    }

    if (TextBlock_CraftingLabel)
    {
        TextBlock_CraftingLabel->SetText(NSLOCTEXT("Crafting", "CraftingLabel", "Crafting..."));
    }

    StartProgressTimer(Duration);
}

void UCraftingPanelWidget::HandleCraftingProgressStopped()
{
    StopProgressTimer();

    if (Border_CraftProgress)
    {
        Border_CraftProgress->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UCraftingPanelWidget::HandleCraftStartFailed(FName RecipeID)
{
    HandleCraftingProgressStopped();
}

void UCraftingPanelWidget::OnRecipeActionRequested(
    FName RecipeID, bool bIsCurrentRecipe)
{
    if (!ViewModel.IsValid())
    {
        return;
    }

    if (bIsCurrentRecipe)
    {
        ViewModel->RequestCancelCraft();
    }
    else
    {
        ViewModel->RequestStartCraft(RecipeID);
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
    if (!World || CraftTotalDuration <= 0.f)
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
