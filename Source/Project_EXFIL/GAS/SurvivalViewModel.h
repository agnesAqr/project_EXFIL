// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "SurvivalViewModel.generated.h"

class USurvivalAttributeSet;

UENUM(BlueprintType)
enum class EExfilStatType : uint8
{
    Health   UMETA(DisplayName = "Health"),
    Hunger   UMETA(DisplayName = "Hunger"),
    Thirst   UMETA(DisplayName = "Thirst"),
    Stamina  UMETA(DisplayName = "Stamina")
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(
    FOnStatChanged,
    EExfilStatType,
    float,
    float);

UCLASS()
class PROJECT_EXFIL_API USurvivalViewModel : public UObject
{
    GENERATED_BODY()

public:
    void InitializeWithASC(UAbilitySystemComponent* ASC);

    virtual void BeginDestroy() override;

    FOnStatChanged OnStatChanged;

    float GetStatValue(EExfilStatType StatType) const;
    float GetMaxStatValue(EExfilStatType StatType) const;

private:
    UPROPERTY()
    TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

    void OnAttributeChanged(const FOnAttributeChangeData& Data);
    bool TryGetStatTypeFromAttribute(const FGameplayAttribute& Attribute, EExfilStatType& OutStatType) const;
    void BindAttributeChange(UAbilitySystemComponent* ASC, FGameplayAttribute Attribute);
    void UnbindASC();

    TArray<FGameplayAttribute> BoundAttributes;
    TArray<FDelegateHandle> BoundDelegates;
};
