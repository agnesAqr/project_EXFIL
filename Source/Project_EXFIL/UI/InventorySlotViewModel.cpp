// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventorySlotViewModel.h"
#include "CoreMinimal.h"

void UInventorySlotViewModel::RequestDrop()
{
    // View가 드롭 요청 시 호출 — 부모 UInventoryViewModel을 통해 Model로 전달
    // UInventoryPanelWidget::NativeOnDrop에서 RequestMoveItem 호출이 실제 처리
    // 이 함수는 슬롯 자체에서 아이템을 버릴 때 사용 (향후 확장)
}

void UInventorySlotViewModel::SetEmpty(bool bNewValue)
{
    UE_MVVM_SET_PROPERTY_VALUE(bEmpty, bNewValue);
}

void UInventorySlotViewModel::SetItemDataID(FName NewValue)
{
    UE_MVVM_SET_PROPERTY_VALUE(ItemDataID, NewValue);
}

void UInventorySlotViewModel::SetStackCount(int32 NewValue)
{
    UE_MVVM_SET_PROPERTY_VALUE(StackCount, NewValue);
}

void UInventorySlotViewModel::SetIsRootSlot(bool bNewValue)
{
    UE_MVVM_SET_PROPERTY_VALUE(bIsRootSlot, bNewValue);
}

void UInventorySlotViewModel::SetGridPosition(FIntPoint NewValue)
{
    UE_MVVM_SET_PROPERTY_VALUE(GridPosition, NewValue);
}

void UInventorySlotViewModel::SetItemInstanceID(FGuid NewValue)
{
    UE_MVVM_SET_PROPERTY_VALUE(ItemInstanceID, NewValue);
}

void UInventorySlotViewModel::SetIcon(TSoftObjectPtr<UTexture2D> NewValue)
{
    UE_MVVM_SET_PROPERTY_VALUE(Icon, NewValue);
}

void UInventorySlotViewModel::SetItemSizeX(int32 NewValue)
{
    UE_MVVM_SET_PROPERTY_VALUE(ItemSizeX, NewValue);
}

void UInventorySlotViewModel::SetItemSizeY(int32 NewValue)
{
    UE_MVVM_SET_PROPERTY_VALUE(ItemSizeY, NewValue);
}
