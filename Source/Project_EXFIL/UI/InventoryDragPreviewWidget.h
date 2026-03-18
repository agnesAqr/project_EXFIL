// Copyright Project EXFIL. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryDragPreviewWidget.generated.h"

UCLASS()
class PROJECT_EXFIL_API UInventoryDragPreviewWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    /** CreateWidget 직후 호출 — Width×Height 크기의 드래그 프리뷰 그리드 빌드 */
    void BuildPreview(int32 Width, int32 Height);
};
