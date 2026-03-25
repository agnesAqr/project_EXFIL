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
	// 서버에서만 그리드 초기화 (클라이언트는 리플리케이션으로 수신)
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		InitializeGrid();
	}
}

// ========== Replication ==========

void UInventoryComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 인벤토리는 소유자 클라이언트에게만 전파
	DOREPLIFETIME_CONDITION(UInventoryComponent, Items, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, GridSlots, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, GridHeight, COND_OwnerOnly);
}

void UInventoryComponent::OnRep_Items()
{
	OnInventoryUpdated.Broadcast();
}

void UInventoryComponent::OnRep_GridHeight()
{
	OnGridExpanded.Broadcast(GridHeight);
	OnInventoryUpdated.Broadcast();
}

// ========== Server RPCs ==========

bool UInventoryComponent::Server_TryAddItemByID_Validate(
	FName ItemDataID, int32 StackCount)
{
	return !ItemDataID.IsNone() && StackCount > 0;
}

void UInventoryComponent::Server_TryAddItemByID_Implementation(
	FName ItemDataID, int32 StackCount)
{
	TryAddItemByID(ItemDataID, StackCount);
}

bool UInventoryComponent::Server_RemoveItem_Validate(FGuid ItemInstanceID)
{
	return ItemInstanceID.IsValid();
}

void UInventoryComponent::Server_RemoveItem_Implementation(FGuid ItemInstanceID)
{
	RemoveItem(ItemInstanceID);
}

bool UInventoryComponent::Server_MoveItem_Validate(
	FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated)
{
	return ItemInstanceID.IsValid() && NewPosition.X >= 0 && NewPosition.Y >= 0;
}

void UInventoryComponent::Server_MoveItem_Implementation(
	FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated)
{
	MoveItem(ItemInstanceID, NewPosition, bNewRotated);
}

bool UInventoryComponent::Server_ConsumeItemByID_Validate(
	FName ItemDataID, int32 Count)
{
	return !ItemDataID.IsNone() && Count > 0;
}

