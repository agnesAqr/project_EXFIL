// Copyright Project EXFIL. All Rights Reserved.

#include "GAS/GA_Craft.h"
#include "CoreMinimal.h"
#include "Crafting/CraftingComponent.h"
#include "Project_EXFIL.h"

UGA_Craft::UGA_Craft()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGA_Craft::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                 const FGameplayAbilityActorInfo* ActorInfo,
                                 const FGameplayAbilityActivationInfo ActivationInfo,
                                 const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 소유 Actor에서 CraftingComponent 획득
    AActor* OwnerActor = ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr;
    UCraftingComponent* CraftComp = OwnerActor
        ? OwnerActor->FindComponentByClass<UCraftingComponent>()
        : nullptr;

    if (!CraftComp)
    {
        UE_LOG(LogProject_EXFIL, Warning,
               TEXT("GA_Craft: UCraftingComponent를 찾을 수 없습니다."));
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    CraftingCompRef = CraftComp;

    // 완료 델리게이트 바인딩
    CraftComp->OnCraftingCompleted.AddDynamic(this, &UGA_Craft::OnCraftingCompleted);

    // 크래프팅 시작
    if (!CraftComp->StartCraft(RecipeID))
    {
        UE_LOG(LogProject_EXFIL, Warning,
               TEXT("GA_Craft: StartCraft('%s') 실패"), *RecipeID.ToString());
        CraftComp->OnCraftingCompleted.RemoveDynamic(this, &UGA_Craft::OnCraftingCompleted);
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
    }
}

void UGA_Craft::EndAbility(const FGameplayAbilitySpecHandle Handle,
                            const FGameplayAbilityActorInfo* ActorInfo,
                            const FGameplayAbilityActivationInfo ActivationInfo,
                            bool bReplicateEndAbility,
                            bool bWasCancelled)
{
    // 델리게이트 해제
    if (UCraftingComponent* CraftComp = CraftingCompRef.Get())
    {
        CraftComp->OnCraftingCompleted.RemoveDynamic(this, &UGA_Craft::OnCraftingCompleted);

        // Ability가 외부에서 취소된 경우 크래프팅도 취소
        if (bWasCancelled && CraftComp->IsCrafting())
        {
            CraftComp->CancelCraft();
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Craft::OnCraftingCompleted(FName CompletedRecipeID)
{
    UE_LOG(LogProject_EXFIL, Log, TEXT("GA_Craft: '%s' 완료 → EndAbility"),
           *CompletedRecipeID.ToString());

    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo,
               GetCurrentActivationInfo(), true, false);
}
