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

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

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

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		InitializeGrid();
	}
}

// ========== Replication ==========

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UInventoryComponent, Items, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, GridSlots, COND_OwnerOnly);
}

void UInventoryComponent::OnRep_Items()
{
	RebuildItemIndexMap();
	RebuildItemCountCache();
	RebuildRowBitmap();

	DirtySlotIndices.Empty();

	TMap<FGuid, const FInventoryItemInstance*> PrevMap;
	PrevMap.Reserve(PreviousItems.Num());
	for (const FInventoryItemInstance& Prev : PreviousItems)
	{
		PrevMap.Add(Prev.InstanceID, &Prev);
	}

	for (const FInventoryItemInstance& Item : Items)
	{
		const FInventoryItemInstance** PrevPtr = PrevMap.Find(Item.InstanceID);

		if (!PrevPtr || (*PrevPtr)->RootPosition != Item.RootPosition
			|| (*PrevPtr)->StackCount != Item.StackCount
			|| (*PrevPtr)->bIsRotated != Item.bIsRotated)
		{
			MarkSlotsDirty(Item.RootPosition, Item.GetEffectiveSize());

			if (PrevPtr && (*PrevPtr)->RootPosition != Item.RootPosition)
			{
				MarkSlotsDirty((*PrevPtr)->RootPosition, (*PrevPtr)->GetEffectiveSize());
			}
		}

		if (PrevPtr)
		{
			PrevMap.Remove(Item.InstanceID);
		}
	}

	for (const auto& Pair : PrevMap)
	{
		MarkSlotsDirty(Pair.Value->RootPosition, Pair.Value->GetEffectiveSize());
	}

	PreviousItems = Items;

	if (DirtySlotIndices.Num() == 0 && Items.Num() > 0)
	{
		MarkSlotsDirty(FIntPoint(0, 0), FItemSize(GridWidth, GridHeight));
	}

	OnInventoryUpdated.Broadcast(DirtySlotIndices);
	DirtySlotIndices.Empty();
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

void UInventoryComponent::RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated)
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		Server_RequestMoveItem(ItemInstanceID, NewPosition, bNewRotated);
		return;
	}

	MoveItem_Internal(ItemInstanceID, NewPosition, bNewRotated);
}

void UInventoryComponent::RequestConsumeItemByID(FName ItemDataID, int32 Count)
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		Server_RequestConsumeItemByID(ItemDataID, Count);
		return;
	}

	Server_RequestConsumeItemByID_Implementation(ItemDataID, Count);
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
	if (ItemDataID.IsNone() || Count <= 0)
	{
		return;
	}

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

	ConsumeItemByID_Internal(ItemDataID, Count);
}

void UInventoryComponent::Server_RequestDropItem_Implementation(FGuid ItemInstanceID)
{
	if (!ItemInstanceID.IsValid())
	{
		return;
	}

	DropItem_Internal(ItemInstanceID);
}

// ========== Dirty Flag ==========

void UInventoryComponent::MarkSlotsDirty(FIntPoint Position, FItemSize Size)
{
	for (int32 Y = Position.Y; Y < Position.Y + Size.Height; ++Y)
	{
		for (int32 X = Position.X; X < Position.X + Size.Width; ++X)
		{
			DirtySlotIndices.Add(Y * GridWidth + X);
		}
	}
}

void UInventoryComponent::BroadcastDirtySlots()
{
	RebuildItemIndexMap();
	RebuildItemCountCache();
	OnInventoryUpdated.Broadcast(DirtySlotIndices);
	DirtySlotIndices.Empty();
}

// ========== Initialize ==========

void UInventoryComponent::InitializeGrid()
{
	if (GridWidth <= 0 || GridHeight <= 0)
	{
		UE_LOG(LogProject_EXFIL, Error, TEXT("Invalid grid dimensions: %dx%d"), GridWidth, GridHeight);
		GridWidth = FMath::Max(GridWidth, 1);
		GridHeight = FMath::Max(GridHeight, 1);
	}

	GridSlots.SetNum(GridWidth * GridHeight);
	for (FInventorySlot& Slot : GridSlots)
	{
		Slot.Clear();
	}

	Items.Empty();
	RebuildItemIndexMap();
	RebuildItemCountCache();
	RebuildRowBitmap();

	UE_LOG(LogProject_EXFIL, Log, TEXT("Inventory grid initialized: %dx%d (%d slots)"),
		GridWidth, GridHeight, GridSlots.Num());
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

	return AddItemAt_Internal(ItemDataID, Size, FoundPosition, false, StackCount, MaxStack);
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

	if (MaxStack > 1)
	{
		for (FInventoryItemInstance& Existing : Items)
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

			MarkSlotsDirty(Existing.RootPosition, Existing.GetEffectiveSize());

			UE_LOG(LogProject_EXFIL, Log,
				TEXT("AddItemByID_Internal: Merged %d into existing stack of '%s' (now %d/%d)"),
				ToMerge, *ItemDataID.ToString(), Existing.StackCount, Existing.MaxStackCount);
		}

		if (Remaining <= 0)
		{
			BroadcastDirtySlots();
			return true;
		}
	}

	return AddItem_Internal(ItemDataID, ItemData->GetItemSize(), Remaining, MaxStack);
}