void UInventoryComponent::Server_ConsumeItemByID_Implementation(
	FName ItemDataID, int32 Count)
{
	// 1. ConsumableEffect → ASC에 적용 (StatsBar 반영)
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = Owner->FindComponentByClass<UAbilitySystemComponent>())
		{
			if (UWorld* World = GetWorld())
			{
				if (UGameInstance* GI = World->GetGameInstance())
				{
					if (UItemDataSubsystem* Sub = GI->GetSubsystem<UItemDataSubsystem>())
					{
						const FItemData* ItemData = Sub->GetItemData(ItemDataID);
						if (ItemData && !ItemData->ConsumableEffect.IsNull())
						{
							TSubclassOf<UGameplayEffect> GEClass =
								ItemData->ConsumableEffect.LoadSynchronous();
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
	}

	// 2. 인벤토리에서 아이템 소비
	ConsumeItemByID(ItemDataID, Count);
}

bool UInventoryComponent::Server_DropItem_Validate(FGuid ItemInstanceID)
{
	return ItemInstanceID.IsValid();
}

void UInventoryComponent::Server_DropItem_Implementation(FGuid ItemInstanceID)
{
	// 1. 아이템 조회
	const FInventoryItemInstance* Item = FindItemByInstanceID(ItemInstanceID);
	if (!Item) return;

	// 드롭 정보 캐싱 (스택 조작 전에)
	const FName DropItemDataID = Item->ItemDataID;
	const int32 CurrentStack   = Item->StackCount;

	// 2. 스택이 2 이상이면 1만 감소, 1이면 슬롯 전체 제거
	if (CurrentStack > 1)
	{
		DecrementStack(ItemInstanceID);
	}
	else
	{
		RemoveItem(ItemInstanceID);
	}

	// 3. 캐릭터 전방에 AWorldItem 스폰 (항상 1개)
	AActor* Owner = GetOwner();
	if (!Owner) return;

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
		DroppedItem->InitializeItem(DropItemDataID, 1); // 항상 1개 드롭
	}
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

	Items.Empty();

	UE_LOG(LogProject_EXFIL, Log, TEXT("Inventory grid initialized: %dx%d (%d slots)"),
	       GridWidth, GridHeight, GridSlots.Num());
}

// ========== 그리드 동적 확장 ==========

bool UInventoryComponent::ExpandGridForItem(FItemSize Size)
{
	// 아이템 높이만큼 행 추가 (최소 확장)
	const int32 RowsNeeded = Size.Height;
	const int32 NewHeight = GridHeight + RowsNeeded;

	if (NewHeight > MaxGridHeight)
	{
		UE_LOG(LogProject_EXFIL, Warning,
		       TEXT("ExpandGridForItem: MaxGridHeight(%d) 초과 — 확장 불가"), MaxGridHeight);
		return false;
	}

	// 새 행의 슬롯을 GridSlots 끝에 추가
	const int32 OldSlotCount = GridSlots.Num();
	const int32 NewSlotCount = NewHeight * GridWidth;
	GridSlots.SetNum(NewSlotCount);

	for (int32 i = OldSlotCount; i < NewSlotCount; ++i)
	{
		GridSlots[i].Clear();
	}

	GridHeight = NewHeight;

	UE_LOG(LogProject_EXFIL, Log,
	       TEXT("ExpandGridForItem: Grid expanded to %dx%d (+%d rows)"),
	       GridWidth, GridHeight, RowsNeeded);

	OnGridExpanded.Broadcast(GridHeight);
	return true;
}

// ========== 핵심 API ==========

bool UInventoryComponent::TryAddItem(FName ItemDataID, FItemSize Size,
                                      int32 StackCount, int32 MaxStack)
{
	// 클라이언트 → Server RPC 포워딩
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		Server_TryAddItemByID(ItemDataID, StackCount);
		return true; // 낙관적 반환
	}

	FIntPoint FoundPosition;
	if (!FindFirstAvailableSlot(Size, FoundPosition))
	{
		// 빈 슬롯 없음 → 그리드 확장 시도
		if (!ExpandGridForItem(Size))
		{
			UE_LOG(LogProject_EXFIL, Warning, TEXT("TryAddItem: No available slot for item '%s' (Size: %dx%d)"),
			       *ItemDataID.ToString(), Size.Width, Size.Height);
			return false;
		}

		// 확장 후 재탐색
		if (!FindFirstAvailableSlot(Size, FoundPosition))
		{
			return false;
		}
	}

	return TryAddItemAt(ItemDataID, Size, FoundPosition, false, StackCount, MaxStack);
}

bool UInventoryComponent::TryAddItemByID(FName ItemDataID, int32 StackCount)
{
	// 클라이언트 → Server RPC 포워딩
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		Server_TryAddItemByID(ItemDataID, StackCount);
		return true; // 낙관적 반환
	}

	// 서브시스템에서 아이템 정의 조회
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UItemDataSubsystem* Sub = GI ? GI->GetSubsystem<UItemDataSubsystem>() : nullptr;

	if (!Sub)
	{
		UE_LOG(LogProject_EXFIL, Warning,
		       TEXT("TryAddItemByID: UItemDataSubsystem을 찾을 수 없습니다."));
		return false;
	}

	const FItemData* ItemData = Sub->GetItemData(ItemDataID);
	if (!ItemData)
	{
		UE_LOG(LogProject_EXFIL, Warning,
		       TEXT("TryAddItemByID: ItemDataID '%s'를 DataTable에서 찾을 수 없습니다."),
		       *ItemDataID.ToString());
		return false;
	}

	const int32 MaxStack = ItemData->MaxStackCount;
	int32 Remaining = StackCount;

	// ── 핫픽스 B: 기존 스택에 병합 시도 ──
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

			UE_LOG(LogProject_EXFIL, Log,
			       TEXT("TryAddItemByID: Merged %d into existing stack of '%s' (now %d/%d)"),
			       ToMerge, *ItemDataID.ToString(), Existing.StackCount, Existing.MaxStackCount);
		}

		if (Remaining <= 0)
		{
			// 전량 기존 스택에 병합 완료
			OnInventoryUpdated.Broadcast();
			return true;
		}
	}

	// ── 남은 수량을 새 슬롯에 배치 ──
	const FItemSize Size = ItemData->GetItemSize();
	return TryAddItem(ItemDataID, Size, Remaining, MaxStack);
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
	NewItem.StackCount = FMath::Clamp(StackCount, 1, MaxStack);
	NewItem.MaxStackCount = MaxStack;

	// 4. 슬롯 점유
	OccupySlots(Position, EffectiveSize, NewItem.InstanceID);

	// 5. 인스턴스 등록 (TArray)
	Items.Add(NewItem);

	UE_LOG(LogProject_EXFIL, Log,
	       TEXT("Item added: '%s' ID=%s at (%d,%d) Size:%dx%d Stack:%d/%d Rotated:%s"),
	       *ItemDataID.ToString(), *NewItem.InstanceID.ToString(),
	       Position.X, Position.Y, EffectiveSize.Width, EffectiveSize.Height,
	       NewItem.StackCount, NewItem.MaxStackCount,
	       bRotated ? TEXT("Yes") : TEXT("No"));

	// 6. 델리게이트 브로드캐스트
	OnItemAdded.Broadcast(NewItem);
	OnInventoryUpdated.Broadcast();

	return true;
}

