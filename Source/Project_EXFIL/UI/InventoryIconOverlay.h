// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "InventoryIconOverlay.generated.h"

class UCanvasPanel;
class UInventoryViewModel;
class UInventorySlotViewModel;
class UUniformGridPanel;
class UItemContextMenuWidget;
class UInventoryPanelWidget;

/**
 * UInventoryIconOverlay — 멀티셀 아이템 아이콘 전용 오버레이 위젯
 *
 * UniformGridPanel 위에 CanvasPanel을 겹쳐 배치하여
 * 아이템 실제 크기(W×H 칸)에 맞는 아이콘 이미지를 표시한다.
 *
 * Day 7: 우클릭 → 컨텍스트 메뉴 (Use / Equip / Drop)
 */
UCLASS(Abstract)
class PROJECT_EXFIL_API UInventoryIconOverlay : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * ViewModel의 모든 아이템을 순회하여 루트 슬롯에 아이콘 Image 위젯 배치.
     * 내부적으로 ViewModel/GridPanel/Dimensions를 캐싱하여 우클릭 HitTest에 활용.
     * @param InViewModel   조회할 InventoryViewModel
     * @param InGridPanel   슬롯이 배치된 UniformGridPanel
     * @param InGridWidth   그리드 열 수
     * @param InGridHeight  그리드 행 수
     */
    void RefreshIcons(UInventoryViewModel* InViewModel, UUniformGridPanel* InGridPanel,
                      int32 InGridWidth, int32 InGridHeight);

    /** 컨텍스트 메뉴 위젯 클래스 — BP 디테일에서 할당 */
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UItemContextMenuWidget> ContextMenuWidgetClass;

    /** 외부(InventoryPanelWidget 등)에서 컨텍스트 메뉴가 열려있으면 닫기 */
    void CloseContextMenuIfOpen();

    /** InventoryPanelWidget에서 BuildGrid 후 주입 — 드롭 라우팅에 필요 */
    void SetParentPanel(UInventoryPanelWidget* InPanel);

protected:
    virtual void NativeOnInitialized() override;

    /** WBP의 CanvasPanel — BindWidget으로 연결 */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCanvasPanel> IconCanvas;

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    virtual void NativeOnDragDetected(const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

    virtual bool NativeOnDrop(const FGeometry& InGeometry,
        const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    virtual bool NativeOnDragOver(const FGeometry& InGeometry,
        const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent,
        UDragDropOperation* InOperation) override;

private:
    // 우클릭 HitTest용 캐시
    UPROPERTY()
    TObjectPtr<UInventoryViewModel> CachedViewModel;

    UPROPERTY()
    TWeakObjectPtr<UUniformGridPanel> CachedGridPanel;

    int32 CachedGridWidth = 0;
    int32 CachedGridHeight = 0;

    /** 지연 생성되는 컨텍스트 메뉴 인스턴스 */
    UPROPERTY()
    TObjectPtr<UItemContextMenuWidget> ContextMenuWidget;

    /** 드래그 시작 시 캐싱 — NativeOnDragDetected에서 DragOp 생성용 */
    FGuid PendingDragInstanceID;

    /** 클릭 지점 로컬 좌표 — DragOffset 계산용 */
    FVector2D PendingDragClickLocalPos = FVector2D::ZeroVector;

    /** 드롭 라우팅 및 하이라이트용 부모 패널 참조 */
    UPROPERTY()
    TWeakObjectPtr<UInventoryPanelWidget> ParentPanel;

    /**
     * LocalPos(캔버스 좌표)에 해당하는 인벤토리 아이템 검색.
     * @param OutInstanceID  찾은 아이템의 InstanceID
     * @param OutItemDataID  찾은 아이템의 ItemDataID
     * @return 찾으면 true
     */
    bool FindItemAtPosition(const FVector2D& LocalPos,
                            FGuid& OutInstanceID, FName& OutItemDataID) const;

    /** 컨텍스트 메뉴를 지연 생성하거나 기존 것을 반환 */
    UItemContextMenuWidget* GetOrCreateContextMenu();
};
