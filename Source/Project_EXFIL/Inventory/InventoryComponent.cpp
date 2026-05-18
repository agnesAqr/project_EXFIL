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

namespace
{
	static void CollectChangedItemCountIDs(const TMap<FName, int32>& OldCounts,
		const TMap<FName, int32>& NewCounts, TSet<FName>& OutChangedIDs)
	{
		for (const TPair<FName, int32>& Pair : OldCounts)
		{
			const int32* NewCount = NewCounts.Find(Pair.Key);
			if (!NewCount || *NewCount != Pair.Value)
			{
				OutChangedIDs.Add(Pair.Key);
			}
		}

		for (const TPair<FName, int32>& Pair : NewCounts)
		{
			const int32* OldCount = OldCounts.Find(Pair.Key);
			if (!OldCount || *OldCount != Pair.Value)
			{
				OutChangedIDs.Add(Pair.Key);
			}
		}
	}

	static bool ShouldHandleReplicationCallbacks(const UInventoryComponent* Component)
	{
		if (!Component)
		{
			return false;
		}

		const AActor* Owner = Component->GetOwner();
		return Owner && !Owner->HasAuthority();
	}
}

void FInventoryFastArray::PreReplicatedRemove(
	const TArrayView<int32>& RemovedIndices, int32 )
{
	if (!OwnerComponent)
	{
		return;
	}

	if (!ShouldHandleReplicationCallbacks(OwnerComponent))
	{
		return;
	}

	if (!OwnerComponent->bCachesInitialized)
	{
		return;
	}

	for (int32 ArrayIdx = RemovedIndices.Num() - 1; ArrayIdx >= 0; --ArrayIdx)
	{
		const int32 RemovedIndex = RemovedIndices[ArrayIdx];
		if (!Items.IsValidIndex(RemovedIndex))
		{
			continue;
		}

		FPendingRemove Pending;
		Pending.Removed = Items[RemovedIndex];
		Pending.RemovedIndex = RemovedIndex;
		PendingRemoves.Add(Pending);

		PendingChangedItemDataIDs.Add(Items[RemovedIndex].ItemDataID);
	}
}

void FInventoryFastArray::PostReplicatedAdd(
	const TArrayView<int32>& AddedIndices, int32 )
{
	if (!OwnerComponent)
	{
		return;
	}

	if (!ShouldHandleReplicationCallbacks(OwnerComponent))
	{
		return;
	}

	if (!OwnerComponent->bCachesInitialized)
	{
		return;
	}

	for (const int32 AddedIndex : AddedIndices)
	{
		if (!Items.IsValidIndex(AddedIndex))
		{
			continue;
		}

		OwnerComponent->ApplyItemAdded_Local(
			Items[AddedIndex], AddedIndex, &PendingDirtyIndices);
		PendingChangedItemDataIDs.Add(Items[AddedIndex].ItemDataID);
	}
}

void FInventoryFastArray::PostReplicatedChange(
	const TArrayView<int32>& ChangedIndices, int32)
{
	if (!OwnerComponent)
	{
		return;
	}

	if (!ShouldHandleReplicationCallbacks(OwnerComponent))
	{
		return;
	}

	if (!OwnerComponent->bCachesInitialized)
	{
		return;
	}

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

		const int32 OldCount = OwnerComponent->GetItemCountByID_Cached(Item.ItemDataID);
		const bool bFootprintChanged = !OwnerComponent->DoesGridMatchItemFootprint(Item);
		if (bFootprintChanged)
		{
			OwnerComponent->ApplyItemMovedByScan_Local(Item, &PendingDirtyIndices);
		}
		else
		{
			OwnerComponent->CollectFootprintIndices(
				Item.RootPosition, Item.GetEffectiveSize(), PendingDirtyIndices);
		}

		OwnerComponent->RecalculateItemCountForID(Item.ItemDataID);
		const int32 NewCount = OwnerComponent->GetItemCountByID_Cached(Item.ItemDataID);
		if (OldCount != NewCount)
		{
			PendingChangedItemDataIDs.Add(Item.ItemDataID);
		}
	}
}

