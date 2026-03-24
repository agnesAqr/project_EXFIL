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
class UEquipmentComponent;
class UItemContextMenuWidget;

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
 * NativeConstruct에서 EquipmentComponent 자동 바인딩.
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
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

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

    /** 컨텍스트 메뉴 위젯 클래스 — BP 디테일에서 할당 */
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UItemContextMenuWidget> ContextMenuWidgetClass;

private:
    /** 현재 장착 상태 캐시 */
    FEquipmentSlotData CachedSlotData;

    /** 지연 생성되는 컨텍스트 메뉴 인스턴스 */
    UPROPERTY()
    TObjectPtr<UItemContextMenuWidget> ContextMenuWidget;

    /** 바인딩된 EquipmentComponent 참조 */
    UPROPERTY()
    TWeakObjectPtr<UEquipmentComponent> BoundEquipComp;

    /** EquipmentComponent 델리게이트 콜백 */
    UFUNCTION()
    void OnEquipmentItemEquipped(EEquipmentSlot InSlot, const FInventoryItemInstance& Item);

    UFUNCTION()
    void OnEquipmentItemUnequipped(EEquipmentSlot InSlot, const FInventoryItemInstance& Item);

    /** EquipmentComponent에서 현재 슬롯 상태를 조회하여 RefreshSlot 호출 */
    void RefreshFromEquipmentComponent();

    /** 슬롯 상태별 Border 색상 적용 */
    void ApplyEmptyStyle();
    void ApplyEquippedStyle();
    void ApplyDragHoverStyle(bool bIsValid);
};
