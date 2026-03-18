// Copyright Project EXFIL. All Rights Reserved.

#include "InventoryComponent.h"
#include "Project_EXFIL.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeGrid();
}

// ========== 초기화 ==========

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

	ItemInstances.Empty();

	UE_LOG(LogProject_EXFIL, Log, TEXT("Inventory grid initialized: %dx%d (%d slots)"),
	       GridWidth, GridHeight, GridSlots.Num());
}

// ========== 핵심 API ==========

bool UInventoryComponent::TryAddItem(FName ItemDataID, FItemSize Size,
                                      int32 StackCount, int32 MaxStack)
{
	FIntPoint FoundPosition;
	if (!FindFirstAvailableSlot(Size, FoundPosition))
	{
		UE_LOG(LogProject_EXFIL, Warning, TEXT("TryAddItem: No available slot for item '%s' (Size: %dx%d)"),
		       *ItemDataID.ToString(), Size.Width, Size.Height);
		return false;
	}

	return TryAddItemAt(ItemDataID, Size, FoundPosition, false, StackCount, MaxStack);
}

bool UInventoryComponent::TryAddItemAt(FName ItemDataID, FItemSize Size,
                                        FIntPoint Position, bool bRotated,
                                        int32 StackCount, int32 MaxStack)
{
	// 1. 회전 상태 반영
	const FItemSize EffectiveSize = bRotated ? Size.GetRotated() : Size;

	// 2. 공간 검증
	if (!AreSlotsFree(Position, EffectiveSize))
	{
		UE_LOG(LogProject_EXFIL, Warning,
		       TEXT("TryAddItemAt: Cannot place item '%s' at (%d,%d) Size:%dx%d"),
		       *ItemDataID.ToString(), Position.X, Position.Y,
		       EffectiveSize.Width, EffectiveSize.Height);
		return false;
	}

	// 3. 인스턴스 생성
	FInventoryItemInstance NewItem;
	NewItem.InstanceID = FGuid::NewGuid();
	NewItem.ItemDataID = ItemDataID;
	NewItem.RootPosition = Position;
	NewItem.ItemSize = Size;
	NewItem.bIsRotated = bRotated;
	NewItem.StackCount = StackCount;
	NewItem.MaxStackCount = MaxStack;

	// 4. 슬롯 점유
	OccupySlots(Position, EffectiveSize, NewItem.InstanceID);

	// 5. 인스턴스 등록
	ItemInstances.Add(NewItem.InstanceID, NewItem);

	UE_LOG(LogProject_EXFIL, Log,
	       TEXT("Item added: '%s' ID=%s at (%d,%d) Size:%dx%d Rotated:%s"),
	       *ItemDataID.ToString(), *NewItem.InstanceID.ToString(),
	       Position.X, Position.Y, EffectiveSize.Width, EffectiveSize.Height,
	       bRotated ? TEXT("Yes") : TEXT("No"));

	// 6. 델리게이트 브로드캐스트
	OnItemAdded.Broadcast(NewItem);
	OnInventoryUpdated.Broadcast();

	return true;
}

bool UInventoryComponent::RemoveItem(const FGuid& InstanceID)
{
	FInventoryItemInstance* FoundItem = ItemInstances.Find(InstanceID);
	if (!FoundItem)
	{
		UE_LOG(LogProject_EXFIL, Warning, TEXT("RemoveItem: Item not found: %s"),
		       *InstanceID.ToString());
		return false;
	}

	// 슬롯 해제
	FreeSlots(*FoundItem);

	// 인스턴스 제거
	FGuid RemovedID = InstanceID;
	ItemInstances.Remove(InstanceID);

	UE_LOG(LogProject_EXFIL, Log, TEXT("Item removed: %s"), *RemovedID.ToString());

	// 델리게이트 브로드캐스트
	OnItemRemoved.Broadcast(RemovedID);
	OnInventoryUpdated.Broadcast();

	return true;
}

