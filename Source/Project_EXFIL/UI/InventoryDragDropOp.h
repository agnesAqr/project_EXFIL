// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "InventoryDragDropOp.generated.h"

UCLASS()
class PROJECT_EXFIL_API UInventoryDragDropOp : public UDragDropOperation
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite)
    FGuid DraggedItemInstanceID;

    UPROPERTY(BlueprintReadWrite)
    FIntPoint OriginalPosition;

    /** 드래그한 슬롯과 아이템 루트 간의 오프셋 (비루트 슬롯 드래그 시 역산용) */
    UPROPERTY(BlueprintReadWrite)
    FIntPoint DragOffset = FIntPoint(0, 0);

    UPROPERTY(BlueprintReadWrite)
    FItemSize ItemSize;

    /** 드래그 중인 아이템의 유효 크기 (회전 반영) */
    UPROPERTY(BlueprintReadWrite)
    FItemSize DragItemSize;

    UPROPERTY(BlueprintReadWrite)
    bool bWasRotated = false;

    UPROPERTY(BlueprintReadWrite)
    FName ItemDataID;
};
