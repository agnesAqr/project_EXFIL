// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryIconOverlay.generated.h"

class UCanvasPanel;
class UInventoryViewModel;
class UUniformGridPanel;

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
     * GridPanel의 실제 자식 슬롯 geometry에서 셀 크기/stride를 직접 계산하여
     * SlotPadding을 정확히 반영한다.
     * @param InViewModel   조회할 InventoryViewModel
     * @param InGridPanel   슬롯이 배치된 UniformGridPanel
     * @param InGridWidth   그리드 열 수
     * @param InGridHeight  그리드 행 수
     */
    void RefreshIcons(UInventoryViewModel* InViewModel, UUniformGridPanel* InGridPanel,
                      int32 InGridWidth, int32 InGridHeight);

protected:
    /** WBP의 CanvasPanel — BindWidget으로 연결 */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCanvasPanel> IconCanvas;
};
