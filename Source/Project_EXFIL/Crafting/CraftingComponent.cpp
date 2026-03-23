// Copyright Project EXFIL. All Rights Reserved.

#include "Crafting/CraftingComponent.h"
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Inventory/InventoryComponent.h"
#include "Data/ItemDataSubsystem.h"
#include "Data/EXFILItemTypes.h"
#include "Project_EXFIL.h"

UCraftingComponent::UCraftingComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCraftingComponent::BeginPlay()
{
    Super::BeginPlay();
}

// ─── 공개 API ─────────────────────────────────────────────────────────────────

bool UCraftingComponent::CanCraft(FName RecipeID) const
{
    UItemDataSubsystem* Sub = GetItemDataSubsystem();
    if (!Sub)
    {
        return false;
    }

    const FCraftingRecipe* Recipe = Sub->GetCraftingRecipe(RecipeID);
    if (!Recipe)
    {
        return false;
    }

    UInventoryComponent* InvComp = GetInventoryComp();
    if (!InvComp)
    {
        return false;
    }

    for (const FCraftingIngredient& Ing : Recipe->Ingredients)
    {
        if (InvComp->GetItemCountByID(Ing.ItemDataID) < Ing.RequiredCount)
        {
            return false;
        }
    }
    return true;
}

bool UCraftingComponent::StartCraft(FName RecipeID)
{
    if (bIsCrafting)
    {
        UE_LOG(LogProject_EXFIL, Warning, TEXT("CraftingComponent: 이미 크래프팅 중입니다."));
        return false;
    }

    if (!CanCraft(RecipeID))
    {
        UE_LOG(LogProject_EXFIL, Warning,
               TEXT("CraftingComponent: CanCraft 실패 — RecipeID '%s'"), *RecipeID.ToString());
        return false;
    }

    UItemDataSubsystem* Sub = GetItemDataSubsystem();
    const FCraftingRecipe* Recipe = Sub->GetCraftingRecipe(RecipeID);
    UInventoryComponent* InvComp = GetInventoryComp();

    // 재료 소비 + 스냅샷 저장 (취소 시 복구용)
    ConsumedIngredients.Empty();
    for (const FCraftingIngredient& Ing : Recipe->Ingredients)
    {
        if (!InvComp->ConsumeItemByID(Ing.ItemDataID, Ing.RequiredCount))
        {
            // 부분 소비 후 실패 — 이미 소비된 재료 복구
            for (const FConsumedIngredient& C : ConsumedIngredients)
            {
                InvComp->TryAddItemByID(C.ItemDataID, C.Count);
            }
            ConsumedIngredients.Empty();
            return false;
        }
        ConsumedIngredients.Add({ Ing.ItemDataID, Ing.RequiredCount });
    }

    bIsCrafting = true;
    CurrentRecipeID = RecipeID;

    // 타이머 시작
    GetWorld()->GetTimerManager().SetTimer(
        CraftTimerHandle,
        this,
        &UCraftingComponent::OnCraftTimerComplete,
        Recipe->CraftDuration,
        false);

    OnCraftingStateChanged.Broadcast(true, Recipe->CraftDuration);

    UE_LOG(LogProject_EXFIL, Log, TEXT("CraftingComponent: '%s' 크래프팅 시작 (%.1fs)"),
           *RecipeID.ToString(), Recipe->CraftDuration);
    return true;
}

void UCraftingComponent::CancelCraft()
{
    if (!bIsCrafting)
    {
        return;
    }

    // 타이머 클리어
    GetWorld()->GetTimerManager().ClearTimer(CraftTimerHandle);

    // 소비한 재료 복구
    if (UInventoryComponent* InvComp = GetInventoryComp())
    {
        for (const FConsumedIngredient& C : ConsumedIngredients)
        {
            InvComp->TryAddItemByID(C.ItemDataID, C.Count);
        }
    }
    ConsumedIngredients.Empty();

    bIsCrafting = false;
    const FName CancelledRecipe = CurrentRecipeID;
    CurrentRecipeID = NAME_None;

    OnCraftingStateChanged.Broadcast(false, 0.f);

    UE_LOG(LogProject_EXFIL, Log, TEXT("CraftingComponent: '%s' 크래프팅 취소 — 재료 복구"),
           *CancelledRecipe.ToString());
}

TArray<FName> UCraftingComponent::GetAvailableRecipes() const
{
    UItemDataSubsystem* Sub = GetItemDataSubsystem();
    if (!Sub)
    {
        return {};
    }
    return Sub->GetAllRecipeIDs();
}

// ─── 내부 ──────────────────────────────────────────────────────────────────────

void UCraftingComponent::OnCraftTimerComplete()
{
    if (!bIsCrafting)
    {
        return;
    }

    UItemDataSubsystem* Sub = GetItemDataSubsystem();
    const FCraftingRecipe* Recipe = Sub ? Sub->GetCraftingRecipe(CurrentRecipeID) : nullptr;
    UInventoryComponent* InvComp = GetInventoryComp();

    if (Recipe && InvComp)
    {
        const bool bAdded = InvComp->TryAddItemByID(Recipe->ResultItemID, Recipe->ResultCount);
        if (!bAdded)
        {
            UE_LOG(LogProject_EXFIL, Warning,
                   TEXT("CraftingComponent: 결과물 '%s' 추가 실패 (인벤토리 공간 부족)"),
                   *Recipe->ResultItemID.ToString());
        }
    }

    const FName CompletedRecipe = CurrentRecipeID;
    bIsCrafting = false;
    CurrentRecipeID = NAME_None;
    ConsumedIngredients.Empty();

    OnCraftingCompleted.Broadcast(CompletedRecipe);
    OnCraftingStateChanged.Broadcast(false, 0.f);

    UE_LOG(LogProject_EXFIL, Log, TEXT("CraftingComponent: '%s' 크래프팅 완료"),
           *CompletedRecipe.ToString());
}

UInventoryComponent* UCraftingComponent::GetInventoryComp() const
{
    if (AActor* Owner = GetOwner())
    {
        return Owner->FindComponentByClass<UInventoryComponent>();
    }
    return nullptr;
}

UItemDataSubsystem* UCraftingComponent::GetItemDataSubsystem() const
{
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            return GI->GetSubsystem<UItemDataSubsystem>();
        }
    }
    return nullptr;
}
