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
    UE_LOG(LogTemp, Log, TEXT("EquipmentComponent: Slot[%d] — '%s' 장착"),
        static_cast<int32>(Slot), *ItemInstance.ItemDataID.ToString());
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
    UE_LOG(LogTemp, Log, TEXT("EquipmentComponent: Slot[%d] — '%s' 해제"),
        static_cast<int32>(Slot), *Item.ItemDataID.ToString());
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
        UE_LOG(LogTemp, Log, TEXT("EquipmentComponent: '%s' EquipmentEffect 적용"),
            *Item.ItemDataID.ToString());
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
    return Slot != EEquipmentSlot::None && ItemInstanceID.IsValid();
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
        UE_LOG(LogTemp, Warning, TEXT("Server_EquipFromInventory: Item %s not found in inventory"),
            *ItemInstanceID.ToString());
        return;
    }

    // DataTable에서 슬롯 타입 검증
    UItemDataSubsystem* Sub = nullptr;
    if (UWorld* World = Owner->GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            Sub = GI->GetSubsystem<UItemDataSubsystem>();
        }
    }

    if (Sub)
    {
        const FItemData* ItemData = Sub->GetItemData(ItemInstance.ItemDataID);
        if (ItemData)
        {
            const EEquipmentSlot RequiredSlot = SlotTagToEnum(ItemData->EquipmentSlotTag);
            if (RequiredSlot != EEquipmentSlot::None && RequiredSlot != Slot)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("Server_EquipFromInventory: Slot mismatch — item wants %d, target is %d"),
                    static_cast<int32>(RequiredSlot), static_cast<int32>(Slot));
                return;
            }
        }
    }

    // 원자적 실행: 인벤토리에서 1개 차감 → 장비 장착
    // StackCount > 1이면 1개만 차감, ==1이면 완전 제거
    InvComp->DecrementStack(ItemInstanceID);

    // 장비슬롯에는 항상 StackCount=1짜리로 장착
    FInventoryItemInstance EquipInstance = ItemInstance;
    EquipInstance.StackCount = 1;
    EquipInstance.InstanceID = FGuid::NewGuid(); // 장비용 새 인스턴스 ID
    EquipItem(Slot, EquipInstance);
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

// ========== SlotTag → Enum 매핑 ==========

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
