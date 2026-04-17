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

void FInventoryFastArray::PostReplicatedReceive(
	const FFastArraySerializer::FPostReplicatedReceiveParameters& /*Parameters*/)
{
	if (OwnerComponent)
	{
		OwnerComponent->HandleReplicatedInventoryReceived();
	}
}

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	InventoryList.OwnerComponent = this;
}

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
	RebuildAllCachesFromItems();
}

// ========== Replication ==========

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UInventoryComponent, InventoryList, COND_OwnerOnly);
}

// ========== Request API ==========

void UInventoryComponent::RequestRemoveItem(FGuid ItemInstanceID)
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		Server_RequestRemoveItem(ItemInstanceID);
		return;
	}

	RemoveItem_Internal(ItemInstanceID);
}

void UInventoryComponent::RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition)
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		UE_LOG(LogProject_EXFIL, Verbose,
			TEXT("RequestMoveItem(Client): Item=%s -> (%d,%d)"),
			*ItemInstanceID.ToString(), NewPosition.X, NewPosition.Y);
		Server_RequestMoveItem(ItemInstanceID, NewPosition);
		return;
	}

	UE_LOG(LogProject_EXFIL, Verbose,
		TEXT("RequestMoveItem(ServerLocal): Item=%s -> (%d,%d)"),
		*ItemInstanceID.ToString(), NewPosition.X, NewPosition.Y);
	MoveItem_Internal(ItemInstanceID, NewPosition);
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

// ========== Server RPCs ==========

void UInventoryComponent::Server_RequestRemoveItem_Implementation(FGuid ItemInstanceID)
{
	if (!ItemInstanceID.IsValid())
	{
		return;
	}

	RemoveItem_Internal(ItemInstanceID);
}

void UInventoryComponent::Server_RequestMoveItem_Implementation(
	FGuid ItemInstanceID, FIntPoint NewPosition)
{
	if (!ItemInstanceID.IsValid() || NewPosition.X < 0 || NewPosition.Y < 0)
	{
		return;
	}

	UE_LOG(LogProject_EXFIL, Verbose,
		TEXT("Server_RequestMoveItem: Item=%s -> (%d,%d)"),
		*ItemInstanceID.ToString(), NewPosition.X, NewPosition.Y);
	MoveItem_Internal(ItemInstanceID, NewPosition);
}

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

// ========== State Change ==========

void UInventoryComponent::HandleInventoryStateChanged()
{
	RebuildAllCachesFromItems();
	BroadcastFullInventoryRefresh();
}

void UInventoryComponent::HandleReplicatedInventoryReceived()
{
	RebuildAllCachesFromItems();
	BroadcastFullInventoryRefresh();
}

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

// ========== Initialize / Cache Rebuild ==========

void UInventoryComponent::InitializeGridStorage()
{
	if (GridWidth <= 0)
	{
		GridWidth = 1;
	}

	if (GridHeight <= 0)
	{
		GridHeight = 1;
	}

	GridSlots.SetNum(GridWidth * GridHeight);
	for (FInventorySlot& Slot : GridSlots)
	{
		Slot.Clear();
	}

	// Rebuild paths must clear every row bit even when the array size is unchanged.
	RowBitmap.Init(0, GridHeight);
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

// ========== Internal Write API ==========

bool UInventoryComponent::AddItem_Internal(FName ItemDataID, FItemSize Size,
	int32 StackCount, int32 MaxStack)
{
	checkf(GetOwner() && GetOwner()->HasAuthority(),
		TEXT("AddItem_Internal must run on the server."));

	FIntPoint FoundPosition;
	if (!FindFirstAvailableSlot(Size, FoundPosition))
	{
		return false;
	}

	return AddItemAt_Internal(ItemDataID, Size, FoundPosition, StackCount, MaxStack);
}

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
	bool bMergedExistingStacks = false;

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
			Existing.StackCount += ToMerge;
			Remaining -= ToMerge;
			bMergedExistingStacks = true;
			InventoryList.MarkItemDirty(Existing);

			UE_LOG(LogProject_EXFIL, Verbose,
				TEXT("AddItemByID_Internal: Merged %d into existing stack of '%s' (now %d/%d)"),
				ToMerge, *ItemDataID.ToString(), Existing.StackCount, Existing.MaxStackCount);
		}

		if (Remaining <= 0)
		{
			HandleInventoryStateChanged();
			return true;
		}
	}

	const bool bAddedNewStack = AddItem_Internal(ItemDataID, ItemData->GetItemSize(), Remaining, MaxStack);
	if (!bAddedNewStack && bMergedExistingStacks)
	{
		HandleInventoryStateChanged();
	}

	return bAddedNewStack;
}

