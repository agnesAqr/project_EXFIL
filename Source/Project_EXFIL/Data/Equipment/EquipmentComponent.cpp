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

    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            CachedItemSub = GI->GetSubsystem<UItemDataSubsystem>();
        }
    }

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
    RebuildSlotIndexMap();
    int32 ChangedSlotCount = 0;

    for (const FEquipmentSlotData& SlotData : ReplicatedSlots)
    {
        const FEquipmentSlotData* PrevSlot = PrevReplicatedSlots.FindByPredicate(
            [&](const FEquipmentSlotData& Candidate)
            {
                return Candidate.SlotType == SlotData.SlotType;
            });

        const FGuid PrevID = PrevSlot ? PrevSlot->ItemInstance.InstanceID : FGuid();
        const FGuid CurrID = SlotData.ItemInstance.InstanceID;
        if (PrevID == CurrID)
        {
            continue;
        }

        ++ChangedSlotCount;
        UE_LOG(LogEXFIL, Log,
            TEXT("[EquipmentRep][Client] SlotChanged Slot=%d Prev=%s Curr=%s Empty=%s"),
            static_cast<int32>(SlotData.SlotType),
            *PrevID.ToString(),
            *CurrID.ToString(),
            SlotData.IsEmpty() ? TEXT("true") : TEXT("false"));

        if (!SlotData.IsEmpty())
        {
            OnItemEquipped.Broadcast(SlotData.SlotType, SlotData.ItemInstance);
        }
        else
        {
            OnItemUnequipped.Broadcast(SlotData.SlotType, FInventoryItemInstance());
        }
    }

    PrevReplicatedSlots = ReplicatedSlots;

    UE_LOG(LogEXFIL, Log,
        TEXT("[EquipmentRep][Client] OnRep_Slots ChangedSlotCount=%d TotalSlots=%d"),
        ChangedSlotCount,
        ReplicatedSlots.Num());
}

// ========== Request API ==========

void UEquipmentComponent::RequestEquipFromInventory(EEquipmentSlot Slot, FGuid ItemInstanceID)
{
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        Server_RequestEquipFromInventory(Slot, ItemInstanceID);
        return;
    }

    EquipFromInventory_Internal(Slot, ItemInstanceID);
}

void UEquipmentComponent::RequestUnequipToInventory(EEquipmentSlot Slot)
{
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        Server_RequestUnequipToInventory(Slot);
        return;
    }

    UnequipToInventory_Internal(Slot);
}

void UEquipmentComponent::RequestUnequipToInventoryAt(
    EEquipmentSlot Slot, FIntPoint Position, bool bRotated)
{
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        Server_RequestUnequipToInventoryAt(Slot, Position, bRotated);
        return;
    }

    UnequipToInventoryAt_Internal(Slot, Position, bRotated);
}

void UEquipmentComponent::RequestDropEquippedItem(EEquipmentSlot Slot)
{
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        Server_RequestDropEquippedItem(Slot);
        return;
    }

    DropEquippedItem_Internal(Slot);
}

// ========== Server RPCs ==========

void UEquipmentComponent::Server_RequestEquipFromInventory_Implementation(
    EEquipmentSlot Slot, FGuid ItemInstanceID)
{
    if (!ItemInstanceID.IsValid())
    {
        return;
    }

    EquipFromInventory_Internal(Slot, ItemInstanceID);
}

void UEquipmentComponent::Server_RequestUnequipToInventory_Implementation(EEquipmentSlot Slot)
{
    if (Slot == EEquipmentSlot::None)
    {
        return;
    }

    UnequipToInventory_Internal(Slot);
}

void UEquipmentComponent::Server_RequestUnequipToInventoryAt_Implementation(
    EEquipmentSlot Slot, FIntPoint Position, bool bRotated)
{
    if (Slot == EEquipmentSlot::None)
    {
        return;
    }

    UnequipToInventoryAt_Internal(Slot, Position, bRotated);
}

void UEquipmentComponent::Server_RequestDropEquippedItem_Implementation(EEquipmentSlot Slot)
{
    if (Slot == EEquipmentSlot::None)
    {
        return;
    }

    DropEquippedItem_Internal(Slot);
}

// ========== Initialize ==========

