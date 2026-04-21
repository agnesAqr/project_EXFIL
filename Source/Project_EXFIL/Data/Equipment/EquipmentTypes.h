// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "EquipmentTypes.generated.h"

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

USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FEquipmentSlotData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EEquipmentSlot SlotType = EEquipmentSlot::None;

    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGuid EquippedItemID;

    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FInventoryItemInstance ItemInstance;

    
    UPROPERTY(NotReplicated)
    FActiveGameplayEffectHandle ActiveGEHandle;

    FEquipmentSlotData() = default;
    explicit FEquipmentSlotData(EEquipmentSlot InSlotType)
        : SlotType(InSlotType) {}

    bool IsEmpty() const { return !EquippedItemID.IsValid(); }
};
