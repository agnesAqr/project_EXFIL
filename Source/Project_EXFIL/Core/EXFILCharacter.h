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
class UGameplayAbility;
class UGameplayEffect;
class USpringArmComponent;
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

    // === Combat (Day 8) ===

    /** 인벤토리 UI가 현재 보이는지 반환 (GA_Fire에서 참조) */
    bool IsInventoryUIVisible() const;

    /** 서버 히트 확인 — 클라이언트에서 라인 트레이스 히트 결과를 전송 */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ConfirmHit(AActor* HitActor, FVector_NetQuantize HitLocation, FVector_NetQuantize HitNormal);

    /** 히트 이펙트 — 모든 클라이언트에 표시 */
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayHitEffect(FVector_NetQuantize HitLocation, FVector_NetQuantize HitNormal);

    /** 피격 애니메이션 — 모든 클라이언트에서 재생 */
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayHitReact();

    /** 사망 연출 — 래그돌 + 입력 차단 */
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnDeath();

    /** 리스폰 연출 — 래그돌 해제 + 입력 복구 */
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_Respawn();

    /** 사망 처리 — 서버 전용, 래그돌 → 3초 후 리스폰 */
    void OnDeath();

    /** 인벤토리 풀 등 서버→클라이언트 알림 */
    UFUNCTION(Client, Reliable)
    void Client_ShowNotification(const FString& Message);

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

    // === Combat (Day 8) ===

    UPROPERTY(EditAnywhere, Category = "Combat")
    TSubclassOf<UGameplayAbility> GA_FireClass;

    UPROPERTY(EditAnywhere, Category = "Combat")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditAnywhere, Category = "Combat")
    float FireRange = 5000.f;

    /** 피격 시 0.2초간 오버레이할 반투명 머티리얼 (에디터에서 M_HitOverlay 할당) */
    UPROPERTY(EditAnywhere, Category = "Combat")
    TObjectPtr<UMaterialInterface> HitOverlayMaterial;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> IA_Fire;

    void OnFirePressed();

    // === 조준 (Day 8) ===

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> IA_Aim;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    bool bIsAiming = false;

    UPROPERTY(EditAnywhere, Category = "Combat")
    float DefaultArmLength = 300.f;

    UPROPERTY(EditAnywhere, Category = "Combat")
    float AimArmLength = 15.f;

    UPROPERTY(EditAnywhere, Category = "Combat")
    float DefaultFOV = 90.f;

    UPROPERTY(EditAnywhere, Category = "Combat")
    float AimFOV = 60.f;

    UPROPERTY(EditAnywhere, Category = "Combat")
    FVector DefaultSocketOffset = FVector(0.f, 0.f, 0.f);

    UPROPERTY(EditAnywhere, Category = "Combat")
    FVector AimSocketOffset = FVector(0.f, 30.f, 70.f);

    void OnAimToggled();

    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> CrosshairWidgetClass;

    UPROPERTY()
    TObjectPtr<UUserWidget> CrosshairWidget;

    // === 인터랙션 ===

    /** 라인 트레이스 최대 거리 (cm) */
    UPROPERTY(EditAnywhere, Category = "Interaction")
    float InteractionDistance = 300.f;

    /** 서버 픽업 검증 최대 거리 (cm) — 네트워크 지연 보상 포함 */
    UPROPERTY(EditAnywhere, Category = "Interaction")
    float MaxPickupDistance = 400.f;

    /** 피격 오버레이 머티리얼 표시 지속 시간 (초) */
    UPROPERTY(EditAnywhere, Category = "Combat")
    float HitOverlayDuration = 0.2f;

    /** 사망 후 액터 제거까지 대기 시간 (초) */
    UPROPERTY(EditAnywhere, Category = "Combat")
    float DeathLifeSpan = 6.f;

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
