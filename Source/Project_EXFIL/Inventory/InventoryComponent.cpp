// Copyright Project EXFIL. All Rights Reserved.

#include "InventoryComponent.h"
#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "Engine/GameInstance.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Data/ItemDataSubsystem.h"
#include "World/WorldItem.h"
#include "Project_EXFIL.h"

namespace InventoryDebug
{
	static bool ShouldHandleReplicationCallbacks(const UInventoryComponent* Component)
	{
		if (!Component)
		{
			return false;
		}

		const AActor* Owner = Component->GetOwner();
		return Owner && !Owner->HasAuthority();
	}

	static FString GetSideLabel(const UInventoryComponent* Component)
	{
		if (!Component)
		{
			return TEXT("Unknown");
		}

		const AActor* Owner = Component->GetOwner();
		if (!Owner)
		{
			return TEXT("NoOwner");
		}

		return Owner->HasAuthority() ? TEXT("Server") : TEXT("Client");
	}

	static FString FormatDirtyIndices(const TSet<int32>& DirtyIndices)
	{
		TArray<int32> SortedIndices = DirtyIndices.Array();
		SortedIndices.Sort();

		TArray<FString> Parts;
		const int32 PreviewCount = FMath::Min(SortedIndices.Num(), 16);
		Parts.Reserve(PreviewCount + 1);

		for (int32 i = 0; i < PreviewCount; ++i)
		{
			Parts.Add(FString::FromInt(SortedIndices[i]));
		}

		if (SortedIndices.Num() > PreviewCount)
		{
			Parts.Add(TEXT("..."));
		}

		return FString::Printf(TEXT("[%s]"), *FString::Join(Parts, TEXT(",")));
	}

	static FString FormatIndicesView(const TArrayView<int32>& Indices)
	{
		TSet<int32> UniqueIndices;
		for (const int32 Index : Indices)
		{
			UniqueIndices.Add(Index);
		}

		return FormatDirtyIndices(UniqueIndices);
	}

	static FString FormatChangedItemDataIDs(const TSet<FName>& ChangedItemDataIDs)
	{
		TArray<FName> SortedIDs = ChangedItemDataIDs.Array();
		SortedIDs.Sort(FNameLexicalLess());

		TArray<FString> Parts;
		const int32 PreviewCount = FMath::Min(SortedIDs.Num(), 16);
		Parts.Reserve(PreviewCount + 1);

		for (int32 i = 0; i < PreviewCount; ++i)
		{
			Parts.Add(SortedIDs[i].ToString());
		}

		if (SortedIDs.Num() > PreviewCount)
		{
			Parts.Add(TEXT("..."));
		}

		return FString::Printf(TEXT("[%s]"), *FString::Join(Parts, TEXT(",")));
	}

	static void LogPatch(UInventoryComponent* Component, const TCHAR* Phase,
		const FInventoryItemInstance& Item, const TSet<int32>& DirtyIndices)
	{
		UE_LOG(LogProject_EXFIL, Log,
			TEXT("[InventoryPatch][%s] %s Item=%s DataID=%s Root=(%d,%d) Size=%dx%d Stack=%d DirtyCount=%d Dirty=%s"),
			*GetSideLabel(Component),
			Phase,
			*Item.InstanceID.ToString(),
			*Item.ItemDataID.ToString(),
			Item.RootPosition.X,
			Item.RootPosition.Y,
			Item.GetEffectiveSize().Width,
			Item.GetEffectiveSize().Height,
			Item.StackCount,
			DirtyIndices.Num(),
			*FormatDirtyIndices(DirtyIndices));
	}

	static void LogBroadcast(UInventoryComponent* Component, const TCHAR* Source,
		const TSet<int32>& DirtyIndices)
	{
		UE_LOG(LogProject_EXFIL, Log,
			TEXT("[InventoryBroadcast][%s] %s DirtyCount=%d Dirty=%s"),
			*GetSideLabel(Component),
			Source,
			DirtyIndices.Num(),
			*FormatDirtyIndices(DirtyIndices));
	}

	static void LogCacheSummary(UInventoryComponent* Component, const TCHAR* Source)
	{
		if (!Component)
		{
			return;
		}

		UE_LOG(LogProject_EXFIL, Log,
			TEXT("[InventoryCache][%s] %s Items=%d Grid=%dx%d TotalCells=%d"),
			*GetSideLabel(Component),
			Source,
			Component->GetAllItems().Num(),
			Component->GridWidth,
			Component->GridHeight,
			Component->GridWidth * Component->GridHeight);
	}

	static void LogCountBroadcast(UInventoryComponent* Component, const TCHAR* Source,
		const TSet<FName>& ChangedItemDataIDs)
	{
		UE_LOG(LogProject_EXFIL, Log,
			TEXT("[InventoryBroadcast][%s] %s ChangedItemCount=%d IDs=%s"),
			*GetSideLabel(Component),
			Source,
			ChangedItemDataIDs.Num(),
			*FormatChangedItemDataIDs(ChangedItemDataIDs));
	}
}

void FInventoryFastArray::PreReplicatedRemove(
	const TArrayView<int32>& RemovedIndices, int32 /*FinalSize*/)
{
	if (!OwnerComponent)
	{
		return;
	}

	if (!InventoryDebug::ShouldHandleReplicationCallbacks(OwnerComponent))
	{
		return;
	}

	if (!OwnerComponent->bCachesInitialized)
	{
		UE_LOG(LogProject_EXFIL, Log,
			TEXT("[InventoryRep][Client] PreReplicatedRemove skipped during initial sync Count=%d Indices=%s"),
			RemovedIndices.Num(),
			*InventoryDebug::FormatIndicesView(RemovedIndices));
		return;
	}

	UE_LOG(LogProject_EXFIL, Log,
		TEXT("[InventoryRep][Client] PreReplicatedRemove Count=%d Indices=%s"),
		RemovedIndices.Num(),
		*InventoryDebug::FormatIndicesView(RemovedIndices));

	for (int32 ArrayIdx = RemovedIndices.Num() - 1; ArrayIdx >= 0; --ArrayIdx)
	{
		const int32 RemovedIndex = RemovedIndices[ArrayIdx];
		if (!Items.IsValidIndex(RemovedIndex))
		{
			continue;
		}

		OwnerComponent->ApplyItemRemoved_Local(
			Items[RemovedIndex], RemovedIndex, PendingDirtyIndices);
		PendingChangedItemDataIDs.Add(Items[RemovedIndex].ItemDataID);
	}
}