bool UInventoryComponent::AddItemAt_Internal(FName ItemDataID, FItemSize Size,
	FIntPoint Position, int32 StackCount, int32 MaxStack)
{
	checkf(GetOwner() && GetOwner()->HasAuthority(),
		TEXT("AddItemAt_Internal must run on the server."));

	if (!AreSlotsFree(Position, Size))
	{
		UE_LOG(LogProject_EXFIL, Warning,
			TEXT("AddItemAt_Internal: Cannot place item '%s' at (%d,%d) Size:%dx%d"),
			*ItemDataID.ToString(), Position.X, Position.Y,
			Size.Width, Size.Height);
		return false;
	}

	FInventoryItemInstance& NewItem = InventoryList.Items.AddDefaulted_GetRef();
	NewItem.InstanceID = FGuid::NewGuid();
	NewItem.ItemDataID = ItemDataID;
	NewItem.RootPosition = Position;
	NewItem.ItemSize = Size;
	NewItem.bIsRotated = false;
	NewItem.StackCount = FMath::Clamp(StackCount, 1, MaxStack);
	NewItem.MaxStackCount = MaxStack;

	InventoryList.MarkItemDirty(NewItem);

	UE_LOG(LogProject_EXFIL, Verbose,
		TEXT("Item added: '%s' ID=%s at (%d,%d) Size:%dx%d Stack:%d/%d"),
		*ItemDataID.ToString(), *NewItem.InstanceID.ToString(),
		Position.X, Position.Y, Size.Width, Size.Height,
		NewItem.StackCount, NewItem.MaxStackCount);

	HandleInventoryStateChanged();
	return true;
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

	InventoryList.Items.RemoveAt(Index);
	InventoryList.MarkArrayDirty();

	UE_LOG(LogProject_EXFIL, Verbose, TEXT("Item removed: %s"), *InstanceID.ToString());

	HandleInventoryStateChanged();
	return true;
}

bool UInventoryComponent::MoveItem_Internal(const FGuid& InstanceID, FIntPoint NewPosition)
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
	const FItemSize NewEffectiveSize = FoundItem->ItemSize;

	UE_LOG(LogProject_EXFIL, Verbose,
		TEXT("MoveItem_Internal: Attempt Item=%s From=(%d,%d) To=(%d,%d) Size=%dx%d"),
		*InstanceID.ToString(),
		OldPosition.X, OldPosition.Y,
		NewPosition.X, NewPosition.Y,
		NewEffectiveSize.Width, NewEffectiveSize.Height);

	FreeSlots(*FoundItem);

	if (!AreSlotsFree(NewPosition, NewEffectiveSize))
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

		OccupySlots(OldPosition, OldEffectiveSize, InstanceID);

		UE_LOG(LogProject_EXFIL, Warning,
			TEXT("MoveItem_Internal: Cannot move %s to (%d,%d) Size:%dx%d. Rolling back to (%d,%d)"),
			*InstanceID.ToString(),
			NewPosition.X, NewPosition.Y,
			NewEffectiveSize.Width, NewEffectiveSize.Height,
			OldPosition.X, OldPosition.Y);
		return false;
	}

	OccupySlots(NewPosition, NewEffectiveSize, InstanceID);
	FoundItem->RootPosition = NewPosition;
	FoundItem->bIsRotated = false;
	InventoryList.MarkItemDirty(*FoundItem);

	UE_LOG(LogProject_EXFIL, Verbose,
		TEXT("Item moved: %s from (%d,%d) to (%d,%d)"),
		*InstanceID.ToString(),
		OldPosition.X, OldPosition.Y,
		NewPosition.X, NewPosition.Y);

	HandleInventoryStateChanged();
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
	TArray<int32> IndicesToRemove;
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
			IndicesToRemove.Add(i);
			bModifiedAny = true;
		}
		else
		{
			Item.StackCount -= Remaining;
			Remaining = 0;
			bModifiedAny = true;
			InventoryList.MarkItemDirty(Item);
		}
	}

	for (int32 i = IndicesToRemove.Num() - 1; i >= 0; --i)
	{
		InventoryList.Items.RemoveAt(IndicesToRemove[i]);
	}

	if (IndicesToRemove.Num() > 0)
	{
		InventoryList.MarkArrayDirty();
	}

	if (bModifiedAny)
	{
		HandleInventoryStateChanged();
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
		DecrementStack_Internal(ItemInstanceID);
	}
	else
	{
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

	return DroppedItem != nullptr;
}

// ========== Query API ==========

bool UInventoryComponent::CanPlaceItemAt(FIntPoint Position, FItemSize Size) const
{
	return AreSlotsFree(Position, Size);
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

// ========== Crafting / Equipment API ==========

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

// ========== Stack Helper ==========

int32 UInventoryComponent::DecrementStack_Internal(const FGuid& InstanceID)
{
	checkf(GetOwner() && GetOwner()->HasAuthority(),
		TEXT("DecrementStack_Internal must run on the server."));

	FInventoryItemInstance* Item = FindItemByInstanceID(InstanceID);
	if (!Item)
	{
		return 0;
	}

	Item->StackCount--;

	if (Item->StackCount <= 0)
	{
		RemoveItem_Internal(InstanceID);
		return 0;
	}

	InventoryList.MarkItemDirty(*Item);
	HandleInventoryStateChanged();

	UE_LOG(LogProject_EXFIL, Verbose, TEXT("DecrementStack_Internal: '%s' now %d"),
		*Item->ItemDataID.ToString(), Item->StackCount);
	return Item->StackCount;
}

// ========== Helpers ==========

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
	const FItemSize EffectiveSize = Item.GetEffectiveSize();
	const FIntPoint& RootPos = Item.RootPosition;

	for (int32 Y = RootPos.Y; Y < RootPos.Y + EffectiveSize.Height; Y++)
	{
		for (int32 X = RootPos.X; X < RootPos.X + EffectiveSize.Width; X++)
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

// ========== Item Lookup ==========

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
