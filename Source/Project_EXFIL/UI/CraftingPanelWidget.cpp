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
#include "Data/EXFILItemTypes.h"
#include "UI/CraftingRecipeWidget.h"
#include "Engine/GameInstance.h"

void UCraftingPanelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // 초기 진행 바 숨김
    if (Border_CraftProgress)
    {
        Border_CraftProgress->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UCraftingPanelWidget::SetupCrafting(UCraftingComponent* InCraftingComp,
                                          UInventoryComponent* InInventoryComp)
{
    CraftingComp = InCraftingComp;
    InventoryComp = InInventoryComp;

    if (InCraftingComp)
    {
        InCraftingComp->OnCraftingStateChanged.AddDynamic(
            this, &UCraftingPanelWidget::OnCraftingStateChanged);
        InCraftingComp->OnCraftingCompleted.AddDynamic(
            this, &UCraftingPanelWidget::OnCraftingCompleted);
    }

    if (InInventoryComp)
    {
        InInventoryComp->OnInventoryUpdated.AddUObject(
            this, &UCraftingPanelWidget::OnInventoryChanged);
    }
}

void UCraftingPanelWidget::OnInventoryChanged(const TSet<int32>& /*DirtyIndices*/)
{
    RefreshRecipeList();
}

void UCraftingPanelWidget::RefreshRecipeList()
{
    if (!ScrollBox_Recipes || !RecipeWidgetClass)
    {
        return;
    }

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

    UCraftingComponent* Crafting = CraftingComp.Get();
    UInventoryComponent* Inventory = InventoryComp.Get();

    // 초회만 위젯 생성, 이후는 상태만 갱신
    if (!bRecipesInitialized)
    {
        bRecipesInitialized = true;

        for (const FName& RecipeID : Sub->GetAllRecipeIDs())
        {
            UCraftingRecipeWidget* RecipeWidget =
                CreateWidget<UCraftingRecipeWidget>(this, RecipeWidgetClass);
            if (!RecipeWidget)
            {
                continue;
            }

            const bool bCanCraft = Crafting ? Crafting->CanCraft(RecipeID) : false;
            RecipeWidget->SetRecipe(RecipeID, Inventory, bCanCraft);

            RecipeWidget->OnRecipeClicked.BindUObject(
                this, &UCraftingPanelWidget::OnRecipeSelected);

            ScrollBox_Recipes->AddChild(RecipeWidget);
            RecipeWidgetCache.Add(RecipeID, RecipeWidget);
        }
    }
    else
    {
        // 캐시된 위젯의 상태만 갱신 — ClearChildren/CreateWidget 호출 없음
        for (auto& Pair : RecipeWidgetCache)
        {
            const bool bCanCraft = Crafting ? Crafting->CanCraft(Pair.Key) : false;
            Pair.Value->SetRecipe(Pair.Key, Inventory, bCanCraft);
        }
    }
}

// ─── 내부 콜백 ────────────────────────────────────────────────────────────────

void UCraftingPanelWidget::OnCraftingStateChanged(bool bIsCrafting, float RemainingTime)
{
    if (bIsCrafting)
    {
        // 프로그레스 바를 0%로 초기화한 뒤 표시 (이전 상태가 보이는 것 방지)
        if (Image_ProgressFill)
        {
            Image_ProgressFill->SetRenderScale(FVector2D(0.f, 1.f));
        }
        if (TextBlock_CraftingTime)
        {
            TextBlock_CraftingTime->SetText(FText::FromString(
                FString::Printf(TEXT("0.0s / %.1fs"), RemainingTime)));
        }

        // 진행 바 표시
        if (Border_CraftProgress)
        {
            Border_CraftProgress->SetVisibility(ESlateVisibility::Visible);
        }

        // 크래프팅 중인 레시피 이름 표시
        if (TextBlock_CraftingLabel)
        {
            UCraftingComponent* Crafting = CraftingComp.Get();
            FName RecipeID = Crafting ? Crafting->GetCurrentRecipeID() : NAME_None;
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

    // 버튼 상태 갱신
    RefreshRecipeList();
}

void UCraftingPanelWidget::OnCraftingCompleted(FName CompletedRecipeID)
{
    // RefreshRecipeList는 OnCraftingStateChanged(false) 이후 자동 호출됨
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
        // 이미 크래프팅 중이면 취소
        Crafting->CancelCraft();
    }
    else
    {
        Crafting->StartCraft(ClickedRecipeID);
    }
}

// ─── 프로그레스 바 ─────────────────────────────────────────────────────────────

void UCraftingPanelWidget::StartProgressTimer(float Duration)
{
    UWorld* World = GetWorld();
    CraftStartTime = World ? World->GetTimeSeconds() : 0.f;
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
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ProgressTimerHandle);
    }

    // 프로그레스 바 리셋
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

    // 소수점 1자리 반올림값이 변했을 때만 SetText (0.05초 타이머 → 불필요한 문자열 생성 방지)
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
