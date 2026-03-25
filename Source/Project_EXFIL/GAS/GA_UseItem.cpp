// Copyright Project EXFIL. All Rights Reserved.

#include "GAS/GA_UseItem.h"
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Engine/GameInstance.h"
#include "Data/EXFILItemTypes.h"
#include "Data/ItemDataSubsystem.h"
#include "Inventory/InventoryComponent.h"
#include "Core/EXFILLog.h"

UGA_UseItem::UGA_UseItem()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_UseItem::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!ASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // ItemDataSubsystem에서 아이템 데이터 조회
    UItemDataSubsystem* ItemSub = nullptr;
    if (AActor* Avatar = ActorInfo->AvatarActor.Get())
    {
        if (UWorld* World = Avatar->GetWorld())
        {
            if (UGameInstance* GI = World->GetGameInstance())
            {
                ItemSub = GI->GetSubsystem<UItemDataSubsystem>();
            }
        }
    }

    if (!ItemSub)
    {
        UE_LOG(LogEXFIL, Warning, TEXT("GA_UseItem: ItemDataSubsystem 없음"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const FItemData* ItemData = ItemSub->GetItemData(ItemDataID);
    if (!ItemData)
    {
        UE_LOG(LogEXFIL, Warning, TEXT("GA_UseItem: ItemDataID '%s' 조회 실패"), *ItemDataID.ToString());
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // ConsumableEffect 로드 및 적용
    if (!ItemData->ConsumableEffect.IsNull())
    {
        TSubclassOf<UGameplayEffect> GEClass = ItemData->ConsumableEffect.LoadSynchronous();
        if (GEClass)
        {
            FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
            FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(GEClass, 1.f, ContextHandle);
            if (SpecHandle.IsValid())
            {
                ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            }
        }
    }

    // 인벤토리에서 아이템 제거 (소비)
    if (ItemInstanceID.IsValid())
    {
        if (AActor* Avatar = ActorInfo->AvatarActor.Get())
        {
            if (UInventoryComponent* InvComp = Avatar->FindComponentByClass<UInventoryComponent>())
            {
                InvComp->RemoveItem(ItemInstanceID);
            }
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