void FInventoryFastArray::PostReplicatedAdd(
	const TArrayView<int32>& AddedIndices, int32 /*FinalSize*/)
{
	if (!OwnerComponent)
	{
		return;
	}

	if (!InventoryDebug::ShouldHandleReplicationCallbacks(OwnerComponent))
	{
		return;
	}

	if (!OwnerComponent->bCachesInitialized)
	{
		UE_LOG(LogProject_EXFIL, Log,
			TEXT("[InventoryRep][Client] PostReplicatedAdd skipped during initial sync Count=%d Indices=%s"),
			AddedIndices.Num(),
			*InventoryDebug::FormatIndicesView(AddedIndices));
		return;
	}

	UE_LOG(LogProject_EXFIL, Log,
		TEXT("[InventoryRep][Client] PostReplicatedAdd Count=%d Indices=%s"),
		AddedIndices.Num(),
		*InventoryDebug::FormatIndicesView(AddedIndices));

	for (const int32 AddedIndex : AddedIndices)
	{
		if (!Items.IsValidIndex(AddedIndex))
		{
			continue;
		}

		OwnerComponent->ApplyItemAdded_Local(
			Items[AddedIndex], AddedIndex, PendingDirtyIndices);
		PendingChangedItemDataIDs.Add(Items[AddedIndex].ItemDataID);
	}
}

void FInventoryFastArray::PostReplicatedChange(
	const TArrayView<int32>& ChangedIndices, int32 /*FinalSize*/)
{
	if (!OwnerComponent)
	{
		return;
	}

	if (!InventoryDebug::ShouldHandleReplicationCallbacks(OwnerComponent))
	{
		return;
	}

	if (!OwnerComponent->bCachesInitialized)
	{
		UE_LOG(LogProject_EXFIL, Log,
			TEXT("[InventoryRep][Client] PostReplicatedChange skipped during initial sync Count=%d Indices=%s"),
			ChangedIndices.Num(),
			*InventoryDebug::FormatIndicesView(ChangedIndices));
		return;
	}

	UE_LOG(LogProject_EXFIL, Log,
		TEXT("[InventoryRep][Client] PostReplicatedChange Count=%d Indices=%s"),
		ChangedIndices.Num(),
		*InventoryDebug::FormatIndicesView(ChangedIndices));

	for (const int32 ChangedIndex : ChangedIndices)
	{
		if (!Items.IsValidIndex(ChangedIndex))
		{
			continue;
		}

		const FInventoryItemInstance& Item = Items[ChangedIndex];
		int32* ExistingIndex = OwnerComponent->ItemIndexMap.Find(Item.InstanceID);
		if (!ExistingIndex || *ExistingIndex != ChangedIndex)
		{
			OwnerComponent->ItemIndexMap.FindOrAdd(Item.InstanceID) = ChangedIndex;
		}

		if (!OwnerComponent->DoesGridMatchItemFootprint(Item))
		{
			OwnerComponent->ApplyItemMovedByScan_Local(Item, PendingDirtyIndices);
		}
		else
		{
			OwnerComponent->CollectFootprintIndices(
				Item.RootPosition, Item.GetEffectiveSize(), PendingDirtyIndices);
		}

		const int32 OldCount = OwnerComponent->GetItemCountByID_Cached(Item.ItemDataID);
		OwnerComponent->RecalculateItemCountForID(Item.ItemDataID);
		const int32 NewCount = OwnerComponent->GetItemCountByID_Cached(Item.ItemDataID);
		if (OldCount != NewCount)
		{
			PendingChangedItemDataIDs.Add(Item.ItemDataID);
		}
	}
}

void FInventoryFastArray::PostReplicatedReceive(
	const FFastArraySerializer::FPostReplicatedReceiveParameters& /*Parameters*/)
{
	if (!OwnerComponent)
	{
		return;
	}

	if (!InventoryDebug::ShouldHandleReplicationCallbacks(OwnerComponent))
	{
		PendingDirtyIndices.Reset();
		PendingChangedItemDataIDs.Reset();
		return;
	}

	if (!OwnerComponent->bCachesInitialized)
	{
		UE_LOG(LogProject_EXFIL, Log,
			TEXT("[InventoryRep][Client] PostReplicatedReceive -> InitialFullSync"));
		OwnerComponent->HandleReplicatedInventoryReceived();
		PendingDirtyIndices.Reset();
		PendingChangedItemDataIDs.Reset();
		return;
	}

	if (PendingDirtyIndices.Num() > 0)
	{
		InventoryDebug::LogBroadcast(
			OwnerComponent, TEXT("PostReplicatedReceive(PacketFlush)"), PendingDirtyIndices);
		OwnerComponent->OnInventoryUpdated.Broadcast(PendingDirtyIndices);
		PendingDirtyIndices.Reset();
	}
	

	if (PendingChangedItemDataIDs.Num() > 0)
	{
		InventoryDebug::LogCountBroadcast(
			OwnerComponent, TEXT("PostReplicatedReceive(ItemCountFlush)"), PendingChangedItemDataIDs);
		OwnerComponent->OnInventoryItemCountsChanged.Broadcast(PendingChangedItemDataIDs);
		PendingChangedItemDataIDs.Reset();
	}
}

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	InventoryList.OwnerComponent = this;
}

#pragma region Engine Lifecycle / Replication
void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UInventoryComponent, InventoryList, COND_OwnerOnly);
}
#pragma endregion

#pragma region External Entry: Request API
void UInventoryComponent::RequestRemoveItem(FGuid ItemInstanceID)
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		Server_RequestRemoveItem(ItemInstanceID);
		return;
	}

	RemoveItem_Internal(ItemInstanceID);
}

void UInventoryComponent::RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated)
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		UE_LOG(LogProject_EXFIL, Verbose,
			TEXT("RequestMoveItem(Client): Item=%s -> (%d,%d) Rotated=%s"),
			*ItemInstanceID.ToString(), NewPosition.X, NewPosition.Y,
			bNewRotated ? TEXT("true") : TEXT("false"));
		Server_RequestMoveItem(ItemInstanceID, NewPosition, bNewRotated);
		return;
	}

	UE_LOG(LogProject_EXFIL, Verbose,
		TEXT("RequestMoveItem(ServerLocal): Item=%s -> (%d,%d) Rotated=%s"),
		*ItemInstanceID.ToString(), NewPosition.X, NewPosition.Y,
		bNewRotated ? TEXT("true") : TEXT("false"));
	MoveItem_Internal(ItemInstanceID, NewPosition, bNewRotated);
}

