// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Fire.generated.h"

class UEquipmentComponent;

/**
 * UGA_Fire — 라인 트레이스 슈팅 GameplayAbility
 *
 * Day 8: 클라이언트에서 카메라 Forward 라인 트레이스 → 히트 결과를 Server RPC로 전송
 * NetExecutionPolicy = LocalPredicted (클라이언트 즉시 디버그 라인, 서버에서 데미지 확정)
 */
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