bool UInventoryComponent::MoveItem(const FGuid& InstanceID, FIntPoint NewPosition,
                                    bool bNewRotated)
{
	FInventoryItemInstance* FoundItem = ItemInstances.Find(InstanceID);
	if (!FoundItem)
	{
		UE_LOG(LogProject_EXFIL, Warning, TEXT("MoveItem: Item not found: %s"),
		       *InstanceID.ToString());
		return false;
	}

	// 기존 정보 백업 (롤백용)
	const FIntPoint OldPosition = FoundItem->RootPosition;
	const bool bOldRotated = FoundItem->bIsRotated;
	const FItemSize OldEffectiveSize = FoundItem->GetEffectiveSize();

	// 기존 위치 해제
	FreeSlots(*FoundItem);

	// 새 사이즈 계산
	const FItemSize NewEffectiveSize = bNewRotated
		? FoundItem->ItemSize.GetRotated()
		: FoundItem->ItemSize;

	// 새 위치 검증
	if (!AreSlotsFree(NewPosition, NewEffectiveSize))
	{
		// 롤백: 기존 위치 재점유
		OccupySlots(OldPosition, OldEffectiveSize, InstanceID);

		UE_LOG(LogProject_EXFIL, Warning,
		       TEXT("MoveItem: Cannot move to (%d,%d). Rolling back to (%d,%d)"),
		       NewPosition.X, NewPosition.Y, OldPosition.X, OldPosition.Y);
		return false;
	}

	// 새 위치 점유
	OccupySlots(NewPosition, NewEffectiveSize, InstanceID);

	// 아이템 정보 갱신
	FoundItem->RootPosition = NewPosition;
	FoundItem->bIsRotated = bNewRotated;

	UE_LOG(LogProject_EXFIL, Log,
	       TEXT("Item moved: %s from (%d,%d) to (%d,%d)"),
	       *InstanceID.ToString(),
	       OldPosition.X, OldPosition.Y,
	       NewPosition.X, NewPosition.Y);

	// 델리게이트 브로드캐스트
	OnInventoryUpdated.Broadcast();

	return true;
}

// ========== 쿼리 API ==========

bool UInventoryComponent::CanPlaceItemAt(FIntPoint Position, FItemSize Size) const
{
	return AreSlotsFree(Position, Size);
}

bool UInventoryComponent::FindFirstAvailableSlot(FItemSize Size,
                                                  FIntPoint& OutPosition) const
{
	for (int32 Y = 0; Y <= GridHeight - Size.Height; Y++)
	{
		for (int32 X = 0; X <= GridWidth - Size.Width; X++)
		{
			if (AreSlotsFree(FIntPoint(X, Y), Size))
			{
				OutPosition = FIntPoint(X, Y);
				return true;
			}
		}
	}

	return false;
}

bool UInventoryComponent::GetItemAt(FIntPoint Position,
                                     FInventoryItemInstance& OutItem) const
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

	const FInventoryItemInstance* Found = ItemInstances.Find(Slot.OccupyingItemID);
	if (Found)
	{
		OutItem = *Found;
		return true;
	}

	return false;
}

bool UInventoryComponent::GetItemByID(const FGuid& InstanceID,
                                       FInventoryItemInstance& OutItem) const
{
	const FInventoryItemInstance* Found = ItemInstances.Find(InstanceID);
	if (Found)
	{
		OutItem = *Found;
		return true;
	}

	return false;
}

TArray<FInventoryItemInstance> UInventoryComponent::GetAllItems() const
{
	TArray<FInventoryItemInstance> Result;
	ItemInstances.GenerateValueArray(Result);
	return Result;
}

bool UInventoryComponent::IsEmpty() const
{
	return ItemInstances.Num() == 0;
}

int32 UInventoryComponent::GetItemCount(FName ItemDataID) const
{
	int32 Count = 0;
	for (const auto& Pair : ItemInstances)
	{
		if (Pair.Value.ItemDataID == ItemDataID)
		{
			Count += Pair.Value.StackCount;
		}
	}
	return Count;
}

// ========== 유틸리티 ==========

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

// ========== 내부 헬퍼 ==========

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
	// 경계 체크를 루프 밖에서 먼저 수행
	if (Position.X < 0 || Position.Y < 0)
	{
		return false;
	}

	if (Position.X + Size.Width > GridWidth || Position.Y + Size.Height > GridHeight)
	{
		return false;
	}

	// 영역 내 모든 슬롯 검증
	for (int32 Y = Position.Y; Y < Position.Y + Size.Height; Y++)
	{
		for (int32 X = Position.X; X < Position.X + Size.Width; X++)
		{
			const int32 Index = Y * GridWidth + X;
			if (!GridSlots[Index].IsEmpty())
			{
				return false;
			}
		}
	}

	return true;
}

void UInventoryComponent::OccupySlots(FIntPoint Position, FItemSize Size,
                                       const FGuid& ItemID)
{
	for (int32 Y = Position.Y; Y < Position.Y + Size.Height; Y++)
	{
		for (int32 X = Position.X; X < Position.X + Size.Width; X++)
		{
			const int32 Index = Y * GridWidth + X;
			const bool bIsRoot = (X == Position.X && Y == Position.Y);
			GridSlots[Index].Occupy(ItemID, bIsRoot);
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
			}
		}
	}
}