void UInventoryComponent::RequestConsumeItemByID(FName ItemDataID, int32 Count)
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		Server_RequestConsumeItemByID(ItemDataID, Count);
		return;
	}

	HandleConsumeRequest_Internal(ItemDataID, Count);
}

void UInventoryComponent::RequestDropItem(FGuid ItemInstanceID)
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		Server_RequestDropItem(ItemInstanceID);
		return;
	}

	DropItem_Internal(ItemInstanceID);
}
#pragma endregion

#pragma region Server Authority: Mutation API
bool UInventoryComponent::AddItemByID_Internal(FName ItemDataID, int32 StackCount)
{
	checkf(GetOwner() && GetOwner()->HasAuthority(),
		TEXT("AddItemByID_Internal must run on the server."));

	if (ItemDataID.IsNone() || StackCount <= 0)
	{
		return false;
	}

	if (!CachedItemSub)
	{
		UE_LOG(LogProject_EXFIL, Warning,
			TEXT("AddItemByID_Internal: UItemDataSubsystem not found."));
		return false;
	}

	const FItemData* ItemData = CachedItemSub->GetItemData(ItemDataID);
	if (!ItemData)
	{
		UE_LOG(LogProject_EXFIL, Warning,
			TEXT("AddItemByID_Internal: ItemDataID '%s' not found."),
			*ItemDataID.ToString());
		return false;
	}

	const int32 MaxStack = ItemData->MaxStackCount;
	int32 Remaining = StackCount;
	TSet<int32> Affected;

	if (MaxStack > 1)
	{
		for (FInventoryItemInstance& Existing : InventoryList.Items)
		{
			if (Remaining <= 0)
			{
				break;
			}

			if (Existing.ItemDataID != ItemDataID)
			{
				continue;
			}

			const int32 Space = Existing.MaxStackCount - Existing.StackCount;
			if (Space <= 0)
			{
				continue;
			}

			const int32 ToMerge = FMath::Min(Remaining, Space);
			const int32 OldStackCount = Existing.StackCount;
			Existing.StackCount += ToMerge;
			Remaining -= ToMerge;
			InventoryList.MarkItemDirty(Existing);
			ApplyItemStackChanged_Local(Existing, OldStackCount, Affected);

			UE_LOG(LogProject_EXFIL, Verbose,
				TEXT("AddItemByID_Internal: Merged %d into existing stack of '%s' (now %d/%d)"),
				ToMerge, *ItemDataID.ToString(), Existing.StackCount, Existing.MaxStackCount);
		}
	}

	bool bAddedNewStack = true;
	if (Remaining > 0)
	{
		bAddedNewStack = AddItem_Internal(
			ItemDataID, ItemData->GetItemSize(), Remaining, MaxStack, Affected);
	}

	return Remaining <= 0 || bAddedNewStack;
}

bool UInventoryComponent::AddItemByIDAt_Internal(FName ItemDataID, FIntPoint Position,
	bool bRotated, int32 StackCount)
{
	checkf(GetOwner() && GetOwner()->HasAuthority(),
		TEXT("AddItemByIDAt_Internal must run on the server."));

	if (ItemDataID.IsNone() || StackCount <= 0)
	{
		return false;
	}

	if (!CachedItemSub)
	{
		UE_LOG(LogProject_EXFIL, Warning,
			TEXT("AddItemByIDAt_Internal: UItemDataSubsystem not found."));
		return false;
	}

	const FItemData* ItemData = CachedItemSub->GetItemData(ItemDataID);
	if (!ItemData)
	{
		UE_LOG(LogProject_EXFIL, Warning,
			TEXT("AddItemByIDAt_Internal: ItemDataID '%s' not found."),
			*ItemDataID.ToString());
		return false;
	}

	TSet<int32> Affected;
	return AddItemAt_Internal(
		ItemDataID,
		ItemData->GetItemSize(),
		Position,
		bRotated,
		StackCount,
		ItemData->MaxStackCount,
		Affected);
}

bool UInventoryComponent::RemoveItem_Internal(const FGuid& InstanceID)
{
	checkf(GetOwner() && GetOwner()->HasAuthority(),
		TEXT("RemoveItem_Internal must run on the server."));

	const int32 Index = FindItemIndexByInstanceID(InstanceID);
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogProject_EXFIL, Warning, TEXT("RemoveItem_Internal: Item not found: %s"),
			*InstanceID.ToString());
		return false;
	}

	TSet<int32> Affected;
	const FInventoryItemInstance RemovedItem = InventoryList.Items[Index];
	InventoryList.Items.RemoveAt(Index);
	InventoryList.MarkArrayDirty();
	ApplyItemRemoved_Local(RemovedItem, Index, Affected);

	UE_LOG(LogProject_EXFIL, Verbose, TEXT("Item removed: %s"), *InstanceID.ToString());

	return true;
}

