// Copyright Project EXFIL. All Rights Reserved.

#include "Data/Equipment/EquipmentComponent.h"
#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "Engine/GameInstance.h"
#include "Data/EXFILItemTypes.h"
#include "Data/ItemDataSubsystem.h"
#include "Inventory/InventoryComponent.h"
#include "World/WorldItem.h"
#include "Core/EXFILLog.h"

UEquipmentComponent::UEquipmentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UEquipmentComponent::BeginPlay()
{
    Super::BeginPlay();

    // 서버에서만 슬롯 초기화
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        InitializeSlots();
    }
}

// ========== Replication ==========

void UEquipmentComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(UEquipmentComponent, ReplicatedSlots, COND_OwnerOnly);
}

void UEquipmentComponent::OnRep_Slots()
{
    // 모든 슬롯에 대해 장착/해제 델리게이트 브로드캐스트
    for (const FEquipmentSlotData& SlotData : ReplicatedSlots)
    {
        if (!SlotData.IsEmpty())
        {
            OnItemEquipped.Broadcast(SlotData.SlotType, SlotData.ItemInstance);
        }
        else
        {
            // 빈 슬롯 → 해제 알림 (위젯 비주얼 Clear용)
            OnItemUnequipped.Broadcast(SlotData.SlotType, FInventoryItemInstance());
        }
    }
}

// ========== Server RPCs ==========

bool UEquipmentComponent::Server_EquipItem_Validate(
    EEquipmentSlot Slot, FInventoryItemInstance ItemInstance)
{
    return Slot != EEquipmentSlot::None && ItemInstance.IsValid();
}

void UEquipmentComponent::Server_EquipItem_Implementation(
    EEquipmentSlot Slot, FInventoryItemInstance ItemInstance)
{
    EquipItem(Slot, ItemInstance);
}

bool UEquipmentComponent::Server_UnequipItem_Validate(EEquipmentSlot Slot)
{
    return Slot != EEquipmentSlot::None;
}

void UEquipmentComponent::Server_UnequipItem_Implementation(EEquipmentSlot Slot)
{
    UnequipItem(Slot);
}

// ========== 초기화 ==========

void UEquipmentComponent::InitializeSlots()
{
    ReplicatedSlots.Empty();
    ReplicatedSlots.Add(FEquipmentSlotData(EEquipmentSlot::Head));
    ReplicatedSlots.Add(FEquipmentSlotData(EEquipmentSlot::Face));
    ReplicatedSlots.Add(FEquipmentSlotData(EEquipmentSlot::Eyewear));
    ReplicatedSlots.Add(FEquipmentSlotData(EEquipmentSlot::Body));
    ReplicatedSlots.Add(FEquipmentSlotData(EEquipmentSlot::Weapon1));
    ReplicatedSlots.Add(FEquipmentSlotData(EEquipmentSlot::Weapon2));
}

// ========== 핵심 API ==========

bool UEquipmentComponent::EquipItem(EEquipmentSlot Slot, const FInventoryItemInstance& ItemInstance)
{
    if (Slot == EEquipmentSlot::None)
    {
        return false;
    }

    // 클라이언트 → Server RPC 포워딩
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        Server_EquipItem(Slot, ItemInstance);
        return true;
    }

    FEquipmentSlotData* SlotData = FindSlotData(Slot);
    if (!SlotData)
    {
        return false;
    }

    // 이미 점유된 슬롯은 먼저 해제
    if (!SlotData->IsEmpty())
    {
        UnequipItem(Slot);
    }

    SlotData->EquippedItemID = ItemInstance.InstanceID;
    SlotData->ItemInstance = ItemInstance;
    ApplyEquipmentEffect(*SlotData, ItemInstance);

    OnItemEquipped.Broadcast(Slot, ItemInstance);
    return true;
}

bool UEquipmentComponent::UnequipItem(EEquipmentSlot Slot)
{
    // 클라이언트 → Server RPC 포워딩
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        Server_UnequipItem(Slot);
        return true;
    }

    FEquipmentSlotData* SlotData = FindSlotData(Slot);
    if (!SlotData || SlotData->IsEmpty())
    {
        return false;
    }

    RemoveEquipmentEffect(*SlotData);

    const FInventoryItemInstance Item = SlotData->ItemInstance;
    SlotData->EquippedItemID.Invalidate();
    SlotData->ItemInstance = FInventoryItemInstance();

    OnItemUnequipped.Broadcast(Slot, Item);
    return true;
}

