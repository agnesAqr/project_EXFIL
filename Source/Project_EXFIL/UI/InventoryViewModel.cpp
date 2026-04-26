// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventoryViewModel.h"
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Inventory/InventoryComponent.h"
#include "Data/EXFILItemTypes.h"
#include "Data/ItemDataSubsystem.h"
#include "Project_EXFIL.h"

void UInventoryViewModel::Initialize(UInventoryComponent* InInventoryComponent)
{
    if (!InInventoryComponent)
    {
        return;
    }

	InventoryComp = InInventoryComponent;
    InInventoryComponent->EnsureReplicatedCachesReady();

    if (UWorld* World = InInventoryComponent->GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            CachedItemSub = GI->GetSubsystem<UItemDataSubsystem>();
        }
    }
	UE_MVVM_SET_PROPERTY_VALUE(GridWidth, InInventoryComponent->GridWidth);
	UE_MVVM_SET_PROPERTY_VALUE(GridHeight, InInventoryComponent->GridHeight);
    const int32 TotalSlots = GridWidth * GridHeight;
    SlotViewModels.SetNum(TotalSlots);

    for (int32 i = 0; i < TotalSlots; ++i)
    {
        UInventorySlotViewModel* SlotVM = NewObject<UInventorySlotViewModel>(this);
        SlotVM->SetGridPosition(FIntPoint(i % GridWidth, i / GridWidth));
        SlotViewModels[i] = SlotVM;
    }
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

void UInventoryViewModel::RequestConsumeItem(FGuid ItemInstanceID)
{
    if (!InventoryComp.IsValid() || !ItemInstanceID.IsValid())
    {
        return;
    }

    FInventoryItemInstance Item;
    if (InventoryComp->GetItemByID(ItemInstanceID, Item))
    {
        InventoryComp->RequestConsumeItemByID(Item.ItemDataID, 1);
    }
}

void UInventoryViewModel::RequestDropItem(FGuid ItemInstanceID)
{
    if (!InventoryComp.IsValid() || !ItemInstanceID.IsValid())
    {
        return;
    }

    InventoryComp->RequestDropItem(ItemInstanceID);
}

bool UInventoryViewModel::TryGetItemContextMenuViewDataAtCell(
    FIntPoint Cell, FItemContextMenuViewData& OutViewData) const
{
    FInventoryItemInstance Item;
    if (!TryGetItemAtCell(Cell, Item))
    {
        return false;
    }

    OutViewData = FItemContextMenuViewData();
    OutViewData.TargetItemInstanceID = Item.InstanceID;
    OutViewData.TargetEquipmentSlot = EEquipmentSlot::None;
    OutViewData.bShowDrop = true;

    const UItemDataSubsystem* ItemSub = CachedItemSub.Get();
    const FItemData* ItemData = ItemSub ? ItemSub->GetItemData(Item.ItemDataID) : nullptr;
    if (!ItemData)
    {
        return true;
    }

    OutViewData.bShowUse = ItemData->ItemType == EItemType::Consumable;
    OutViewData.bShowEquip = ItemData->ItemType == EItemType::Equipment;
    return true;
}

bool UInventoryViewModel::ConsumePendingOverlayDelta(FInventoryOverlayDeltaViewData& OutDelta)
{
    if (!bHasPendingOverlayDelta)
    {
        return false;
    }

    OutDelta = PendingOverlayDelta;
    PendingOverlayDelta = FInventoryOverlayDeltaViewData();
    bHasPendingOverlayDelta = false;
    return true;
}

bool UInventoryViewModel::BuildFullOverlayDelta(FInventoryOverlayDeltaViewData& OutDelta) const
{
    OutDelta = FInventoryOverlayDeltaViewData();
    OutDelta.bFullRefresh = true;

    if (!InventoryComp.IsValid())
    {
        return false;
    }

    for (const FInventoryItemInstance& Item : InventoryComp->GetAllItems())
    {
        FInventoryIconViewData IconViewData;
        if (TryBuildIconViewData(Item.InstanceID, IconViewData))
        {
            OutDelta.UpsertItems.Add(IconViewData);
        }
    }

    return true;
}

bool UInventoryViewModel::TryBuildDragSourceAtCell(
    FIntPoint Cell, FIntPoint DragStartCell, FInventoryDragSourceViewData& OutViewData) const
{
    FInventoryItemInstance Item;
    if (!TryGetItemAtCell(Cell, Item))
    {
        return false;
    }

    const bool bCanRotate = !Item.ItemSize.IsSquare();
    const bool bInitialRotated = bCanRotate ? Item.bIsRotated : false;
    const FItemSize InitialDragSize = Item.GetEffectiveSize();

    FIntPoint Offset = DragStartCell - Item.RootPosition;
    Offset.X = FMath::Clamp(Offset.X, 0, InitialDragSize.Width - 1);
    Offset.Y = FMath::Clamp(Offset.Y, 0, InitialDragSize.Height - 1);

    OutViewData = FInventoryDragSourceViewData();
    OutViewData.InstanceID = Item.InstanceID;
    OutViewData.ItemDataID = Item.ItemDataID;
    OutViewData.OriginalRootPosition = Item.RootPosition;
    OutViewData.BaseItemSize = Item.ItemSize;
    OutViewData.InitialDragItemSize = InitialDragSize;
    OutViewData.bOriginalRotated = bInitialRotated;
    OutViewData.DragOffset = Offset;
    return true;
}