bool UInventoryComponent::MoveItem_Internal(const FGuid& InstanceID, FIntPoint NewPosition, bool bNewRotated)
{
	checkf(GetOwner() && GetOwner()->HasAuthority(),
		TEXT("MoveItem_Internal must run on the server."));

	FInventoryItemInstance* FoundItem = FindItemByInstanceID(InstanceID);
	if (!FoundItem)
	{
		UE_LOG(LogProject_EXFIL, Warning, TEXT("MoveItem_Internal: Item not found: %s"),
			*InstanceID.ToString());
		return false;
	}

	const FIntPoint OldPosition = FoundItem->RootPosition;
	const FItemSize OldEffectiveSize = FoundItem->GetEffectiveSize();
	bNewRotated = bNewRotated && !FoundItem->ItemSize.IsSquare();
	const FItemSize NewEffectiveSize = bNewRotated
		? FoundItem->ItemSize.GetRotated()
		: FoundItem->ItemSize;

	UE_LOG(LogProject_EXFIL, Verbose,
		TEXT("MoveItem_Internal: Attempt Item=%s From=(%d,%d) To=(%d,%d) Size=%dx%d Rotated=%s"),
		*InstanceID.ToString(),
		OldPosition.X, OldPosition.Y,
		NewPosition.X, NewPosition.Y,
		NewEffectiveSize.Width, NewEffectiveSize.Height,
		bNewRotated ? TEXT("true") : TEXT("false"));

	if (!AreSlotsFreeForItem(NewPosition, NewEffectiveSize, InstanceID))
	{
		const bool bOutOfBounds =
			NewPosition.X < 0 || NewPosition.Y < 0 ||
			NewPosition.X + NewEffectiveSize.Width > GridWidth ||
			NewPosition.Y + NewEffectiveSize.Height > GridHeight;

		if (bOutOfBounds)
		{
			UE_LOG(LogProject_EXFIL, Warning,
				TEXT("MoveItem_Internal: Target area is out of bounds. Grid=%dx%d Target=(%d,%d) Size=%dx%d"),
				GridWidth, GridHeight,
				NewPosition.X, NewPosition.Y,
				NewEffectiveSize.Width, NewEffectiveSize.Height);
		}
		else
		{
			for (int32 Y = NewPosition.Y; Y < NewPosition.Y + NewEffectiveSize.Height; ++Y)
			{
				for (int32 X = NewPosition.X; X < NewPosition.X + NewEffectiveSize.Width; ++X)
				{
					const FIntPoint TargetPos(X, Y);
					if (!IsValidGridPosition(TargetPos))
					{
						continue;
					}

					const int32 TargetIndex = GridPositionToIndex(TargetPos);
					if (!GridSlots.IsValidIndex(TargetIndex))
					{
						continue;
					}

					const FInventorySlot& TargetSlot = GridSlots[TargetIndex];
					if (TargetSlot.IsEmpty())
					{
						continue;
					}

					const FGuid BlockingItemID = TargetSlot.OccupyingItemID;
					const FInventoryItemInstance* BlockingItem = FindItemByInstanceID(BlockingItemID);

					if (BlockingItem)
					{
						UE_LOG(LogProject_EXFIL, Warning,
							TEXT("MoveItem_Internal: Blocked cell (%d,%d) by Item=%s DataID=%s Root=(%d,%d) Size=%dx%d RootSlot=%s"),
							X, Y,
							*BlockingItemID.ToString(),
							*BlockingItem->ItemDataID.ToString(),
							BlockingItem->RootPosition.X, BlockingItem->RootPosition.Y,
							BlockingItem->ItemSize.Width, BlockingItem->ItemSize.Height,
							TargetSlot.bIsRootSlot ? TEXT("true") : TEXT("false"));
					}
					else
					{
						UE_LOG(LogProject_EXFIL, Warning,
							TEXT("MoveItem_Internal: Blocked cell (%d,%d) by unknown ItemID=%s RootSlot=%s"),
							X, Y,
							*BlockingItemID.ToString(),
							TargetSlot.bIsRootSlot ? TEXT("true") : TEXT("false"));
					}
				}
			}
		}
		return false;
	}

	FoundItem->RootPosition = NewPosition;
	FoundItem->bIsRotated = bNewRotated;
	InventoryList.MarkItemDirty(*FoundItem);
	TSet<int32> Affected;
	ApplyItemMoved_Local(*FoundItem, OldPosition, OldEffectiveSize, Affected);

	UE_LOG(LogProject_EXFIL, Verbose,
		TEXT("Item moved: %s from (%d,%d) to (%d,%d)"),
		*InstanceID.ToString(),
		OldPosition.X, OldPosition.Y,
		NewPosition.X, NewPosition.Y);

	return true;
}

bool UInventoryComponent::ConsumeItemByID_Internal(FName ItemDataID, int32 Count)
{
	checkf(GetOwner() && GetOwner()->HasAuthority(),
		TEXT("ConsumeItemByID_Internal must run on the server."));

	if (ItemDataID.IsNone() || Count <= 0)
	{
		return false;
	}

	if (GetItemCountByID_Cached(ItemDataID) < Count)
	{
		UE_LOG(LogProject_EXFIL, Warning,
			TEXT("ConsumeItemByID_Internal: Not enough '%s' (need %d)"),
			*ItemDataID.ToString(), Count);
		return false;
	}

	int32 Remaining = Count;
	TSet<int32> Affected;
	struct FPendingRemoval
	{
		FInventoryItemInstance Item;
		int32 Index = INDEX_NONE;
	};
	TArray<FPendingRemoval> PendingRemovals;
	bool bModifiedAny = false;

	for (int32 i = 0; i < InventoryList.Items.Num() && Remaining > 0; ++i)
	{
		FInventoryItemInstance& Item = InventoryList.Items[i];
		if (Item.ItemDataID != ItemDataID)
		{
			continue;
		}

		if (Item.StackCount <= Remaining)
		{
			Remaining -= Item.StackCount;
			FPendingRemoval PendingRemoval;
			PendingRemoval.Item = Item;
			PendingRemoval.Index = i;
			PendingRemovals.Add(PendingRemoval);
			bModifiedAny = true;
		}
		else
		{
			const int32 OldStackCount = Item.StackCount;
			Item.StackCount -= Remaining;
			Remaining = 0;
			bModifiedAny = true;
			InventoryList.MarkItemDirty(Item);
			ApplyItemStackChanged_Local(Item, OldStackCount, Affected);
		}
	}

	for (int32 i = PendingRemovals.Num() - 1; i >= 0; --i)
	{
		InventoryList.Items.RemoveAt(PendingRemovals[i].Index);
		ApplyItemRemoved_Local(PendingRemovals[i].Item, PendingRemovals[i].Index, Affected);
	}

	if (PendingRemovals.Num() > 0)
	{
		InventoryList.MarkArrayDirty();
	}

	UE_LOG(LogProject_EXFIL, Verbose,
		TEXT("ConsumeItemByID_Internal: '%s' x%d consumed"), *ItemDataID.ToString(), Count);
	return true;
}

