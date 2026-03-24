// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventorySlotWidget.h"
#include "CoreMinimal.h"
#include "Components/Border.h"
#include "UI/InventorySlotViewModel.h"
#include "UI/InventoryPanelWidget.h"
#include "FieldNotificationId.h"

void UInventorySlotWidget::SetSlotViewModel(UInventorySlotViewModel* InSlotVM)
{
    // 기존 바인딩 해제
    if (SlotVM)
    {
        SlotVM->RemoveAllFieldValueChangedDelegates(this);
    }

    SlotVM = InSlotVM;

    // FieldNotify 바인딩 — SlotVM 변경 시 자동 RefreshVisuals
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
        // 기본 슬롯 배경색: 빈 슬롯 = 어두운 회색, 점유 슬롯 = 파란색
        const FLinearColor DefaultColor = bIsEmpty
            ? FLinearColor(0.1f, 0.1f, 0.1f, 0.8f)
            : FLinearColor(0.0f, 0.3f, 0.6f, 0.9f);
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
            // 배치 가능 — 초록
            SlotBorder->SetBrushColor(FLinearColor(0.0f, 0.8f, 0.0f, 0.7f));
        }
        else
        {
            // 배치 불가 — 빨강
            SlotBorder->SetBrushColor(FLinearColor(0.8f, 0.0f, 0.0f, 0.7f));
        }
    }
    else
    {
        // 하이라이트 해제 — RefreshVisuals로 원래 색상 복원
        RefreshVisuals();
    }
}
