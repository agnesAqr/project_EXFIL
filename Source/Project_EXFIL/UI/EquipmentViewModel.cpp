// Copyright Project EXFIL. All Rights Reserved.

#include "UI/EquipmentViewModel.h"
#include "CoreMinimal.h"
#include "Data/Equipment/EquipmentComponent.h"
#include "Data/EXFILItemTypes.h"
#include "Data/ItemDataSubsystem.h"
#include "Engine/GameInstance.h"

void UEquipmentViewModel::Initialize(UEquipmentComponent* InEquipmentComponent)
{
    UnbindEquipmentComponent();

    EquipmentComp = InEquipmentComponent;
    if (!InEquipmentComponent)
    {
        return;
    }

    if (UWorld* World = InEquipmentComponent->GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            CachedItemSub = GI->GetSubsystem<UItemDataSubsystem>();
        }
    }

    InEquipmentComponent->OnItemEquipped.AddDynamic(
        this, &UEquipmentViewModel::HandleItemEquipped);
    InEquipmentComponent->OnItemUnequipped.AddDynamic(
        this, &UEquipmentViewModel::HandleItemUnequipped);

    BroadcastSlot(EEquipmentSlot::Head);
    BroadcastSlot(EEquipmentSlot::Face);
    BroadcastSlot(EEquipmentSlot::Eyewear);
    BroadcastSlot(EEquipmentSlot::Body);
    BroadcastSlot(EEquipmentSlot::Weapon1);
    BroadcastSlot(EEquipmentSlot::Weapon2);
}

void UEquipmentViewModel::BeginDestroy()
{
    UnbindEquipmentComponent();
    Super::BeginDestroy();
}

void UEquipmentViewModel::RequestEquip(EEquipmentSlot Slot, FGuid ItemInstanceID)
{
    if (EquipmentComp.IsValid())
    {
        EquipmentComp->RequestEquipFromInventory(Slot, ItemInstanceID);
    }
}

void UEquipmentViewModel::RequestUnequip(EEquipmentSlot Slot)
{
    if (EquipmentComp.IsValid())
    {
        EquipmentComp->RequestUnequipToInventory(Slot);
    }
}

void UEquipmentViewModel::RequestUnequipAt(
    EEquipmentSlot Slot, FIntPoint Position, bool bRotated)
{
    if (EquipmentComp.IsValid())
    {
        EquipmentComp->RequestUnequipToInventoryAt(Slot, Position, bRotated);
    }
}

void UEquipmentViewModel::RequestDropEquipped(EEquipmentSlot Slot)
{
    if (EquipmentComp.IsValid())
    {
        EquipmentComp->RequestDropEquippedItem(Slot);
    }
}

bool UEquipmentViewModel::GetSlotViewData(
    EEquipmentSlot Slot, FEquipmentSlotViewData& OutViewData) const
{
    OutViewData = FEquipmentSlotViewData();
    OutViewData.SlotType = Slot;

    if (!EquipmentComp.IsValid())
    {
        return false;
    }

    FInventoryItemInstance Item;
    if (!EquipmentComp->GetEquippedItem(Slot, Item))
    {
        return true;
    }

    OutViewData.bEquipped = true;

    UItemDataSubsystem* ItemSub = CachedItemSub.Get();
    const FItemData* ItemData = ItemSub ? ItemSub->GetItemData(Item.ItemDataID) : nullptr;
    OutViewData.DisplayName = ItemData ? ItemData->DisplayName : FText::FromName(Item.ItemDataID);
    if (ItemData && ItemSub && !ItemData->Icon.IsNull())
    {
        OutViewData.Icon = ItemSub->GetCachedTexture(ItemData->Icon);
    }

    return true;
}

bool UEquipmentViewModel::TryGetContextMenuViewDataForSlot(
    EEquipmentSlot Slot, FItemContextMenuViewData& OutViewData) const
{
    if (!EquipmentComp.IsValid())
    {
        return false;
    }

    FInventoryItemInstance Item;
    if (!EquipmentComp->GetEquippedItem(Slot, Item))
    {
        return false;
    }

    OutViewData = FItemContextMenuViewData();
    OutViewData.bCanUnequip = true;
    OutViewData.bCanDrop = true;
    OutViewData.TargetItemInstanceID = Item.InstanceID;
    OutViewData.TargetEquipmentSlot = Slot;
    return true;
}

bool UEquipmentViewModel::TryBuildDragSourceForSlot(
    EEquipmentSlot Slot, FEquipmentDragSourceViewData& OutViewData) const
{
    if (!EquipmentComp.IsValid())
    {
        return false;
    }

    FInventoryItemInstance Item;
    if (!EquipmentComp->GetEquippedItem(Slot, Item))
    {
        return false;
    }

    const bool bInitialRotated = Item.bIsRotated && !Item.ItemSize.IsSquare();
    OutViewData = FEquipmentDragSourceViewData();
    OutViewData.ItemInstanceID = Item.InstanceID;
    OutViewData.SourceSlot = Slot;
    OutViewData.BaseItemSize = Item.ItemSize;
    OutViewData.InitialDragItemSize = bInitialRotated
        ? Item.ItemSize.GetRotated()
        : Item.ItemSize;
    OutViewData.bOriginalRotated = bInitialRotated;
    return true;
}

void UEquipmentViewModel::HandleItemEquipped(
    EEquipmentSlot Slot, const FInventoryItemInstance& Item)
{
    BroadcastSlot(Slot);
}

void UEquipmentViewModel::HandleItemUnequipped(
    EEquipmentSlot Slot, const FInventoryItemInstance& Item)
{
    BroadcastSlot(Slot);
}

void UEquipmentViewModel::BroadcastSlot(EEquipmentSlot Slot)
{
    FEquipmentSlotViewData SlotViewData;
    GetSlotViewData(Slot, SlotViewData);
    OnEquipmentSlotChanged.Broadcast(Slot, SlotViewData);
}

void UEquipmentViewModel::UnbindEquipmentComponent()
{
    if (!EquipmentComp.IsValid())
    {
        return;
    }

    EquipmentComp->OnItemEquipped.RemoveDynamic(
        this, &UEquipmentViewModel::HandleItemEquipped);
    EquipmentComp->OnItemUnequipped.RemoveDynamic(
        this, &UEquipmentViewModel::HandleItemUnequipped);
    EquipmentComp.Reset();
}
