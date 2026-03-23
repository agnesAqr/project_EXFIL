// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/Equipment/EquipmentTypes.h"
#include "EquipmentSlotWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UInventoryDragDropOp;

/**
 * UEquipmentSlotWidget — 장비 슬롯 하나를 표현하는 위젯
 *
 * WBP_EquipmentSlot 레이아웃:
 *   Border_Slot
 *   └─ Overlay
 *       ├─ Image_ItemIcon      (BindWidget)
 *       ├─ TextBlock_SlotLabel (BindWidget)
 *       └─ TextBlock_ItemName  (BindWidgetOptional)
 *
 * SlotType은 BP 디테일 패널에서 지정.
 * NativeOnInitialized에서 SlotLabel 자동 설정.
 */
UCLASS(Abstract)
class PROJECT_EXFIL_API UEquipmentSlotWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    /** BP 디테일 패널에서 슬롯 타입 지정 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
    EEquipmentSlot SlotType = EEquipmentSlot::None;

    /** 슬롯 데이터로 UI 갱신 */
    UFUNCTION(BlueprintCallable, Category = "Equipment|UI")
    void RefreshSlot(const FEquipmentSlotData& SlotData);

    /** 드래그 호버 하이라이트 */
    UFUNCTION(BlueprintCallable, Category = "Equipment|UI")
    void SetDragHighlight(bool bVisible, bool bIsValid);

protected:
    virtual void NativeOnInitialized() override;

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                           const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry,
                                      const FPointerEvent& InMouseEvent,
                                      UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry,
                               const FDragDropEvent& InDragDropEvent,
                               UDragDropOperation* InOperation) override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> Border_Slot;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_ItemIcon;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TextBlock_SlotLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TextBlock_ItemName;

private:
    /** 현재 장착 상태 캐시 */
    FEquipmentSlotData CachedSlotData;

    /** 슬롯 상태별 Border 색상 적용 */
    void ApplyEmptyStyle();
    void ApplyEquippedStyle();
    void ApplyDragHoverStyle(bool bIsValid);
};
