// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "SurvivalAttributeSet.generated.h"

/** GAS 어트리뷰트 접근자 매크로 — Getter/Setter/Initter 자동 생성 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * USurvivalAttributeSet — 생존 스탯 어트리뷰트
 * Health, Hunger, Thirst, Stamina (각 Max 포함)
 */
UCLASS()
class PROJECT_EXFIL_API USurvivalAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    USurvivalAttributeSet();

    // ─── Health ───────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Health")
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Health")
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, MaxHealth)

    // ─── Hunger ───────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Hunger, Category = "Attributes|Survival")
    FGameplayAttributeData Hunger;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, Hunger)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHunger, Category = "Attributes|Survival")
    FGameplayAttributeData MaxHunger;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, MaxHunger)

    // ─── Thirst ───────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Thirst, Category = "Attributes|Survival")
    FGameplayAttributeData Thirst;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, Thirst)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxThirst, Category = "Attributes|Survival")
    FGameplayAttributeData MaxThirst;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, MaxThirst)

    // ─── Stamina ──────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Attributes|Survival")
    FGameplayAttributeData Stamina;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, Stamina)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Attributes|Survival")
    FGameplayAttributeData MaxStamina;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, MaxStamina)

    // ─── 초기값 설정 (에디터에서 조절 가능) ──────────────────────
    UPROPERTY(EditAnywhere, Category = "Attributes|Defaults")
    float InitialHealth = 50.f;

    UPROPERTY(EditAnywhere, Category = "Attributes|Defaults")
    float InitialMaxHealth = 100.f;

    UPROPERTY(EditAnywhere, Category = "Attributes|Defaults")
    float InitialHunger = 50.f;

    UPROPERTY(EditAnywhere, Category = "Attributes|Defaults")
    float InitialMaxHunger = 100.f;

    UPROPERTY(EditAnywhere, Category = "Attributes|Defaults")
    float InitialThirst = 50.f;

    UPROPERTY(EditAnywhere, Category = "Attributes|Defaults")
    float InitialMaxThirst = 100.f;

    UPROPERTY(EditAnywhere, Category = "Attributes|Defaults")
    float InitialStamina = 50.f;

    UPROPERTY(EditAnywhere, Category = "Attributes|Defaults")
    float InitialMaxStamina = 100.f;

    // ─── Replication ──────────────────────────────────────────────
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    /** GE 적용 전 클램핑 (CurrentValue 범위 제한) */
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute,
        float& NewValue) override;

    /** GE 적용 후 후처리 (사망 판정 등) */
    virtual void PostGameplayEffectExecute(
        const FGameplayEffectModCallbackData& Data) override;

private:
    UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldHealth);
    UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
    UFUNCTION() void OnRep_Hunger(const FGameplayAttributeData& OldHunger);
    UFUNCTION() void OnRep_MaxHunger(const FGameplayAttributeData& OldMaxHunger);
    UFUNCTION() void OnRep_Thirst(const FGameplayAttributeData& OldThirst);
    UFUNCTION() void OnRep_MaxThirst(const FGameplayAttributeData& OldMaxThirst);
    UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldStamina);
    UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);
};
