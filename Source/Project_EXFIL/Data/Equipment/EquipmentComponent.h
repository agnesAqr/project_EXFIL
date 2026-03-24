// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/Equipment/EquipmentTypes.h"
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
 * Day 6: TMap→TArray 변환 + 리플리케이션 + Server RPC
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

    // ========== Server RPCs (Day 6) ==========

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_EquipItem(EEquipmentSlot Slot, FInventoryItemInstance ItemInstance);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_UnequipItem(EEquipmentSlot Slot);

    // ========== 복합 Server RPCs (핫픽스 A: 드래그 장착/해제) ==========

    /** 인벤토리에서 아이템 제거 + 장비 슬롯에 장착 (원자적 서버 연산) */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_EquipFromInventory(EEquipmentSlot Slot, FGuid ItemInstanceID);

    /** 장비 해제 + 인벤토리에 아이템 추가 (원자적 서버 연산) */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_UnequipToInventory(EEquipmentSlot Slot);

    /** 장비 해제 + 월드 드롭 (원자적 서버 연산) */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_DropEquippedItem(EEquipmentSlot InSlot);

    /**
     * EquipmentSlotTag → 빈 슬롯 자동 탐색.
     * 태그 하나에 복수 슬롯(예: Weapon→Weapon1,Weapon2)이 대응될 수 있으며,
     * 빈 슬롯을 우선 반환하고 없으면 첫 번째 후보(스왑 대상)를 반환.
     */
    EEquipmentSlot FindTargetSlot(const FName& EquipmentSlotTag) const;

    /** @deprecated SlotTagToEnum은 Weapon 등 1:N 태그를 지원하지 않음. FindTargetSlot 사용 권장. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
    static EEquipmentSlot SlotTagToEnum(FName SlotTag);

    // ========== 델리게이트 ==========

    UPROPERTY(BlueprintAssignable, Category = "Equipment|Events")
    FOnEquipmentChanged OnItemEquipped;

    UPROPERTY(BlueprintAssignable, Category = "Equipment|Events")
    FOnEquipmentChanged OnItemUnequipped;

    // ========== Replication (Day 6) ==========
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

private:
    /** Replicated 슬롯 배열 (TMap→TArray: TMap은 리플리케이션 불가) */
    UPROPERTY(ReplicatedUsing = OnRep_Slots)
    TArray<FEquipmentSlotData> ReplicatedSlots;

    UFUNCTION()
    void OnRep_Slots();

    /** 슬롯 초기화 */
    void InitializeSlots();

    /** TArray에서 슬롯 검색 헬퍼 */
    FEquipmentSlotData* FindSlotData(EEquipmentSlot SlotType);
    const FEquipmentSlotData* FindSlotData(EEquipmentSlot SlotType) const;

    /** EquipmentEffect(Infinite Duration GE) 로드 및 ASC Apply */
    void ApplyEquipmentEffect(FEquipmentSlotData& SlotData, const FInventoryItemInstance& Item);

    /** ASC에서 저장된 핸들로 GE 제거 */
    void RemoveEquipmentEffect(FEquipmentSlotData& SlotData);

    /** 소유 Actor의 ASC 획득 헬퍼 */
    UAbilitySystemComponent* GetASC() const;
};
