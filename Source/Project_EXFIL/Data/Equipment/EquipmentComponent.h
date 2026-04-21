// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/Equipment/EquipmentTypes.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "EquipmentComponent.generated.h"

class UAbilitySystemComponent;
class UItemDataSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnEquipmentChanged,
    EEquipmentSlot, Slot,
    const FInventoryItemInstance&, Item);

UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class PROJECT_EXFIL_API UEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEquipmentComponent();

    // ========== Request API ==========

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void RequestEquipFromInventory(EEquipmentSlot Slot, FGuid ItemInstanceID);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void RequestUnequipToInventory(EEquipmentSlot Slot);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void RequestUnequipToInventoryAt(EEquipmentSlot Slot, FIntPoint Position, bool bRotated = false);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void RequestDropEquippedItem(EEquipmentSlot Slot);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
    bool GetEquippedItem(EEquipmentSlot Slot, FInventoryItemInstance& OutItem) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
    bool IsSlotOccupied(EEquipmentSlot Slot) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
    bool HasWeaponEquipped() const;

    // ========== Server RPCs ==========

    EEquipmentSlot FindTargetSlot(const FName& EquipmentSlotTag) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
    static EEquipmentSlot SlotTagToEnum(FName SlotTag);

    UPROPERTY(BlueprintAssignable, Category = "Equipment|Events")
    FOnEquipmentChanged OnItemEquipped;

    UPROPERTY(BlueprintAssignable, Category = "Equipment|Events")
    FOnEquipmentChanged OnItemUnequipped;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

private:
    UFUNCTION(Server, Reliable)
    void Server_RequestEquipFromInventory(EEquipmentSlot Slot, FGuid ItemInstanceID);

    UFUNCTION(Server, Reliable)
    void Server_RequestUnequipToInventory(EEquipmentSlot Slot);

    UFUNCTION(Server, Reliable)
    void Server_RequestUnequipToInventoryAt(EEquipmentSlot Slot, FIntPoint Position, bool bRotated);

    UFUNCTION(Server, Reliable)
    void Server_RequestDropEquippedItem(EEquipmentSlot Slot);

    // ========== Internal Write API ==========

    bool EquipItem_Internal(EEquipmentSlot Slot, const FInventoryItemInstance& ItemInstance);
    bool UnequipItem_Internal(EEquipmentSlot Slot);
    bool EquipFromInventory_Internal(EEquipmentSlot Slot, FGuid ItemInstanceID);
    bool UnequipToInventory_Internal(EEquipmentSlot Slot);
    bool UnequipToInventoryAt_Internal(EEquipmentSlot Slot, FIntPoint Position, bool bRotated);
    bool DropEquippedItem_Internal(EEquipmentSlot Slot);

    UPROPERTY(ReplicatedUsing = OnRep_Slots)
    TArray<FEquipmentSlotData> ReplicatedSlots;

    UFUNCTION()
    void OnRep_Slots();

    void InitializeSlots();
    void InitializeSlotMapping();

    TMap<FName, TArray<EEquipmentSlot>> SlotTagToCandidates;

    FEquipmentSlotData* FindSlotData(EEquipmentSlot SlotType);
    const FEquipmentSlotData* FindSlotData(EEquipmentSlot SlotType) const;

    TMap<EEquipmentSlot, int32> SlotIndexMap;
    TArray<FEquipmentSlotData> PrevReplicatedSlots;

    void RebuildSlotIndexMap();
    void ApplyEquipmentEffect(FEquipmentSlotData& SlotData, const FInventoryItemInstance& Item);
    void RemoveEquipmentEffect(FEquipmentSlotData& SlotData);
    UAbilitySystemComponent* GetASC() const;

    UPROPERTY()
    TObjectPtr<UItemDataSubsystem> CachedItemSub;
};
