// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryIconOverlay.generated.h"

class UCanvasPanel;
class UInventoryViewModel;

/**
 * UInventoryIconOverlay — 멀티셀 아이템 아이콘 전용 오버레이 위젯
 *
 * UniformGridPanel 위에 CanvasPanel을 겹쳐 배치하여
 * 아이템 실제 크기(W×H 칸)에 맞는 아이콘 이미지를 표시한다.
 */
UCLASS(Abstract)
class PROJECT_EXFIL_API UInventoryIconOverlay : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * ViewModel의 모든 아이템을 순회하여 루트 슬롯에 아이콘 Image 위젯 배치.
     * @param InViewModel  조회할 InventoryViewModel
     * @param InCellSize   슬롯 1칸의 픽셀 크기 (InventorySlotWidget::CellPixelSize)
     */
    void RefreshIcons(UInventoryViewModel* InViewModel, float InCellSize);

protected:
    /** WBP의 CanvasPanel — BindWidget으로 연결 */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCanvasPanel> IconCanvas;
};
