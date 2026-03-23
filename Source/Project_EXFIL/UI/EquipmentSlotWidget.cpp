// Copyright Project EXFIL. All Rights Reserved.

#include "UI/EquipmentSlotWidget.h"
#include "CoreMinimal.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blueprint/DragDropOperation.h"
#include "UI/InventoryDragDropOp.h"

void UEquipmentSlotWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // SlotLabel 자동 설정
    static const TMap<EEquipmentSlot, FText> SlotLabels =
    {
        { EEquipmentSlot::Head,    NSLOCTEXT("Equip", "Head",    "HEAD")     },
        { EEquipmentSlot::Face,    NSLOCTEXT("Equip", "Face",    "FACE")     },
        { EEquipmentSlot::Eyewear, NSLOCTEXT("Equip", "Eye",     "EYEWEAR")  },
        { EEquipmentSlot::Body,    NSLOCTEXT("Equip", "Body",    "BODY")     },
        { EEquipmentSlot::Weapon1, NSLOCTEXT("Equip", "W1",      "WEAPON 1") },
        { EEquipmentSlot::Weapon2, NSLOCTEXT("Equip", "W2",      "WEAPON 2") },
    };

    if (TextBlock_SlotLabel)
    {
        if (const FText* Label = SlotLabels.Find(SlotType))
        {
            TextBlock_SlotLabel->SetText(*Label);
        }
    }

    ApplyEmptyStyle();
}

void UEquipmentSlotWidget::RefreshSlot(const FEquipmentSlotData& SlotData)
{
    CachedSlotData = SlotData;

    if (SlotData.bIsOccupied)
    {
        // 아이콘 표시
        if (Image_ItemIcon)
        {
            Image_ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        // 아이템 이름 표시
        if (TextBlock_ItemName)
        {
            TextBlock_ItemName->SetText(FText::FromName(SlotData.ItemInstance.ItemDataID));
            TextBlock_ItemName->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        ApplyEquippedStyle();
    }
    else
    {
        if (Image_ItemIcon)
        {
            Image_ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (TextBlock_ItemName)
        {
            TextBlock_ItemName->SetVisibility(ESlateVisibility::Collapsed);
        }
        ApplyEmptyStyle();
    }
}

void UEquipmentSlotWidget::SetDragHighlight(bool bVisible, bool bIsValid)
{
    if (bVisible)
    {
        ApplyDragHoverStyle(bIsValid);
    }
    else
    {
        if (CachedSlotData.bIsOccupied)
        {
            ApplyEquippedStyle();
        }
        else
        {
            ApplyEmptyStyle();
        }
    }
}

// ─── 스타일 헬퍼 ───────────────────────────────────────────────────────────────

void UEquipmentSlotWidget::ApplyEmptyStyle()
{
    if (!Border_Slot)
    {
        return;
    }
    // Background: (0.08, 0.08, 0.08, 1.0)
    FLinearColor BgColor(0.08f, 0.08f, 0.08f, 1.0f);
    FSlateBrush Brush = Border_Slot->GetContentColorAndOpacity() == FLinearColor::White
        ? FSlateBrush()
        : FSlateBrush();
    Brush.TintColor = FSlateColor(BgColor);
    Border_Slot->SetBrushColor(BgColor);
}

void UEquipmentSlotWidget::ApplyEquippedStyle()
{
    if (!Border_Slot)
    {
        return;
    }
    // Background: (0.05, 0.25, 0.15, 1.0)
    Border_Slot->SetBrushColor(FLinearColor(0.05f, 0.25f, 0.15f, 1.0f));
}

void UEquipmentSlotWidget::ApplyDragHoverStyle(bool bIsValid)
{
    if (!Border_Slot)
    {
        return;
    }
    if (bIsValid)
    {
        // (0.08, 0.18, 0.3, 1.0)
        Border_Slot->SetBrushColor(FLinearColor(0.08f, 0.18f, 0.3f, 1.0f));
    }
    else
    {
        // (0.25, 0.08, 0.08, 1.0)
        Border_Slot->SetBrushColor(FLinearColor(0.25f, 0.08f, 0.08f, 1.0f));
    }
}

// ─── 드래그앤드롭 ──────────────────────────────────────────────────────────────

FReply UEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                                      const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && CachedSlotData.bIsOccupied)
    {
        return FReply::Handled().DetectDrag(GetCachedWidget().ToSharedRef(), EKeys::LeftMouseButton);
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UEquipmentSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry,
                                                 const FPointerEvent& InMouseEvent,
                                                 UDragDropOperation*& OutOperation)
{
    if (!CachedSlotData.bIsOccupied)
    {
        return;
    }

    UInventoryDragDropOp* DragOp = NewObject<UInventoryDragDropOp>(this);
    DragOp->DraggedItemInstanceID = CachedSlotData.ItemInstance.InstanceID;
    DragOp->ItemDataID             = CachedSlotData.ItemInstance.ItemDataID;
    DragOp->ItemSize               = CachedSlotData.ItemInstance.ItemSize;
    DragOp->DragItemSize           = CachedSlotData.ItemInstance.GetEffectiveSize();
    DragOp->bWasRotated            = CachedSlotData.ItemInstance.bIsRotated;
    OutOperation = DragOp;
}

bool UEquipmentSlotWidget::NativeOnDrop(const FGeometry& InGeometry,
                                         const FDragDropEvent& InDragDropEvent,
                                         UDragDropOperation* InOperation)
{
    // 기본 구현: 드롭 수락 여부만 반환 (실제 장착 로직은 BP 또는 Character에서 처리)
    UInventoryDragDropOp* DragOp = Cast<UInventoryDragDropOp>(InOperation);
    return DragOp != nullptr;
}
