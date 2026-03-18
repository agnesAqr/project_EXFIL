// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventorySlotWidget.h"
#include "CoreMinimal.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/InventorySlotViewModel.h"
#include "UI/InventoryDragDropOp.h"
#include "UI/InventoryDragPreviewWidget.h"
#include "UI/InventoryViewModel.h"
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
        SlotVM->AddFieldValueChangedDelegate(
            UInventorySlotViewModel::FFieldNotificationClassDescriptor::StackCount, Delegate);
        SlotVM->AddFieldValueChangedDelegate(
            UInventorySlotViewModel::FFieldNotificationClassDescriptor::ItemDataID, Delegate);
    }

    RefreshVisuals();
}

void UInventorySlotWidget::SetParentPanel(UInventoryPanelWidget* InPanel)
{
    ParentPanel = InPanel;
}

FReply UInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    // 좌클릭 시 드래그 감지를 위해 PreviewReply 반환
    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        return UWidgetBlueprintLibrary::DetectDragIfPressed(
            InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    UE_LOG(LogTemp, Warning, TEXT("DragDetected — Pos:(%d,%d) isEmpty:%d isRoot:%d"),
        SlotVM ? SlotVM->GetGridPosition().X : -1,
        SlotVM ? SlotVM->GetGridPosition().Y : -1,
        SlotVM ? SlotVM->IsEmpty() : 1,
        SlotVM ? SlotVM->IsRootSlot() : 0);

    if (!SlotVM || SlotVM->IsEmpty())
    {
        // 빈 슬롯은 드래그 불가
        return;
    }

    UInventoryDragDropOp* DragOp = NewObject<UInventoryDragDropOp>(this);
    DragOp->DraggedItemInstanceID = SlotVM->GetItemInstanceID();
    DragOp->OriginalPosition = SlotVM->GetGridPosition();
    DragOp->ItemDataID = SlotVM->GetItemDataID();
    DragOp->Pivot = EDragPivot::TopLeft;

    // 비루트 슬롯 드래그 시 루트까지의 오프셋 계산 + 유효 크기 저장
    if (ParentPanel.IsValid())
    {
        if (UInventoryViewModel* VM = ParentPanel->GetViewModel())
        {
            const FIntPoint RootPos = VM->GetItemRootPosition(SlotVM->GetItemInstanceID());
            if (RootPos.X >= 0)
            {
                DragOp->DragOffset = SlotVM->GetGridPosition() - RootPos;
            }
            DragOp->DragItemSize = VM->GetItemEffectiveSize(SlotVM->GetItemInstanceID());
        }
    }

    // 아이템 크기에 맞는 프리뷰 그리드를 드래그 비주얼로 사용
    APlayerController* PC = GetOwningPlayer();
    UInventoryDragPreviewWidget* DragVisual = nullptr;
    if (PC)
    {
        DragVisual = CreateWidget<UInventoryDragPreviewWidget>(PC, UInventoryDragPreviewWidget::StaticClass());
        if (DragVisual)
        {
            DragVisual->BuildPreview(DragOp->DragItemSize.Width, DragOp->DragItemSize.Height);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("InventorySlotWidget: GetOwningPlayer() null — drag visual 생성 실패"));
    }
    DragOp->DefaultDragVisual = DragVisual; // nullptr이면 비주얼 없음

    OutOperation = DragOp;
}

bool UInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UInventoryDragDropOp* DragOp = Cast<UInventoryDragDropOp>(InOperation);
    if (!DragOp || !SlotVM)
    {
        return false;
    }

    // 같은 루트 위치에 드롭하면 무시
    const FIntPoint NewRootCheck = SlotVM->GetGridPosition() - DragOp->DragOffset;
    const FIntPoint OldRootCheck = DragOp->OriginalPosition - DragOp->DragOffset;
    if (NewRootCheck == OldRootCheck)
    {
        return false;
    }

    // SlotWidget → PanelWidget → ViewModel → Model 단방향 흐름 유지
    UInventoryPanelWidget* PanelWidget = ParentPanel.Get();
    if (!PanelWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("NativeOnDrop — ParentPanel is NULL at Pos:(%d,%d)"),
            SlotVM ? SlotVM->GetGridPosition().X : -1,
            SlotVM ? SlotVM->GetGridPosition().Y : -1);
        return false;
    }

    // 드롭 위치에서 오프셋을 역산해 아이템 루트 위치 계산
    const FIntPoint NewRootPos = SlotVM->GetGridPosition() - DragOp->DragOffset;

    const bool bSuccess = PanelWidget->ForwardMoveRequest(
        DragOp->DraggedItemInstanceID,
        NewRootPos,
        DragOp->bWasRotated);

    // 드롭 성공/실패 피드백 — 전체 하이라이트 해제
    if (ParentPanel.IsValid())
    {
        ParentPanel->ClearAreaHighlights();
    }
    return bSuccess;
}

void UInventorySlotWidget::NativeOnDragEnter(const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    const UInventoryDragDropOp* DragOp = Cast<UInventoryDragDropOp>(InOperation);
    if (DragOp && SlotVM && ParentPanel.IsValid())
    {
        const FIntPoint RootPos = SlotVM->GetGridPosition() - DragOp->DragOffset;
        // 범위 내 빈 슬롯인지 간단 체크 (자기 슬롯 기준)
        const bool bCanPlace = SlotVM->IsEmpty();
        ParentPanel->HighlightArea(RootPos, DragOp->DragItemSize, bCanPlace);
    }
}

void UInventorySlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    if (ParentPanel.IsValid())
    {
        ParentPanel->ClearAreaHighlights();
    }
}

void UInventorySlotWidget::RefreshVisuals()
{
    if (!SlotVM)
    {
        return;
    }

    const bool bIsEmpty = SlotVM->IsEmpty();

    if (ItemIcon)
    {
        ItemIcon->SetVisibility(bIsEmpty ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
    }

    if (StackCountText)
    {
        if (!bIsEmpty && SlotVM->IsRootSlot() && SlotVM->GetStackCount() > 1)
        {
            StackCountText->SetText(FText::AsNumber(SlotVM->GetStackCount()));
            StackCountText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            StackCountText->SetVisibility(ESlateVisibility::Hidden);
        }
    }

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
