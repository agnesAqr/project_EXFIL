// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventoryViewModel.h"
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Inventory/InventoryComponent.h"
#include "Data/ItemDataSubsystem.h"

void UInventoryViewModel::Initialize(UInventoryComponent* InInventoryComponent)
{
    if (!InInventoryComponent)
    {
        return;
    }

    InventoryComp = InInventoryComponent;

    // 그리드 사이즈 캐시
    UE_MVVM_SET_PROPERTY_VALUE(GridWidth, InInventoryComponent->GridWidth);
    UE_MVVM_SET_PROPERTY_VALUE(GridHeight, InInventoryComponent->GridHeight);

    // SlotViewModel 배열 생성
    const int32 TotalSlots = GridWidth * GridHeight;
    SlotViewModels.SetNum(TotalSlots);

    for (int32 i = 0; i < TotalSlots; ++i)
    {
        UInventorySlotViewModel* SlotVM = NewObject<UInventorySlotViewModel>(this);
        SlotVM->SetGridPosition(FIntPoint(i % GridWidth, i / GridWidth));
        SlotViewModels[i] = SlotVM;
    }

    // Model 델리게이트 바인딩
    InInventoryComponent->OnInventoryUpdated.AddDynamic(
        this, &UInventoryViewModel::OnInventoryUpdated);
    InInventoryComponent->OnItemAdded.AddDynamic(
        this, &UInventoryViewModel::OnItemAdded);
    InInventoryComponent->OnItemRemoved.AddDynamic(
        this, &UInventoryViewModel::OnItemRemoved);

    // 초기 상태 동기화
    RefreshAllSlots();
}

UInventorySlotViewModel* UInventoryViewModel::GetSlotAt(FIntPoint Position) const
{
    const int32 Index = PositionToIndex(Position);
    if (SlotViewModels.IsValidIndex(Index))
    {
        return SlotViewModels[Index];
    }
    return nullptr;
}

TArray<UInventorySlotViewModel*> UInventoryViewModel::GetAllSlots() const
{
    TArray<UInventorySlotViewModel*> Result;
    Result.Reserve(SlotViewModels.Num());
    for (const TObjectPtr<UInventorySlotViewModel>& VM : SlotViewModels)
    {
        Result.Add(VM.Get());
    }
    return Result;
}

bool UInventoryViewModel::RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition,
                                           bool bNewRotated)
{
    if (!InventoryComp.IsValid())
    {
        return false;
    }
    return InventoryComp->MoveItem(ItemInstanceID, NewPosition, bNewRotated);
}

FIntPoint UInventoryViewModel::GetItemRootPosition(FGuid ItemInstanceID) const
{
    if (!InventoryComp.IsValid())
    {
        return FIntPoint(-1, -1);
    }
    FInventoryItemInstance Item;
    if (InventoryComp->GetItemByID(ItemInstanceID, Item))
    {
        return Item.RootPosition;
    }
    return FIntPoint(-1, -1);
}

FItemSize UInventoryViewModel::GetItemEffectiveSize(FGuid ItemInstanceID) const
{
    if (!InventoryComp.IsValid())
    {
        return FItemSize(1, 1);
    }
    FInventoryItemInstance Item;
    if (InventoryComp->GetItemByID(ItemInstanceID, Item))
    {
        return Item.bIsRotated ? Item.ItemSize.GetRotated() : Item.ItemSize;
    }
    return FItemSize(1, 1);
}

bool UInventoryViewModel::RequestRemoveItem(FGuid ItemInstanceID)
{
    if (!InventoryComp.IsValid())
    {
        return false;
    }
    return InventoryComp->RemoveItem(ItemInstanceID);
}

void UInventoryViewModel::OnInventoryUpdated()
{
    RefreshAllSlots();
}

void UInventoryViewModel::OnItemAdded(const FInventoryItemInstance& AddedItem)
{
    // OnInventoryUpdated도 함께 브로드캐스트되므로 RefreshAllSlots는 거기서 처리
}

void UInventoryViewModel::OnItemRemoved(const FGuid& RemovedItemID)
{
    // OnInventoryUpdated도 함께 브로드캐스트되므로 RefreshAllSlots는 거기서 처리
}

void UInventoryViewModel::RefreshAllSlots()
{
    if (!InventoryComp.IsValid())
    {
        return;
    }

    // ItemDataSubsystem — 아이콘 조회용 (없어도 크래시하지 않음)
    UItemDataSubsystem* ItemSub = nullptr;
    if (UWorld* World = InventoryComp->GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            ItemSub = GI->GetSubsystem<UItemDataSubsystem>();
        }
    }

    for (int32 i = 0; i < SlotViewModels.Num(); ++i)
    {
        UInventorySlotViewModel* SlotVM = SlotViewModels[i].Get();
        if (!SlotVM)
        {
            continue;
        }

        const FIntPoint Position(i % GridWidth, i / GridWidth);
        FInventoryItemInstance ItemInstance;
        const bool bHasItem = InventoryComp->GetItemAt(Position, ItemInstance);

        if (bHasItem)
        {
            // 슬롯이 점유된 경우: bIsRootSlot은 루트 위치 슬롯만 true
            const bool bIsRoot = (ItemInstance.RootPosition == Position);
            SlotVM->SetEmpty(false);
            SlotVM->SetItemDataID(ItemInstance.ItemDataID);
            SlotVM->SetStackCount(ItemInstance.StackCount);
            SlotVM->SetIsRootSlot(bIsRoot);
            SlotVM->SetItemInstanceID(ItemInstance.InstanceID);

            // 아이콘 — DataTable에서 조회 (없으면 null Soft Ref 유지)
            if (ItemSub)
            {
                const FItemData* ItemData = ItemSub->GetItemData(ItemInstance.ItemDataID);
                SlotVM->SetIcon(ItemData ? ItemData->Icon : TSoftObjectPtr<UTexture2D>());
            }
        }
        else
        {
            SlotVM->SetEmpty(true);
            SlotVM->SetItemDataID(NAME_None);
            SlotVM->SetStackCount(0);
            SlotVM->SetIsRootSlot(false);
            SlotVM->SetItemInstanceID(FGuid());
            SlotVM->SetIcon(TSoftObjectPtr<UTexture2D>());
        }
    }
}

int32 UInventoryViewModel::PositionToIndex(FIntPoint Position) const
{
    return Position.Y * GridWidth + Position.X;
}