bool UEquipmentComponent::GetEquippedItem(EEquipmentSlot Slot, FInventoryItemInstance& OutItem) const
{
    const FEquipmentSlotData* SlotData = FindSlotData(Slot);
    if (SlotData && !SlotData->IsEmpty())
    {
        OutItem = SlotData->ItemInstance;
        return true;
    }
    return false;
}

bool UEquipmentComponent::IsSlotOccupied(EEquipmentSlot Slot) const
{
    const FEquipmentSlotData* SlotData = FindSlotData(Slot);
    return SlotData && !SlotData->IsEmpty();
}

bool UEquipmentComponent::HasWeaponEquipped() const
{
    for (const FEquipmentSlotData& SlotData : ReplicatedSlots)
    {
        if ((SlotData.SlotType == EEquipmentSlot::Weapon1 ||
             SlotData.SlotType == EEquipmentSlot::Weapon2) &&
            !SlotData.IsEmpty())
        {
            return true;
        }
    }
    return false;
}

// ========== 내부 헬퍼 ==========

FEquipmentSlotData* UEquipmentComponent::FindSlotData(EEquipmentSlot SlotType)
{
    return ReplicatedSlots.FindByPredicate(
        [SlotType](const FEquipmentSlotData& S) { return S.SlotType == SlotType; });
}

const FEquipmentSlotData* UEquipmentComponent::FindSlotData(EEquipmentSlot SlotType) const
{
    return ReplicatedSlots.FindByPredicate(
        [SlotType](const FEquipmentSlotData& S) { return S.SlotType == SlotType; });
}

void UEquipmentComponent::ApplyEquipmentEffect(FEquipmentSlotData& SlotData,
                                                const FInventoryItemInstance& Item)
{
    UAbilitySystemComponent* ASC = GetASC();
    if (!ASC)
    {
        return;
    }

    // ItemDataSubsystem에서 EquipmentEffect 조회
    UItemDataSubsystem* ItemSub = nullptr;
    if (AActor* Owner = GetOwner())
    {
        if (UWorld* World = Owner->GetWorld())
        {
            if (UGameInstance* GI = World->GetGameInstance())
            {
                ItemSub = GI->GetSubsystem<UItemDataSubsystem>();
            }
        }
    }

    if (!ItemSub)
    {
        return;
    }

    const FItemData* ItemData = ItemSub->GetItemData(Item.ItemDataID);
    if (!ItemData || ItemData->EquipmentEffect.IsNull())
    {
        return;
    }

    TSubclassOf<UGameplayEffect> GEClass = ItemData->EquipmentEffect.LoadSynchronous();
    if (!GEClass)
    {
        return;
    }

    FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
    FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(GEClass, 1.f, ContextHandle);
    if (SpecHandle.IsValid())
    {
        const FActiveGameplayEffectHandle ActiveHandle =
            ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        SlotData.ActiveGEHandle = ActiveHandle;
    }
}

void UEquipmentComponent::RemoveEquipmentEffect(FEquipmentSlotData& SlotData)
{
    UAbilitySystemComponent* ASC = GetASC();
    if (!ASC)
    {
        return;
    }

    if (SlotData.ActiveGEHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(SlotData.ActiveGEHandle);
        SlotData.ActiveGEHandle = FActiveGameplayEffectHandle();
    }
}

UAbilitySystemComponent* UEquipmentComponent::GetASC() const
{
    if (AActor* Owner = GetOwner())
    {
        if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner))
        {
            return ASI->GetAbilitySystemComponent();
        }
    }
    return nullptr;
}

// ========== 복합 Server RPCs (핫픽스 A) ==========

bool UEquipmentComponent::Server_EquipFromInventory_Validate(
    EEquipmentSlot Slot, FGuid ItemInstanceID)
{
    // Slot == None 허용 — 서버 측 FindTargetSlot이 슬롯을 결정
    return ItemInstanceID.IsValid();
}

