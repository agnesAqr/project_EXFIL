// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventorySlotViewModel.h"
#include "CoreMinimal.h"

void UInventorySlotViewModel::RequestDrop()
{
}

void UInventorySlotViewModel::SetEmpty(bool bNewValue)
{
    UE_MVVM_SET_PROPERTY_VALUE(bEmpty, bNewValue);
}
