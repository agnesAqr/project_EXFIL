// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EXFILInventoryTypes.h"
#include "InventoryComponent.generated.h"

// 델리게이트 선언 (Day 2 ViewModel 바인딩용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnItemAdded, const FInventoryItemInstance&, AddedItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnItemRemoved, const FGuid&, RemovedItemID);

UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent))
class PROJECT_EXFIL_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	// ========== 설정 ==========
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Config")
	int32 GridWidth = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Config")
	int32 GridHeight = 6;

	// ========== 핵심 API ==========

	/** 자동 빈칸 탐색 후 아이템 추가 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryAddItem(FName ItemDataID, FItemSize Size,
	                int32 StackCount = 1, int32 MaxStack = 1);

	/** 지정 위치에 아이템 배치 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryAddItemAt(FName ItemDataID, FItemSize Size,
	                  FIntPoint Position, bool bRotated = false,
	                  int32 StackCount = 1, int32 MaxStack = 1);

	/** 아이템 제거 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(const FGuid& InstanceID);

	/** 아이템 이동 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool MoveItem(const FGuid& InstanceID, FIntPoint NewPosition,
	              bool bNewRotated = false);

	// ========== 쿼리 API ==========

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool CanPlaceItemAt(FIntPoint Position, FItemSize Size) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool FindFirstAvailableSlot(FItemSize Size,
	                            FIntPoint& OutPosition) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool GetItemAt(FIntPoint Position,
	               FInventoryItemInstance& OutItem) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool GetItemByID(const FGuid& InstanceID,
	                 FInventoryItemInstance& OutItem) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	TArray<FInventoryItemInstance> GetAllItems() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool IsEmpty() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetItemCount(FName ItemDataID) const;

	// ========== 유틸리티 ==========

	UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
	void DebugPrintGrid() const;

	// ========== 델리게이트 ==========
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnInventoryUpdated OnInventoryUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemAdded OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemRemoved OnItemRemoved;

protected:
	virtual void BeginPlay() override;

private:
	// ========== 내부 데이터 ==========

	/** 1D 배열로 표현한 2D 그리드 (Index = Y * GridWidth + X) */
	UPROPERTY()
	TArray<FInventorySlot> GridSlots;

	/** 인벤토리에 존재하는 모든 아이템 인스턴스 */
	UPROPERTY()
	TMap<FGuid, FInventoryItemInstance> ItemInstances;

	// ========== 내부 헬퍼 ==========
	bool IsValidGridPosition(FIntPoint Position) const;
	int32 GridPositionToIndex(FIntPoint Position) const;
	FIntPoint IndexToGridPosition(int32 Index) const;

	bool AreSlotsFree(FIntPoint Position, FItemSize Size) const;
	void OccupySlots(FIntPoint Position, FItemSize Size,
	                 const FGuid& ItemID);
	void FreeSlots(const FInventoryItemInstance& Item);
	void InitializeGrid();
};
