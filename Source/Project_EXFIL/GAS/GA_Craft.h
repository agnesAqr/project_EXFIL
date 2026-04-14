// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Craft.generated.h"

class UCraftingComponent;

UCLASS()
class PROJECT_EXFIL_API UGA_Craft : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Craft();

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Craft")
    FName RecipeID;

protected:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

private:
    UPROPERTY()
    TWeakObjectPtr<UCraftingComponent> CraftingCompRef;

    UFUNCTION()
    void OnCraftingCompleted(FName CompletedRecipeID);

    UFUNCTION()
    void OnCraftStartFailed(FName FailedRecipeID);
};