void FInventoryFastArray::PostReplicatedReceive(
	const FFastArraySerializer::FPostReplicatedReceiveParameters& )
{
	if (!OwnerComponent)
	{
		PendingDirtyIndices.Reset();
		PendingChangedItemDataIDs.Reset();
		PendingRemoves.Reset();
		return;
	}

	if (!ShouldHandleReplicationCallbacks(OwnerComponent))
	{
		PendingDirtyIndices.Reset();
		PendingChangedItemDataIDs.Reset();
		PendingRemoves.Reset();
		return;
	}

	if (!OwnerComponent->bCachesInitialized)
	{
		const TMap<FName, int32> OldCounts = OwnerComponent->ItemCountCache;
		OwnerComponent->RebuildAllCachesFromItems();

		TSet<FName> ChangedItemDataIDs;
		CollectChangedItemCountIDs(
			OldCounts, OwnerComponent->ItemCountCache, ChangedItemDataIDs);

		OwnerComponent->BroadcastFullInventoryRefresh();
		OwnerComponent->bCachesInitialized = true;

		if (ChangedItemDataIDs.Num() > 0)
		{
			OwnerComponent->OnInventoryItemCountsChanged.Broadcast(ChangedItemDataIDs);
		}

		PendingDirtyIndices.Reset();
		PendingChangedItemDataIDs.Reset();
		PendingRemoves.Reset();
		return;
	}

	for (const FPendingRemove& Pending : PendingRemoves)
	{
		OwnerComponent->ApplyItemRemoved_Local(
			Pending.Removed, Pending.RemovedIndex, &PendingDirtyIndices);
	}
	PendingRemoves.Reset();

	if (PendingDirtyIndices.Num() > 0)
	{
		OwnerComponent->OnInventoryUpdated.Broadcast(PendingDirtyIndices);
		PendingDirtyIndices.Reset();
	}

	if (PendingChangedItemDataIDs.Num() > 0)
	{
		OwnerComponent->OnInventoryItemCountsChanged.Broadcast(PendingChangedItemDataIDs);
		PendingChangedItemDataIDs.Reset();
	}

	OwnerComponent->EnsureCacheConsistency_Debug();
}

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

#pragma region Engine Lifecycle / Replication
void UInventoryComponent::OnRegister()
{
	Super::OnRegister();
	InventoryList.OwnerComponent = this;
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UInventoryComponent, InventoryList, COND_OwnerOnly);
}
#pragma endregion

#pragma region External Entry: Request API
void UInventoryComponent::RequestRemoveItem(FGuid ItemInstanceID)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!Owner->HasAuthority())
	{
		Server_RequestRemoveItem(ItemInstanceID);
		return;
	}

	RemoveItem_Internal(ItemInstanceID);
}

void UInventoryComponent::RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!Owner->HasAuthority())
	{
		Server_RequestMoveItem(ItemInstanceID, NewPosition, bNewRotated);
		return;
	}

	MoveItem_Internal(ItemInstanceID, NewPosition, bNewRotated);
}

void UInventoryComponent::RequestConsumeItemByID(FName ItemDataID, int32 Count)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!Owner->HasAuthority())
	{
		Server_RequestConsumeItemByID(ItemDataID, Count);
		return;
	}

	HandleConsumeRequest_Internal(ItemDataID, Count);
}

