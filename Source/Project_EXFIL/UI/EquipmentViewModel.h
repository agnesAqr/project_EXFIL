// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Data/Equipment/EquipmentTypes.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "UI/InventoryViewModel.h"
#include "EquipmentViewModel.generated.h"

class UEquipmentComponent;
class UItemDataSubsystem;
class UTexture2D;

USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FEquipmentSlotViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    EEquipmentSlot SlotType = EEquipmentSlot::None;

    UPROPERTY(BlueprintReadOnly)
    bool bEquipped = false;

    UPROPERTY(BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UTexture2D> Icon = nullptr;
};

USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FEquipmentDragSourceViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid ItemInstanceID;

    UPROPERTY(BlueprintReadOnly)
    EEquipmentSlot SourceSlot = EEquipmentSlot::None;

    UPROPERTY(BlueprintReadOnly)
    FItemSize BaseItemSize;

    UPROPERTY(BlueprintReadOnly)
    FItemSize InitialDragItemSize;

    UPROPERTY(BlueprintReadOnly)
    bool bOriginalRotated = false;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(
    FOnEquipmentSlotViewDataChanged, EEquipmentSlot, const FEquipmentSlotViewData&);

UCLASS()
class PROJECT_EXFIL_API UEquipmentViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Equipment|ViewModel")
    void Initialize(UEquipmentComponent* InEquipmentComponent);

    virtual void BeginDestroy() override;

    UFUNCTION(BlueprintCallable, Category = "Equipment|ViewModel")
    void RequestEquip(EEquipmentSlot Slot, FGuid ItemInstanceID);

    UFUNCTION(BlueprintCallable, Category = "Equipment|ViewModel")
    void RequestUnequip(EEquipmentSlot Slot);

    UFUNCTION(BlueprintCallable, Category = "Equipment|ViewModel")
    void RequestUnequipAt(EEquipmentSlot Slot, FIntPoint Position, bool bRotated = false);

    UFUNCTION(BlueprintCallable, Category = "Equipment|ViewModel")
    void RequestDropEquipped(EEquipmentSlot Slot);

    bool GetSlotViewData(EEquipmentSlot Slot, FEquipmentSlotViewData& OutViewData) const;
    bool TryGetContextMenuViewDataForSlot(
        EEquipmentSlot Slot, FItemContextMenuViewData& OutViewData) const;
    bool TryBuildDragSourceForSlot(
        EEquipmentSlot Slot, FEquipmentDragSourceViewData& OutViewData) const;

    FOnEquipmentSlotViewDataChanged OnEquipmentSlotChanged;

private:
    UPROPERTY()
    TWeakObjectPtr<UEquipmentComponent> EquipmentComp;

    UPROPERTY()
    TWeakObjectPtr<UItemDataSubsystem> CachedItemSub;

    void HandleItemEquipped(EEquipmentSlot Slot, const FInventoryItemInstance& Item);

    void HandleItemUnequipped(EEquipmentSlot Slot, const FInventoryItemInstance& Item);

    FDelegateHandle ItemEquippedHandle;
    FDelegateHandle ItemUnequippedHandle;

    void BroadcastSlot(EEquipmentSlot Slot);
    void UnbindEquipmentComponent();
};
