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

	/** DataTable에서 크기·스택 정보를 자동 조회한 뒤 추가 (Day 3) */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryAddItemByID(FName ItemDataID, int32 StackCount = 1);

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

	// ========== Day 5 추가 API ==========

	/**
	 * 인벤토리에서 해당 ItemDataID 아이템을 Count만큼 스택 소비.
	 * 여러 슬롯에 분산된 경우 순서대로 소비.
	 * 스택이 0이 되는 인스턴스는 자동 제거.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ConsumeItemByID(FName ItemDataID, int32 Count = 1);

	/** 인벤토리 내 해당 ItemDataID의 총 스택 수 합산 반환 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetItemCountByID(FName ItemDataID) const;

	/**
	 * 특정 아이템 인스턴스의 StackCount를 1 감소.
	 * 0이 되면 자동 RemoveItem. 서버 전용.
	 * @return 감소 후 남은 StackCount (제거됐으면 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 DecrementStack(const FGuid& InstanceID);

	// ========== Server RPCs (Day 6) ==========

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_TryAddItemByID(FName ItemDataID, int32 StackCount);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RemoveItem(FGuid ItemInstanceID);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_MoveItem(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ConsumeItemByID(FName ItemDataID, int32 Count);

	/** 인벤토리 아이템을 월드에 드롭 — 아이템 제거 + AWorldItem 스폰 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_DropItem(FGuid ItemInstanceID);

	// ========== 델리게이트 ==========
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnInventoryUpdated OnInventoryUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemAdded OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemRemoved OnItemRemoved;

	// ========== Replication (Day 6) ==========
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	// ========== Replicated 데이터 (Day 6) ==========

	/** 1D 배열로 표현한 2D 그리드 (Index = Y * GridWidth + X) */
	UPROPERTY(Replicated)
	TArray<FInventorySlot> GridSlots;

	/** 인벤토리에 존재하는 모든 아이템 인스턴스 (TMap→TArray: TMap은 리플리케이션 불가) */
	UPROPERTY(ReplicatedUsing = OnRep_Items)
	TArray<FInventoryItemInstance> Items;

	UFUNCTION()
	void OnRep_Items();

	// ========== 내부 헬퍼 ==========
	bool IsValidGridPosition(FIntPoint Position) const;
	int32 GridPositionToIndex(FIntPoint Position) const;
	FIntPoint IndexToGridPosition(int32 Index) const;

	bool AreSlotsFree(FIntPoint Position, FItemSize Size) const;
	void OccupySlots(FIntPoint Position, FItemSize Size,
	                 const FGuid& ItemID);
	void FreeSlots(const FInventoryItemInstance& Item);
	void InitializeGrid();

	/** TArray에서 InstanceID로 아이템 검색 (TMap→TArray 전환 헬퍼) */
	FInventoryItemInstance* FindItemByInstanceID(const FGuid& InstanceID);
	const FInventoryItemInstance* FindItemByInstanceID(const FGuid& InstanceID) const;
	int32 FindItemIndexByInstanceID(const FGuid& InstanceID) const;
};
