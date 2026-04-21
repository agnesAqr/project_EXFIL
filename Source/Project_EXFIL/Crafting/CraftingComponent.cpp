// Copyright Project EXFIL. All Rights Reserved.

#include "Crafting/CraftingComponent.h"
#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Inventory/InventoryComponent.h"
#include "Data/ItemDataSubsystem.h"
#include "Data/EXFILItemTypes.h"
#include "World/WorldItem.h"
#include "Project_EXFIL.h"

UCraftingComponent::UCraftingComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UCraftingComponent::BeginPlay()
{
    Super::BeginPlay();

    if (AActor* Owner = GetOwner())
    {
        CachedInventoryComp = Owner->FindComponentByClass<UInventoryComponent>();
    }

    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            CachedItemSub = GI->GetSubsystem<UItemDataSubsystem>();
        }
    }
}

// ========== Replication ==========

void UCraftingComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(UCraftingComponent, bIsCrafting, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UCraftingComponent, CurrentRecipeID, COND_OwnerOnly);
}

void UCraftingComponent::OnRep_CraftingState()
{
    UE_LOG(LogProject_EXFIL, Log,
        TEXT("[CraftingRep][Client] OnRep_CraftingState bIsCrafting=%s RecipeID=%s"),
        bIsCrafting ? TEXT("true") : TEXT("false"),
        *CurrentRecipeID.ToString());

    if (bIsCrafting)
    {
        UItemDataSubsystem* Sub = GetItemDataSubsystem();
        const FCraftingRecipe* Recipe = Sub ? Sub->GetCraftingRecipe(CurrentRecipeID) : nullptr;
        const float Duration = Recipe ? Recipe->CraftDuration : 0.f;
        OnCraftingStateChanged.Broadcast(true, Duration);
    }
    else
    {
        OnCraftingStateChanged.Broadcast(false, 0.f);
    }
}

// ========== Request API ==========

void UCraftingComponent::RequestStartCraft(FName RecipeID)
{
    if (RecipeID.IsNone())
    {
        NotifyCraftStartFailed(RecipeID);
        return;
    }

    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        Server_RequestStartCraft(RecipeID);
        return;
    }

    if (!StartCraft_Internal(RecipeID))
    {
        NotifyCraftStartFailed(RecipeID);
    }
}

void UCraftingComponent::RequestCancelCraft()
{
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        Server_RequestCancelCraft();
        return;
    }

    CancelCraft_Internal();
}

// ========== Server RPCs ==========

void UCraftingComponent::Server_RequestStartCraft_Implementation(FName RecipeID)
{
    if (RecipeID.IsNone())
    {
        NotifyCraftStartFailed(RecipeID);
        return;
    }

    if (!StartCraft_Internal(RecipeID))
    {
        NotifyCraftStartFailed(RecipeID);
    }
}

void UCraftingComponent::Server_RequestCancelCraft_Implementation()
{
    CancelCraft_Internal();
}

void UCraftingComponent::Client_NotifyCraftStartFailed_Implementation(FName RecipeID)
{
    OnCraftStartFailed.Broadcast(RecipeID);
}

// ========== Query API ==========

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
        if (InvComp->GetItemCountByID_Cached(Ing.ItemDataID) < Ing.RequiredCount)
        {
            return false;
        }
    }

    return true;
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

// ========== Internal Write API ==========