FInventoryPlacementHintViewData UInventoryViewModel::BuildPlacementHint(
    FIntPoint HoverCell, FIntPoint DragOffset,
    FGuid IgnoredInstanceID, FItemSize PreviewSize) const
{
    FInventoryPlacementHintViewData Hint;
    Hint.PreviewRootPosition = HoverCell - DragOffset;
    Hint.PreviewSize = PreviewSize;

    if (InventoryComp.IsValid())
    {
        Hint.bPredictedPlaceable = InventoryComp->CanPlaceItemAtIgnoringInstance(
            Hint.PreviewRootPosition, PreviewSize, IgnoredInstanceID);
    }

    return Hint;
}

void UInventoryViewModel::HandleInventoryUpdated(const TSet<int32>& DirtyIndices)
{
    if (DirtyIndices.Num() == 0)
    {
        return;
    }

    if (UInventoryComponent* Inventory = InventoryComp.Get())
    {
        Inventory->EnsureReplicatedCachesReady();
    }

    BuildPendingOverlayDelta(DirtyIndices);
    OnViewModelRefreshed.Broadcast(DirtyIndices);
}

void UInventoryViewModel::RefreshAllSlots()
{
    TSet<int32> AllIndices;
    AllIndices.Reserve(SlotViewModels.Num());
    for (int32 i = 0; i < SlotViewModels.Num(); ++i)
    {
        AllIndices.Add(i);
    }
    BuildPendingOverlayDelta(AllIndices);
    OnViewModelRefreshed.Broadcast(AllIndices);
}

void UInventoryViewModel::RefreshDirtySlots(const TSet<int32>& DirtyIndices)
{
    if (!InventoryComp.IsValid())
    {
        return;
    }

    UItemDataSubsystem* ItemSub = CachedItemSub.Get();

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
}

void UInventoryViewModel::BuildPendingOverlayDelta(const TSet<int32>& DirtyIndices)
{
    TSet<FGuid> OldRootIDs;
    CollectItemIDsFromSlotViewModels(DirtyIndices, OldRootIDs);

    RefreshDirtySlots(DirtyIndices);

    TSet<FGuid> NewRootIDs;
    CollectItemIDsFromSlotViewModels(DirtyIndices, NewRootIDs);

    TSet<FGuid> AffectedIDs = OldRootIDs;
    AffectedIDs.Append(NewRootIDs);

    FInventoryOverlayDeltaViewData Delta;
    Delta.bFullRefresh = DirtyIndices.Num() >= SlotViewModels.Num();

    if (Delta.bFullRefresh)
    {
        BuildFullOverlayDelta(Delta);
    }
    else
    {
        for (const FGuid& ItemID : AffectedIDs)
        {
            FInventoryIconViewData IconViewData;
            if (TryBuildIconViewData(ItemID, IconViewData))
            {
                Delta.UpsertItems.Add(IconViewData);
            }
            else
            {
                Delta.RemovedItemInstanceIDs.Add(ItemID);
            }
        }
    }

    StorePendingOverlayDelta(Delta);
}

void UInventoryViewModel::CollectItemIDsFromSlotViewModels(
    const TSet<int32>& SlotIndices, TSet<FGuid>& OutItemIDs) const
{
    for (int32 SlotIndex : SlotIndices)
    {
        if (!SlotViewModels.IsValidIndex(SlotIndex))
        {
            continue;
        }

        const UInventorySlotViewModel* SlotVM = SlotViewModels[SlotIndex];
        if (!SlotVM || SlotVM->IsEmpty())
        {
            continue;
        }

        const FGuid ItemID = SlotVM->GetItemInstanceID();
        if (ItemID.IsValid())
        {
            OutItemIDs.Add(ItemID);
        }
    }
}

bool UInventoryViewModel::TryBuildIconViewData(
    FGuid ItemInstanceID, FInventoryIconViewData& OutViewData) const
{
    if (!InventoryComp.IsValid() || !ItemInstanceID.IsValid())
    {
        return false;
    }

    FInventoryItemInstance Item;
    if (!InventoryComp->GetItemByID(ItemInstanceID, Item))
    {
        return false;
    }

    UTexture2D* IconTexture = nullptr;
    UItemDataSubsystem* ItemSub = CachedItemSub.Get();
    if (ItemSub)
    {
        const FItemData* ItemData = ItemSub->GetItemData(Item.ItemDataID);
        if (ItemData && !ItemData->Icon.IsNull())
        {
            IconTexture = ItemSub->GetCachedTexture(ItemData->Icon);
        }
    }

    OutViewData = FInventoryIconViewData();
    OutViewData.InstanceID = Item.InstanceID;
    OutViewData.Icon = IconTexture;
    OutViewData.StackCount = Item.StackCount;
    OutViewData.RootGridPosition = Item.RootPosition;
    OutViewData.GridSpan = Item.GetEffectiveSize();
    OutViewData.bRotated = Item.bIsRotated && !Item.ItemSize.IsSquare();
    return true;
}

bool UInventoryViewModel::TryGetItemAtCell(
    FIntPoint Cell, FInventoryItemInstance& OutItem) const
{
    if (!InventoryComp.IsValid())
    {
        return false;
    }

    return InventoryComp->GetItemAt(Cell, OutItem);
}

void UInventoryViewModel::StorePendingOverlayDelta(
    const FInventoryOverlayDeltaViewData& Delta)
{
    if (Delta.bFullRefresh)
    {
        PendingOverlayDelta = Delta;
        bHasPendingOverlayDelta = true;
        return;
    }

    PendingOverlayDelta.UpsertItems.Append(Delta.UpsertItems);
    PendingOverlayDelta.RemovedItemInstanceIDs.Append(Delta.RemovedItemInstanceIDs);
    bHasPendingOverlayDelta =
        PendingOverlayDelta.UpsertItems.Num() > 0 ||
        PendingOverlayDelta.RemovedItemInstanceIDs.Num() > 0;
}

int32 UInventoryViewModel::PositionToIndex(FIntPoint Position) const
{
    return Position.Y * GridWidth + Position.X;
}