void UInventoryComponent::RequestDropItem(FGuid ItemInstanceID)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!Owner->HasAuthority())
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

	const int32 MaxStack = FMath::Max(1, ItemData->MaxStackCount);
	int32 Remaining = StackCount;

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
			ApplyItemStackChanged_Local(Existing, OldStackCount);
		}
	}

	while (Remaining > 0)
	{
		const int32 StackToAdd = FMath::Min(Remaining, MaxStack);
		if (!AddItem_Internal(ItemDataID, ItemData->GetItemSize(), StackToAdd, MaxStack))
		{
			return false;
		}
		Remaining -= StackToAdd;
	}

	return true;
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

	return AddItemAt_Internal(
		ItemDataID,
		ItemData->GetItemSize(),
		Position,
		bRotated,
		StackCount,
		ItemData->MaxStackCount);
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

	const FInventoryItemInstance RemovedItem = InventoryList.Items[Index];
	InventoryList.Items.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	InventoryList.MarkArrayDirty();
	ApplyItemRemoved_Local(RemovedItem, Index);
	EnsureCacheConsistency_Debug();

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

	if (!AreSlotsFreeForItem(NewPosition, NewEffectiveSize, InstanceID))
	{
		return false;
	}

	FoundItem->RootPosition = NewPosition;
	FoundItem->bIsRotated = bNewRotated;
	InventoryList.MarkItemDirty(*FoundItem);
	ApplyItemMoved_Local(*FoundItem, OldPosition, OldEffectiveSize);

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
	struct FPendingRemoval
	{
		FInventoryItemInstance Item;
		int32 Index = INDEX_NONE;
	};
	TArray<FPendingRemoval> PendingRemovals;

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
		}
		else
		{
			const int32 OldStackCount = Item.StackCount;
			Item.StackCount -= Remaining;
			Remaining = 0;
			InventoryList.MarkItemDirty(Item);
			ApplyItemStackChanged_Local(Item, OldStackCount);
		}
	}

	// PendingRemovals is collected in ascending index order. Iterate backward so
	// RemoveAtSwap does not invalidate indices that have not been processed yet.
	for (int32 i = PendingRemovals.Num() - 1; i >= 0; --i)
	{
		const int32 RemovedIndex = PendingRemovals[i].Index;
		InventoryList.Items.RemoveAtSwap(RemovedIndex, 1, EAllowShrinking::No);
		ApplyItemRemoved_Local(PendingRemovals[i].Item, RemovedIndex);
	}

	if (PendingRemovals.Num() > 0)
	{
		InventoryList.MarkArrayDirty();
		EnsureCacheConsistency_Debug();
	}
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
		if (CurrentStack > 1)
		{
			DecrementStack_Internal(ItemInstanceID);
		}
		else
		{
			RemoveItem_Internal(ItemInstanceID);
		}
	}

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
	ApplyItemStackChanged_Local(*Item, OldStackCount);
	return Item->StackCount;
}
#pragma endregion

#pragma region Read Only: Query API
bool UInventoryComponent::CanPlaceItemAt(FIntPoint Position, FItemSize Size) const
{
	return AreSlotsFree(Position, Size);
}

bool UInventoryComponent::CanPlaceItemAtIgnoringInstance(
	FIntPoint Position, FItemSize Size, FGuid IgnoreInstanceID) const
{
	return AreSlotsFreeForItem(Position, Size, IgnoreInstanceID);
}

bool UInventoryComponent::FindFirstAvailableSlot(FItemSize Size, FIntPoint& OutPosition) const
{
	const int32 W = Size.Width;
	const int32 H = Size.Height;
	if (W <= 0 || H <= 0 || W > GridWidth || H > GridHeight)
	{
		return false;
	}

	const uint16 BaseMask = static_cast<uint16>((static_cast<uint32>(1) << W) - 1U);

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

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			CachedItemSub = GI->GetSubsystem<UItemDataSubsystem>();
		}
	}

	RebuildAllCachesFromItems();
	const AActor* Owner = GetOwner();
	if (Owner && !Owner->HasAuthority() && InventoryList.Items.Num() > 0)
	{
		BroadcastFullInventoryRefresh();
	}
	bCachesInitialized = true;
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
	if (ItemDataID.IsNone() || Count != 1)
	{
		return;
	}

	const FItemData* ItemData = CachedItemSub ? CachedItemSub->GetItemData(ItemDataID) : nullptr;
	if (!ItemData || ItemData->ItemType != EItemType::Consumable)
	{
		UE_LOG(LogProject_EXFIL, Warning,
			TEXT("HandleConsumeRequest_Internal: '%s' is not consumable"),
			*ItemDataID.ToString());
		return;
	}

	UAbilitySystemComponent* ConsumableASC = nullptr;
	FGameplayEffectSpecHandle ConsumableEffectSpec;
	if (!BuildConsumableEffectSpec_Internal(ItemDataID, ConsumableASC, ConsumableEffectSpec))
	{
		return;
	}

	if (!ConsumeItemByID_Internal(ItemDataID, 1))
	{
		return;
	}

	ApplyConsumableEffect_Internal(ConsumableASC, ConsumableEffectSpec);
}

