// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_UseItem.generated.h"

/**
 * UGA_UseItem — 소비 아이템 사용 GameplayAbility
 *
 * 호출 측에서 SetByCallerMagnitude 또는 AbilitySpec.GameplayEffectLevel로 아이템 ID를 전달.
 * Day 4: 기초 구조 — ActivateAbility에서 ConsumableEffect 로드 후 ASC에 Apply.
 * Day 5: UCraftingComponent 연동 예정.
 */
UCLASS()
class PROJECT_EXFIL_API UGA_UseItem : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_UseItem();

    /** 사용할 아이템 데이터 ID (호출 전 설정) */
    UPROPERTY(BlueprintReadWrite, Category = "UseItem")
    FName ItemDataID;

    /** 사용 후 인벤토리에서 제거할 아이템 인스턴스 ID */
    UPROPERTY(BlueprintReadWrite, Category = "UseItem")
    FGuid ItemInstanceID;

protected:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
};
