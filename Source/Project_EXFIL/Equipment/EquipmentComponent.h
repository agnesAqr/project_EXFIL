// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "Equipment/EquipmentTypes.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "EquipmentComponent.generated.h"

class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnEquipmentChanged,
    EEquipmentSlot, Slot,
    const FInventoryItemInstance&, Item);

/**
 * UEquipmentComponent — Head / Body / Weapon 슬롯 장비 관리
 *
 * EquipItem 시 FItemData::EquipmentEffect(Infinite Duration GE)를 ASC에 Apply.
 * UnequipItem 시 저장된 핸들로 GE 제거.
 * 소유 Actor의 AbilitySystemComponent를 GetOwner()->FindComponentByClass로 획득.
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class PROJECT_EXFIL_API UEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEquipmentComponent();

    /**
     * 슬롯에 아이템 장착.
     * 이미 점유된 경우 기존 아이템을 먼저 해제 후 장착.
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool EquipItem(EEquipmentSlot Slot, const FInventoryItemInstance& ItemInstance);

    /** 슬롯 아이템 해제 */
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool UnequipItem(EEquipmentSlot Slot);

    /** 슬롯에 장착된 아이템 반환 */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
    bool GetEquippedItem(EEquipmentSlot Slot, FInventoryItemInstance& OutItem) const;

    /** 슬롯 점유 여부 */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
    bool IsSlotOccupied(EEquipmentSlot Slot) const;

    UPROPERTY(BlueprintAssignable, Category = "Equipment|Events")
    FOnEquipmentChanged OnItemEquipped;

    UPROPERTY(BlueprintAssignable, Category = "Equipment|Events")
    FOnEquipmentChanged OnItemUnequipped;

private:
    UPROPERTY()
    TMap<EEquipmentSlot, FInventoryItemInstance> EquippedItems;

    UPROPERTY()
    TMap<EEquipmentSlot, FActiveGameplayEffectHandle> ActiveEffectHandles;

    /** EquipmentEffect(Infinite Duration GE) 로드 및 ASC Apply */
    void ApplyEquipmentEffect(EEquipmentSlot Slot, const FInventoryItemInstance& Item);

    /** ASC에서 저장된 핸들로 GE 제거 */
    void RemoveEquipmentEffect(EEquipmentSlot Slot);

    /** 소유 Actor의 ASC 획득 헬퍼 */
    UAbilitySystemComponent* GetASC() const;
};
