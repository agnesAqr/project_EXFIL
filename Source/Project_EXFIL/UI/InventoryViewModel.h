// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "UI/InventorySlotViewModel.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "InventoryViewModel.generated.h"

class UInventoryComponent;
class UItemDataSubsystem;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryViewModelRefreshed, const TSet<int32>&);

UCLASS()
class PROJECT_EXFIL_API UInventoryViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    
    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void Initialize(UInventoryComponent* InInventoryComponent);

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    UInventorySlotViewModel* GetSlotAt(FIntPoint Position) const;

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    const TArray<UInventorySlotViewModel*>& GetAllSlots() const;

    
    UFUNCTION(BlueprintPure, FieldNotify)
    int32 GetGridWidth() const { return GridWidth; }

    
    UFUNCTION(BlueprintPure, FieldNotify)
    int32 GetGridHeight() const { return GridHeight; }

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated = false);

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void RequestRemoveItem(FGuid ItemInstanceID);

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    FIntPoint GetItemRootPosition(FGuid ItemInstanceID) const;

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    FItemSize GetItemEffectiveSize(FGuid ItemInstanceID) const;

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    bool IsItemRotated(FGuid ItemInstanceID) const;

    
    FOnInventoryViewModelRefreshed OnViewModelRefreshed;

private:
    UPROPERTY()
    TWeakObjectPtr<UInventoryComponent> InventoryComp;

    UPROPERTY()
    TWeakObjectPtr<UItemDataSubsystem> CachedItemSub;

    UPROPERTY()
    TArray<UInventorySlotViewModel*> SlotViewModels;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter,
              meta = (AllowPrivateAccess = true))
    int32 GridWidth = 0;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter,
              meta = (AllowPrivateAccess = true))
    int32 GridHeight = 0;
    void HandleInventoryUpdated(const TSet<int32>& DirtyIndices);

    
    void RefreshAllSlots();

    
    void RefreshDirtySlots(const TSet<int32>& DirtyIndices);

    int32 PositionToIndex(FIntPoint Position) const;
};
