// Copyright Project EXFIL. All Rights Reserved.

#include "Data/Equipment/EquipmentComponent.h"
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "Engine/GameInstance.h"
#include "Data/EXFILItemTypes.h"
#include "Data/ItemDataSubsystem.h"

UEquipmentComponent::UEquipmentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UEquipmentComponent::EquipItem(EEquipmentSlot Slot, const FInventoryItemInstance& ItemInstance)
{
    if (Slot == EEquipmentSlot::None)
    {
        return false;
    }

    // 이미 점유된 슬롯은 먼저 해제
    if (IsSlotOccupied(Slot))
    {
        UnequipItem(Slot);
    }

    EquippedItems.Add(Slot, ItemInstance);
    ApplyEquipmentEffect(Slot, ItemInstance);

    OnItemEquipped.Broadcast(Slot, ItemInstance);
    UE_LOG(LogTemp, Log, TEXT("EquipmentComponent: Slot[%d] — '%s' 장착"),
        static_cast<int32>(Slot), *ItemInstance.ItemDataID.ToString());
    return true;
}

bool UEquipmentComponent::UnequipItem(EEquipmentSlot Slot)
{
    if (!IsSlotOccupied(Slot))
    {
        return false;
    }

    RemoveEquipmentEffect(Slot);

    const FInventoryItemInstance Item = EquippedItems[Slot];
    EquippedItems.Remove(Slot);

    OnItemUnequipped.Broadcast(Slot, Item);
    UE_LOG(LogTemp, Log, TEXT("EquipmentComponent: Slot[%d] — '%s' 해제"),
        static_cast<int32>(Slot), *Item.ItemDataID.ToString());
    return true;
}

bool UEquipmentComponent::GetEquippedItem(EEquipmentSlot Slot, FInventoryItemInstance& OutItem) const
{
    if (const FInventoryItemInstance* Found = EquippedItems.Find(Slot))
    {
        OutItem = *Found;
        return true;
    }
    return false;
}

bool UEquipmentComponent::IsSlotOccupied(EEquipmentSlot Slot) const
{
    return EquippedItems.Contains(Slot);
}

void UEquipmentComponent::ApplyEquipmentEffect(EEquipmentSlot Slot,
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
        ActiveEffectHandles.Add(Slot, ActiveHandle);
        UE_LOG(LogTemp, Log, TEXT("EquipmentComponent: '%s' EquipmentEffect 적용"),
            *Item.ItemDataID.ToString());
    }
}

void UEquipmentComponent::RemoveEquipmentEffect(EEquipmentSlot Slot)
{
    UAbilitySystemComponent* ASC = GetASC();
    if (!ASC)
    {
        return;
    }

    if (const FActiveGameplayEffectHandle* Handle = ActiveEffectHandles.Find(Slot))
    {
        if (Handle->IsValid())
        {
            ASC->RemoveActiveGameplayEffect(*Handle);
        }
        ActiveEffectHandles.Remove(Slot);
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
