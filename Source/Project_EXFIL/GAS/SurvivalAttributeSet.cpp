// Copyright Project EXFIL. All Rights Reserved.

#include "GAS/SurvivalAttributeSet.h"
#include "CoreMinimal.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Core/EXFILCharacter.h"

USurvivalAttributeSet::USurvivalAttributeSet()
{
    // 초기값 설정 — UPROPERTY 기본값 사용 (에디터에서 조절 가능)
    InitHealth(InitialHealth);
    InitMaxHealth(InitialMaxHealth);
    InitHunger(InitialHunger);
    InitMaxHunger(InitialMaxHunger);
    InitThirst(InitialThirst);
    InitMaxThirst(InitialMaxThirst);
    InitStamina(InitialStamina);
    InitMaxStamina(InitialMaxStamina);
}

void USurvivalAttributeSet::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, Health,    COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, Hunger,    COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, MaxHunger, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, Thirst,    COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, MaxThirst, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, Stamina,   COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, MaxStamina,COND_None, REPNOTIFY_Always);
}

void USurvivalAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    // CurrentValue 클램핑 — BaseValue는 PostGameplayEffectExecute에서 처리
    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
    }
    else if (Attribute == GetHungerAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHunger());
    }
    else if (Attribute == GetThirstAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxThirst());
    }
    else if (Attribute == GetStaminaAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
    }
}

void USurvivalAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        // Final clamp
        SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));

        if (GetHealth() <= 0.f)
        {
            AActor* OwnerActor = GetOwningAbilitySystemComponent()->GetAvatarActor();
            if (AEXFILCharacter* Character = Cast<AEXFILCharacter>(OwnerActor))
            {
                Character->OnDeath();
            }
        }
    }
    else if (Data.EvaluatedData.Attribute == GetHungerAttribute())
    {
        SetHunger(FMath::Clamp(GetHunger(), 0.f, GetMaxHunger()));
    }
    else if (Data.EvaluatedData.Attribute == GetThirstAttribute())
    {
        SetThirst(FMath::Clamp(GetThirst(), 0.f, GetMaxThirst()));
    }
    else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
    {
        SetStamina(FMath::Clamp(GetStamina(), 0.f, GetMaxStamina()));
    }
}

// ─── OnRep 함수 ───────────────────────────────────────────────────────────────

void USurvivalAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, Health, OldHealth);
}

void USurvivalAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, MaxHealth, OldMaxHealth);
}

void USurvivalAttributeSet::OnRep_Hunger(const FGameplayAttributeData& OldHunger)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, Hunger, OldHunger);
}

void USurvivalAttributeSet::OnRep_MaxHunger(const FGameplayAttributeData& OldMaxHunger)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, MaxHunger, OldMaxHunger);
}

void USurvivalAttributeSet::OnRep_Thirst(const FGameplayAttributeData& OldThirst)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, Thirst, OldThirst);
}

void USurvivalAttributeSet::OnRep_MaxThirst(const FGameplayAttributeData& OldMaxThirst)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, MaxThirst, OldMaxThirst);
}

void USurvivalAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldStamina)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, Stamina, OldStamina);
}

void USurvivalAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, MaxStamina, OldMaxStamina);
}
