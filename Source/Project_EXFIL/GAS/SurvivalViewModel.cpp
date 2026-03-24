// Copyright Project EXFIL. All Rights Reserved.

#include "GAS/SurvivalViewModel.h"
#include "CoreMinimal.h"
#include "GAS/SurvivalAttributeSet.h"

void USurvivalViewModel::InitializeWithASC(UAbilitySystemComponent* ASC)
{
    if (!ASC)
    {
        UE_LOG(LogTemp, Warning, TEXT("SurvivalViewModel::InitializeWithASC — ASC NULL"));
        return;
    }

    const USurvivalAttributeSet* AttrSet = ASC->GetSet<USurvivalAttributeSet>();
    if (!AttrSet)
    {
        UE_LOG(LogTemp, Warning, TEXT("SurvivalViewModel::InitializeWithASC — AttributeSet NULL"));
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

    UE_LOG(LogTemp, Warning, TEXT("SurvivalViewModel: bound to ASC, %d attributes"),
        BoundDelegates.Num());
}

void USurvivalViewModel::OnAttributeChanged(const FOnAttributeChangeData& Data)
{
    const FName StatName = FName(*Data.Attribute.GetName());
    const float NewValue = Data.NewValue;

    UE_LOG(LogTemp, Warning, TEXT("[STAT-4] ViewModel: %s = %.1f"),
        *StatName.ToString(), NewValue);

    OnStatChanged.Broadcast(StatName, NewValue);
}

float USurvivalViewModel::GetStatValue(FName StatName) const
{
    if (!CachedASC.IsValid()) return 0.f;
    const USurvivalAttributeSet* AttrSet = CachedASC->GetSet<USurvivalAttributeSet>();
    if (!AttrSet) return 0.f;

    if (StatName == FName("Health"))  return AttrSet->GetHealth();
    if (StatName == FName("Hunger"))  return AttrSet->GetHunger();
    if (StatName == FName("Thirst"))  return AttrSet->GetThirst();
    if (StatName == FName("Stamina")) return AttrSet->GetStamina();
    return 0.f;
}

float USurvivalViewModel::GetMaxStatValue(FName StatName) const
{
    if (!CachedASC.IsValid()) return 100.f;
    const USurvivalAttributeSet* AttrSet = CachedASC->GetSet<USurvivalAttributeSet>();
    if (!AttrSet) return 100.f;

    if (StatName == FName("Health"))  return AttrSet->GetMaxHealth();
    if (StatName == FName("Hunger"))  return AttrSet->GetMaxHunger();
    if (StatName == FName("Thirst"))  return AttrSet->GetMaxThirst();
    if (StatName == FName("Stamina")) return AttrSet->GetMaxStamina();
    return 100.f;
}