bool UInventoryComponent::BuildConsumableEffectSpec_Internal(
	FName ItemDataID,
	UAbilitySystemComponent*& OutASC,
	FGameplayEffectSpecHandle& OutSpec)
{
	OutASC = nullptr;
	OutSpec = FGameplayEffectSpecHandle();

	AActor* Owner = GetOwner();
	if (!Owner || !CachedItemSub)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = Owner->FindComponentByClass<UAbilitySystemComponent>();
	if (!ASC)
	{
		return false;
	}

	const FItemData* ItemData = CachedItemSub->GetItemData(ItemDataID);
	if (!ItemData || ItemData->ConsumableEffect.IsNull())
	{
		return false;
	}

	const TSubclassOf<UGameplayEffect> GEClass =
		CachedItemSub->GetCachedEffect(ItemData->ConsumableEffect);
	if (!GEClass)
	{
		return false;
	}

	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	Ctx.AddSourceObject(Owner);
	OutSpec = ASC->MakeOutgoingSpec(GEClass, 1.f, Ctx);
	OutASC = ASC;
	return OutSpec.IsValid();
}

bool UInventoryComponent::ApplyConsumableEffect_Internal(
	UAbilitySystemComponent* ASC,
	const FGameplayEffectSpecHandle& EffectSpec)
{
	if (!ASC || !EffectSpec.IsValid())
	{
		return false;
	}

	ASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	return true;
}
#pragma endregion

#pragma region Server Authority: Mutation Helpers
bool UInventoryComponent::AddItem_Internal(FName ItemDataID, FItemSize Size,
	int32 StackCount, int32 MaxStack, TSet<int32>* OutAffected)
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
	TSet<int32>* OutAffected)
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
	InventoryList.MarkArrayDirty();
	InventoryList.MarkItemDirty(NewItem);
	ApplyItemAdded_Local(NewItem, InventoryList.Items.Num() - 1, OutAffected);

	return true;
}
#pragma endregion

#pragma region State Sync / Cache Rebuild
void UInventoryComponent::BroadcastFullInventoryRefresh()
{
	TSet<int32> AllIndices;
	AllIndices.Reserve(GridSlots.Num());
	for (int32 i = 0; i < GridSlots.Num(); ++i)
	{
		AllIndices.Add(i);
	}

	OnInventoryUpdated.Broadcast(AllIndices);
}

void UInventoryComponent::InitializeGridStorage()
{
	GridWidth = FMath::Clamp(GridWidth, 1, MaxGridBitmapWidth);
	GridHeight = FMath::Max(1, GridHeight);

	GridSlots.SetNum(GridWidth * GridHeight);
	for (FInventorySlot& Slot : GridSlots)
		Slot.Clear();

	RowBitmap.Init(0, GridHeight);
}

