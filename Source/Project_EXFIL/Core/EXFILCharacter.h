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
class UEquipmentComponent;
class USurvivalViewModel;
class AWorldItem;
class UInputAction;
struct FInputActionValue;

/**
 * AEXFILCharacter — EXFIL 프로젝트의 플레이어 캐릭터
 * 인벤토리, GAS(ASC+AttributeSet), 장비, 크래프팅 컴포넌트의 부착 대상
 *
 * Day 6: bReplicates, SetIsReplicated, ASC Mixed Mode, IsLocallyControlled 가드
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

    // === Equipment ===
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
    UEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;

    /** SurvivalViewModel 생성 + ASC 바인딩 + UI 연결 (ASC 초기화 후 호출) */
    void InitializeViewModels();

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

    UPROPERTY()
    TObjectPtr<USurvivalViewModel> SurvivalViewModel;


    // === Crafting ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCraftingComponent> CraftingComponent;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting")
    UCraftingComponent* GetCraftingComponent() const { return CraftingComponent; }

    // === Equipment ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UEquipmentComponent> EquipmentComponent;

    // === 인터랙션 ===

    /** 라인 트레이스 최대 거리 (cm) */
    UPROPERTY(EditAnywhere, Category = "Interaction")
    float InteractionDistance = 300.f;

    /** F키 인터랙션 InputAction — 에디터에서 할당 */
    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> IA_Interact;

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    void OnInteractPressed();

    /** 카메라 Forward 방향으로 라인 트레이스하여 AWorldItem 탐색 */
    AWorldItem* TraceForWorldItem() const;

    // === 픽업 Server RPC ===
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestPickupItem(AWorldItem* TargetItem);

    void ExecutePickup(AWorldItem* TargetItem);

};
