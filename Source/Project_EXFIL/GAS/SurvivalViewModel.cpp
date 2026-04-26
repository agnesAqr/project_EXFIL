// Copyright Project EXFIL. All Rights Reserved.

#include "GAS/SurvivalViewModel.h"
#include "CoreMinimal.h"
#include "GAS/SurvivalAttributeSet.h"
#include "Core/EXFILLog.h"

void USurvivalViewModel::InitializeWithASC(UAbilitySystemComponent* ASC)
{
    if (!ASC)
    {
        UE_LOG(LogEXFIL, Warning, TEXT("SurvivalViewModel::InitializeWithASC — ASC NULL"));
        return;
    }

    CachedASC = ASC;

    BoundDelegates.Add(
        ASC->GetGameplayAttributeValueChangeDelegate(
            USurvivalAttributeSet::GetHealthAttribute())
        .AddUObject(this, &USurvivalViewModel::OnAttributeChanged));

    BoundDelegates.Add(
        ASC->GetGameplayAttributeValueChangeDelegate(
            USurvivalAttributeSet::GetHungerAttribute())
        .AddUObject(this, &USurvivalViewModel::OnAttributeChanged));

    BoundDelegates.Add(
        ASC->GetGameplayAttributeValueChangeDelegate(
            USurvivalAttributeSet::GetThirstAttribute())
        .AddUObject(this, &USurvivalViewModel::OnAttributeChanged));

    BoundDelegates.Add(
        ASC->GetGameplayAttributeValueChangeDelegate(
            USurvivalAttributeSet::GetStaminaAttribute())
        .AddUObject(this, &USurvivalViewModel::OnAttributeChanged));

    BoundDelegates.Add(
        ASC->GetGameplayAttributeValueChangeDelegate(
            USurvivalAttributeSet::GetMaxHealthAttribute())
        .AddUObject(this, &USurvivalViewModel::OnAttributeChanged));

    BoundDelegates.Add(
        ASC->GetGameplayAttributeValueChangeDelegate(
            USurvivalAttributeSet::GetMaxHungerAttribute())
        .AddUObject(this, &USurvivalViewModel::OnAttributeChanged));

    BoundDelegates.Add(
        ASC->GetGameplayAttributeValueChangeDelegate(
            USurvivalAttributeSet::GetMaxThirstAttribute())
        .AddUObject(this, &USurvivalViewModel::OnAttributeChanged));

    BoundDelegates.Add(
        ASC->GetGameplayAttributeValueChangeDelegate(
            USurvivalAttributeSet::GetMaxStaminaAttribute())
        .AddUObject(this, &USurvivalViewModel::OnAttributeChanged));
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