bool UCraftingComponent::StartCraft_Internal(FName RecipeID)
{
    checkf(GetOwner() && GetOwner()->HasAuthority(),
        TEXT("StartCraft_Internal must run on the server."));

    if (bIsCrafting)
    {
        UE_LOG(LogProject_EXFIL, Warning, TEXT("CraftingComponent: already crafting."));
        return false;
    }

    if (!CanCraft(RecipeID))
    {
        UE_LOG(LogProject_EXFIL, Warning,
            TEXT("CraftingComponent: CanCraft failed for '%s'"), *RecipeID.ToString());
        return false;
    }

    UItemDataSubsystem* Sub = GetItemDataSubsystem();
    const FCraftingRecipe* Recipe = Sub ? Sub->GetCraftingRecipe(RecipeID) : nullptr;
    UInventoryComponent* InvComp = GetInventoryComp();
    if (!Recipe || !InvComp)
    {
        return false;
    }

    ConsumedIngredients.Empty();
    for (const FCraftingIngredient& Ing : Recipe->Ingredients)
    {
        if (!InvComp->ConsumeItemByID_Internal(Ing.ItemDataID, Ing.RequiredCount))
        {
            for (const FConsumedIngredient& C : ConsumedIngredients)
            {
                InvComp->AddItemByID_Internal(C.ItemDataID, C.Count);
            }
            ConsumedIngredients.Empty();
            return false;
        }

        ConsumedIngredients.Add({ Ing.ItemDataID, Ing.RequiredCount });
    }

    bIsCrafting = true;
    CurrentRecipeID = RecipeID;

    GetWorld()->GetTimerManager().SetTimer(
        CraftTimerHandle,
        this,
        &UCraftingComponent::OnCraftTimerComplete,
        Recipe->CraftDuration,
        false);

    UE_LOG(LogProject_EXFIL, Log, TEXT("CraftingComponent: '%s' started (%.1fs)"),
        *RecipeID.ToString(), Recipe->CraftDuration);
    return true;
}

void UCraftingComponent::CancelCraft_Internal()
{
    checkf(GetOwner() && GetOwner()->HasAuthority(),
        TEXT("CancelCraft_Internal must run on the server."));

    if (!bIsCrafting)
    {
        return;
    }

    GetWorld()->GetTimerManager().ClearTimer(CraftTimerHandle);

    if (UInventoryComponent* InvComp = GetInventoryComp())
    {
        for (const FConsumedIngredient& C : ConsumedIngredients)
        {
            InvComp->AddItemByID_Internal(C.ItemDataID, C.Count);
        }
    }
    ConsumedIngredients.Empty();

    bIsCrafting = false;
    const FName CancelledRecipe = CurrentRecipeID;
    CurrentRecipeID = NAME_None;

    UE_LOG(LogProject_EXFIL, Log, TEXT("CraftingComponent: '%s' canceled"),
        *CancelledRecipe.ToString());
}

void UCraftingComponent::NotifyCraftStartFailed(FName RecipeID)
{
    OnCraftStartFailed.Broadcast(RecipeID);

    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (OwnerPawn && OwnerPawn->IsLocallyControlled())
    {
        return;
    }

    Client_NotifyCraftStartFailed(RecipeID);
}

// ========== Server-only Completion ==========

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
        const bool bAdded =
            InvComp->AddItemByID_Internal(Recipe->ResultItemID, Recipe->ResultCount);
        if (!bAdded)
        {
            UE_LOG(LogProject_EXFIL, Warning,
                TEXT("CraftingComponent: Result '%s' inventory full, dropping to world"),
                *Recipe->ResultItemID.ToString());

            AActor* Owner = GetOwner();
            UWorld* World = Owner ? Owner->GetWorld() : nullptr;
            if (World && Owner)
            {
                const FVector SpawnLoc =
                    Owner->GetActorLocation() + Owner->GetActorForwardVector() * 80.f;
                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = Owner;
                SpawnParams.SpawnCollisionHandlingOverride =
                    ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

                AWorldItem* DroppedItem = World->SpawnActor<AWorldItem>(
                    AWorldItem::StaticClass(), SpawnLoc, FRotator::ZeroRotator, SpawnParams);
                if (DroppedItem)
                {
                    DroppedItem->InitializeItem(Recipe->ResultItemID, Recipe->ResultCount);
                }
            }
        }
    }

    const FName CompletedRecipe = CurrentRecipeID;
    bIsCrafting = false;
    CurrentRecipeID = NAME_None;
    ConsumedIngredients.Empty();

    OnCraftingCompleted.Broadcast(CompletedRecipe);

    UE_LOG(LogProject_EXFIL, Log, TEXT("CraftingComponent: '%s' completed"),
        *CompletedRecipe.ToString());
}

UInventoryComponent* UCraftingComponent::GetInventoryComp() const
{
    return CachedInventoryComp.Get();
}

UItemDataSubsystem* UCraftingComponent::GetItemDataSubsystem() const
{
    return CachedItemSub.Get();
}
