// Copyright Project EXFIL. All Rights Reserved.

#include "EXFILCharacter.h"
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GAS/SurvivalAttributeSet.h"
#include "Inventory/InventoryComponent.h"
#include "Crafting/CraftingComponent.h"
#include "Data/Equipment/EquipmentComponent.h"
#include "UI/InventoryViewModel.h"
#include "UI/InventoryPanelWidget.h"
#include "UI/CraftingPanelWidget.h"
#include "GAS/SurvivalViewModel.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EngineUtils.h"
#include "World/WorldItem.h"
#include "DrawDebugHelpers.h"
#include "GameplayEffect.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/EXFILLog.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"

AEXFILCharacter::AEXFILCharacter()
{
    // Day 6: 리플리케이션 활성화 (ACharacter 기본값이 true이지만 명시)
    bReplicates = true;

    InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
    CraftingComponent  = CreateDefaultSubobject<UCraftingComponent>(TEXT("CraftingComponent"));
    EquipmentComponent = CreateDefaultSubobject<UEquipmentComponent>(TEXT("EquipmentComponent"));

    // GAS — AttributeSet은 반드시 생성자에서 CreateDefaultSubobject
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    SurvivalAttributes = CreateDefaultSubobject<USurvivalAttributeSet>(TEXT("SurvivalAttributes"));
}

UAbilitySystemComponent* AEXFILCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AEXFILCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // 서버 권위 초기화
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);

        // GA_Fire를 ASC에 부여 (서버)
        if (GA_FireClass)
        {
            FGameplayAbilitySpec FireSpec(GA_FireClass, 1, INDEX_NONE, this);
            AbilitySystemComponent->GiveAbility(FireSpec);
        }
    }
}

void AEXFILCharacter::BeginPlay()
{
    Super::BeginPlay();

    // SpringArm 기본값 캐싱 (에디터 설정값 보존)
    if (USpringArmComponent* SpringArm = GetCameraBoom())
    {
        DefaultArmLength = SpringArm->TargetArmLength;
        DefaultSocketOffset = SpringArm->SocketOffset;
    }

    // 단독 세션(Standalone)에서 PossessedBy 없이 BeginPlay만 오는 경우 대비
    if (AbilitySystemComponent && !AbilitySystemComponent->GetOwnerActor())
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);

        // Standalone: GA_Fire 부여
        if (GA_FireClass)
        {
            FGameplayAbilitySpec FireSpec(GA_FireClass, 1, INDEX_NONE, this);
            AbilitySystemComponent->GiveAbility(FireSpec);
        }
    }

    // ===== 서버 전용: 아이템 초기 지급 =====
    if (HasAuthority())
    {
        if (InventoryComponent)
        {
            InventoryComponent->TryAddItemByID(FName("Bandage"),    3);  // 1x1, 스택 3
            InventoryComponent->TryAddItemByID(FName("Pistol"), 2);         // 2x1
            InventoryComponent->TryAddItemByID(FName("BodyArmor"));      // 2x3
            InventoryComponent->TryAddItemByID(FName("Painkillers"), 5);  // 1x1, 스택 5
            InventoryComponent->TryAddItemByID(FName("Medkit"));         // 1x2
        }
    }

    // ===== 클라이언트 전용: UI 생성 + 델리게이트 바인딩 =====
    if (IsLocallyControlled())
    {

        // 1. InventoryViewModel
        if (InventoryComponent)
        {
            InventoryViewModel = NewObject<UInventoryViewModel>(this);
            InventoryViewModel->Initialize(InventoryComponent);
        }

        // 2. InventoryPanelWidget 생성 및 연결
        if (InventoryViewModel && InventoryPanelWidgetClass)
        {
            APlayerController* PC = Cast<APlayerController>(GetController());
            if (PC)
            {
                InventoryPanelWidget = CreateWidget<UInventoryPanelWidget>(PC, InventoryPanelWidgetClass);
                if (InventoryPanelWidget)
                {
                    InventoryPanelWidget->SetViewModel(InventoryViewModel);

                    // CraftingPanel이 WBP 안에 있으면 컴포넌트 연결
                    if (UCraftingPanelWidget* CraftingPanel = InventoryPanelWidget->GetCraftingPanel())
                    {
                        CraftingPanel->SetupCrafting(CraftingComponent, InventoryComponent);
                    }
                }
            }
        }

        // 3. SurvivalViewModel — Standalone에서는 BeginPlay에서 ASC가 이미 초기화됨
        InitializeViewModels();

        // 4. 크로스헤어 위젯 생성 (초기에 숨김)
        if (CrosshairWidgetClass)
        {
            APlayerController* CrosshairPC = Cast<APlayerController>(GetController());
            if (CrosshairPC)
            {
                CrosshairWidget = CreateWidget<UUserWidget>(CrosshairPC, CrosshairWidgetClass);
                if (CrosshairWidget)
                {
                    CrosshairWidget->AddToViewport();
                    CrosshairWidget->SetVisibility(ESlateVisibility::Collapsed);
                }
            }
        }
    }
}

