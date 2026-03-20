// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EquipmentTypes.generated.h"

/** 장비 슬롯 종류 */
UENUM(BlueprintType)
enum class EEquipmentSlot : uint8
{
    None    UMETA(DisplayName = "None"),
    Head    UMETA(DisplayName = "Head"),
    Body    UMETA(DisplayName = "Body"),
    Weapon  UMETA(DisplayName = "Weapon")
};