void UInventoryComponent::ApplyItemAdded_Local(const FInventoryItemInstance& Item, int32 ItemIndex,
	TSet<int32>* OutAffected)
{
	OccupySlots(Item.RootPosition, Item.GetEffectiveSize(), Item.InstanceID);
	ItemIndexMap.Add(Item.InstanceID, ItemIndex);
	ItemCountCache.FindOrAdd(Item.ItemDataID) += Item.StackCount;
	if (OutAffected)
	{
		CollectFootprintIndices(Item.RootPosition, Item.GetEffectiveSize(), *OutAffected);
	}
}

void UInventoryComponent::ApplyItemMoved_Local(const FInventoryItemInstance& NewItem, FIntPoint OldPos,
	FItemSize OldEffSize, TSet<int32>* OutAffected)
{
	if (OutAffected)
	{
		CollectFootprintIndices(OldPos, OldEffSize, *OutAffected);
	}
	FreeSlotsAt(OldPos, OldEffSize);
	OccupySlots(NewItem.RootPosition, NewItem.GetEffectiveSize(), NewItem.InstanceID);
	if (OutAffected)
	{
		CollectFootprintIndices(NewItem.RootPosition, NewItem.GetEffectiveSize(), *OutAffected);
	}
}

void UInventoryComponent::ApplyItemStackChanged_Local(const FInventoryItemInstance& NewItem,
	int32 OldStackCount, TSet<int32>* OutAffected)
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

	if (OutAffected)
	{
		CollectFootprintIndices(NewItem.RootPosition, NewItem.GetEffectiveSize(), *OutAffected);
	}
}

void UInventoryComponent::ApplyItemRemoved_Local(const FInventoryItemInstance& Removed,
	int32 RemovedIndex, TSet<int32>* OutAffected)
{
	const FItemSize EffectiveSize = Removed.GetEffectiveSize();
	for (int32 Y = Removed.RootPosition.Y; Y < Removed.RootPosition.Y + EffectiveSize.Height; ++Y)
	{
		for (int32 X = Removed.RootPosition.X; X < Removed.RootPosition.X + EffectiveSize.Width; ++X)
		{
			const FIntPoint Position(X, Y);
			if (!IsValidGridPosition(Position))
			{
				continue;
			}

			const int32 Index = GridPositionToIndex(Position);
			if (!GridSlots.IsValidIndex(Index))
			{
				continue;
			}

			if (GridSlots[Index].OccupyingItemID == Removed.InstanceID)
			{
				GridSlots[Index].Clear();
				SetBit(X, Y, false);
			}
			if (OutAffected)
			{
				OutAffected->Add(Index);
			}
		}
	}

	ItemIndexMap.Remove(Removed.InstanceID);
	if (InventoryList.Items.IsValidIndex(RemovedIndex))
	{
		const FGuid& MovedID = InventoryList.Items[RemovedIndex].InstanceID;
		ItemIndexMap.FindOrAdd(MovedID) = RemovedIndex;
	}

	int32& CachedCount = ItemCountCache.FindOrAdd(Removed.ItemDataID);
	CachedCount -= Removed.StackCount;
	if (CachedCount <= 0)
	{
		ItemCountCache.Remove(Removed.ItemDataID);
	}
}

