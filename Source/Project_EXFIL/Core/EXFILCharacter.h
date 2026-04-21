// Copyright Project EXFIL. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Project_EXFILCharacter.h"
#include "EXFILCharacter.generated.h"

class AWorldItem;
class UAbilitySystemComponent;
class UCraftingComponent;
class UEquipmentComponent;
class UEXFILUIManager;
class UGameplayAbility;
class UGameplayEffect;
class UInputAction;
class UInventoryComponent;
class UInventoryViewModel;
class UMaterialInterface;
class USpringArmComponent;
class USurvivalAttributeSet;
class USurvivalViewModel;
struct FInputActionValue;

/** 죽음/리스폰 phase. late-joiner는 이 값을 복제받아 현재 상태를 복구한다. */
UENUM(BlueprintType)
enum class ERespawnPhase : uint8
{
	Alive,
	Dead,
	HiddenDead,
	Respawning
};

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

	// === Equipment ===
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	UEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	// === Combat ===

	/** 인벤토리 UI가 현재 보이는지 반환 (GA_Fire에서 참조) */
	bool IsInventoryUIVisible() const;

	/** 서버 히트 확인 후 클라이언트에 라인트레이스 히트 결과를 전송 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ConfirmHit(AActor* HitActor, FVector_NetQuantize HitLocation, FVector_NetQuantize HitNormal);

	/** 히트 이펙트를 모든 클라이언트에 표시 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitEffect(FVector_NetQuantize HitLocation, FVector_NetQuantize HitNormal);

	/** 피격 애니메이션을 모든 클라이언트에 재생 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitReact();

	/** 죽음 처리 시작. 서버가 phase를 변경하고 respawn 타이머를 건다. */
	void OnDeath();

	/** 서버가 리스폰 위치를 결정하고 Respawning phase로 전환한다. */
	void Server_PrepareRespawn();

	/** 서버가 최종적으로 Alive phase로 전환한다. */
	void Server_FinishRespawn();

	/** 서버가 시체를 숨기고 HiddenDead phase로 전환한다. */
	void Server_HideDeadBody();

	/** 인벤토리 부족 등 서버 -> 클라 알림 */
	UFUNCTION(Client, Reliable)
	void Client_ShowNotification(const FString& Message);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	/** SurvivalViewModel 생성 + ASC 바인딩 + UI 연결 */
	void InitializeViewModels();

	// === Inventory ===
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY()
	TObjectPtr<UInventoryViewModel> InventoryViewModel;

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

	// === Combat ===

	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<UGameplayAbility> GA_FireClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float FireRange = 5000.f;

	/** 피격 시 잠깐 표시할 오버레이 머티리얼 */
	UPROPERTY(EditAnywhere, Category = "Combat")
	TObjectPtr<UMaterialInterface> HitOverlayMaterial;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> IA_Fire;

	void OnFirePressed();

	// === Aim ===

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

	/** 인벤토리 토글 처리 */
	void OnToggleInventory();

	// === Interaction ===

	/** 월드 아이템 탐색 거리 (cm) */
	UPROPERTY(EditAnywhere, Category = "Interaction")
	float InteractionDistance = 300.f;

	/** 서버 픽업 검증 최대 거리 (cm) */
	UPROPERTY(EditAnywhere, Category = "Interaction")
	float MaxPickupDistance = 400.f;

	/** 피격 오버레이 표시 시간 (초) */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float HitOverlayDuration = 0.2f;

	/** 시체를 보여주는 시간 (초) */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float CorpseVisibleDuration = 2.5f;

	/** 시체를 숨긴 뒤 실제 리스폰 준비 전까지 대기 시간 (초) */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float HiddenRespawnDelay = 2.5f;

	/** Respawning phase 후 다시 보이기까지 짧은 지연 (초) */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float RespawnRevealDelay = 0.3f;

	/** 상호작용 InputAction */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> IA_Interact;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void OnInteractPressed();

	/** 카메라 Forward 방향으로 라인트레이스하여 AWorldItem 탐색 */
	AWorldItem* TraceForWorldItem() const;

	// === Pickup Server RPC ===
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestPickupItem(AWorldItem* TargetItem);

	void ExecutePickup(AWorldItem* TargetItem);

	/** Dead -> HiddenDead 전환 타이머 */
	FTimerHandle HideCorpseTimerHandle;

	/** HiddenDead -> Respawning 전환 타이머 */
	FTimerHandle RespawnPrepareTimerHandle;

	/** Respawning -> Alive 전환 타이머 */
	FTimerHandle RespawnRevealTimerHandle;

	/** Respawning phase에서 사용할 authoritative respawn 위치 */
	UPROPERTY(Replicated)
	FVector_NetQuantize PendingRespawnLocation = FVector::ZeroVector;

	/** Respawning phase에서 사용할 authoritative respawn 회전 */
	UPROPERTY(Replicated)
	FRotator PendingRespawnRotation = FRotator::ZeroRotator;

	/** 서버 authoritative respawn phase. late-joiner는 OnRep로 상태를 복원한다. */
	UPROPERTY(ReplicatedUsing = OnRep_RespawnPhase)
	ERespawnPhase RespawnPhase = ERespawnPhase::Alive;

	UFUNCTION()
	void OnRep_RespawnPhase();

	/** Dead 상태 비주얼/물리/입력 적용 */
	void ApplyDeadState();

	/** HiddenDead 상태 비주얼/물리 적용 */
	void ApplyHiddenDeadState();

	/** Respawning 상태 비주얼/물리 적용 */
	void ApplyRespawningState();

	/** Alive 상태 비주얼/물리/입력 복구 */
	void ApplyAliveState();

	/** 서버가 정한 respawn transform으로 즉시 스냅 */
	void SnapToPendingRespawnTransform();
};
