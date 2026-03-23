// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Craft.generated.h"

class UCraftingComponent;

/**
 * UGA_Craft — 크래프팅 GameplayAbility
 *
 * ActivateAbility:
 *   1. 소유 Actor에서 UCraftingComponent 획득
 *   2. CraftingComp->StartCraft(RecipeID) 호출
 *   3. OnCraftingCompleted 델리게이트 바인딩 → 완료 시 EndAbility()
 *   4. 실패 시 CancelAbility()
 *
 * 참고: GA 없이 UCraftingComponent::StartCraft()를 직접 호출해도 동작함.
 *       GA는 GAS 태그 기반 제어(크래프팅 중 이동 불가 등)가 필요할 때 사용.
 */
UCLASS()
class PROJECT_EXFIL_API UGA_Craft : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Craft();

    /** 크래프팅할 레시피 ID (AbilitySpec에서 설정하거나 BP에서 지정) */
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

    /** 크래프팅 완료 콜백 */
    UFUNCTION()
    void OnCraftingCompleted(FName CompletedRecipeID);
};
