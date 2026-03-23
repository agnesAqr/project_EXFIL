// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Project_EXFILCharacter.h"
#include "EXFILCharacter.generated.h"

class UInventoryComponent;
class UInventoryViewModel;
class UInventoryPanelWidget;
class UAbilitySystemComponent;
class USurvivalAttributeSet;
class UCraftingComponent;
class UInputAction;
struct FInputActionValue;

/**
 * AEXFILCharacter — EXFIL 프로젝트의 플레이어 캐릭터
 * 인벤토리, GAS(ASC+AttributeSet), 장비, 크래프팅 컴포넌트의 부착 대상
 */
UCLASS()
class PROJECT_EXFIL_API AEXFILCharacter : public AProject_EXFILCharacter,
                                           public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AEXFILCharacter();

    // === IAbilitySystemInterface ===
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // === Inventory ===
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    UInventoryViewModel* GetInventoryViewModel() const { return InventoryViewModel; }

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;

    // === Inventory ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInventoryComponent> InventoryComponent;

    /** BP에서 WBP_InventoryPanel 클래스 지정 */
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI")
    TSubclassOf<UInventoryPanelWidget> InventoryPanelWidgetClass;

    UPROPERTY()
    TObjectPtr<UInventoryViewModel> InventoryViewModel;

    UPROPERTY(BlueprintReadOnly, Category = "Inventory|UI", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInventoryPanelWidget> InventoryPanelWidget;

    // === GAS ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<USurvivalAttributeSet> SurvivalAttributes;

    // === Crafting ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCraftingComponent> CraftingComponent;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting")
    UCraftingComponent* GetCraftingComponent() const { return CraftingComponent; }

};