bool UInventoryComponent::DropItem_Internal(FGuid ItemInstanceID)
{
	checkf(GetOwner() && GetOwner()->HasAuthority(),
		TEXT("DropItem_Internal must run on the server."));

	const FInventoryItemInstance* Item = FindItemByInstanceID(ItemInstanceID);
	if (!Item)
	{
		return false;
	}

	const FName DropItemDataID = Item->ItemDataID;
	const int32 CurrentStack = Item->StackCount;

	if (CurrentStack > 1)
	{
		UE_LOG(LogProject_EXFIL, Log,
			TEXT("[InventoryDrop][Server] Item=%s DataID=%s Path=DecrementStack"),
			*ItemInstanceID.ToString(),
			*DropItemDataID.ToString());
		DecrementStack_Internal(ItemInstanceID);
	}
	else
	{
		UE_LOG(LogProject_EXFIL, Log,
			TEXT("[InventoryDrop][Server] Item=%s DataID=%s Path=RemoveItem"),
			*ItemInstanceID.ToString(),
			*DropItemDataID.ToString());
		RemoveItem_Internal(ItemInstanceID);
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	const FVector SpawnLocation =
		Owner->GetActorLocation()
		+ Owner->GetActorForwardVector() * DropForwardOffset
		+ FVector(0.f, 0.f, DropUpwardOffset);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AWorldItem* DroppedItem = GetWorld()->SpawnActor<AWorldItem>(
		AWorldItem::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParams);

	if (DroppedItem)
	{
		DroppedItem->InitializeItem(DropItemDataID, 1);
	}

	UE_LOG(LogProject_EXFIL, Log,
		TEXT("[InventoryDrop][Server] SpawnResult=%s ItemDataID=%s"),
		DroppedItem ? TEXT("Success") : TEXT("Failed"),
		*DropItemDataID.ToString());

	return DroppedItem != nullptr;
}

int32 UInventoryComponent::DecrementStack_Internal(const FGuid& InstanceID)
{
	checkf(GetOwner() && GetOwner()->HasAuthority(),
		TEXT("DecrementStack_Internal must run on the server."));

	FInventoryItemInstance* Item = FindItemByInstanceID(InstanceID);
	if (!Item)
	{
		return 0;
	}

	const int32 OldStackCount = Item->StackCount;
	Item->StackCount--;

	if (Item->StackCount <= 0)
	{
		RemoveItem_Internal(InstanceID);
		return 0;
	}

	InventoryList.MarkItemDirty(*Item);
	TSet<int32> Affected;
	ApplyItemStackChanged_Local(*Item, OldStackCount, Affected);

	UE_LOG(LogProject_EXFIL, Verbose, TEXT("DecrementStack_Internal: '%s' now %d"),
		*Item->ItemDataID.ToString(), Item->StackCount);
	return Item->StackCount;
}
#pragma endregion

#pragma region Read Only: Query API
bool UInventoryComponent::CanPlaceItemAt(FIntPoint Position, FItemSize Size) const
{
	return AreSlotsFree(Position, Size);
}

void UInventoryComponent::EnsureReplicatedCachesReady()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return;
	}

	if (InventoryList.Items.Num() == 0)
	{
		return;
	}

	bool bHasOccupiedSlot = false;
	for (const FInventorySlot& Slot : GridSlots)
	{
		if (!Slot.IsEmpty())
		{
			bHasOccupiedSlot = true;
			break;
		}
	}

	const bool bIndexMapReady = (ItemIndexMap.Num() == InventoryList.Items.Num());
	if (bHasOccupiedSlot && bIndexMapReady)
	{
		return;
	}

	RebuildAllCachesFromItems();
}

bool UInventoryComponent::FindFirstAvailableSlot(FItemSize Size, FIntPoint& OutPosition) const
{
	const int32 W = Size.Width;
	const int32 H = Size.Height;
	const uint16 BaseMask = static_cast<uint16>((1 << W) - 1);

	for (int32 Y = 0; Y <= GridHeight - H; ++Y)
	{
		uint16 Merged = 0;
		for (int32 DY = 0; DY < H; ++DY)
		{
			Merged |= RowBitmap[Y + DY];
		}

		for (int32 X = 0; X <= GridWidth - W; ++X)
		{
			if ((Merged & static_cast<uint16>(BaseMask << X)) == 0)
			{
				OutPosition = FIntPoint(X, Y);
				return true;
			}
		}
	}
	return false;
}

bool UInventoryComponent::GetItemAt(FIntPoint Position, FInventoryItemInstance& OutItem) const
{
	if (!IsValidGridPosition(Position))
	{
		return false;
	}

	const int32 Index = GridPositionToIndex(Position);
	if (!GridSlots.IsValidIndex(Index))
	{
		return false;
	}

	const FInventorySlot& Slot = GridSlots[Index];
	if (Slot.IsEmpty())
	{
		return false;
	}

	const FInventoryItemInstance* Found = FindItemByInstanceID(Slot.OccupyingItemID);
	if (Found)
	{
		OutItem = *Found;
		return true;
	}

	return false;
}

bool UInventoryComponent::GetItemByID(const FGuid& InstanceID, FInventoryItemInstance& OutItem) const
{
	const FInventoryItemInstance* Found = FindItemByInstanceID(InstanceID);
	if (Found)
	{
		OutItem = *Found;
		return true;
	}

	return false;
}

TArray<FInventoryItemInstance> UInventoryComponent::GetAllItems() const
{
	return InventoryList.Items;
}

bool UInventoryComponent::IsEmpty() const
{
	return InventoryList.Items.Num() == 0;
}

int32 UInventoryComponent::GetItemCountByID(FName ItemDataID) const
{
	int32 Total = 0;
	for (const FInventoryItemInstance& Item : InventoryList.Items)
	{
		if (Item.ItemDataID == ItemDataID)
		{
			Total += Item.StackCount;
		}
	}
	return Total;
}

int32 UInventoryComponent::GetItemCountByID_Cached(FName ItemDataID) const
{
	const int32* Count = ItemCountCache.Find(ItemDataID);
	return Count ? *Count : 0;
}
#pragma endregion

#pragma region Engine Lifecycle
void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	InventoryList.OwnerComponent = this;

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			CachedItemSub = GI->GetSubsystem<UItemDataSubsystem>();
		}
	}

	InitializeGridStorage();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		RebuildAllCachesFromItems();
		bCachesInitialized = true;
	}
}
#pragma endregion

#pragma region Network Bridge: Server RPC
void UInventoryComponent::Server_RequestRemoveItem_Implementation(FGuid ItemInstanceID)
{
	if (!ItemInstanceID.IsValid())
	{
		return;
	}

	RemoveItem_Internal(ItemInstanceID);
}

void UInventoryComponent::Server_RequestMoveItem_Implementation(
	FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated)
{
	if (!ItemInstanceID.IsValid() || NewPosition.X < 0 || NewPosition.Y < 0)
	{
		return;
	}

	UE_LOG(LogProject_EXFIL, Verbose,
		TEXT("Server_RequestMoveItem: Item=%s -> (%d,%d) Rotated=%s"),
		*ItemInstanceID.ToString(), NewPosition.X, NewPosition.Y,
		bNewRotated ? TEXT("true") : TEXT("false"));
	MoveItem_Internal(ItemInstanceID, NewPosition, bNewRotated);
}