// ========== 인터랙션 입력 ==========

void AEXFILCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput =
            Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (IA_Interact)
        {
            EnhancedInput->BindAction(
                IA_Interact, ETriggerEvent::Started, this,
                &AEXFILCharacter::OnInteractPressed);
        }

        if (IA_Fire)
        {
            EnhancedInput->BindAction(
                IA_Fire, ETriggerEvent::Started, this,
                &AEXFILCharacter::OnFirePressed);
        }

        if (IA_Aim)
        {
            EnhancedInput->BindAction(
                IA_Aim, ETriggerEvent::Started, this,
                &AEXFILCharacter::OnAimToggled);
        }
    }
}

void AEXFILCharacter::OnInteractPressed()
{
    AWorldItem* Target = TraceForWorldItem();
    if (!Target) return;

    Server_RequestPickupItem(Target);
}

AWorldItem* AEXFILCharacter::TraceForWorldItem() const
{
    // 월드의 모든 WorldItem 중 InteractionDistance 이내에서 가장 가까운 것 반환
    AWorldItem* NearestItem = nullptr;
    float NearestDist = InteractionDistance;

    for (TActorIterator<AWorldItem> It(GetWorld()); It; ++It)
    {
        const float Dist = FVector::Dist(GetActorLocation(), It->GetActorLocation());
        if (Dist < NearestDist)
        {
            NearestDist = Dist;
            NearestItem = *It;
        }
    }

    return NearestItem;
}

// ========== 픽업 Server RPC ==========

bool AEXFILCharacter::Server_RequestPickupItem_Validate(AWorldItem* TargetItem)
{
    return TargetItem != nullptr;
}

void AEXFILCharacter::Server_RequestPickupItem_Implementation(AWorldItem* TargetItem)
{
    ExecutePickup(TargetItem);
}

void AEXFILCharacter::ExecutePickup(AWorldItem* TargetItem)
{
    if (!HasAuthority()) return;

    // 1. WorldItem 유효성 검증 (다른 플레이어가 이미 주웠을 수 있음)
    if (!IsValid(TargetItem)) return;

    // 2. 거리 검증 (치트 방지, 네트워크 지연 보상으로 여유값 부여)
    const float Distance = FVector::Dist(GetActorLocation(), TargetItem->GetActorLocation());
    if (Distance > MaxPickupDistance)
    {
        UE_LOG(LogEXFIL, Warning, TEXT("ExecutePickup: 거리 초과 — %.1fcm > %.1fcm"),
            Distance, MaxPickupDistance);
        return;
    }

    // 3. 인벤토리에 추가 시도
    UInventoryComponent* Inventory = FindComponentByClass<UInventoryComponent>();
    if (!Inventory) return;

    const bool bAdded = Inventory->TryAddItemByID(
        TargetItem->GetItemDataID(), TargetItem->GetStackCount());

    if (!bAdded)
    {
        Client_ShowNotification(FString::Printf(
            TEXT("Inventory Full — Cannot pick up '%s'"),
            *TargetItem->GetItemDataID().ToString()));
        return;
    }

    // 4. WorldItem 제거 (bReplicates=true Actor의 Destroy는 모든 클라이언트에 전파)
    TargetItem->Destroy();
}

// ========== Dedicated Server: 클라이언트 ASC 초기화 ==========

void AEXFILCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    // 클라이언트에서 ASC 초기화
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }

    // ViewModel 초기화 (클라이언트 전용)
    if (IsLocallyControlled())
    {
        InitializeViewModels();
    }
}

void AEXFILCharacter::InitializeViewModels()
{
    if (SurvivalViewModel) return; // 이미 초기화됨
    if (!AbilitySystemComponent) return;

    // ASC에 AttributeSet이 있는지 확인
    const USurvivalAttributeSet* AttrSet = AbilitySystemComponent->GetSet<USurvivalAttributeSet>();
    if (!AttrSet)
    {
        UE_LOG(LogEXFIL, Warning, TEXT("InitializeViewModels: AttributeSet still null"));
        return;
    }

    SurvivalViewModel = NewObject<USurvivalViewModel>(this);
    SurvivalViewModel->InitializeWithASC(AbilitySystemComponent);

    // StatEntry 바인딩
    if (InventoryPanelWidget)
    {
        InventoryPanelWidget->BindStatsToViewModel(SurvivalViewModel);
    }

}

// ========== Combat (Day 8) ==========

bool AEXFILCharacter::IsInventoryUIVisible() const
{
    return InventoryPanelWidget &&
           InventoryPanelWidget->IsInViewport() &&
           InventoryPanelWidget->GetVisibility() == ESlateVisibility::Visible;
}

