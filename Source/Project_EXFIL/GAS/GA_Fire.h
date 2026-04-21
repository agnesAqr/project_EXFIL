// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Fire.generated.h"

class UEquipmentComponent;

UCLASS()
class PROJECT_EXFIL_API UGA_Fire : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Fire();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags) const override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Fire")
    float FireRange = 5000.f;

    UPROPERTY(EditDefaultsOnly, Category = "Fire")
    TSubclassOf<UGameplayEffect> DamageEffectClass;
};
