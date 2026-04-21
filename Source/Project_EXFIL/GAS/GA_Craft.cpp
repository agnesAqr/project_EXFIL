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

    AActor* OwnerActor = ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr;
    UCraftingComponent* CraftComp = OwnerActor
        ? OwnerActor->FindComponentByClass<UCraftingComponent>()
        : nullptr;

    if (!CraftComp)
    {
        UE_LOG(LogProject_EXFIL, Warning,
            TEXT("GA_Craft: UCraftingComponent not found."));
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    CraftingCompRef = CraftComp;

    CraftComp->OnCraftingCompleted.AddDynamic(this, &UGA_Craft::OnCraftingCompleted);
    CraftComp->OnCraftStartFailed.AddDynamic(this, &UGA_Craft::OnCraftStartFailed);

    CraftComp->RequestStartCraft(RecipeID);
}

void UGA_Craft::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    if (UCraftingComponent* CraftComp = CraftingCompRef.Get())
    {
        CraftComp->OnCraftingCompleted.RemoveDynamic(this, &UGA_Craft::OnCraftingCompleted);
        CraftComp->OnCraftStartFailed.RemoveDynamic(this, &UGA_Craft::OnCraftStartFailed);

        if (bWasCancelled && CraftComp->IsCrafting())
        {
            CraftComp->RequestCancelCraft();
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Craft::OnCraftingCompleted(FName CompletedRecipeID)
{

    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo,
        GetCurrentActivationInfo(), true, false);
}

void UGA_Craft::OnCraftStartFailed(FName FailedRecipeID)
{
    if (FailedRecipeID != RecipeID)
    {
        return;
    }

    UE_LOG(LogProject_EXFIL, Warning, TEXT("GA_Craft: StartCraft('%s') failed"),
        *FailedRecipeID.ToString());

    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    CancelAbility(GetCurrentAbilitySpecHandle(), ActorInfo,
        GetCurrentActivationInfo(), true);
}