void UEquipmentComponent::InitializeSlots()
{
    ReplicatedSlots.Empty();
    ReplicatedSlots.Add(FEquipmentSlotData(EEquipmentSlot::Head));
    ReplicatedSlots.Add(FEquipmentSlotData(EEquipmentSlot::Face));
    ReplicatedSlots.Add(FEquipmentSlotData(EEquipmentSlot::Eyewear));
    ReplicatedSlots.Add(FEquipmentSlotData(EEquipmentSlot::Body));
    ReplicatedSlots.Add(FEquipmentSlotData(EEquipmentSlot::Weapon1));
    ReplicatedSlots.Add(FEquipmentSlotData(EEquipmentSlot::Weapon2));

    InitializeSlotMapping();
    RebuildSlotIndexMap();
}

void UEquipmentComponent::InitializeSlotMapping()
{
    SlotTagToCandidates.Empty();
    SlotTagToCandidates.Add(FName("Weapon"), { EEquipmentSlot::Weapon1, EEquipmentSlot::Weapon2 });
    SlotTagToCandidates.Add(FName("Head"), { EEquipmentSlot::Head });
    SlotTagToCandidates.Add(FName("Face"), { EEquipmentSlot::Face });
    SlotTagToCandidates.Add(FName("Eyewear"), { EEquipmentSlot::Eyewear });
    SlotTagToCandidates.Add(FName("Body"), { EEquipmentSlot::Body });
}

// ========== Internal Write API ==========

bool UEquipmentComponent::EquipItem_Internal(
    EEquipmentSlot Slot, const FInventoryItemInstance& ItemInstance)
{
    checkf(GetOwner() && GetOwner()->HasAuthority(),
        TEXT("EquipItem_Internal must run on the server."));

    if (Slot == EEquipmentSlot::None || !ItemInstance.IsValid())
    {
        return false;
    }

    FEquipmentSlotData* SlotData = FindSlotData(Slot);
    if (!SlotData)
    {
        return false;
    }

    if (!SlotData->IsEmpty())
    {
        UnequipItem_Internal(Slot);
    }

    SlotData->EquippedItemID = ItemInstance.InstanceID;
    SlotData->ItemInstance = ItemInstance;
    ApplyEquipmentEffect(*SlotData, ItemInstance);

    return true;
}

bool UEquipmentComponent::UnequipItem_Internal(EEquipmentSlot Slot)
{
    checkf(GetOwner() && GetOwner()->HasAuthority(),
        TEXT("UnequipItem_Internal must run on the server."));

    FEquipmentSlotData* SlotData = FindSlotData(Slot);
    if (!SlotData || SlotData->IsEmpty())
    {
        return false;
    }

    RemoveEquipmentEffect(*SlotData);

    const FInventoryItemInstance Item = SlotData->ItemInstance;
    SlotData->EquippedItemID.Invalidate();
    SlotData->ItemInstance = FInventoryItemInstance();

    return true;
}

