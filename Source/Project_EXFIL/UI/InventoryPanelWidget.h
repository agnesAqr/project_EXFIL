// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "InventoryPanelWidget.generated.h"

class UInventoryViewModel;
class UInventorySlotWidget;
class UInventoryIconOverlay;
class UCraftingPanelWidget;
class UUniformGridPanel;
class UWidgetSwitcher;
class UButton;

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

    /** CraftingPanel 접근용 (Character BeginPlay에서 Initialize 호출) */
    UCraftingPanelWidget* GetCraftingPanel() const { return CraftingPanel; }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual void NativeConstruct() override;
    virtual bool NativeOnHandleBackAction() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                              const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                              int32 LayerId, const FWidgetStyle& InWidgetStyle,
                              bool bParentEnabled) const override;

    // ─── BindWidget: 그리드 영역 ───────────────────────────────────────────────

    /** BP에서 UniformGridPanel 위젯을 BindWidget으로 연결 */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UUniformGridPanel> GridPanel;

    /** 멀티셀 아이콘 오버레이 — CanvasPanel 기반 (WBP에서 BindWidget으로 연결) */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UInventoryIconOverlay> IconOverlay;

    // ─── BindWidget: 탭 + WidgetSwitcher ──────────────────────────────────────

    /** 인벤토리 / 크래프팅 탭 전환 */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UWidgetSwitcher> WidgetSwitcher_Content;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_InventoryTab;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_CraftingTab;

    /** 크래프팅 패널 위젯 (WBP 안에 인스턴스로 배치) */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UCraftingPanelWidget> CraftingPanel;

    // ─── 설정 ─────────────────────────────────────────────────────────────────

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

    /** NativePaint에서 1회 셀 정사각형 보정용 플래그 */
    mutable bool bNeedsCellSquareFix = true;

    /** 셀 정사각형 보정 후 아이콘 배치용 타이머 */
    FTimerHandle IconRefreshTimerHandle;

    /** ViewModel RefreshAllSlots 후 콜백 핸들 */
    FDelegateHandle ViewModelRefreshedHandle;

    // ─── 탭 버튼 콜백 ─────────────────────────────────────────────────────────

    UFUNCTION()
    void OnInventoryTabClicked();

    UFUNCTION()
    void OnCraftingTabClicked();

    /** 탭 인덱스(0=인벤토리, 1=크래프팅)에 따라 버튼 스타일 갱신 */
    void UpdateTabStyles(int32 ActiveIndex);
};