void AEXFILCharacter::OnFirePressed()
{
    if (IsInventoryUIVisible()) return;
    if (!bIsAiming) return;

    if (AbilitySystemComponent && GA_FireClass)
    {
        FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromClass(GA_FireClass);
        if (Spec)
        {
            AbilitySystemComponent->TryActivateAbility(Spec->Handle);
        }
    }
}

void AEXFILCharacter::OnAimToggled()
{
    if (IsInventoryUIVisible()) return;

    bIsAiming = !bIsAiming;

    // Spring Arm 길이 + 어깨 너머 오프셋 조절
    if (USpringArmComponent* SpringArm = GetCameraBoom())
    {
        SpringArm->TargetArmLength = bIsAiming ? AimArmLength : DefaultArmLength;
        SpringArm->SocketOffset = bIsAiming ? AimSocketOffset : DefaultSocketOffset;
    }

    // 카메라 FOV 조절
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (APlayerCameraManager* CM = PC->PlayerCameraManager)
        {
            CM->SetFOV(bIsAiming ? AimFOV : DefaultFOV);
        }
    }

    // 조준 시: 캐릭터가 카메라(컨트롤러) 방향을 즉시 따라감
    // 비조준 시: 이동 방향으로 회전 (기존 3인칭 동작)
    if (UCharacterMovementComponent* CMC = GetCharacterMovement())
    {
        bUseControllerRotationYaw = bIsAiming;
        CMC->bOrientRotationToMovement = !bIsAiming;
    }

    // 크로스헤어 토글
    if (CrosshairWidget)
    {
        CrosshairWidget->SetVisibility(
            bIsAiming ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

bool AEXFILCharacter::Server_ConfirmHit_Validate(
    AActor* HitActor, FVector_NetQuantize HitLocation, FVector_NetQuantize HitNormal)
{
    return HitActor != nullptr;
}

void AEXFILCharacter::Server_ConfirmHit_Implementation(
    AActor* HitActor, FVector_NetQuantize HitLocation, FVector_NetQuantize HitNormal)
{
    // 서버 라인 트레이스 재검증
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    FHitResult VerifyHit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * FireRange;
    GetWorld()->LineTraceSingleByChannel(VerifyHit, CameraLocation, TraceEnd, ECC_Pawn, Params);

    if (VerifyHit.GetActor() != HitActor)
    {
        UE_LOG(LogEXFIL, Warning, TEXT("Server_ConfirmHit: hit verification mismatch"));
        return;
    }

    // 데미지 GE 적용
    AEXFILCharacter* TargetChar = Cast<AEXFILCharacter>(HitActor);
    if (!TargetChar || !TargetChar->GetAbilitySystemComponent())
    {
        return;
    }

    if (DamageEffectClass && AbilitySystemComponent)
    {
        FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
        Context.AddSourceObject(this);
        Context.AddHitResult(VerifyHit);

        FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
            DamageEffectClass, 1.f, Context);

        if (SpecHandle.IsValid())
        {
            AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(
                *SpecHandle.Data.Get(), TargetChar->GetAbilitySystemComponent());
        }
    }

    Multicast_PlayHitEffect(HitLocation, HitNormal);
    TargetChar->Multicast_PlayHitReact();
}

void AEXFILCharacter::Multicast_PlayHitEffect_Implementation(
    FVector_NetQuantize HitLocation, FVector_NetQuantize HitNormal)
{
    // TODO: 파티클/사운드 이펙트로 교체
}

void AEXFILCharacter::Multicast_PlayHitReact_Implementation()
{
    if (!HitOverlayMaterial) return;

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->SetOverlayMaterial(HitOverlayMaterial);

        FTimerHandle ResetTimer;
        GetWorldTimerManager().SetTimer(ResetTimer, [WeakThis = TWeakObjectPtr<AEXFILCharacter>(this)]()
        {
            if (AEXFILCharacter* Self = WeakThis.Get())
            {
                if (USkeletalMeshComponent* M = Self->GetMesh())
                {
                    M->SetOverlayMaterial(nullptr);
                }
            }
        }, HitOverlayDuration, false);
    }
}

void AEXFILCharacter::OnDeath()
{
    if (!HasAuthority()) return;

    Multicast_OnDeath();
    SetLifeSpan(DeathLifeSpan);
}

void AEXFILCharacter::Multicast_OnDeath_Implementation()
{
    // 래그돌
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->SetSimulatePhysics(true);
        MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
    }

    // 캡슐 콜리전 끄기
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 이동 중지
    if (UCharacterMovementComponent* CMC = GetCharacterMovement())
    {
        CMC->DisableMovement();
    }

    // 본인 클라이언트: 조준 해제 + 입력 차단
    if (IsLocallyControlled())
    {
        if (bIsAiming)
        {
            OnAimToggled();
        }

        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            DisableInput(PC);
        }
    }
}

void AEXFILCharacter::Client_ShowNotification_Implementation(const FString& Message)
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, Message);
    }
}
