// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "SurvivalViewModel.generated.h"

class USurvivalAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnStatChanged, FName, StatName, float, NewValue);

/**
 * USurvivalViewModel — GAS AttributeSet 변화를 View로 중계하는 ViewModel
 *
 * MVVM: ASC(Model) → SurvivalViewModel → StatEntryWidget(View)
 * ASC에서 직접 View를 건드리지 않는다.
 */
UCLASS()
class PROJECT_EXFIL_API USurvivalViewModel : public UObject
{
    GENERATED_BODY()

public:
    /** ASC에 바인딩. ASC::GetSet<USurvivalAttributeSet>()로 AttributeSet을 얻음. */
    void InitializeWithASC(UAbilitySystemComponent* ASC);

    /** View(StatEntryWidget)에서 구독하는 델리게이트 */
    UPROPERTY(BlueprintAssignable)
    FOnStatChanged OnStatChanged;

    /** BindToViewModel 시 초기값 설정용 현재값 조회 */
    float GetStatValue(FName StatName) const;

    /** BindToViewModel 시 초기값 설정용 최대값 조회 */
    float GetMaxStatValue(FName StatName) const;

private:
    UPROPERTY()
    TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

    /** 속성 변경 콜백 — 4개 속성 모두 이 함수로 라우팅 */
    void OnAttributeChanged(const FOnAttributeChangeData& Data);

    /** 델리게이트 핸들 저장 (BeginDestroy에서 해제) */
    TArray<FDelegateHandle> BoundDelegates;
};
