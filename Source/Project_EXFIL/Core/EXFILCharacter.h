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
class UGameplayAbility;
class UGameplayEffect;
class UInputAction;
class UInventoryComponent;
class UMaterialInterface;
class USpringArmComponent;
class USurvivalAttributeSet;
struct FInputActionValue;

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
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	UEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting")
	UCraftingComponent* GetCraftingComponent() const { return CraftingComponent; }

	void InitAbilityActorInfoForClient();

	
	bool IsInventoryUIVisible() const;

	
	UFUNCTION(Server, Reliable)
	void Server_ConfirmHit(AActor* HitActor, FVector_NetQuantize HitLocation, FVector_NetQuantize HitNormal);

	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitEffect(FVector_NetQuantize HitLocation, FVector_NetQuantize HitNormal);

	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitReact();

	
	void OnDeath();

	
	void Server_PrepareRespawn();

	
	void Server_FinishRespawn();

	
	void Server_HideDeadBody();

	
	UFUNCTION(Client, Reliable)
	void Client_ShowNotification(const FString& Message);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<USurvivalAttributeSet> SurvivalAttributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCraftingComponent> CraftingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEquipmentComponent> EquipmentComponent;

	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<UGameplayAbility> GA_FireClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float FireRange = 5000.f;

	
	UPROPERTY(EditAnywhere, Category = "Combat")
	TObjectPtr<UMaterialInterface> HitOverlayMaterial;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> IA_Fire;

	void OnFirePressed();

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

	
	UPROPERTY(EditAnywhere, Category = "Interaction")
	float InteractionDistance = 300.f;

	
	UPROPERTY(EditAnywhere, Category = "Interaction")
	float MaxPickupDistance = 400.f;

	
	UPROPERTY(EditAnywhere, Category = "Combat")
	float HitOverlayDuration = 0.2f;

	
	UPROPERTY(EditAnywhere, Category = "Combat")
	float CorpseVisibleDuration = 2.5f;

	
	UPROPERTY(EditAnywhere, Category = "Combat")
	float HiddenRespawnDelay = 2.5f;

	
	UPROPERTY(EditAnywhere, Category = "Combat")
	float RespawnRevealDelay = 0.3f;

	
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> IA_Interact;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void OnInteractPressed();

	
	AWorldItem* TraceForWorldItem() const;
	UFUNCTION(Server, Reliable)
	void Server_RequestPickupItem(AWorldItem* TargetItem);

	void ExecutePickup(AWorldItem* TargetItem);

	
	FTimerHandle HideCorpseTimerHandle;

	
	FTimerHandle RespawnPrepareTimerHandle;

	
	FTimerHandle RespawnRevealTimerHandle;

	
	UPROPERTY(Replicated)
	FVector_NetQuantize PendingRespawnLocation = FVector::ZeroVector;

	
	UPROPERTY(Replicated)
	FRotator PendingRespawnRotation = FRotator::ZeroRotator;

	
	UPROPERTY(ReplicatedUsing = OnRep_RespawnPhase)
	ERespawnPhase RespawnPhase = ERespawnPhase::Alive;

	UFUNCTION()
	void OnRep_RespawnPhase();

	
	void ApplyDeadState();

	
	void ApplyHiddenDeadState();

	
	void ApplyRespawningState();

	
	void ApplyAliveState();

	
	void SnapToPendingRespawnTransform();
};
