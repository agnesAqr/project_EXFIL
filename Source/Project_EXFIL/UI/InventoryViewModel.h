// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "UI/InventorySlotViewModel.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "InventoryViewModel.generated.h"

class UInventoryComponent;

/** PanelWidget이 아이콘 오버레이 갱신 시점을 알기 위한 델리게이트 */
DECLARE_MULTICAST_DELEGATE(FOnInventoryViewModelRefreshed);

UCLASS()
class PROJECT_EXFIL_API UInventoryViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    /** Model(UInventoryComponent)에 바인딩. 초기화 및 델리게이트 구독 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void Initialize(UInventoryComponent* InInventoryComponent);

    /** 특정 위치의 슬롯 ViewModel 반환 */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    UInventorySlotViewModel* GetSlotAt(FIntPoint Position) const;

    /** 모든 슬롯 ViewModel 반환 */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    TArray<UInventorySlotViewModel*> GetAllSlots() const;

    /** 그리드 너비 */
    UFUNCTION(BlueprintPure, FieldNotify)
    int32 GetGridWidth() const { return GridWidth; }

    /** 그리드 높이 */
    UFUNCTION(BlueprintPure, FieldNotify)
    int32 GetGridHeight() const { return GridHeight; }

    // === 사용자 액션 (View → ViewModel → Model) ===

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    bool RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated = false);

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    bool RequestRemoveItem(FGuid ItemInstanceID);

    /** 아이템 루트 위치 반환 (비루트 슬롯 드래그 오프셋 계산용) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    FIntPoint GetItemRootPosition(FGuid ItemInstanceID) const;

    /** 아이템의 유효 크기 반환 (회전 반영) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    FItemSize GetItemEffectiveSize(FGuid ItemInstanceID) const;

    /** 인벤토리 갱신 시 브로드캐스트 — IconOverlay 갱신용 */
    FOnInventoryViewModelRefreshed OnViewModelRefreshed;

private:
    UPROPERTY()
    TWeakObjectPtr<UInventoryComponent> InventoryComp;

    UPROPERTY()
    TArray<TObjectPtr<UInventorySlotViewModel>> SlotViewModels;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter,
              meta = (AllowPrivateAccess = true))
    int32 GridWidth = 0;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter,
              meta = (AllowPrivateAccess = true))
    int32 GridHeight = 0;

    // Model 델리게이트 콜백
    UFUNCTION()
    void OnInventoryUpdated();

    UFUNCTION()
    void OnItemAdded(const FInventoryItemInstance& AddedItem);

    UFUNCTION()
    void OnItemRemoved(const FGuid& RemovedItemID);

    /** 전체 그리드 상태를 SlotViewModels에 동기화 */
    void RefreshAllSlots();

    int32 PositionToIndex(FIntPoint Position) const;
};
