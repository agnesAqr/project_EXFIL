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

    friend class UInventoryViewModel; // ViewModel만 데이터 세팅 가능

public:
    // === Getters (View가 바인딩) ===

    UFUNCTION(BlueprintPure, FieldNotify)
    bool IsEmpty() const { return bEmpty; }

    UFUNCTION(BlueprintPure, FieldNotify)
    FName GetItemDataID() const { return ItemDataID; }

    UFUNCTION(BlueprintPure, FieldNotify)
    int32 GetStackCount() const { return StackCount; }

    UFUNCTION(BlueprintPure, FieldNotify)
    bool IsRootSlot() const { return bIsRootSlot; }

    UFUNCTION(BlueprintPure, FieldNotify)
    FIntPoint GetGridPosition() const { return GridPosition; }

    UFUNCTION(BlueprintPure, FieldNotify)
    FGuid GetItemInstanceID() const { return ItemInstanceID; }

    UFUNCTION(BlueprintPure, FieldNotify)
    TSoftObjectPtr<UTexture2D> GetIcon() const { return Icon; }

    UFUNCTION(BlueprintPure, FieldNotify)
    int32 GetItemSizeX() const { return ItemSizeX; }

    UFUNCTION(BlueprintPure, FieldNotify)
    int32 GetItemSizeY() const { return ItemSizeY; }

    // === Request (View → ViewModel → Model) ===
    UFUNCTION(BlueprintCallable, Category = "Inventory|Request")
    void RequestDrop();

protected:
    // === Setters (UInventoryViewModel이 호출, View 접근 불가) ===
    void SetEmpty(bool bNewValue);
    void SetItemDataID(FName NewValue);
    void SetStackCount(int32 NewValue);
    void SetIsRootSlot(bool bNewValue);
    void SetGridPosition(FIntPoint NewValue);
    void SetItemInstanceID(FGuid NewValue);
    void SetIcon(TSoftObjectPtr<UTexture2D> NewValue);
    void SetItemSizeX(int32 NewValue);
    void SetItemSizeY(int32 NewValue);

private:
    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter = "IsEmpty", Setter = "SetEmpty",
              meta = (AllowPrivateAccess = true))
    bool bEmpty = true;

    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter = "SetItemDataID",
              meta = (AllowPrivateAccess = true))
    FName ItemDataID;

    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter = "SetStackCount",
              meta = (AllowPrivateAccess = true))
    int32 StackCount = 0;

    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter = "IsRootSlot", Setter = "SetIsRootSlot",
              meta = (AllowPrivateAccess = true))
    bool bIsRootSlot = false;

    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter = "SetGridPosition",
              meta = (AllowPrivateAccess = true))
    FIntPoint GridPosition = FIntPoint::ZeroValue;

    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter = "SetItemInstanceID",
              meta = (AllowPrivateAccess = true))
    FGuid ItemInstanceID;

    /** 아이콘 텍스처 — Getter 명시적 지정 (UHT GetbXxx 오탐 방지) */
    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter = "GetIcon", Setter = "SetIcon",
              meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UTexture2D> Icon;

    /** 아이템 그리드 크기 — 루트 슬롯에서 아이콘 크기 계산용 */
    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter = "GetItemSizeX", Setter = "SetItemSizeX",
              meta = (AllowPrivateAccess = true))
    int32 ItemSizeX = 1;

    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter = "GetItemSizeY", Setter = "SetItemSizeY",
              meta = (AllowPrivateAccess = true))
    int32 ItemSizeY = 1;
};