void UInventoryComponent::Server_RequestConsumeItemByID_Implementation(FName ItemDataID, int32 Count)
{
	HandleConsumeRequest_Internal(ItemDataID, Count);
}

void UInventoryComponent::Server_RequestDropItem_Implementation(FGuid ItemInstanceID)
{
	if (!ItemInstanceID.IsValid())
	{
		return;
	}

	DropItem_Internal(ItemInstanceID);
}
#pragma endregion

#pragma region Gameplay Integration: Consumable / World Interaction
void UInventoryComponent::HandleConsumeRequest_Internal(FName ItemDataID, int32 Count)
{
	if (ItemDataID.IsNone() || Count <= 0)
	{
		return;
	}

	ApplyConsumableEffect_Internal(ItemDataID);
	ConsumeItemByID_Internal(ItemDataID, Count);
}

void UInventoryComponent::ApplyConsumableEffect_Internal(FName ItemDataID)
{
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = Owner->FindComponentByClass<UAbilitySystemComponent>())
		{
			if (CachedItemSub)
			{
				const FItemData* ItemData = CachedItemSub->GetItemData(ItemDataID);
				if (ItemData && !ItemData->ConsumableEffect.IsNull())
				{
					TSubclassOf<UGameplayEffect> GEClass =
						CachedItemSub->GetCachedEffect(ItemData->ConsumableEffect);
					if (GEClass)
					{
						FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
						FGameplayEffectSpecHandle Spec =
							ASC->MakeOutgoingSpec(GEClass, 1.f, Ctx);
						if (Spec.IsValid())
						{
							ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
						}
					}
				}
			}
		}
	}
}
#pragma endregion

#pragma region Server Authority: Mutation Helpers
bool UInventoryComponent::AddItem_Internal(FName ItemDataID, FItemSize Size,
	int32 StackCount, int32 MaxStack, TSet<int32>& OutAffected)
{
	checkf(GetOwner() && GetOwner()->HasAuthority(),
		TEXT("AddItem_Internal must run on the server."));

	FIntPoint FoundPosition;
	if (!FindFirstAvailableSlot(Size, FoundPosition))
	{
		const FItemSize RotatedSize = Size.GetRotated();
		if (RotatedSize == Size || !FindFirstAvailableSlot(RotatedSize, FoundPosition))
		{
			return false;
		}

		return AddItemAt_Internal(
			ItemDataID, Size, FoundPosition, true, StackCount, MaxStack, OutAffected);
	}

	return AddItemAt_Internal(
		ItemDataID, Size, FoundPosition, false, StackCount, MaxStack, OutAffected);
}

bool UInventoryComponent::AddItemAt_Internal(FName ItemDataID, FItemSize Size,
	FIntPoint Position, bool bRotated, int32 StackCount, int32 MaxStack,
	TSet<int32>& OutAffected)
{
	checkf(GetOwner() && GetOwner()->HasAuthority(),
		TEXT("AddItemAt_Internal must run on the server."));

	bRotated = bRotated && !Size.IsSquare();
	const FItemSize EffectiveSize = bRotated ? Size.GetRotated() : Size;
	if (!AreSlotsFree(Position, EffectiveSize))
	{
		UE_LOG(LogProject_EXFIL, Warning,
			TEXT("AddItemAt_Internal: Cannot place item '%s' at (%d,%d) Size:%dx%d Rotated=%s"),
			*ItemDataID.ToString(), Position.X, Position.Y,
			EffectiveSize.Width, EffectiveSize.Height,
			bRotated ? TEXT("true") : TEXT("false"));
		return false;
	}

	FInventoryItemInstance& NewItem = InventoryList.Items.AddDefaulted_GetRef();
	NewItem.InstanceID = FGuid::NewGuid();
	NewItem.ItemDataID = ItemDataID;
	NewItem.RootPosition = Position;
	NewItem.ItemSize = Size;
	NewItem.bIsRotated = bRotated;
	NewItem.StackCount = FMath::Clamp(StackCount, 1, MaxStack);
	NewItem.MaxStackCount = MaxStack;

	// New FastArray entries should mark the container dirty so the initial owner sync
	// is not delayed until a later mutation touches an existing item.
	InventoryList.MarkArrayDirty();
	InventoryList.MarkItemDirty(NewItem);
	ApplyItemAdded_Local(NewItem, InventoryList.Items.Num() - 1, OutAffected);

	UE_LOG(LogProject_EXFIL, Verbose,
		TEXT("Item added: '%s' ID=%s at (%d,%d) Size:%dx%d Rotated=%s Stack:%d/%d"),
		*ItemDataID.ToString(), *NewItem.InstanceID.ToString(),
		Position.X, Position.Y, EffectiveSize.Width, EffectiveSize.Height,
		bRotated ? TEXT("true") : TEXT("false"),
		NewItem.StackCount, NewItem.MaxStackCount);

	return true;
}
#pragma endregion

#pragma region State Sync / Cache Rebuild
void UInventoryComponent::HandleReplicatedInventoryReceived()
{
	RebuildAllCachesFromItems();
	InventoryDebug::LogCacheSummary(this, TEXT("HandleReplicatedInventoryReceived(AfterRebuild)"));
	BroadcastFullInventoryRefresh();
	bCachesInitialized = true;
}

void UInventoryComponent::BroadcastFullInventoryRefresh()
{
	ensureAlwaysMsgf(!bCachesInitialized,
		TEXT("BroadcastFullInventoryRefresh called after cache initialization."));

	TSet<int32> AllIndices;
	AllIndices.Reserve(GridSlots.Num());
	for (int32 i = 0; i < GridSlots.Num(); ++i)
	{
		AllIndices.Add(i);
	}

	InventoryDebug::LogBroadcast(this, TEXT("BroadcastFullInventoryRefresh"), AllIndices);
	OnInventoryUpdated.Broadcast(AllIndices);
}

void UInventoryComponent::InitializeGridStorage()
{
	const int32 SafeGridWidth = FMath::Max(1, GridWidth);
	const int32 SafeGridHeight = FMath::Max(1, GridHeight);

	GridSlots.SetNum(SafeGridWidth * SafeGridHeight);
	for (FInventorySlot& Slot : GridSlots)
		Slot.Clear();

	RowBitmap.Init(0, SafeGridHeight);
}

