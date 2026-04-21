// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "Data/Equipment/EquipmentTypes.h"
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

    UPROPERTY(BlueprintReadWrite)
    bool bOriginalRotated = false;

    
    UPROPERTY(BlueprintReadWrite)
    FIntPoint DragOffset = FIntPoint(0, 0);

    UPROPERTY(BlueprintReadWrite)
    FItemSize ItemSize;

    
    UPROPERTY(BlueprintReadWrite)
    FItemSize DragItemSize;

    UPROPERTY(BlueprintReadWrite)
    bool bIsRotated = false;

    UPROPERTY(BlueprintReadWrite)
    FName ItemDataID;

    
    UPROPERTY(BlueprintReadWrite)
    bool bFromEquipment = false;

    
    UPROPERTY(BlueprintReadWrite)
    EEquipmentSlot SourceEquipmentSlot = EEquipmentSlot::None;
};
