// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "EquipmentTypes.generated.h"

/** 장비 슬롯 종류 */
UENUM(BlueprintType)
enum class EEquipmentSlot : uint8
{
    None     UMETA(DisplayName = "None"),
    Head     UMETA(DisplayName = "Head"),
    Face     UMETA(DisplayName = "Face"),
    Eyewear  UMETA(DisplayName = "Eyewear"),
    Body     UMETA(DisplayName = "Body"),
    Weapon1  UMETA(DisplayName = "Weapon 1"),
    Weapon2  UMETA(DisplayName = "Weapon 2")
};

/** 슬롯 UI에 전달되는 데이터 (EquipmentSlotWidget::RefreshSlot 인자) */
USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FEquipmentSlotData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    EEquipmentSlot SlotType = EEquipmentSlot::None;

    UPROPERTY(BlueprintReadOnly)
    bool bIsOccupied = false;

    /** 장착된 아이템 (bIsOccupied == false 이면 무효) */
    UPROPERTY(BlueprintReadOnly)
    FInventoryItemInstance ItemInstance;
};