void UEquipmentComponent::Server_EquipFromInventory_Implementation(
    EEquipmentSlot Slot, FGuid ItemInstanceID)
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    UInventoryComponent* InvComp = Owner->FindComponentByClass<UInventoryComponent>();
    if (!InvComp)
    {
        return;
    }

    // 인벤토리에서 아이템 조회
    FInventoryItemInstance ItemInstance;
    if (!InvComp->GetItemByID(ItemInstanceID, ItemInstance))
    {
        UE_LOG(LogEXFIL, Warning, TEXT("Server_EquipFromInventory: Item %s not found in inventory"),
            *ItemInstanceID.ToString());
        return;
    }

    // DataTable에서 EquipmentSlotTag 조회 → FindTargetSlot으로 실제 슬롯 결정
    UItemDataSubsystem* Sub = nullptr;
    if (UWorld* World = Owner->GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            Sub = GI->GetSubsystem<UItemDataSubsystem>();
        }
    }

    EEquipmentSlot TargetSlot = EEquipmentSlot::None;
    if (Sub)
    {
        const FItemData* ItemData = Sub->GetItemData(ItemInstance.ItemDataID);
        if (!ItemData || ItemData->ItemType != EItemType::Equipment)
        {
            UE_LOG(LogEXFIL, Warning,
                TEXT("Server_EquipFromInventory: '%s' 는 Equipment 타입이 아님"),
                *ItemInstance.ItemDataID.ToString());
            return;
        }
        TargetSlot = FindTargetSlot(ItemData->EquipmentSlotTag);
    }

    if (TargetSlot == EEquipmentSlot::None)
    {
        UE_LOG(LogEXFIL, Warning,
            TEXT("Server_EquipFromInventory: FindTargetSlot 결과 None — EquipmentSlotTag 확인 필요"));
        return;
    }

    // ── Day 7: 스왑 로직 ──
    // 슬롯이 이미 점유된 경우 기존 장비를 인벤토리에 먼저 복귀 (실패 시 거부)
    FEquipmentSlotData* SlotData = FindSlotData(TargetSlot);
    if (SlotData && !SlotData->IsEmpty())
    {
        const FName OldItemDataID = SlotData->ItemInstance.ItemDataID;
        const bool bCanReturn = InvComp->TryAddItemByID(OldItemDataID, 1);
        if (!bCanReturn)
        {
            UE_LOG(LogEXFIL, Warning,
                TEXT("Server_EquipFromInventory: 스왑 거부 — 인벤토리 가득 참 (기존 장비: '%s')"),
                *OldItemDataID.ToString());
            return;
        }
        // 기존 GE 제거 + 슬롯 비우기
        RemoveEquipmentEffect(*SlotData);
        SlotData->EquippedItemID.Invalidate();
        SlotData->ItemInstance = FInventoryItemInstance();
    }

    // 원자적 실행: 인벤토리에서 1개 차감 → 장비 장착
    // StackCount > 1이면 1개만 차감, ==1이면 완전 제거
    InvComp->DecrementStack(ItemInstanceID);

    // 장비슬롯에는 항상 StackCount=1짜리로 장착
    FInventoryItemInstance EquipInstance = ItemInstance;
    EquipInstance.StackCount = 1;
    EquipInstance.InstanceID = FGuid::NewGuid(); // 장비용 새 인스턴스 ID
    EquipItem(TargetSlot, EquipInstance);
}

bool UEquipmentComponent::Server_UnequipToInventory_Validate(EEquipmentSlot Slot)
{
    return Slot != EEquipmentSlot::None;
}

void UEquipmentComponent::Server_UnequipToInventory_Implementation(EEquipmentSlot Slot)
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // 현재 장착 아이템 조회
    FInventoryItemInstance EquippedItem;
    if (!GetEquippedItem(Slot, EquippedItem))
    {
        return;
    }

    UInventoryComponent* InvComp = Owner->FindComponentByClass<UInventoryComponent>();
    if (!InvComp)
    {
        return;
    }

    // 원자적 실행: 장비 해제 → 인벤토리 추가
    UnequipItem(Slot);
    InvComp->TryAddItemByID(EquippedItem.ItemDataID, EquippedItem.StackCount);
}

