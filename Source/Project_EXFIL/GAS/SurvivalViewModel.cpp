// Copyright Project EXFIL. All Rights Reserved.

#include "GAS/SurvivalViewModel.h"
#include "CoreMinimal.h"
#include "GAS/SurvivalAttributeSet.h"
#include "Core/EXFILLog.h"

void USurvivalViewModel::InitializeWithASC(UAbilitySystemComponent* ASC)
{
    UnbindASC();

    if (!ASC)
    {
        UE_LOG(LogEXFIL, Warning, TEXT("SurvivalViewModel::InitializeWithASC — ASC NULL"));
        return;
    }

    CachedASC = ASC;

    BindAttributeChange(ASC, USurvivalAttributeSet::GetHealthAttribute());
    BindAttributeChange(ASC, USurvivalAttributeSet::GetHungerAttribute());
    BindAttributeChange(ASC, USurvivalAttributeSet::GetThirstAttribute());
    BindAttributeChange(ASC, USurvivalAttributeSet::GetStaminaAttribute());
    BindAttributeChange(ASC, USurvivalAttributeSet::GetMaxHealthAttribute());
    BindAttributeChange(ASC, USurvivalAttributeSet::GetMaxHungerAttribute());
    BindAttributeChange(ASC, USurvivalAttributeSet::GetMaxThirstAttribute());
    BindAttributeChange(ASC, USurvivalAttributeSet::GetMaxStaminaAttribute());
}

void USurvivalViewModel::BeginDestroy()
{
    UnbindASC();
    Super::BeginDestroy();
}

void USurvivalViewModel::BindAttributeChange(
    UAbilitySystemComponent* ASC, FGameplayAttribute Attribute)
{
    if (!ASC)
    {
        return;
    }

    BoundAttributes.Add(Attribute);
    BoundDelegates.Add(
        ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
        .AddUObject(this, &USurvivalViewModel::OnAttributeChanged));
}

void USurvivalViewModel::UnbindASC()
{
    if (UAbilitySystemComponent* ASC = CachedASC.Get())
    {
        const int32 BindingCount = FMath::Min(BoundAttributes.Num(), BoundDelegates.Num());
        for (int32 Index = 0; Index < BindingCount; ++Index)
        {
            if (BoundDelegates[Index].IsValid())
            {
                ASC->GetGameplayAttributeValueChangeDelegate(BoundAttributes[Index])
                    .Remove(BoundDelegates[Index]);
            }
        }
    }

    BoundAttributes.Reset();
    BoundDelegates.Reset();
    CachedASC.Reset();
}

void USurvivalViewModel::OnAttributeChanged(const FOnAttributeChangeData& Data)
{
    EExfilStatType ChangedStatType;
    if (TryGetStatTypeFromAttribute(Data.Attribute, ChangedStatType))
    {
        OnStatChanged.Broadcast(
            ChangedStatType,
            GetStatValue(ChangedStatType),
            GetMaxStatValue(ChangedStatType));
    }
}

float USurvivalViewModel::GetStatValue(EExfilStatType StatType) const
{
    if (!CachedASC.IsValid()) return 0.f;
    const USurvivalAttributeSet* AS = CachedASC->GetSet<USurvivalAttributeSet>();
    if (!AS) return 0.f;

    switch (StatType)
    {
    case EExfilStatType::Health:  return AS->GetHealth();
    case EExfilStatType::Hunger:  return AS->GetHunger();
    case EExfilStatType::Thirst:  return AS->GetThirst();
    case EExfilStatType::Stamina: return AS->GetStamina();
    default:                      return 0.f;
    }
}

float USurvivalViewModel::GetMaxStatValue(EExfilStatType StatType) const
{
    if (!CachedASC.IsValid()) return 0.f;
    const USurvivalAttributeSet* AS = CachedASC->GetSet<USurvivalAttributeSet>();
    if (!AS) return 0.f;

    switch (StatType)
    {
    case EExfilStatType::Health:  return AS->GetMaxHealth();
    case EExfilStatType::Hunger:  return AS->GetMaxHunger();
    case EExfilStatType::Thirst:  return AS->GetMaxThirst();
    case EExfilStatType::Stamina: return AS->GetMaxStamina();
    default:                      return 0.f;
    }
}

bool USurvivalViewModel::TryGetStatTypeFromAttribute(
    const FGameplayAttribute& Attribute,
    EExfilStatType& OutStatType) const
{
    if (Attribute == USurvivalAttributeSet::GetHealthAttribute() ||
        Attribute == USurvivalAttributeSet::GetMaxHealthAttribute())
    {
        OutStatType = EExfilStatType::Health;
        return true;
    }

    if (Attribute == USurvivalAttributeSet::GetHungerAttribute() ||
        Attribute == USurvivalAttributeSet::GetMaxHungerAttribute())
    {
        OutStatType = EExfilStatType::Hunger;
        return true;
    }

    if (Attribute == USurvivalAttributeSet::GetThirstAttribute() ||
        Attribute == USurvivalAttributeSet::GetMaxThirstAttribute())
    {
        OutStatType = EExfilStatType::Thirst;
        return true;
    }

    if (Attribute == USurvivalAttributeSet::GetStaminaAttribute() ||
        Attribute == USurvivalAttributeSet::GetMaxStaminaAttribute())
    {
        OutStatType = EExfilStatType::Stamina;
        return true;
    }

    return false;
}
