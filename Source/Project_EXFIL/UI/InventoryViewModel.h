// Copyright Project EXFIL. All Rights Reserved.
// InventoryViewModel.h - inventory MVVM view model that mirrors slot state
// and forwards user actions to the model layer.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "UI/InventorySlotViewModel.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "InventoryViewModel.generated.h"

class UInventoryComponent;

/** Broadcasts the updated slot indices for overlay refreshes. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryViewModelRefreshed, const TSet<int32>&);

UCLASS()
class PROJECT_EXFIL_API UInventoryViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    /** Bind to the inventory model and subscribe to change notifications. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void Initialize(UInventoryComponent* InInventoryComponent);

    /** Return the slot view model at the given grid position. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    UInventorySlotViewModel* GetSlotAt(FIntPoint Position) const;

    /** Return every slot view model. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    const TArray<UInventorySlotViewModel*>& GetAllSlots() const;

    /** Current grid width. */
    UFUNCTION(BlueprintPure, FieldNotify)
    int32 GetGridWidth() const { return GridWidth; }

    /** Current grid height. */
    UFUNCTION(BlueprintPure, FieldNotify)
    int32 GetGridHeight() const { return GridHeight; }

    // === User Actions (View -> ViewModel -> Model) ===

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition);

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void RequestRemoveItem(FGuid ItemInstanceID);

    /** Return the root position for an item instance. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    FIntPoint GetItemRootPosition(FGuid ItemInstanceID) const;

    /** Return the effective item size after rotation is applied. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    FItemSize GetItemEffectiveSize(FGuid ItemInstanceID) const;

    /** Broadcast after the cached slot state has been refreshed. */
    FOnInventoryViewModelRefreshed OnViewModelRefreshed;

private:
    UPROPERTY()
    TWeakObjectPtr<UInventoryComponent> InventoryComp;

    UPROPERTY()
    TArray<UInventorySlotViewModel*> SlotViewModels;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter,
              meta = (AllowPrivateAccess = true))
    int32 GridWidth = 0;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter,
              meta = (AllowPrivateAccess = true))
    int32 GridHeight = 0;

    // Model delegate callback.
    void HandleInventoryUpdated(const TSet<int32>& DirtyIndices);

    /** Rebuild the entire slot cache and broadcast every index as dirty. */
    void RefreshAllSlots();

    /** Refresh only the supplied indices and rebroadcast them. */
    void RefreshDirtySlots(const TSet<int32>& DirtyIndices);

    int32 PositionToIndex(FIntPoint Position) const;
};
