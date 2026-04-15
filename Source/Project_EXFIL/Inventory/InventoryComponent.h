// Copyright Project EXFIL. All Rights Reserved.
// InventoryComponent.h ??洹몃━???몃깽?좊━ ?곗씠??愿由? ?꾩씠??異붽?/?쒓굅/?대룞, Bitmap 湲곕컲 諛곗튂, FastArray 由ы뵆由ъ??댁뀡

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

UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent))
class PROJECT_EXFIL_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	// ========== ?ㅼ젙 ==========

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Config")
	int32 GridWidth = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Config")
	int32 GridHeight = 12;

	/** ?꾩씠???쒕엻 ??罹먮┃???욎そ ?ㅽ봽??(cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Config")
	float DropForwardOffset = 100.f;

	/** ?꾩씠???쒕엻 ???꾩そ ?ㅽ봽??(cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Config")
	float DropUpwardOffset = 50.f;

	// ========== ?대씪?댁뼵???붿껌 API ==========

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestRemoveItem(FGuid ItemInstanceID);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestConsumeItemByID(FName ItemDataID, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestDropItem(FGuid ItemInstanceID);

	// ========== ?쒕쾭 吏곸젒 ?ㅽ뻾 API ==========

	bool AddItemByID_Internal(FName ItemDataID, int32 StackCount = 1);

	bool RemoveItem_Internal(const FGuid& InstanceID);

	bool MoveItem_Internal(const FGuid& InstanceID, FIntPoint NewPosition);

	bool ConsumeItemByID_Internal(FName ItemDataID, int32 Count = 1);

	bool DropItem_Internal(FGuid ItemInstanceID);

	/**
	 * ?뱀젙 ?꾩씠???몄뒪?댁뒪??StackCount瑜?1 媛먯냼.
	 * 0???섎㈃ ?먮룞 RemoveItem_Internal. ?쒕쾭 ?꾩슜 ?ы띁.
	 * @return 媛먯냼 ???⑥? StackCount (?쒓굅?먯쑝硫?0)
	 */
	int32 DecrementStack_Internal(const FGuid& InstanceID);

	// ========== 荑쇰━ API ==========

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

	// ========== ?щ옒?꾪똿/?λ퉬 ?곕룞 API ==========

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetItemCountByID(FName ItemDataID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetItemCountByID_Cached(FName ItemDataID) const;

	// ========== ?좏떥由ы떚 ==========

	UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
	void DebugPrintGrid() const;

	// ========== ?몃━寃뚯씠??==========
	FOnInventoryUpdated OnInventoryUpdated;

	// ========== Replication ==========
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	friend struct FInventoryFastArray;

	// ========== Server RPCs ==========

	UFUNCTION(Server, Reliable)
	void Server_RequestRemoveItem(FGuid ItemInstanceID);

	UFUNCTION(Server, Reliable)
	void Server_RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition);

	UFUNCTION(Server, Reliable)
	void Server_RequestConsumeItemByID(FName ItemDataID, int32 Count);

	UFUNCTION(Server, Reliable)
	void Server_RequestDropItem(FGuid ItemInstanceID);

	// ========== ?대? ?곌린 ?ы띁 ==========

	bool AddItem_Internal(FName ItemDataID, FItemSize Size,
		int32 StackCount = 1, int32 MaxStack = 1);

	bool AddItemAt_Internal(FName ItemDataID, FItemSize Size,
		FIntPoint Position,
		int32 StackCount = 1, int32 MaxStack = 1);

	void HandleInventoryStateChanged();
	void HandleReplicatedInventoryReceived();
	void RebuildGridSlotsFromItems();
	void RebuildAllCachesFromItems();
	void BroadcastFullInventoryRefresh();
	void InitializeGridStorage();

	// ========== Replicated ?곗씠??==========

	UPROPERTY(Replicated)
	FInventoryFastArray InventoryList;

	UPROPERTY(Transient)
	TArray<FInventorySlot> GridSlots;

	// ========== ?대? ?ы띁 ==========

	bool IsValidGridPosition(FIntPoint Position) const;
	int32 GridPositionToIndex(FIntPoint Position) const;
	FIntPoint IndexToGridPosition(int32 Index) const;

	bool AreSlotsFree(FIntPoint Position, FItemSize Size) const;
	void OccupySlots(FIntPoint Position, FItemSize Size, const FGuid& ItemID);
	void FreeSlots(const FInventoryItemInstance& Item);

	UPROPERTY()
	TObjectPtr<class UItemDataSubsystem> CachedItemSub;

	TMap<FName, int32> ItemCountCache;
	TMap<FGuid, int32> ItemIndexMap;

	void RebuildItemIndexMap();

	TArray<uint16> RowBitmap;
	void RebuildItemCountCache();
	void SetBit(int32 Col, int32 Row, bool bOccupied);

	FInventoryItemInstance* FindItemByInstanceID(const FGuid& InstanceID);
	const FInventoryItemInstance* FindItemByInstanceID(const FGuid& InstanceID) const;
	int32 FindItemIndexByInstanceID(const FGuid& InstanceID) const;
};
