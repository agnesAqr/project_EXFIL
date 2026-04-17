// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventoryViewModel.h"
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Inventory/InventoryComponent.h"
#include "Data/ItemDataSubsystem.h"

void UInventoryViewModel::Initialize(UInventoryComponent* InInventoryComponent)
{
    if (!InInventoryComponent)
    {
        return;
    }

    InventoryComp = InInventoryComponent;

    // Cache the grid dimensions for the view model.
    UE_MVVM_SET_PROPERTY_VALUE(GridWidth, InInventoryComponent->GridWidth);
    UE_MVVM_SET_PROPERTY_VALUE(GridHeight, InInventoryComponent->GridHeight);

    // Create one slot view model per grid cell.
    const int32 TotalSlots = GridWidth * GridHeight;
    SlotViewModels.SetNum(TotalSlots);

    for (int32 i = 0; i < TotalSlots; ++i)
    {
        UInventorySlotViewModel* SlotVM = NewObject<UInventorySlotViewModel>(this);
        SlotVM->SetGridPosition(FIntPoint(i % GridWidth, i / GridWidth));
        SlotViewModels[i] = SlotVM;
    }

    // Bind the model delegate once and mirror the initial state immediately.
    InInventoryComponent->OnInventoryUpdated.AddUObject(
        this, &UInventoryViewModel::HandleInventoryUpdated);
    RefreshAllSlots();
}

UInventorySlotViewModel* UInventoryViewModel::GetSlotAt(FIntPoint Position) const
{
    const int32 Index = PositionToIndex(Position);
    if (SlotViewModels.IsValidIndex(Index))
    {
        return SlotViewModels[Index];
    }
    return nullptr;
}

const TArray<UInventorySlotViewModel*>& UInventoryViewModel::GetAllSlots() const
{
    return SlotViewModels;
}

void UInventoryViewModel::RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated)
{
    if (!InventoryComp.IsValid())
    {
        return;
    }
    InventoryComp->RequestMoveItem(ItemInstanceID, NewPosition, bNewRotated);
}

FIntPoint UInventoryViewModel::GetItemRootPosition(FGuid ItemInstanceID) const
{
    if (!InventoryComp.IsValid())
    {
        return FIntPoint(-1, -1);
    }
    FInventoryItemInstance Item;
    if (InventoryComp->GetItemByID(ItemInstanceID, Item))
    {
        return Item.RootPosition;
    }
    return FIntPoint(-1, -1);
}

FItemSize UInventoryViewModel::GetItemEffectiveSize(FGuid ItemInstanceID) const
{
    if (!InventoryComp.IsValid())
    {
        return FItemSize(1, 1);
    }
    FInventoryItemInstance Item;
    if (InventoryComp->GetItemByID(ItemInstanceID, Item))
    {
        return Item.GetEffectiveSize();
    }
    return FItemSize(1, 1);
}

bool UInventoryViewModel::IsItemRotated(FGuid ItemInstanceID) const
{
    if (!InventoryComp.IsValid())
    {
        return false;
    }

    FInventoryItemInstance Item;
    if (InventoryComp->GetItemByID(ItemInstanceID, Item))
    {
        return Item.bIsRotated && !Item.ItemSize.IsSquare();
    }

    return false;
}

void UInventoryViewModel::RequestRemoveItem(FGuid ItemInstanceID)
{
    if (!InventoryComp.IsValid())
    {
        return;
    }
    InventoryComp->RequestRemoveItem(ItemInstanceID);
}

void UInventoryViewModel::HandleInventoryUpdated(const TSet<int32>& /*DirtyIndices*/)
{
    RefreshAllSlots();
}

void UInventoryViewModel::RefreshAllSlots()
{
    TSet<int32> AllIndices;
    AllIndices.Reserve(SlotViewModels.Num());
    for (int32 i = 0; i < SlotViewModels.Num(); ++i)
    {
        AllIndices.Add(i);
    }
    RefreshDirtySlots(AllIndices);
}

void UInventoryViewModel::RefreshDirtySlots(const TSet<int32>& DirtyIndices)
{
    if (!InventoryComp.IsValid())
    {
        return;
    }

    // Resolve item icons through the shared item data subsystem.
    UItemDataSubsystem* ItemSub = nullptr;
    if (UWorld* World = InventoryComp->GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            ItemSub = GI->GetSubsystem<UItemDataSubsystem>();
        }
    }

    for (int32 i : DirtyIndices)
    {
        if (!SlotViewModels.IsValidIndex(i))
        {
            continue;
        }

        UInventorySlotViewModel* SlotVM = SlotViewModels[i];
        if (!SlotVM)
        {
            continue;
        }

        const FIntPoint Position(i % GridWidth, i / GridWidth);
        FInventoryItemInstance ItemInstance;
        const bool bHasItem = InventoryComp->GetItemAt(Position, ItemInstance);

        if (bHasItem)
        {
            const bool bIsRoot = (ItemInstance.RootPosition == Position);
            const FItemSize EffectiveSize = ItemInstance.GetEffectiveSize();
            SlotVM->SetEmpty(false);
            SlotVM->SetItemDataID(ItemInstance.ItemDataID);
            SlotVM->SetStackCount(ItemInstance.StackCount);
            SlotVM->SetIsRootSlot(bIsRoot);
            SlotVM->SetItemInstanceID(ItemInstance.InstanceID);
            SlotVM->SetRotated(ItemInstance.bIsRotated && !ItemInstance.ItemSize.IsSquare());

            if (bIsRoot)
            {
                SlotVM->SetItemSizeX(EffectiveSize.Width);
                SlotVM->SetItemSizeY(EffectiveSize.Height);
            }
            else
            {
                SlotVM->SetItemSizeX(1);
                SlotVM->SetItemSizeY(1);
            }

            if (ItemSub)
            {
                const FItemData* ItemData = ItemSub->GetItemData(ItemInstance.ItemDataID);
                SlotVM->SetIcon(ItemData ? ItemData->Icon : TSoftObjectPtr<UTexture2D>());
            }
        }
        else
        {
            SlotVM->SetEmpty(true);
            SlotVM->SetItemDataID(NAME_None);
            SlotVM->SetStackCount(0);
            SlotVM->SetIsRootSlot(false);
            SlotVM->SetItemInstanceID(FGuid());
            SlotVM->SetItemSizeX(1);
            SlotVM->SetItemSizeY(1);
            SlotVM->SetRotated(false);
            SlotVM->SetIcon(TSoftObjectPtr<UTexture2D>());
        }
    }

    // Notify the overlay that these slots were refreshed.
    OnViewModelRefreshed.Broadcast(DirtyIndices);
}

int32 UInventoryViewModel::PositionToIndex(FIntPoint Position) const
{
    return Position.Y * GridWidth + Position.X;
}
