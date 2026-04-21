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

UCLASS()
class PROJECT_EXFIL_API USurvivalViewModel : public UObject
{
    GENERATED_BODY()

public:
    
    void InitializeWithASC(UAbilitySystemComponent* ASC);

    
    UPROPERTY(BlueprintAssignable)
    FOnStatChanged OnStatChanged;

    
    float GetStatValue(FName StatName) const;

    
    float GetMaxStatValue(FName StatName) const;

private:
    UPROPERTY()
    TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

    
    void OnAttributeChanged(const FOnAttributeChangeData& Data);

    
    TArray<FDelegateHandle> BoundDelegates;
};