bool UInventoryComponent::RemoveItem(const FGuid& InstanceID)
{
	// 클라이언트 → Server RPC 포워딩
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		Server_RemoveItem(InstanceID);
		return true;
	}

	const int32 Index = FindItemIndexByInstanceID(InstanceID);
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogProject_EXFIL, Warning, TEXT("RemoveItem: Item not found: %s"),
		       *InstanceID.ToString());
		return false;
	}

	// 슬롯 해제
	FreeSlots(Items[Index]);

	// 인스턴스 제거
	Items.RemoveAt(Index);

	UE_LOG(LogProject_EXFIL, Log, TEXT("Item removed: %s"), *InstanceID.ToString());

	// 델리게이트 브로드캐스트
	OnItemRemoved.Broadcast(InstanceID);
	OnInventoryUpdated.Broadcast();

	return true;
}

bool UInventoryComponent::MoveItem(const FGuid& InstanceID, FIntPoint NewPosition,
                                    bool bNewRotated)
{
	// 클라이언트 → Server RPC 포워딩
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		Server_MoveItem(InstanceID, NewPosition, bNewRotated);
		return true;
	}

	FInventoryItemInstance* FoundItem = FindItemByInstanceID(InstanceID);
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

	const FInventoryItemInstance* Found = FindItemByInstanceID(Slot.OccupyingItemID);
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

// ========== Day 5 추가 API ==========

bool UInventoryComponent::ConsumeItemByID(FName ItemDataID, int32 Count)
{
	// 클라이언트 → Server RPC 포워딩
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		Server_ConsumeItemByID(ItemDataID, Count);
		return true;
	}

	if (Count <= 0)
	{
		return false;
	}

	// 보유량 사전 확인
	if (GetItemCountByID(ItemDataID) < Count)
	{
		UE_LOG(LogProject_EXFIL, Warning,
		       TEXT("ConsumeItemByID: Not enough '%s' (need %d)"), *ItemDataID.ToString(), Count);
		return false;
	}

	int32 Remaining = Count;

	// Items 배열을 순회하며 해당 ID 아이템을 소비
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
			Remaining = 0;
		}
	}

	// 스택이 0이 된 인스턴스 제거 (역순으로 제거해야 인덱스 안전)
	for (int32 i = IndicesToRemove.Num() - 1; i >= 0; --i)
	{
		const int32 RemoveIdx = IndicesToRemove[i];
		FreeSlots(Items[RemoveIdx]);
		const FGuid RemovedID = Items[RemoveIdx].InstanceID;
		Items.RemoveAt(RemoveIdx);
		OnItemRemoved.Broadcast(RemovedID);
	}

	OnInventoryUpdated.Broadcast();

	UE_LOG(LogProject_EXFIL, Log,
	       TEXT("ConsumeItemByID: '%s' x%d consumed"), *ItemDataID.ToString(), Count);
	return true;
}

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

// ========== 스택 조작 ==========

int32 UInventoryComponent::DecrementStack(const FGuid& InstanceID)
{
	FInventoryItemInstance* Item = FindItemByInstanceID(InstanceID);
	if (!Item)
	{
		return 0;
	}

	Item->StackCount--;

	if (Item->StackCount <= 0)
	{
		RemoveItem(InstanceID);
		return 0;
	}

	// 스택만 감소 — 그리드 점유 유지, 리플리케이션 트리거
	OnInventoryUpdated.Broadcast();

	UE_LOG(LogProject_EXFIL, Log, TEXT("DecrementStack: '%s' now %d"),
	       *Item->ItemDataID.ToString(), Item->StackCount);
	return Item->StackCount;
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

// ========== TMap→TArray 전환 헬퍼 ==========

FInventoryItemInstance* UInventoryComponent::FindItemByInstanceID(const FGuid& InstanceID)
{
	return Items.FindByPredicate(
		[&InstanceID](const FInventoryItemInstance& Item)
		{
			return Item.InstanceID == InstanceID;
		});
}

const FInventoryItemInstance* UInventoryComponent::FindItemByInstanceID(const FGuid& InstanceID) const
{
	return Items.FindByPredicate(
		[&InstanceID](const FInventoryItemInstance& Item)
		{
			return Item.InstanceID == InstanceID;
		});
}

int32 UInventoryComponent::FindItemIndexByInstanceID(const FGuid& InstanceID) const
{
	return Items.IndexOfByPredicate(
		[&InstanceID](const FInventoryItemInstance& Item)
		{
			return Item.InstanceID == InstanceID;
		});
}
