// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventorySlotWidget.h"
#include "CoreMinimal.h"
#include "Components/Border.h"
#include "UI/InventorySlotViewModel.h"
#include "UI/InventoryPanelWidget.h"
#include "FieldNotificationId.h"

void UInventorySlotWidget::SetSlotViewModel(UInventorySlotViewModel* InSlotVM)
{
    if (SlotVM)
    {
        SlotVM->RemoveAllFieldValueChangedDelegates(this);
    }

    SlotVM = InSlotVM;
    if (SlotVM)
    {
        auto Delegate = INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
            this, &UInventorySlotWidget::OnSlotFieldChanged);
        SlotVM->AddFieldValueChangedDelegate(
            UInventorySlotViewModel::FFieldNotificationClassDescriptor::bEmpty, Delegate);
    }

    RefreshVisuals();
}

void UInventorySlotWidget::SetParentPanel(UInventoryPanelWidget* InPanel)
{
    ParentPanel = InPanel;
}

void UInventorySlotWidget::RefreshVisuals()
{
    if (!SlotVM)
    {
        return;
    }

    const bool bIsEmpty = SlotVM->IsEmpty();

    if (SlotBorder)
    {
        const FLinearColor DefaultColor = bIsEmpty
            ? FLinearColor(0.1f, 0.1f, 0.1f, 0.8f)
            : FLinearColor(0.12f, 0.15f, 0.10f, 1.0f);
        SlotBorder->SetBrushColor(DefaultColor);
    }
}

void UInventorySlotWidget::OnSlotFieldChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId)
{
    RefreshVisuals();
}

void UInventorySlotWidget::SetHighlight(bool bHighlighted, bool bIsValid)
{
    if (!SlotBorder)
    {
        return;
    }

    if (bHighlighted)
    {
        if (bIsValid)
        {
            SlotBorder->SetBrushColor(FLinearColor(0.0f, 0.8f, 0.0f, 0.7f));
        }
        else
        {
            SlotBorder->SetBrushColor(FLinearColor(0.8f, 0.0f, 0.0f, 0.7f));
        }
    }
    else
    {
        RefreshVisuals();
    }
}