bool UInventoryComponent::AddItemAt_Internal(FName ItemDataID, FItemSize Size,
	FIntPoint Position, bool bRotated, int32 StackCount, int32 MaxStack)
{
	checkf(GetOwner() && GetOwner()->HasAuthority(),
		TEXT("AddItemAt_Internal must run on the server."));

	const FItemSize EffectiveSize = bRotated ? Size.GetRotated() : Size;

	if (!AreSlotsFree(Position, EffectiveSize))
	{
		UE_LOG(LogProject_EXFIL, Warning,
			TEXT("AddItemAt_Internal: Cannot place item '%s' at (%d,%d) Size:%dx%d"),
			*ItemDataID.ToString(), Position.X, Position.Y,
			EffectiveSize.Width, EffectiveSize.Height);
		return false;
	}

	FInventoryItemInstance NewItem;
	NewItem.InstanceID = FGuid::NewGuid();
	NewItem.ItemDataID = ItemDataID;
	NewItem.RootPosition = Position;
	NewItem.ItemSize = Size;
	NewItem.bIsRotated = bRotated;
	NewItem.StackCount = FMath::Clamp(StackCount, 1, MaxStack);
	NewItem.MaxStackCount = MaxStack;

	OccupySlots(Position, EffectiveSize, NewItem.InstanceID);
	Items.Add(NewItem);

	UE_LOG(LogProject_EXFIL, Log,
		TEXT("Item added: '%s' ID=%s at (%d,%d) Size:%dx%d Stack:%d/%d Rotated:%s"),
		*ItemDataID.ToString(), *NewItem.InstanceID.ToString(),
		Position.X, Position.Y, EffectiveSize.Width, EffectiveSize.Height,
		NewItem.StackCount, NewItem.MaxStackCount,
		bRotated ? TEXT("Yes") : TEXT("No"));

	MarkSlotsDirty(Position, EffectiveSize);
	BroadcastDirtySlots();

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

	MarkSlotsDirty(Items[Index].RootPosition, Items[Index].GetEffectiveSize());
	FreeSlots(Items[Index]);
	Items.RemoveAt(Index);

	UE_LOG(LogProject_EXFIL, Log, TEXT("Item removed: %s"), *InstanceID.ToString());

	BroadcastDirtySlots();
	return true;
}

bool UInventoryComponent::MoveItem_Internal(const FGuid& InstanceID, FIntPoint NewPosition,
	bool bNewRotated)
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

	FreeSlots(*FoundItem);

	const FItemSize NewEffectiveSize = bNewRotated
		? FoundItem->ItemSize.GetRotated()
		: FoundItem->ItemSize;

	if (!AreSlotsFree(NewPosition, NewEffectiveSize))
	{
		OccupySlots(OldPosition, OldEffectiveSize, InstanceID);

		UE_LOG(LogProject_EXFIL, Warning,
			TEXT("MoveItem_Internal: Cannot move to (%d,%d). Rolling back to (%d,%d)"),
			NewPosition.X, NewPosition.Y, OldPosition.X, OldPosition.Y);
		return false;
	}

	OccupySlots(NewPosition, NewEffectiveSize, InstanceID);
	FoundItem->RootPosition = NewPosition;
	FoundItem->bIsRotated = bNewRotated;

	UE_LOG(LogProject_EXFIL, Log,
		TEXT("Item moved: %s from (%d,%d) to (%d,%d)"),
		*InstanceID.ToString(),
		OldPosition.X, OldPosition.Y,
		NewPosition.X, NewPosition.Y);

	MarkSlotsDirty(OldPosition, OldEffectiveSize);
	MarkSlotsDirty(NewPosition, NewEffectiveSize);
	BroadcastDirtySlots();

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

	for (int32 i = 0; i < Items.Num() && Remaining > 0; ++i)
	{
		if (Items[i].ItemDataID != ItemDataID)
		{
			continue;
		}

		if (Items[i].StackCount <= Remaining)
		{
			Remaining -= Items[i].StackCount;
			IndicesToRemove.Add(i);
		}
		else
		{
			Items[i].StackCount -= Remaining;
			MarkSlotsDirty(Items[i].RootPosition, Items[i].GetEffectiveSize());
			Remaining = 0;
		}
	}

	for (int32 i = IndicesToRemove.Num() - 1; i >= 0; --i)
	{
		const int32 RemoveIdx = IndicesToRemove[i];
		MarkSlotsDirty(Items[RemoveIdx].RootPosition, Items[RemoveIdx].GetEffectiveSize());
		FreeSlots(Items[RemoveIdx]);
		Items.RemoveAt(RemoveIdx);
	}

	BroadcastDirtySlots();

	UE_LOG(LogProject_EXFIL, Log,
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
	return Items;
}