bool UEquipmentComponent::EquipFromInventory_Internal(EEquipmentSlot Slot, FGuid ItemInstanceID)
{
    checkf(GetOwner() && GetOwner()->HasAuthority(),
        TEXT("EquipFromInventory_Internal must run on the server."));

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

    UInventoryComponent* InvComp = Owner->FindComponentByClass<UInventoryComponent>();
    if (!InvComp)
    {
        return false;
    }

    FInventoryItemInstance ItemInstance;
    if (!InvComp->GetItemByID(ItemInstanceID, ItemInstance))
    {
        UE_LOG(LogEXFIL, Warning, TEXT("EquipFromInventory_Internal: Item %s not found in inventory"),
            *ItemInstanceID.ToString());
        return false;
    }

    if (!CachedItemSub)
    {
        return false;
    }

    const FItemData* ItemData = CachedItemSub->GetItemData(ItemInstance.ItemDataID);
    if (!ItemData || ItemData->ItemType != EItemType::Equipment)
    {
        UE_LOG(LogEXFIL, Warning,
            TEXT("EquipFromInventory_Internal: '%s' is not equipment"),
            *ItemInstance.ItemDataID.ToString());
        return false;
    }

    const TArray<EEquipmentSlot>* ValidSlots =
        SlotTagToCandidates.Find(ItemData->EquipmentSlotTag);
    if (!ValidSlots || ValidSlots->IsEmpty())
    {
        UE_LOG(LogEXFIL, Warning,
            TEXT("EquipFromInventory_Internal: Unknown EquipmentSlotTag '%s'"),
            *ItemData->EquipmentSlotTag.ToString());
        return false;
    }

    EEquipmentSlot TargetSlot = Slot;
    if (TargetSlot == EEquipmentSlot::None)
    {
        TargetSlot = FindTargetSlot(ItemData->EquipmentSlotTag);
    }
    else if (!ValidSlots->Contains(TargetSlot))
    {
        UE_LOG(LogEXFIL, Warning,
            TEXT("EquipFromInventory_Internal: Invalid slot %d for '%s' (tag '%s')"),
            static_cast<int32>(TargetSlot),
            *ItemInstance.ItemDataID.ToString(),
            *ItemData->EquipmentSlotTag.ToString());
        return false;
    }

    if (TargetSlot == EEquipmentSlot::None)
    {
        UE_LOG(LogEXFIL, Warning,
            TEXT("EquipFromInventory_Internal: Target slot is None"));
        return false;
    }

    FEquipmentSlotData* SlotData = FindSlotData(TargetSlot);
    if (!SlotData)
    {
        return false;
    }

    if (!SlotData->IsEmpty())
    {
        const FName OldItemDataID = SlotData->ItemInstance.ItemDataID;
        const bool bCanReturn = InvComp->AddItemByID_Internal(OldItemDataID, 1);
        if (!bCanReturn)
        {
            UE_LOG(LogEXFIL, Warning,
                TEXT("EquipFromInventory_Internal: Swap rejected - inventory full ('%s')"),
                *OldItemDataID.ToString());
            return false;
        }

        RemoveEquipmentEffect(*SlotData);
        SlotData->EquippedItemID.Invalidate();
        SlotData->ItemInstance = FInventoryItemInstance();
    }

    InvComp->DecrementStack_Internal(ItemInstanceID);

    FInventoryItemInstance EquipInstance = ItemInstance;
    EquipInstance.StackCount = 1;
    EquipInstance.InstanceID = FGuid::NewGuid();

    return EquipItem_Internal(TargetSlot, EquipInstance);
}

bool UEquipmentComponent::UnequipToInventory_Internal(EEquipmentSlot Slot)
{
    checkf(GetOwner() && GetOwner()->HasAuthority(),
        TEXT("UnequipToInventory_Internal must run on the server."));

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

    FInventoryItemInstance EquippedItem;
    if (!GetEquippedItem(Slot, EquippedItem))
    {
        return false;
    }

    UInventoryComponent* InvComp = Owner->FindComponentByClass<UInventoryComponent>();
    if (!InvComp)
    {
        return false;
    }

    if (!InvComp->AddItemByID_Internal(EquippedItem.ItemDataID, EquippedItem.StackCount))
    {
        UE_LOG(LogEXFIL, Warning,
            TEXT("UnequipToInventory_Internal: Failed to return '%s' to inventory"),
            *EquippedItem.ItemDataID.ToString());
        return false;
    }

    return UnequipItem_Internal(Slot);
}

bool UEquipmentComponent::UnequipToInventoryAt_Internal(
    EEquipmentSlot Slot, FIntPoint Position, bool bRotated)
{
    checkf(GetOwner() && GetOwner()->HasAuthority(),
        TEXT("UnequipToInventoryAt_Internal must run on the server."));

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

    FInventoryItemInstance EquippedItem;
    if (!GetEquippedItem(Slot, EquippedItem))
    {
        return false;
    }

    UInventoryComponent* InvComp = Owner->FindComponentByClass<UInventoryComponent>();
    if (!InvComp)
    {
        return false;
    }

    if (!InvComp->AddItemByIDAt_Internal(
            EquippedItem.ItemDataID, Position, bRotated, EquippedItem.StackCount))
    {
        UE_LOG(LogEXFIL, Warning,
            TEXT("UnequipToInventoryAt_Internal: Failed to return '%s' to (%d,%d) Rotated=%s"),
            *EquippedItem.ItemDataID.ToString(),
            Position.X,
            Position.Y,
            bRotated ? TEXT("true") : TEXT("false"));
        return false;
    }

    return UnequipItem_Internal(Slot);
}

