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
    if (const FProperty* Prop = Data.Attribute.GetUProperty())
    {
        OnStatChanged.Broadcast(Prop->GetFName(), Data.NewValue);
    }
}

float USurvivalViewModel::GetStatValue(FName StatName) const
{
    if (!CachedASC.IsValid()) return 0.f;
    const USurvivalAttributeSet* AS = CachedASC->GetSet<USurvivalAttributeSet>();
    if (!AS) return 0.f;

    if (StatName == FName("Health"))  return AS->GetHealth();
    if (StatName == FName("Hunger"))  return AS->GetHunger();
    if (StatName == FName("Thirst"))  return AS->GetThirst();
    if (StatName == FName("Stamina")) return AS->GetStamina();
    return 0.f;
}

float USurvivalViewModel::GetMaxStatValue(FName StatName) const
{
    if (!CachedASC.IsValid()) return 100.f;
    const USurvivalAttributeSet* AS = CachedASC->GetSet<USurvivalAttributeSet>();
    if (!AS) return 100.f;

    if (StatName == FName("Health"))  return AS->GetMaxHealth();
    if (StatName == FName("Hunger"))  return AS->GetMaxHunger();
    if (StatName == FName("Thirst"))  return AS->GetMaxThirst();
    if (StatName == FName("Stamina")) return AS->GetMaxStamina();
    return 100.f;
}
