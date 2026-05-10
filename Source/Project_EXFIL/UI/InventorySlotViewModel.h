// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Inventory/EXFILInventoryTypes.h"

class UTexture2D;

#include "InventorySlotViewModel.generated.h"

UCLASS()
class PROJECT_EXFIL_API UInventorySlotViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

    friend class UInventoryViewModel;

public:

    UFUNCTION(BlueprintPure, FieldNotify)
    bool IsEmpty() const { return bEmpty; }

    UFUNCTION(BlueprintPure)
    FName GetItemDataID() const { return ItemDataID; }

    UFUNCTION(BlueprintPure)
    int32 GetStackCount() const { return StackCount; }

    UFUNCTION(BlueprintPure)
    bool IsRootSlot() const { return bIsRootSlot; }

    UFUNCTION(BlueprintPure)
    FIntPoint GetGridPosition() const { return GridPosition; }

    UFUNCTION(BlueprintPure)
    FGuid GetItemInstanceID() const { return ItemInstanceID; }

    UFUNCTION(BlueprintPure)
    TSoftObjectPtr<UTexture2D> GetIcon() const { return Icon; }

    UFUNCTION(BlueprintPure)
    int32 GetItemSizeX() const { return ItemSizeX; }

    UFUNCTION(BlueprintPure)
    int32 GetItemSizeY() const { return ItemSizeY; }

    UFUNCTION(BlueprintPure)
    bool IsRotated() const { return bRotated; }
    UFUNCTION(BlueprintCallable, Category = "Inventory|Request")
    void RequestDrop();

protected:
    void SetEmpty(bool bNewValue);
    void SetItemDataID(FName NewValue) { ItemDataID = NewValue; }
    void SetStackCount(int32 NewValue) { StackCount = NewValue; }
    void SetIsRootSlot(bool bNewValue) { bIsRootSlot = bNewValue; }
    void SetGridPosition(FIntPoint NewValue) { GridPosition = NewValue; }
    void SetItemInstanceID(FGuid NewValue) { ItemInstanceID = NewValue; }
    void SetIcon(TSoftObjectPtr<UTexture2D> NewValue) { Icon = NewValue; }
    void SetItemSizeX(int32 NewValue) { ItemSizeX = NewValue; }
    void SetItemSizeY(int32 NewValue) { ItemSizeY = NewValue; }
    void SetRotated(bool bNewValue) { bRotated = bNewValue; }
private:
    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter = "IsEmpty", Setter = "SetEmpty",
              meta = (AllowPrivateAccess = true))
    bool bEmpty = true;

    UPROPERTY(BlueprintReadWrite, Getter, Setter = "SetItemDataID",
              meta = (AllowPrivateAccess = true))
    FName ItemDataID;

    UPROPERTY(BlueprintReadWrite, Getter, Setter = "SetStackCount",
              meta = (AllowPrivateAccess = true))
    int32 StackCount = 0;

    UPROPERTY(BlueprintReadWrite, Getter = "IsRootSlot", Setter = "SetIsRootSlot",
              meta = (AllowPrivateAccess = true))
    bool bIsRootSlot = false;

    UPROPERTY(BlueprintReadWrite, Getter, Setter = "SetGridPosition",
              meta = (AllowPrivateAccess = true))
    FIntPoint GridPosition = FIntPoint::ZeroValue;

    UPROPERTY(BlueprintReadWrite, Getter, Setter = "SetItemInstanceID",
              meta = (AllowPrivateAccess = true))
    FGuid ItemInstanceID;

    
    UPROPERTY(BlueprintReadWrite, Getter = "GetIcon", Setter = "SetIcon",
              meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UTexture2D> Icon;

    
    UPROPERTY(BlueprintReadWrite, Getter = "GetItemSizeX", Setter = "SetItemSizeX",
              meta = (AllowPrivateAccess = true))
    int32 ItemSizeX = 1;

    UPROPERTY(BlueprintReadWrite, Getter = "GetItemSizeY", Setter = "SetItemSizeY",
              meta = (AllowPrivateAccess = true))
    int32 ItemSizeY = 1;

    UPROPERTY(BlueprintReadWrite, Getter = "IsRotated", Setter = "SetRotated",
              meta = (AllowPrivateAccess = true))
    bool bRotated = false;

};