bool UInventoryComponent::IsEmpty() const
{
	return Items.Num() == 0;
}

int32 UInventoryComponent::GetItemCount(FName ItemDataID) const
{
	int32 Count = 0;
	for (const FInventoryItemInstance& Item : Items)
	{
		if (Item.ItemDataID == ItemDataID)
		{
			Count += Item.StackCount;
		}
	}
	return Count;
}

// ========== Crafting / Equipment API ==========

int32 UInventoryComponent::GetItemCountByID(FName ItemDataID) const
{
	int32 Total = 0;
	for (const FInventoryItemInstance& Item : Items)
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
	for (const FInventoryItemInstance& Item : Items)
	{
		ItemCountCache.FindOrAdd(Item.ItemDataID) += Item.StackCount;
	}
}

void UInventoryComponent::RebuildItemIndexMap()
{
	ItemIndexMap.Empty(Items.Num());
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		ItemIndexMap.Add(Items[i].InstanceID, i);
	}
}

// ========== Bitmap ==========

void UInventoryComponent::RebuildRowBitmap()
{
	RowBitmap.SetNumZeroed(GridHeight);
	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		uint16 Mask = 0;
		for (int32 X = 0; X < GridWidth; ++X)
		{
			const int32 Index = Y * GridWidth + X;
			if (!GridSlots[Index].IsEmpty())
			{
				Mask |= (1 << X);
			}
		}
		RowBitmap[Y] = Mask;
	}
}

void UInventoryComponent::SetBit(int32 Col, int32 Row, bool bOccupied)
{
	if (bOccupied)
	{
		RowBitmap[Row] |= (1 << Col);
	}
	else
	{
		RowBitmap[Row] &= ~(1 << Col);
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

	MarkSlotsDirty(Item->RootPosition, Item->GetEffectiveSize());
	BroadcastDirtySlots();

	UE_LOG(LogProject_EXFIL, Log, TEXT("DecrementStack_Internal: '%s' now %d"),
		*Item->ItemDataID.ToString(), Item->StackCount);
	return Item->StackCount;
}

// ========== Utility ==========

void UInventoryComponent::DebugPrintGrid() const
{
	UE_LOG(LogProject_EXFIL, Log, TEXT("[Inventory Grid %dx%d]"), GridWidth, GridHeight);

	for (int32 Y = 0; Y < GridHeight; Y++)
	{
		FString Row;
		for (int32 X = 0; X < GridWidth; X++)
		{
			const int32 Index = Y * GridWidth + X;
			const FInventorySlot& Slot = GridSlots[Index];

			if (Slot.IsEmpty())
			{
				Row += TEXT(". ");
			}
			else if (Slot.bIsRootSlot)
			{
				Row += TEXT("# ");
			}
			else
			{
				Row += TEXT("X ");
			}
		}
		UE_LOG(LogProject_EXFIL, Log, TEXT("%s"), *Row);
	}
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
			if (Index >= 0 && Index < GridSlots.Num())
			{
				GridSlots[Index].Clear();
				SetBit(X, Y, false);
			}
		}
	}
}

// ========== Item Lookup ==========

FInventoryItemInstance* UInventoryComponent::FindItemByInstanceID(const FGuid& InstanceID)
{
	if (const int32* Index = ItemIndexMap.Find(InstanceID))
	{
		if (Items.IsValidIndex(*Index))
		{
			return &Items[*Index];
		}
	}
	return nullptr;
}

const FInventoryItemInstance* UInventoryComponent::FindItemByInstanceID(const FGuid& InstanceID) const
{
	if (const int32* Index = ItemIndexMap.Find(InstanceID))
	{
		if (Items.IsValidIndex(*Index))
		{
			return &Items[*Index];
		}
	}
	return nullptr;
}

int32 UInventoryComponent::FindItemIndexByInstanceID(const FGuid& InstanceID) const
{
	if (const int32* Index = ItemIndexMap.Find(InstanceID))
	{
		if (Items.IsValidIndex(*Index))
		{
			return *Index;
		}
	}
	return INDEX_NONE;
}
