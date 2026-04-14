// Copyright Project EXFIL. All Rights Reserved.
// InventoryComponent.h — 그리드 인벤토리 데이터 관리: 아이템 추가/제거/이동, Bitmap 기반 배치, 리플리케이션

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EXFILInventoryTypes.h"
#include "InventoryComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryUpdated, const TSet<int32>&);

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
	int32 GridHeight = 12;

	/** 아이템 드랍 시 캐릭터 앞쪽 오프셋 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Config")
	float DropForwardOffset = 100.f;

	/** 아이템 드랍 시 위쪽 오프셋 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Config")
	float DropUpwardOffset = 50.f;

	// ========== 클라이언트 요청 API ==========
	// 비동기 요청. 최종 결과는 replication / OnRep / delegate로 반영됨.

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestRemoveItem(FGuid ItemInstanceID);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated = false);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestConsumeItemByID(FName ItemDataID, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestDropItem(FGuid ItemInstanceID);

	// ========== 서버 직접 실행 API ==========
	// EquipmentComponent, CraftingComponent, Character 등
	// 이미 서버 컨텍스트인 코드가 직접 호출하는 authoritative 함수.

	bool AddItemByID_Internal(FName ItemDataID, int32 StackCount = 1);

	bool RemoveItem_Internal(const FGuid& InstanceID);

	bool MoveItem_Internal(const FGuid& InstanceID, FIntPoint NewPosition, bool bNewRotated = false);

	bool ConsumeItemByID_Internal(FName ItemDataID, int32 Count = 1);

	bool DropItem_Internal(FGuid ItemInstanceID);

	/**
	 * 특정 아이템 인스턴스의 StackCount를 1 감소.
	 * 0이 되면 자동 RemoveItem_Internal. 서버 전용 헬퍼.
	 * @return 감소 후 남은 StackCount (제거됐으면 0)
	 */
	int32 DecrementStack_Internal(const FGuid& InstanceID);

	// ========== 쿼리 API ==========

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool CanPlaceItemAt(FIntPoint Position, FItemSize Size) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool FindFirstAvailableSlot(FItemSize Size, FIntPoint& OutPosition) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool GetItemAt(FIntPoint Position, FInventoryItemInstance& OutItem) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool GetItemByID(const FGuid& InstanceID, FInventoryItemInstance& OutItem) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	TArray<FInventoryItemInstance> GetAllItems() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool IsEmpty() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetItemCount(FName ItemDataID) const;

	// ========== 크래프팅/장비 연동 API ==========

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetItemCountByID(FName ItemDataID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetItemCountByID_Cached(FName ItemDataID) const;

	// ========== 유틸리티 ==========

	UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
	void DebugPrintGrid() const;

	// ========== 델리게이트 ==========
	FOnInventoryUpdated OnInventoryUpdated;

	// ========== Replication ==========
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	// ========== Server RPCs ==========
	// 외부 호출용이 아니라 Request*가 내부에서 호출하는 네트워크 경계

	UFUNCTION(Server, Reliable)
	void Server_RequestRemoveItem(FGuid ItemInstanceID);

	UFUNCTION(Server, Reliable)
	void Server_RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated);

	UFUNCTION(Server, Reliable)
	void Server_RequestConsumeItemByID(FName ItemDataID, int32 Count);

	UFUNCTION(Server, Reliable)
	void Server_RequestDropItem(FGuid ItemInstanceID);

	// ========== 내부 쓰기 헬퍼 ==========
	// InventoryComponent 내부에서만 쓰는 실제 구현 세부 함수

	bool AddItem_Internal(FName ItemDataID, FItemSize Size,
		int32 StackCount = 1, int32 MaxStack = 1);

	bool AddItemAt_Internal(FName ItemDataID, FItemSize Size,
		FIntPoint Position, bool bRotated = false,
		int32 StackCount = 1, int32 MaxStack = 1);

	// ========== Replicated 데이터 ==========

	UPROPERTY(Replicated)
	TArray<FInventorySlot> GridSlots;

	UPROPERTY(ReplicatedUsing = OnRep_Items)
	TArray<FInventoryItemInstance> Items;

	UFUNCTION()
	void OnRep_Items();

	// ========== 내부 헬퍼 ==========

	bool IsValidGridPosition(FIntPoint Position) const;
	int32 GridPositionToIndex(FIntPoint Position) const;
	FIntPoint IndexToGridPosition(int32 Index) const;

	bool AreSlotsFree(FIntPoint Position, FItemSize Size) const;
	void OccupySlots(FIntPoint Position, FItemSize Size, const FGuid& ItemID);
	void FreeSlots(const FInventoryItemInstance& Item);
	void InitializeGrid();

	UPROPERTY()
	TObjectPtr<class UItemDataSubsystem> CachedItemSub;

	TMap<FName, int32> ItemCountCache;
	TMap<FGuid, int32> ItemIndexMap;

	void RebuildItemIndexMap();

	TArray<uint16> RowBitmap;
	void RebuildRowBitmap();
	void SetBit(int32 Col, int32 Row, bool bOccupied);

	void RebuildItemCountCache();

	TSet<int32> DirtySlotIndices;
	void MarkSlotsDirty(FIntPoint Position, FItemSize Size);

	TArray<FInventoryItemInstance> PreviousItems;
	void BroadcastDirtySlots();

	FInventoryItemInstance* FindItemByInstanceID(const FGuid& InstanceID);
	const FInventoryItemInstance* FindItemByInstanceID(const FGuid& InstanceID) const;
	int32 FindItemIndexByInstanceID(const FGuid& InstanceID) const;
};