bool UEquipmentComponent::DropEquippedItem_Internal(EEquipmentSlot Slot)
{
    checkf(GetOwner() && GetOwner()->HasAuthority(),
        TEXT("DropEquippedItem_Internal must run on the server."));

    FEquipmentSlotData* SlotData = FindSlotData(Slot);
    if (!SlotData || SlotData->IsEmpty())
    {
        return false;
    }

    const FName DropItemDataID = SlotData->ItemInstance.ItemDataID;

    RemoveEquipmentEffect(*SlotData);
    SlotData->EquippedItemID.Invalidate();
    SlotData->ItemInstance = FInventoryItemInstance();

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

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
        DroppedItem->InitializeItem(DropItemDataID, 1);
    }

    return DroppedItem != nullptr;
}

// ========== Query API ==========

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

// ========== Helpers ==========

void UEquipmentComponent::RebuildSlotIndexMap()
{
    SlotIndexMap.Empty(ReplicatedSlots.Num());
    for (int32 i = 0; i < ReplicatedSlots.Num(); ++i)
    {
        SlotIndexMap.Add(ReplicatedSlots[i].SlotType, i);
    }
}

FEquipmentSlotData* UEquipmentComponent::FindSlotData(EEquipmentSlot SlotType)
{
    if (const int32* Index = SlotIndexMap.Find(SlotType))
    {
        return &ReplicatedSlots[*Index];
    }
    return nullptr;
}

const FEquipmentSlotData* UEquipmentComponent::FindSlotData(EEquipmentSlot SlotType) const
{
    if (const int32* Index = SlotIndexMap.Find(SlotType))
    {
        return &ReplicatedSlots[*Index];
    }
    return nullptr;
}

void UEquipmentComponent::ApplyEquipmentEffect(
    FEquipmentSlotData& SlotData, const FInventoryItemInstance& Item)
{
    UAbilitySystemComponent* ASC = GetASC();
    if (!ASC || !CachedItemSub)
    {
        return;
    }

    const FItemData* ItemData = CachedItemSub->GetItemData(Item.ItemDataID);
    if (!ItemData || ItemData->EquipmentEffect.IsNull())
    {
        return;
    }

    TSubclassOf<UGameplayEffect> GEClass =
        CachedItemSub->GetCachedEffect(ItemData->EquipmentEffect);
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

// ========== Slot Selection ==========

EEquipmentSlot UEquipmentComponent::FindTargetSlot(const FName& EquipmentSlotTag) const
{
    const TArray<EEquipmentSlot>* Candidates = SlotTagToCandidates.Find(EquipmentSlotTag);
    if (!Candidates || Candidates->IsEmpty())
    {
        UE_LOG(LogEXFIL, Warning, TEXT("FindTargetSlot: Unknown tag '%s'"),
            *EquipmentSlotTag.ToString());
        return EEquipmentSlot::None;
    }

    for (EEquipmentSlot CandidateSlot : *Candidates)
    {
        const FEquipmentSlotData* SlotData = FindSlotData(CandidateSlot);
        if (SlotData && SlotData->IsEmpty())
        {
            return CandidateSlot;
        }
    }

    return (*Candidates)[0];
}

EEquipmentSlot UEquipmentComponent::SlotTagToEnum(FName SlotTag)
{
    if (SlotTag.IsNone())
    {
        return EEquipmentSlot::None;
    }

    static const TMap<FName, EEquipmentSlot> TagMap =
    {
        { FName("Head"), EEquipmentSlot::Head },
        { FName("Face"), EEquipmentSlot::Face },
        { FName("Eyewear"), EEquipmentSlot::Eyewear },
        { FName("Body"), EEquipmentSlot::Body },
        { FName("Weapon1"), EEquipmentSlot::Weapon1 },
        { FName("Weapon2"), EEquipmentSlot::Weapon2 },
    };

    const EEquipmentSlot* Found = TagMap.Find(SlotTag);
    return Found ? *Found : EEquipmentSlot::None;
}