void UInventoryComponent::ApplyItemAdded_Local(const FInventoryItemInstance& Item, int32 ItemIndex,
	TSet<int32>& OutAffected)
{
	OccupySlots(Item.RootPosition, Item.GetEffectiveSize(), Item.InstanceID);
	ItemIndexMap.FindOrAdd(Item.InstanceID) = ItemIndex;
	ItemCountCache.FindOrAdd(Item.ItemDataID) += Item.StackCount;
	CollectFootprintIndices(Item.RootPosition, Item.GetEffectiveSize(), OutAffected);
	InventoryDebug::LogPatch(this, TEXT("Added"), Item, OutAffected);
}

void UInventoryComponent::ApplyItemRemoved_Local(const FInventoryItemInstance& Item, int32 RemovedIndex,
	TSet<int32>& OutAffected)
{
	CollectFootprintIndices(Item.RootPosition, Item.GetEffectiveSize(), OutAffected);
	FreeSlots(Item);
	ItemIndexMap.Remove(Item.InstanceID);

	for (TPair<FGuid, int32>& Pair : ItemIndexMap)
	{
		if (Pair.Value > RemovedIndex)
		{
			--Pair.Value;
		}
	}

	int32* CachedCount = ItemCountCache.Find(Item.ItemDataID);
	if (CachedCount)
	{
		*CachedCount -= Item.StackCount;
		if (*CachedCount <= 0)
		{
			ItemCountCache.Remove(Item.ItemDataID);
		}
	}

	InventoryDebug::LogPatch(this, TEXT("Removed"), Item, OutAffected);
}

void UInventoryComponent::ApplyItemMoved_Local(const FInventoryItemInstance& NewItem, FIntPoint OldPos,
	FItemSize OldEffSize, TSet<int32>& OutAffected)
{
	CollectFootprintIndices(OldPos, OldEffSize, OutAffected);
	FreeSlotsAt(OldPos, OldEffSize);
	OccupySlots(NewItem.RootPosition, NewItem.GetEffectiveSize(), NewItem.InstanceID);
	CollectFootprintIndices(NewItem.RootPosition, NewItem.GetEffectiveSize(), OutAffected);
	InventoryDebug::LogPatch(this, TEXT("Moved"), NewItem, OutAffected);
}

void UInventoryComponent::ApplyItemStackChanged_Local(const FInventoryItemInstance& NewItem,
	int32 OldStackCount, TSet<int32>& OutAffected)
{
	const int32 Delta = NewItem.StackCount - OldStackCount;
	if (Delta != 0)
	{
		int32& CachedCount = ItemCountCache.FindOrAdd(NewItem.ItemDataID);
		CachedCount += Delta;
		if (CachedCount <= 0)
		{
			ItemCountCache.Remove(NewItem.ItemDataID);
		}
	}

	CollectFootprintIndices(NewItem.RootPosition, NewItem.GetEffectiveSize(), OutAffected);
	InventoryDebug::LogPatch(this, TEXT("StackChanged"), NewItem, OutAffected);
}

void UInventoryComponent::ApplyItemMovedByScan_Local(const FInventoryItemInstance& NewItem,
	TSet<int32>& OutAffected)
{
	for (int32 Index = 0; Index < GridSlots.Num(); ++Index)
	{
		if (GridSlots[Index].OccupyingItemID != NewItem.InstanceID)
		{
			continue;
		}

		GridSlots[Index].Clear();
		OutAffected.Add(Index);

		const FIntPoint Position = IndexToGridPosition(Index);
		SetBit(Position.X, Position.Y, false);
	}

	OccupySlots(NewItem.RootPosition, NewItem.GetEffectiveSize(), NewItem.InstanceID);
	CollectFootprintIndices(NewItem.RootPosition, NewItem.GetEffectiveSize(), OutAffected);
	InventoryDebug::LogPatch(this, TEXT("MovedByScan"), NewItem, OutAffected);
}

bool UInventoryComponent::DoesGridMatchItemFootprint(const FInventoryItemInstance& Item) const
{
	const FItemSize EffectiveSize = Item.GetEffectiveSize();
	int32 OccupiedCellCount = 0;

	for (int32 Index = 0; Index < GridSlots.Num(); ++Index)
	{
		const bool bOccupiedByItem = (GridSlots[Index].OccupyingItemID == Item.InstanceID);
		if (!bOccupiedByItem)
		{
			continue;
		}

		++OccupiedCellCount;

		const FIntPoint Position = IndexToGridPosition(Index);
		const bool bWithinExpectedFootprint =
			Position.X >= Item.RootPosition.X &&
			Position.Y >= Item.RootPosition.Y &&
			Position.X < Item.RootPosition.X + EffectiveSize.Width &&
			Position.Y < Item.RootPosition.Y + EffectiveSize.Height;

		if (!bWithinExpectedFootprint)
		{
			return false;
		}
	}

	for (int32 Y = Item.RootPosition.Y; Y < Item.RootPosition.Y + EffectiveSize.Height; ++Y)
	{
		for (int32 X = Item.RootPosition.X; X < Item.RootPosition.X + EffectiveSize.Width; ++X)
		{
			const FIntPoint Position(X, Y);
			if (!IsValidGridPosition(Position))
			{
				return false;
			}

			const int32 Index = GridPositionToIndex(Position);
			if (!GridSlots.IsValidIndex(Index) || GridSlots[Index].OccupyingItemID != Item.InstanceID)
			{
				return false;
			}
		}
	}

	return OccupiedCellCount == (EffectiveSize.Width * EffectiveSize.Height);
}

void UInventoryComponent::RecalculateItemCountForID(FName ItemDataID)
{
	int32 RecalculatedCount = 0;
	for (const FInventoryItemInstance& Item : InventoryList.Items)
	{
		if (Item.ItemDataID == ItemDataID)
		{
			RecalculatedCount += Item.StackCount;
		}
	}

	if (RecalculatedCount > 0)
	{
		ItemCountCache.FindOrAdd(ItemDataID) = RecalculatedCount;
	}
	else
	{
		ItemCountCache.Remove(ItemDataID);
	}

	UE_LOG(LogProject_EXFIL, Log,
		TEXT("[InventoryCache][%s] RecalculateItemCountForID ItemDataID=%s Total=%d"),
		*InventoryDebug::GetSideLabel(this),
		*ItemDataID.ToString(),
		RecalculatedCount);
}

