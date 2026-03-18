// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventoryPanelWidget.h"
#include "CoreMinimal.h"
#include "Components/UniformGridPanel.h"
#include "UI/InventoryViewModel.h"
#include "UI/InventorySlotWidget.h"
#include "Input/CommonUIInputTypes.h"

void UInventoryPanelWidget::SetViewModel(UInventoryViewModel* InViewModel)
{
    ViewModel = InViewModel;

    if (ViewModel)
    {
        BuildGrid();
    }
}

void UInventoryPanelWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
}

void UInventoryPanelWidget::NativeOnDeactivated()
{
    Super::NativeOnDeactivated();
}

bool UInventoryPanelWidget::NativeOnHandleBackAction()
{
    // ESC는 PIE를 종료시키므로 CommonUI BackAction 비활성화
    // 인벤토리 닫기는 BP 이벤트 그래프의 I 키 토글로 처리
    return false;
}

TOptional<FUIInputConfig> UInventoryPanelWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UInventoryPanelWidget::BuildGrid()
{
    if (!ViewModel || !SlotWidgetClass || !GridPanel)
    {
        return;
    }

    ClearGrid();

    const int32 Width = ViewModel->GetGridWidth();
    const int32 Height = ViewModel->GetGridHeight();

    SlotWidgets.Reserve(Width * Height);

    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            UInventorySlotWidget* SlotWidget = CreateWidget<UInventorySlotWidget>(this, SlotWidgetClass);
            if (!SlotWidget)
            {
                continue;
            }

            UInventorySlotViewModel* SlotVM = ViewModel->GetSlotAt(FIntPoint(X, Y));
            SlotWidget->SetParentPanel(this);
            SlotWidget->SetSlotViewModel(SlotVM);

            // UniformGridPanel: Column = X, Row = Y
            GridPanel->AddChildToUniformGrid(SlotWidget, Y, X);
            SlotWidgets.Add(SlotWidget);
        }
    }
}

bool UInventoryPanelWidget::ForwardMoveRequest(FGuid ItemInstanceID, FIntPoint NewPosition,
                                                bool bNewRotated)
{
    if (!ViewModel)
    {
        return false;
    }
    return ViewModel->RequestMoveItem(ItemInstanceID, NewPosition, bNewRotated);
}

void UInventoryPanelWidget::ClearGrid()
{
    if (GridPanel)
    {
        GridPanel->ClearChildren();
    }
    SlotWidgets.Empty();
}

void UInventoryPanelWidget::HighlightArea(FIntPoint RootPos, FItemSize ItemSize, bool bIsValid)
{
    if (!ViewModel)
    {
        return;
    }

    const int32 GridWidth = ViewModel->GetGridWidth();

    for (int32 Y = RootPos.Y; Y < RootPos.Y + ItemSize.Height; ++Y)
    {
        for (int32 X = RootPos.X; X < RootPos.X + ItemSize.Width; ++X)
        {
            const int32 Index = Y * GridWidth + X;
            if (SlotWidgets.IsValidIndex(Index))
            {
                SlotWidgets[Index]->SetHighlight(true, bIsValid);
            }
        }
    }
}

void UInventoryPanelWidget::ClearAreaHighlights()
{
    for (UInventorySlotWidget* SlotWidget : SlotWidgets)
    {
        if (SlotWidget)
        {
            SlotWidget->SetHighlight(false);
        }
    }
}
