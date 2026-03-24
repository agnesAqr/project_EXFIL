// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "InventorySlotWidget.generated.h"

class UInventorySlotViewModel;
class UInventoryPanelWidget;
class UImage;
class UBorder;

UCLASS(Abstract)
class PROJECT_EXFIL_API UInventorySlotWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    /** 슬롯 ViewModel 연결 후 시각 갱신 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void SetSlotViewModel(UInventorySlotViewModel* InSlotVM);

    /** BuildGrid에서 부모 패널 참조 주입 */
    void SetParentPanel(UInventoryPanelWidget* InPanel);

protected:
    /** 슬롯 테두리 — 하이라이트 색상 변경용 (WBP에서 BindWidget) */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UBorder> SlotBorder;

public:
    /** 드래그 진입/이탈 시 테두리 하이라이트 — PanelWidget에서도 호출 */
    void SetHighlight(bool bHighlighted, bool bIsValid = true);

protected:

private:
    UPROPERTY()
    TObjectPtr<UInventorySlotViewModel> SlotVM;

    /** 부모 패널 — GetTypedOuter 대신 직접 참조 */
    UPROPERTY()
    TWeakObjectPtr<UInventoryPanelWidget> ParentPanel;

    /** SlotVM 상태에 따라 아이콘/텍스트 갱신 */
    void RefreshVisuals();

    /** FieldNotify 변경 시 자동 호출 */
    void OnSlotFieldChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId);
};
