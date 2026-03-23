// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
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

/**
 * FEquipmentSlotData — 장비 슬롯 상태 (Day 6: TMap→TArray 변환용)
 *
 * 리플리케이션 대상. ActiveGEHandle은 서버 전용(NotReplicated).
 */
USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FEquipmentSlotData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EEquipmentSlot SlotType = EEquipmentSlot::None;

    /** 장착된 아이템의 InstanceID (Invalid = 빈 슬롯) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGuid EquippedItemID;

    /** 장착된 아이템 인스턴스 (EquippedItemID가 Valid일 때만 유효) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FInventoryItemInstance ItemInstance;

    /** 서버 전용 — GE Handle은 리플리케이션 제외 */
    UPROPERTY(NotReplicated)
    FActiveGameplayEffectHandle ActiveGEHandle;

    FEquipmentSlotData() = default;
    explicit FEquipmentSlotData(EEquipmentSlot InSlotType)
        : SlotType(InSlotType) {}

    bool IsEmpty() const { return !EquippedItemID.IsValid(); }
};