void UInventoryComponent::CollectFootprintIndices(FIntPoint Position, FItemSize Size,
	TSet<int32>& OutAffected) const
{
	for (int32 Y = Position.Y; Y < Position.Y + Size.Height; ++Y)
	{
		for (int32 X = Position.X; X < Position.X + Size.Width; ++X)
		{
			const FIntPoint GridPosition(X, Y);
			if (!IsValidGridPosition(GridPosition))
			{
				continue;
			}

			const int32 Index = GridPositionToIndex(GridPosition);
			if (GridSlots.IsValidIndex(Index))
			{
				OutAffected.Add(Index);
			}
		}
	}
}

void UInventoryComponent::RebuildGridSlotsFromItems()
{
	InitializeGridStorage();

	for (const FInventoryItemInstance& Item : InventoryList.Items)
	{
		OccupySlots(Item.RootPosition, Item.GetEffectiveSize(), Item.InstanceID);
	}
}

void UInventoryComponent::RebuildAllCachesFromItems()
{
	RebuildGridSlotsFromItems();
	RebuildItemIndexMap();
	RebuildItemCountCache();
}

void UInventoryComponent::RebuildItemCountCache()
{
	ItemCountCache.Empty();
	for (const FInventoryItemInstance& Item : InventoryList.Items)
	{
		ItemCountCache.FindOrAdd(Item.ItemDataID) += Item.StackCount;
	}
}

void UInventoryComponent::RebuildItemIndexMap()
{
	ItemIndexMap.Empty(InventoryList.Items.Num());
	for (int32 i = 0; i < InventoryList.Items.Num(); ++i)
	{
		ItemIndexMap.Add(InventoryList.Items[i].InstanceID, i);
	}
}
#pragma endregion

#pragma region 2D Grid Helpers
bool UInventoryComponent::IsValidGridPosition(FIntPoint Position) const
{
	return Position.X >= 0 && Position.X < GridWidth
		&& Position.Y >= 0 && Position.Y < GridHeight;
}

int32 UInventoryComponent::GridPositionToIndex(FIntPoint Position) const
{
	return Position.Y * GridWidth + Position.X;
}

FIntPoint UInventoryComponent::IndexToGridPosition(int32 Index) const
{
	return FIntPoint(Index % GridWidth, Index / GridWidth);
}

bool UInventoryComponent::AreSlotsFree(FIntPoint Position, FItemSize Size) const
{
	if (Position.X < 0 || Position.Y < 0
		|| Position.X + Size.Width > GridWidth
		|| Position.Y + Size.Height > GridHeight)
	{
		return false;
	}

	const uint16 Mask = static_cast<uint16>(((1 << Size.Width) - 1) << Position.X);
	for (int32 Y = Position.Y; Y < Position.Y + Size.Height; ++Y)
	{
		if ((RowBitmap[Y] & Mask) != 0)
		{
			return false;
		}
	}
	return true;
}

bool UInventoryComponent::AreSlotsFreeForItem(FIntPoint Position, FItemSize Size,
	const FGuid& IgnoreInstanceID) const
{
	if (Position.X < 0 || Position.Y < 0
		|| Position.X + Size.Width > GridWidth
		|| Position.Y + Size.Height > GridHeight)
	{
		return false;
	}

	for (int32 Y = Position.Y; Y < Position.Y + Size.Height; ++Y)
	{
		for (int32 X = Position.X; X < Position.X + Size.Width; ++X)
		{
			const int32 Index = GridPositionToIndex(FIntPoint(X, Y));
			if (!GridSlots.IsValidIndex(Index))
			{
				return false;
			}

			const FInventorySlot& Slot = GridSlots[Index];
			if (!Slot.IsEmpty() && Slot.OccupyingItemID != IgnoreInstanceID)
			{
				return false;
			}
		}
	}

	return true;
}

void UInventoryComponent::OccupySlots(FIntPoint Position, FItemSize Size, const FGuid& ItemID)
{
	for (int32 Y = Position.Y; Y < Position.Y + Size.Height; Y++)
	{
		for (int32 X = Position.X; X < Position.X + Size.Width; X++)
		{
			const int32 Index = Y * GridWidth + X;
			if (!GridSlots.IsValidIndex(Index))
			{
				continue;
			}

			const bool bIsRoot = (X == Position.X && Y == Position.Y);
			GridSlots[Index].Occupy(ItemID, bIsRoot);
			SetBit(X, Y, true);
		}
	}
}

void UInventoryComponent::FreeSlots(const FInventoryItemInstance& Item)
{
	FreeSlotsAt(Item.RootPosition, Item.GetEffectiveSize());
}

void UInventoryComponent::FreeSlotsAt(FIntPoint Position, FItemSize EffectiveSize)
{
	for (int32 Y = Position.Y; Y < Position.Y + EffectiveSize.Height; Y++)
	{
		for (int32 X = Position.X; X < Position.X + EffectiveSize.Width; X++)
		{
			const int32 Index = Y * GridWidth + X;
			if (GridSlots.IsValidIndex(Index))
			{
				GridSlots[Index].Clear();
				SetBit(X, Y, false);
			}
		}
	}
}

void UInventoryComponent::SetBit(int32 Col, int32 Row, bool bOccupied)
{
	if (!RowBitmap.IsValidIndex(Row))
	{
		return;
	}

	if (bOccupied)
	{
		RowBitmap[Row] |= (1 << Col);
	}
	else
	{
		RowBitmap[Row] &= ~(1 << Col);
	}
}
#pragma endregion

#pragma region Item Lookup Helpers
FInventoryItemInstance* UInventoryComponent::FindItemByInstanceID(const FGuid& InstanceID)
{
	if (const int32* Index = ItemIndexMap.Find(InstanceID))
	{
		if (InventoryList.Items.IsValidIndex(*Index))
		{
			return &InventoryList.Items[*Index];
		}
	}
	return nullptr;
}

const FInventoryItemInstance* UInventoryComponent::FindItemByInstanceID(const FGuid& InstanceID) const
{
	if (const int32* Index = ItemIndexMap.Find(InstanceID))
	{
		if (InventoryList.Items.IsValidIndex(*Index))
		{
			return &InventoryList.Items[*Index];
		}
	}
	return nullptr;
}

int32 UInventoryComponent::FindItemIndexByInstanceID(const FGuid& InstanceID) const
{
	if (const int32* Index = ItemIndexMap.Find(InstanceID))
	{
		if (InventoryList.Items.IsValidIndex(*Index))
		{
			return *Index;
		}
	}
	return INDEX_NONE;
}
#pragma endregion