void UInventoryComponent::ApplyItemMovedByScan_Local(const FInventoryItemInstance& NewItem,
	TSet<int32>* OutAffected)
{
	for (int32 Index = 0; Index < GridSlots.Num(); ++Index)
	{
		if (GridSlots[Index].OccupyingItemID != NewItem.InstanceID)
		{
			continue;
		}

		GridSlots[Index].Clear();
		if (OutAffected)
		{
			OutAffected->Add(Index);
		}

		const FIntPoint Position = IndexToGridPosition(Index);
		SetBit(Position.X, Position.Y, false);
	}

	OccupySlots(NewItem.RootPosition, NewItem.GetEffectiveSize(), NewItem.InstanceID);
	if (OutAffected)
	{
		CollectFootprintIndices(NewItem.RootPosition, NewItem.GetEffectiveSize(), *OutAffected);
	}
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

void UInventoryComponent::EnsureCacheConsistency_Debug() const
{
#if !UE_BUILD_SHIPPING
	ensureMsgf(ItemIndexMap.Num() == InventoryList.Items.Num(),
		TEXT("ItemIndexMap size mismatch: %d vs %d"),
		ItemIndexMap.Num(), InventoryList.Items.Num());

	for (int32 Index = 0; Index < InventoryList.Items.Num(); ++Index)
	{
		const FInventoryItemInstance& Item = InventoryList.Items[Index];
		const int32* MappedIndex = ItemIndexMap.Find(Item.InstanceID);
		ensureMsgf(MappedIndex && *MappedIndex == Index,
			TEXT("ItemIndexMap stale at %d for %s"),
			Index, *Item.InstanceID.ToString());
	}
#endif
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
	return AreSlotsFree_Internal(Position, Size, nullptr);
}

bool UInventoryComponent::AreSlotsFreeForItem(FIntPoint Position, FItemSize Size,
	const FGuid& IgnoreInstanceID) const
{
	return AreSlotsFree_Internal(Position, Size, &IgnoreInstanceID);
}

bool UInventoryComponent::AreSlotsFree_Internal(FIntPoint Position, FItemSize Size,
	const FGuid* IgnoreInstanceID) const
{
	if (Size.Width <= 0 || Size.Height <= 0
		|| Position.X < 0 || Position.Y < 0
		|| Position.X + Size.Width > GridWidth
		|| Position.Y + Size.Height > GridHeight)
	{
		return false;
	}

	const uint16 PlacementMask = static_cast<uint16>(
		((static_cast<uint32>(1) << Size.Width) - 1U) << Position.X);

	bool bHasIgnore = false;
	FIntPoint IgnorePos(0, 0);
	FItemSize IgnoreSize(0, 0);
	uint16 IgnoreMask = 0;

	if (IgnoreInstanceID)
	{
		if (const int32* IndexPtr = ItemIndexMap.Find(*IgnoreInstanceID))
		{
			if (InventoryList.Items.IsValidIndex(*IndexPtr))
			{
				const FInventoryItemInstance& IgnoreItem = InventoryList.Items[*IndexPtr];
				IgnorePos = IgnoreItem.RootPosition;
				IgnoreSize = IgnoreItem.GetEffectiveSize();
				if (IgnoreSize.Width > 0 && IgnoreSize.Height > 0
					&& IgnorePos.X >= 0 && IgnorePos.Y >= 0
					&& IgnorePos.X + IgnoreSize.Width <= GridWidth
					&& IgnorePos.Y + IgnoreSize.Height <= GridHeight)
				{
					IgnoreMask = static_cast<uint16>(
						((static_cast<uint32>(1) << IgnoreSize.Width) - 1U) << IgnorePos.X);
					bHasIgnore = true;
				}
			}
		}
	}

	for (int32 Y = Position.Y; Y < Position.Y + Size.Height; ++Y)
	{
		if (!RowBitmap.IsValidIndex(Y))
		{
			return false;
		}

		const bool bRowInIgnoreRange = bHasIgnore
			&& Y >= IgnorePos.Y && Y < IgnorePos.Y + IgnoreSize.Height;
		const uint16 IgnoreMaskForRow = bRowInIgnoreRange ? IgnoreMask : 0;
		const uint16 EffectiveRow = static_cast<uint16>(RowBitmap[Y] & ~IgnoreMaskForRow);
		if ((EffectiveRow & PlacementMask) != 0)
		{
			return false;
		}
	}

	return true;
}

void UInventoryComponent::OccupySlots(FIntPoint Position, FItemSize Size, const FGuid& ItemID)
{
	if (Position.X < 0 || Position.Y < 0
		|| Position.X + Size.Width > GridWidth
		|| Position.Y + Size.Height > GridHeight)
	{
		ensureAlwaysMsgf(false,
			TEXT("OccupySlots received invalid footprint. Position=(%d,%d), Size=%dx%d, Grid=%dx%d"),
			Position.X, Position.Y, Size.Width, Size.Height, GridWidth, GridHeight);
		return;
	}

	for (int32 Y = Position.Y; Y < Position.Y + Size.Height; Y++)
	{
		for (int32 X = Position.X; X < Position.X + Size.Width; X++)
		{
			const int32 Index = GridPositionToIndex(FIntPoint(X, Y));
			checkf(GridSlots.IsValidIndex(Index),
				TEXT("Validated grid position produced invalid index."));

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
	if (Position.X < 0 || Position.Y < 0
		|| Position.X + EffectiveSize.Width > GridWidth
		|| Position.Y + EffectiveSize.Height > GridHeight)
	{
		ensureAlwaysMsgf(false,
			TEXT("FreeSlotsAt received invalid footprint. Position=(%d,%d), Size=%dx%d, Grid=%dx%d"),
			Position.X, Position.Y, EffectiveSize.Width, EffectiveSize.Height, GridWidth, GridHeight);
		return;
	}

	for (int32 Y = Position.Y; Y < Position.Y + EffectiveSize.Height; Y++)
	{
		for (int32 X = Position.X; X < Position.X + EffectiveSize.Width; X++)
		{
			const int32 Index = GridPositionToIndex(FIntPoint(X, Y));
			checkf(GridSlots.IsValidIndex(Index),
				TEXT("Validated grid position produced invalid index."));
			GridSlots[Index].Clear();
			SetBit(X, Y, false);
		}
	}
}

void UInventoryComponent::SetBit(int32 Col, int32 Row, bool bOccupied)
{
	if (!RowBitmap.IsValidIndex(Row) || Col < 0 || Col >= GridWidth)
	{
		return;
	}

	if (bOccupied)
	{
		RowBitmap[Row] |= static_cast<uint16>(1U << Col);
	}
	else
	{
		RowBitmap[Row] &= static_cast<uint16>(~(1U << Col));
	}
}
#pragma endregion

#pragma region Item Lookup Helpers
FInventoryItemInstance* UInventoryComponent::FindItemByInstanceID(const FGuid& InstanceID)
{
	if (const int32* Index = ItemIndexMap.Find(InstanceID))
	{
		if (InventoryList.Items.IsValidIndex(*Index) &&
			InventoryList.Items[*Index].InstanceID == InstanceID)
		{
			return &InventoryList.Items[*Index];
		}
	}

	for (int32 Index = 0; Index < InventoryList.Items.Num(); ++Index)
	{
		if (InventoryList.Items[Index].InstanceID == InstanceID)
		{
			ItemIndexMap.FindOrAdd(InstanceID) = Index;
			return &InventoryList.Items[Index];
		}
	}

	ItemIndexMap.Remove(InstanceID);
	return nullptr;
}

const FInventoryItemInstance* UInventoryComponent::FindItemByInstanceID(const FGuid& InstanceID) const
{
	if (const int32* Index = ItemIndexMap.Find(InstanceID))
	{
		if (InventoryList.Items.IsValidIndex(*Index) &&
			InventoryList.Items[*Index].InstanceID == InstanceID)
		{
			return &InventoryList.Items[*Index];
		}
	}

	for (const FInventoryItemInstance& Item : InventoryList.Items)
	{
		if (Item.InstanceID == InstanceID)
		{
			return &Item;
		}
	}

	return nullptr;
}

int32 UInventoryComponent::FindItemIndexByInstanceID(const FGuid& InstanceID) const
{
	if (const int32* Index = ItemIndexMap.Find(InstanceID))
	{
		if (InventoryList.Items.IsValidIndex(*Index) &&
			InventoryList.Items[*Index].InstanceID == InstanceID)
		{
			return *Index;
		}
	}

	for (int32 Index = 0; Index < InventoryList.Items.Num(); ++Index)
	{
		if (InventoryList.Items[Index].InstanceID == InstanceID)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}
#pragma endregion
