// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "InventoryPanelWidget.generated.h"

class UInventoryViewModel;
class UInventorySlotWidget;
class UInventoryIconOverlay;
class UUniformGridPanel;

UCLASS(Abstract)
class PROJECT_EXFIL_API UInventoryPanelWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    /** ViewModel 연결. BuildGrid() 호출로 슬롯 위젯 생성 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void SetViewModel(UInventoryViewModel* InViewModel);

    /**
     * SlotWidget → PanelWidget → ViewModel → Model 이동 요청 중계.
     * View에서 ViewModel을 직접 참조하지 않도록 PanelWidget이 중계.
     */
    bool ForwardMoveRequest(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated);

    /** 드래그 호버 시 영역 하이라이트 */
    void HighlightArea(FIntPoint RootPos, FItemSize ItemSize, bool bIsValid);

    /** 모든 슬롯 하이라이트 해제 */
    void ClearAreaHighlights();

    /** SlotWidget이 오프셋 계산에 사용 */
    UInventoryViewModel* GetViewModel() const { return ViewModel; }

protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual bool NativeOnHandleBackAction() override;
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    /** BP에서 UniformGridPanel 위젯을 BindWidget으로 연결 */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UUniformGridPanel> GridPanel;

    /** 멀티셀 아이콘 오버레이 — CanvasPanel 기반 (WBP에서 BindWidget으로 연결) */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UInventoryIconOverlay> IconOverlay;

    /** BP 서브클래스에서 지정. 실제 레이아웃은 WBP에서 구성 */
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI")
    TSubclassOf<UInventorySlotWidget> SlotWidgetClass;

private:
    UPROPERTY()
    TObjectPtr<UInventoryViewModel> ViewModel;

    UPROPERTY()
    TArray<TObjectPtr<UInventorySlotWidget>> SlotWidgets;

    /** GridWidth × GridHeight 슬롯 위젯 생성 및 GridPanel 배치 */
    void BuildGrid();

    /** GridPanel 초기화 */
    void ClearGrid();

    /** IconOverlay에 아이콘 갱신 요청 */
    void RefreshIconOverlay();

    /** ViewModel RefreshAllSlots 후 콜백 핸들 */
    FDelegateHandle ViewModelRefreshedHandle;
};