// ========== Server_DropEquippedItem ==========

bool UEquipmentComponent::Server_DropEquippedItem_Validate(EEquipmentSlot InSlot)
{
    return InSlot != EEquipmentSlot::None;
}

void UEquipmentComponent::Server_DropEquippedItem_Implementation(EEquipmentSlot InSlot)
{
    FEquipmentSlotData* SlotData = FindSlotData(InSlot);
    if (!SlotData || SlotData->IsEmpty()) return;

    // 장착 아이템 정보 캐싱 (슬롯 비우기 전에)
    const FName DropItemDataID = SlotData->ItemInstance.ItemDataID;

    // GE 제거
    RemoveEquipmentEffect(*SlotData);

    // 슬롯 비우기
    SlotData->EquippedItemID.Invalidate();
    SlotData->ItemInstance = FInventoryItemInstance();

    // OnItemUnequipped 브로드캐스트
    OnItemUnequipped.Broadcast(InSlot, FInventoryItemInstance());

    // 월드에 AWorldItem 스폰
    AActor* Owner = GetOwner();
    if (!Owner) return;

    const FVector SpawnLocation =
        Owner->GetActorLocation()
        + Owner->GetActorForwardVector() * 100.f
        + FVector(0.f, 0.f, 50.f);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = Owner;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AWorldItem* DroppedItem = GetWorld()->SpawnActor<AWorldItem>(
        AWorldItem::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParams);

    if (DroppedItem)
    {
        DroppedItem->InitializeItem(DropItemDataID, 1); // 장비는 항상 1개
    }
}

// ========== FindTargetSlot (빈 슬롯 자동 탐색) ==========

EEquipmentSlot UEquipmentComponent::FindTargetSlot(const FName& EquipmentSlotTag) const
{
    // 태그 → 후보 슬롯 목록 매핑
    TArray<EEquipmentSlot> Candidates;

    if (EquipmentSlotTag == FName("Weapon"))
    {
        Candidates = { EEquipmentSlot::Weapon1, EEquipmentSlot::Weapon2 };
    }
    else if (EquipmentSlotTag == FName("Head"))
    {
        Candidates = { EEquipmentSlot::Head };
    }
    else if (EquipmentSlotTag == FName("Face"))
    {
        Candidates = { EEquipmentSlot::Face };
    }
    else if (EquipmentSlotTag == FName("Eyewear"))
    {
        Candidates = { EEquipmentSlot::Eyewear };
    }
    else if (EquipmentSlotTag == FName("Body"))
    {
        Candidates = { EEquipmentSlot::Body };
    }
    else
    {
        UE_LOG(LogEXFIL, Warning, TEXT("FindTargetSlot: 알 수 없는 태그 '%s'"), *EquipmentSlotTag.ToString());
        return EEquipmentSlot::None;
    }

    // 빈 슬롯 우선 탐색
    for (EEquipmentSlot CandidateSlot : Candidates)
    {
        const FEquipmentSlotData* SlotData = FindSlotData(CandidateSlot);
        if (SlotData && SlotData->IsEmpty())
        {
            return CandidateSlot;
        }
    }

    // 빈 슬롯 없으면 첫 번째 후보에 스왑
    return Candidates[0];
}

// ========== SlotTag → Enum 매핑 (deprecated — FindTargetSlot 사용 권장) ==========

EEquipmentSlot UEquipmentComponent::SlotTagToEnum(FName SlotTag)
{
    if (SlotTag.IsNone())
    {
        return EEquipmentSlot::None;
    }

    static const TMap<FName, EEquipmentSlot> TagMap =
    {
        { FName("Head"),    EEquipmentSlot::Head    },
        { FName("Face"),    EEquipmentSlot::Face    },
        { FName("Eyewear"), EEquipmentSlot::Eyewear },
        { FName("Body"),    EEquipmentSlot::Body    },
        { FName("Weapon1"), EEquipmentSlot::Weapon1 },
        { FName("Weapon2"), EEquipmentSlot::Weapon2 },
    };

    const EEquipmentSlot* Found = TagMap.Find(SlotTag);
    return Found ? *Found : EEquipmentSlot::None;
}
