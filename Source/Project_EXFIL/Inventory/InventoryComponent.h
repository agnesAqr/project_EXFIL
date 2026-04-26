// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EXFILInventoryTypes.h"
#include "InventoryComponent.generated.h"

class UInventoryComponent;

USTRUCT()
struct PROJECT_EXFIL_API FInventoryFastArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FInventoryItemInstance> Items;

	UInventoryComponent* OwnerComponent = nullptr;
	TSet<int32> PendingDirtyIndices;
	TSet<FName> PendingChangedItemDataIDs;

	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FInventoryItemInstance, FInventoryFastArray>(
			Items, DeltaParms, *this);
	}

	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);
};

template<>
struct TStructOpsTypeTraits<FInventoryFastArray> : public TStructOpsTypeTraitsBase2<FInventoryFastArray>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryUpdated, const TSet<int32>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryItemCountsChanged, const TSet<FName>&);

UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent))
class PROJECT_EXFIL_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

#pragma region Config
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Config",
		meta = (ClampMin = "1", ClampMax = "16", UIMin = "1", UIMax = "16"))
	int32 GridWidth = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Config",
		meta = (ClampMin = "1", UIMin = "1"))
	int32 GridHeight = 12;

	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Config")
	float DropForwardOffset = 100.f;

	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Config")
	float DropUpwardOffset = 50.f;
#pragma endregion

#pragma region Engine Lifecycle / Replication
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#pragma endregion

#pragma region External Entry: Request API
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestRemoveItem(FGuid ItemInstanceID);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated = false);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestConsumeItemByID(FName ItemDataID, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestDropItem(FGuid ItemInstanceID);
#pragma endregion

#pragma region Server Authority: Mutation API
	bool AddItemByID_Internal(FName ItemDataID, int32 StackCount = 1);
	bool AddItemByIDAt_Internal(FName ItemDataID, FIntPoint Position,
		bool bRotated = false, int32 StackCount = 1);

	bool RemoveItem_Internal(const FGuid& InstanceID);

	bool MoveItem_Internal(const FGuid& InstanceID, FIntPoint NewPosition, bool bNewRotated = false);

	bool ConsumeItemByID_Internal(FName ItemDataID, int32 Count = 1);

	bool DropItem_Internal(FGuid ItemInstanceID);

	
	int32 DecrementStack_Internal(const FGuid& InstanceID);
#pragma endregion

#pragma region Read Only: Query API
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool CanPlaceItemAt(FIntPoint Position, FItemSize Size) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool CanPlaceItemAtIgnoringInstance(FIntPoint Position, FItemSize Size,
		FGuid IgnoreInstanceID) const;

	
	void EnsureReplicatedCachesReady();

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
	int32 GetItemCountByID(FName ItemDataID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetItemCountByID_Cached(FName ItemDataID) const;
#pragma endregion

#pragma region Delegates
	FOnInventoryUpdated OnInventoryUpdated;
	FOnInventoryItemCountsChanged OnInventoryItemCountsChanged;
#pragma endregion

protected:
#pragma region Engine Lifecycle
	virtual void BeginPlay() override;
#pragma endregion

private:
	friend struct FInventoryFastArray;

#pragma region Network Bridge: Server RPC
	UFUNCTION(Server, Reliable)
	void Server_RequestRemoveItem(FGuid ItemInstanceID);

	UFUNCTION(Server, Reliable)
	void Server_RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated);

	UFUNCTION(Server, Reliable)
	void Server_RequestConsumeItemByID(FName ItemDataID, int32 Count);

	UFUNCTION(Server, Reliable)
	void Server_RequestDropItem(FGuid ItemInstanceID);
#pragma endregion

#pragma region Gameplay Integration: Consumable / World Interaction
	void HandleConsumeRequest_Internal(FName ItemDataID, int32 Count);
	void ApplyConsumableEffect_Internal(FName ItemDataID);
#pragma endregion

#pragma region Server Authority: Mutation Helpers
	bool AddItem_Internal(FName ItemDataID, FItemSize Size,
		int32 StackCount, int32 MaxStack,
		TSet<int32>& OutAffected);

	bool AddItemAt_Internal(FName ItemDataID, FItemSize Size,
		FIntPoint Position, bool bRotated,
		int32 StackCount, int32 MaxStack,
		TSet<int32>& OutAffected);
#pragma endregion

#pragma region State Sync / Cache Rebuild
	void HandleReplicatedInventoryReceived();
	void RebuildGridSlotsFromItems();
	void RebuildAllCachesFromItems();
	void BroadcastFullInventoryRefresh();
	void InitializeGridStorage();
#pragma endregion

#pragma region Incremental Cache Patch
	void ApplyItemAdded_Local(const FInventoryItemInstance& Item, int32 ItemIndex,
		TSet<int32>& OutAffected);
	void ApplyItemRemoved_Local(const FInventoryItemInstance& Item, int32 RemovedIndex,
		TSet<int32>& OutAffected);
	void ApplyItemMoved_Local(const FInventoryItemInstance& NewItem, FIntPoint OldPos,
		FItemSize OldEffSize, TSet<int32>& OutAffected);
	void ApplyItemStackChanged_Local(const FInventoryItemInstance& NewItem, int32 OldStackCount,
		TSet<int32>& OutAffected);
	void ApplyItemMovedByScan_Local(const FInventoryItemInstance& NewItem,
		TSet<int32>& OutAffected);
	bool DoesGridMatchItemFootprint(const FInventoryItemInstance& Item) const;
	void RecalculateItemCountForID(FName ItemDataID);
	void CollectFootprintIndices(FIntPoint Position, FItemSize Size,
		TSet<int32>& OutAffected) const;
#pragma endregion

#pragma region Replicated Data
	UPROPERTY(Replicated)
	FInventoryFastArray InventoryList;
#pragma endregion

#pragma region 2D Grid Helpers
	bool IsValidGridPosition(FIntPoint Position) const;
	int32 GridPositionToIndex(FIntPoint Position) const;
	FIntPoint IndexToGridPosition(int32 Index) const;

	bool AreSlotsFree(FIntPoint Position, FItemSize Size) const;
	bool AreSlotsFreeForItem(FIntPoint Position, FItemSize Size,
		const FGuid& IgnoreInstanceID) const;
	void OccupySlots(FIntPoint Position, FItemSize Size, const FGuid& ItemID);
	void FreeSlots(const FInventoryItemInstance& Item);
	void FreeSlotsAt(FIntPoint Position, FItemSize EffectiveSize);
#pragma endregion

#pragma region Cache / Lookup Data
	UPROPERTY()
	TObjectPtr<class UItemDataSubsystem> CachedItemSub;
	TArray<FInventorySlot> GridSlots;
	TMap<FName, int32> ItemCountCache;
	TMap<FGuid, int32> ItemIndexMap;

	void RebuildItemIndexMap();

	static constexpr int32 MaxGridBitmapWidth = 16;
	TArray<uint16> RowBitmap;
	void RebuildItemCountCache();
	void SetBit(int32 Col, int32 Row, bool bOccupied);
	bool bCachesInitialized = false;
#pragma endregion

#pragma region Item Lookup Helpers
	FInventoryItemInstance* FindItemByInstanceID(const FGuid& InstanceID);
	const FInventoryItemInstance* FindItemByInstanceID(const FGuid& InstanceID) const;
	int32 FindItemIndexByInstanceID(const FGuid& InstanceID) const;
#pragma endregion
};
