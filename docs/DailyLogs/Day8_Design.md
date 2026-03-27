# Day 8 설계 지시서: 라인 트레이스 슈팅 + 코드 클린업 + 시연 준비

> 채팅 탭(Opus)이 상단 "설계 지시서" 섹션을 작성 → 수현이 검토 후 Docs/DailyLogs/Day8_Design.md로 저장
> 프로젝트 연결 탭이 하단 "완료 보고"를 작성 → 수현이 채팅 탭에 전달

---

## ⚠️ Day 7 반영사항

### Day 1~7 완료된 전체 시스템

| Day | 시스템 | 상태 |
|-----|--------|------|
| Day 1 | Inventory Core (1D TArray GridSlots, 멀티셀, 회전) | ✅ |
| Day 2 | MVVM + Common UI (ViewModel, 드래그&드롭) | ✅ |
| Day 3 | Data-Driven (UItemDataSubsystem, DT_ItemData 40종, DT_CraftingRecipe) | ✅ |
| Day 4 | GAS + Equipment (SurvivalAttributeSet 8속성, EquipmentComponent 6슬롯) | ✅ |
| Day 5 | Crafting + UI 개편 (CraftingComponent, GA_Craft, WidgetSwitcher) | ✅ |
| Day 6 | Replication 1 (Dedicated Server, Server RPC 12+, COND_OwnerOnly) | ✅ |
| Day 7 | Replication 2 (WorldItem 픽업/드롭, 컨텍스트 메뉴, SurvivalViewModel) | ✅ |

### Day 8 스코프 결정

**추가:** 라인 트레이스 슈팅 + 네트워크 데미지 (낙원 공고 "슈팅/액션 네트워크 코드" 직격)
**축소:** 프로파일링은 시연 중 stat unit 스크린샷으로 대체
**스킵:** 비주얼 폴리시(그리드 선, 아이콘 해상도, 캐릭터 외형, 애니메이션)

---

## 📋 Day 8 설계 지시서

**작성자:** 채팅 탭 (Opus 4.6)
**WBS 분류:** Combat + Polish
**목표 시간:** 15h
**날짜:** 2026-03-24
**권장 모델:** Sonnet 4.6 (GAS 파이프라인 이미 있으므로 패턴 반복)

## 1. 오늘의 목표 (3개 페이즈)

**Phase A: 라인 트레이스 슈팅 시스템 (5~6h)**
- [ ] GA_Fire GameplayAbility
- [ ] 데미지 GE (Instant, Health 감소)
- [ ] 라인 트레이스 히트 판정 (Server RPC)
- [ ] 히트 이펙트 (디버그 시각화)
- [ ] 무기 장착 상태에서만 발사 가능
- [ ] R키 조준 토글 (카메라 줌 + 크로스헤어 + 조준 시에만 발사)
- [ ] 2P PvP 데미지 동작
- [ ] ⭐ 보너스: Lyra 발사 몽타주 마이그레이트 + AnimBP 레이어링 (시간 남을 때만)

**Phase B: 코드 클린업 (3~4h)**
- [ ] 디버그 UE_LOG 정리
- [ ] 첫 PIE 아이콘 찌그러짐 수정
- [ ] 인벤토리 풀 피드백 (화면 메시지)
- [ ] Client_ShowNotification RPC

**Phase C: 시연 준비 + README (5~6h, 수현 직접)**
- [ ] 시연 영상 촬영 (슈팅 포함)
- [ ] GitHub README.md 작성
- [ ] ARCHITECTURE.md 최종 업데이트
- [ ] 최종 커밋 + 태그

---

## 2. Phase A: 라인 트레이스 슈팅 시스템

### 2-1. 전체 흐름

```
[Client] R키 → 조준 모드 토글
  → Spring Arm Length 300→100, FOV 90→60
  → 크로스헤어 위젯 표시
  → bIsAiming = true

[Client] 좌클릭 (조준 모드 + 인벤토리 닫힌 상태)
  → EquipmentComponent에 Weapon 슬롯 장착 여부 확인
  → 장착됨: ASC→TryActivateAbility(GA_Fire)
  → GA_Fire::ActivateAbility()
    → [Client] 카메라 Forward 라인 트레이스 (사정거리)
    → [Client] 히트 결과를 Server RPC로 전송
  → [Server] Server_ConfirmHit(HitActor, HitLocation, HitNormal)
    → 서버에서 라인 트레이스 재검증 (치트 방지)
    → 히트 유효: 데미지 GE를 타겟 ASC에 적용
    → 타겟 SurvivalAttributeSet::Health 감소
    → 타겟 클라이언트 OnRep → StatsBar 갱신
  → [All Clients] 히트 이펙트 재생 (Multicast RPC)
```

### 2-2. GA_Fire — GameplayAbility

```cpp
// GAS/GA_Fire.h
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Fire.generated.h"

UCLASS()
class PROJECT_EXFIL_API UGA_Fire : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Fire();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags) const override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Fire")
    float FireRange = 5000.f;

    UPROPERTY(EditDefaultsOnly, Category = "Fire")
    TSubclassOf<UGameplayEffect> DamageEffectClass;
};
```

### 2-3. GA_Fire — 구현

```cpp
// GAS/GA_Fire.cpp

UGA_Fire::UGA_Fire()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

bool UGA_Fire::CanActivateAbility(/* params */) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
        return false;

    const AActor* AvatarActor = ActorInfo->AvatarActor.Get();
    if (!AvatarActor) return false;

    const UEquipmentComponent* Equipment = AvatarActor->FindComponentByClass<UEquipmentComponent>();
    if (!Equipment) return false;

    return Equipment->HasWeaponEquipped();
}

void UGA_Fire::ActivateAbility(/* params */)
{
    AActor* AvatarActor = ActorInfo->AvatarActor.Get();
    if (!AvatarActor)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    APlayerController* PC = Cast<APlayerController>(ActorInfo->PlayerController.Get());
    if (!PC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    const FVector TraceStart = CameraLocation;
    const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * FireRange;

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(AvatarActor);

    const bool bHit = AvatarActor->GetWorld()->LineTraceSingleByChannel(
        HitResult, TraceStart, TraceEnd, ECC_Visibility, Params);

    // 디버그 라인
    #if ENABLE_DRAW_DEBUG
    DrawDebugLine(
        AvatarActor->GetWorld(), TraceStart, bHit ? HitResult.ImpactPoint : TraceEnd,
        bHit ? FColor::Red : FColor::Green, false, 1.f, 0, 1.f);
    #endif

    if (bHit && HitResult.GetActor())
    {
        AEXFILCharacter* OwnerChar = Cast<AEXFILCharacter>(AvatarActor);
        if (OwnerChar)
        {
            OwnerChar->Server_ConfirmHit(
                HitResult.GetActor(), HitResult.ImpactPoint, HitResult.ImpactNormal);
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
```

### 2-4. EXFILCharacter — 서버 히트 확인 + 데미지 적용

```cpp
// EXFILCharacter.h 추가

UPROPERTY(EditAnywhere, Category = "Combat")
TSubclassOf<UGameplayAbility> GA_FireClass;

UPROPERTY(EditAnywhere, Category = "Combat")
TSubclassOf<UGameplayEffect> DamageEffectClass;

UPROPERTY(EditAnywhere, Category = "Combat")
float FireRange = 5000.f;

UPROPERTY(EditAnywhere, Category = "Input")
TObjectPtr<UInputAction> IA_Fire;

void OnFirePressed();

UFUNCTION(Server, Reliable, WithValidation)
void Server_ConfirmHit(AActor* HitActor, FVector_NetQuantize HitLocation, FVector_NetQuantize HitNormal);

UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayHitEffect(FVector_NetQuantize HitLocation, FVector_NetQuantize HitNormal);
```

```cpp
// EXFILCharacter.cpp

void AEXFILCharacter::OnFirePressed()
{
    if (bIsInventoryOpen) return;

    if (AbilitySystemComponent && GA_FireClass)
    {
        FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromClass(GA_FireClass);
        if (Spec)
        {
            AbilitySystemComponent->TryActivateAbility(Spec->Handle);
        }
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
    FVector CameraLocation;
    FRotator CameraRotation;
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    FHitResult VerifyHit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * FireRange;
    GetWorld()->LineTraceSingleByChannel(VerifyHit, CameraLocation, TraceEnd, ECC_Visibility, Params);

    if (VerifyHit.GetActor() != HitActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("Server hit verification failed"));
        return;
    }

    // 데미지 GE 적용
    AEXFILCharacter* TargetChar = Cast<AEXFILCharacter>(HitActor);
    if (!TargetChar || !TargetChar->GetAbilitySystemComponent()) return;

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
}

void AEXFILCharacter::Multicast_PlayHitEffect_Implementation(
    FVector_NetQuantize HitLocation, FVector_NetQuantize HitNormal)
{
    #if ENABLE_DRAW_DEBUG
    DrawDebugSphere(GetWorld(), HitLocation, 15.f, 8, FColor::Red, false, 2.f);
    #endif
}
```

### 2-5. 데미지 GameplayEffect — GE_Damage

BP에서 생성 (C++에서 GE 클래스 안 만드는 프로젝트 규칙):

| 설정 | 값 |
|------|-----|
| Duration Policy | Instant |
| Modifier Attribute | SurvivalAttributeSet.Health |
| Modifier Op | Additive |
| Modifier Magnitude | Scalable Float: **-20.0** |

### 2-6. EquipmentComponent — HasWeaponEquipped

```cpp
// EquipmentComponent.h
UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
bool HasWeaponEquipped() const;

// EquipmentComponent.cpp
bool UEquipmentComponent::HasWeaponEquipped() const
{
    for (const FEquipmentSlotData& SlotData : ReplicatedSlots)
    {
        if ((SlotData.SlotType == EEquipmentSlot::Weapon1 ||
             SlotData.SlotType == EEquipmentSlot::Weapon2) &&
            !SlotData.IsEmpty())
        {
            return true;
        }
    }
    return false;
}
```

### 2-7. 입력 바인딩

```cpp
// SetupPlayerInputComponent
if (IA_Fire)
{
    EnhancedInput->BindAction(
        IA_Fire, ETriggerEvent::Started, this,
        &AEXFILCharacter::OnFirePressed);
}
```

### 2-8. GA_Fire를 ASC에 부여

```cpp
// PossessedBy (서버)
if (AbilitySystemComponent && GA_FireClass)
{
    FGameplayAbilitySpec FireSpec(GA_FireClass, 1, INDEX_NONE, this);
    AbilitySystemComponent->GiveAbility(FireSpec);
}
```

### 2-9. 사망 처리 (간단 버전)

```cpp
// SurvivalAttributeSet.cpp — PostGameplayEffectExecute
if (Data.EvaluatedData.Attribute == GetHealthAttribute())
{
    const float NewHealth = FMath::Clamp(GetHealth(), 0.f, GetMaxHealth());
    SetHealth(NewHealth);

    if (NewHealth <= 0.f)
    {
        AActor* OwnerActor = GetOwningAbilitySystemComponent()->GetAvatarActor();
        if (AEXFILCharacter* Character = Cast<AEXFILCharacter>(OwnerActor))
        {
            Character->OnDeath();
        }
    }
}
```

```cpp
// EXFILCharacter
void AEXFILCharacter::OnDeath()
{
    if (!HasAuthority()) return;

    // 3초 후 HP 리셋
    FTimerHandle RespawnTimer;
    GetWorldTimerManager().SetTimer(RespawnTimer, [this]()
    {
        if (AbilitySystemComponent)
        {
            USurvivalAttributeSet* AttrSet = const_cast<USurvivalAttributeSet*>(
                AbilitySystemComponent->GetSet<USurvivalAttributeSet>());
            if (AttrSet)
            {
                AttrSet->SetHealth(AttrSet->GetMaxHealth());
            }
        }
    }, 3.f, false);
}
```

### 2-10. R키 조준 토글 + 크로스헤어

**EXFILCharacter.h 추가:**
```cpp
// === 조준 ===
UPROPERTY(EditAnywhere, Category = "Input")
TObjectPtr<UInputAction> IA_Aim;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
bool bIsAiming = false;

UPROPERTY(EditAnywhere, Category = "Combat")
float DefaultArmLength = 300.f;

UPROPERTY(EditAnywhere, Category = "Combat")
float AimArmLength = 100.f;

UPROPERTY(EditAnywhere, Category = "Combat")
float DefaultFOV = 90.f;

UPROPERTY(EditAnywhere, Category = "Combat")
float AimFOV = 60.f;

void OnAimToggled();

UPROPERTY(EditAnywhere, Category = "UI")
TSubclassOf<UUserWidget> CrosshairWidgetClass;

UPROPERTY()
TObjectPtr<UUserWidget> CrosshairWidget;
```

**입력 바인딩:**
```cpp
// SetupPlayerInputComponent에 추가
if (IA_Aim)
{
    EnhancedInput->BindAction(
        IA_Aim, ETriggerEvent::Started, this,
        &AEXFILCharacter::OnAimToggled);
}
```

**OnAimToggled 구현:**
```cpp
void AEXFILCharacter::OnAimToggled()
{
    if (bIsInventoryOpen) return;

    bIsAiming = !bIsAiming;

    // Spring Arm 길이 조절
    if (USpringArmComponent* SpringArm = FindComponentByClass<USpringArmComponent>())
    {
        SpringArm->TargetArmLength = bIsAiming ? AimArmLength : DefaultArmLength;
    }

    // 카메라 FOV 조절
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (APlayerCameraManager* CM = PC->PlayerCameraManager)
        {
            CM->SetFOV(bIsAiming ? AimFOV : DefaultFOV);
        }
    }

    // 크로스헤어 토글
    if (CrosshairWidget)
    {
        CrosshairWidget->SetVisibility(
            bIsAiming ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}
```

**OnFirePressed 수정 — 조준 시에만 발사:**
```cpp
void AEXFILCharacter::OnFirePressed()
{
    if (bIsInventoryOpen) return;
    if (!bIsAiming) return;  // 조준 모드가 아니면 발사 불가

    // 기존 GA_Fire 로직 그대로...
}
```

**크로스헤어 위젯 생성 (BeginPlay, IsLocallyControlled 가드 안):**
```cpp
if (CrosshairWidgetClass)
{
    CrosshairWidget = CreateWidget<UUserWidget>(
        Cast<APlayerController>(GetController()), CrosshairWidgetClass);
    if (CrosshairWidget)
    {
        CrosshairWidget->AddToViewport();
        CrosshairWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}
```

**인벤토리 열 때 조준 자동 해제:**
```cpp
// 인벤토리 토글 로직에서 인벤토리 열 때:
if (bIsAiming)
{
    OnAimToggled(); // 조준 해제
}
```

**WBP_Crosshair (에디터에서 생성):**
```
[Root] CanvasPanel
└── Image_H (Anchor: 0.5,0.5 — 가로 막대 20×2, 흰색, Alignment 0.5,0.5)
└── Image_V (Anchor: 0.5,0.5 — 세로 막대 2×20, 흰색, Alignment 0.5,0.5)
```

---

## 2-BONUS. Phase A 보너스: Lyra 발사 몽타주 (시간 남을 때만)

> **전제조건:** Phase A 슈팅 시스템이 PIE 2P에서 안정적으로 동작 확인된 후에만 시도.
> 안 되면 즉시 롤백하고 디버그 라인으로 시연.

### 목표

현재: 좌클릭 → DrawDebugLine만 표시
보너스: 좌클릭 → 캐릭터가 총 쏘는 애니메이션 재생 + DrawDebugLine

### 에셋 마이그레이트 (에디터 작업)

1. Lyra 프로젝트를 별도로 열기 (Epic Games Launcher에서 Lyra Starter Game)
2. Content Browser에서 아래 에셋 찾기:
   - 발사 몽타주: `AM_MM_Pistol_Fire` 또는 유사 이름
   - Aim Offset: `AO_MM_Pistol_Idle` 또는 유사 이름 (선택)
3. 우클릭 → Asset Actions → Migrate → EXFIL 프로젝트의 Content 폴더로
4. 의존성 에셋(스켈레톤 등)이 딸려오면 Manny/Quinn 스켈레톤 기준인지 확인
   - 현재 ThirdPerson 템플릿도 동일 스켈레톤이므로 호환

### AnimBP 수정 — Upper Body 레이어링

```
기존 AnimBP 구조:
Locomotion State Machine → Output Pose

수정 후:
Locomotion State Machine → Layered Blend per Bone → Output Pose
                            ↑
                     Slot 'UpperBody' (DefaultSlot 또는 커스텀)
```

```cpp
// AnimBP에서 Layered Blend per Bone 노드 설정:
// - Bone Name: spine_01 (또는 spine_02)
// - Blend Depth: 1
// - Mesh Space Rotation Blend: true
```

이건 AnimBP 에디터에서 수동 작업. C++ 수정 아님.

### GA_Fire에서 몽타주 재생

```cpp
// GA_Fire::ActivateAbility 내부, EndAbility 호출 전에 추가:

// 몽타주 재생 (클라이언트 로컬)
if (UAnimInstance* AnimInstance = AvatarActor->FindComponentByClass<USkeletalMeshComponent>()->GetAnimInstance())
{
    if (FireMontage) // UPROPERTY(EditDefaultsOnly) UAnimMontage* FireMontage;
    {
        AnimInstance->Montage_Play(FireMontage, 1.f);
    }
}
```

```cpp
// GA_Fire.h에 추가
UPROPERTY(EditDefaultsOnly, Category = "Fire")
TObjectPtr<UAnimMontage> FireMontage = nullptr;
```

FireMontage가 nullptr이면 기존대로 디버그 라인만 표시 — 폴백 안전.

### 네트워크 동기화 (다른 플레이어에게도 보이게)

```cpp
// EXFILCharacter.h
UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayFireMontage();

// EXFILCharacter.cpp
void AEXFILCharacter::Multicast_PlayFireMontage_Implementation()
{
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance && FireMontage)
    {
        AnimInstance->Montage_Play(FireMontage, 1.f);
    }
}
```

Server_ConfirmHit_Implementation 끝에서 Multicast_PlayFireMontage() 호출.
Unreliable이므로 가끔 누락돼도 게임플레이에 영향 없음.

### 에디터 작업 목록 (보너스)

| # | 작업 | 비고 |
|---|------|------|
| 1 | Lyra에서 발사 몽타주 마이그레이트 | Content/Animations/ |
| 2 | AnimBP에 Layered Blend per Bone 추가 (spine_01 기준) | AnimBP 에디터 |
| 3 | 마이그레이트한 몽타주의 Slot을 AnimBP의 Slot 이름과 일치 | DefaultSlot 또는 UpperBody |
| 4 | GA_Fire BP에서 FireMontage 할당 | BP 디테일 |
| 5 | BP_EXFILCharacter에서 FireMontage 할당 (Multicast용) | BP 디테일 |

### 롤백 기준

아래 중 하나라도 해당되면 즉시 롤백:
- 마이그레이트 시 의존성 에셋이 100MB 이상 딸려옴
- AnimBP 수정 후 기존 이동 애니메이션이 깨짐
- 몽타주 재생 시 T-포즈 또는 스켈레톤 불일치
- 30분 이상 디버깅해도 해결 안 됨

롤백하면 GA_Fire의 FireMontage = nullptr 상태로 두면 기존 디버그 라인만 동작.

---

## 3. Phase B: 코드 클린업

### 3-1. 디버그 UE_LOG 정리

전체 소스에서 `[STAT-`, `[DIAG]`, 디버깅 전용 `LogTemp` 검색 → 삭제.
핵심 로그만 `LogEXFIL` 카테고리로 유지:

```cpp
// Core/EXFILLog.h
DECLARE_LOG_CATEGORY_EXTERN(LogEXFIL, Log, All);
```

### 3-2. 첫 PIE 아이콘 찌그러짐

```cpp
const float NewCellStride = LocalSize.X / GridWidth;
if (NewCellStride < 1.f) return 0;
```

### 3-3. 인벤토리 풀 피드백

```cpp
UFUNCTION(Client, Reliable)
void Client_ShowNotification(const FString& Message);

void AEXFILCharacter::Client_ShowNotification_Implementation(const FString& Message)
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, Message);
    }
}
```

---

## 4. 생성할 파일 목록

| 파일 경로 | 용도 | 신규/수정 |
|-----------|------|----------|
| `Source/Project_EXFIL/GAS/GA_Fire.h` | 발사 GA | **신규** |
| `Source/Project_EXFIL/GAS/GA_Fire.cpp` | 발사 GA 구현 | **신규** |
| `Source/Project_EXFIL/Core/EXFILCharacter.h` | 전투 RPC, OnDeath | 수정 |
| `Source/Project_EXFIL/Core/EXFILCharacter.cpp` | 전투 로직, 입력 | 수정 |
| `Source/Project_EXFIL/Equipment/EquipmentComponent.h` | HasWeaponEquipped | 수정 |
| `Source/Project_EXFIL/Equipment/EquipmentComponent.cpp` | 구현 | 수정 |
| `Source/Project_EXFIL/GAS/SurvivalAttributeSet.cpp` | 사망 처리 | 수정 |
| `Source/Project_EXFIL/Core/EXFILLog.h` | 로그 카테고리 | **신규** |
| `Source/Project_EXFIL/UI/InventoryIconOverlay.cpp` | CellStride 검사 | 수정 |
| 전체 Source/ | 디버그 로그 정리 | 수정 |

---

## 5. 에디터 수동 작업

| # | 작업 | 위치 |
|---|------|------|
| 1 | `GE_Damage` GameplayEffect BP 생성 (Instant, Health -20) | Content/GAS/ |
| 2 | `IA_Fire` InputAction 에셋 생성 (Left Mouse Button) | Content/Input/ |
| 3 | InputMappingContext에 `IA_Fire` 추가 | IMC_Default |
| 4 | BP_EXFILCharacter에서 `GA_FireClass` 할당 | BP 디테일 |
| 5 | BP_EXFILCharacter에서 `DamageEffectClass` → `GE_Damage` 할당 | BP 디테일 |
| 6 | BP_EXFILCharacter에서 `IA_Fire` 할당 | BP 디테일 |
| 7 | `IA_Aim` InputAction 에셋 생성 (R키) | Content/Input/ |
| 8 | InputMappingContext에 `IA_Aim` 추가 | IMC_Default |
| 9 | BP_EXFILCharacter에서 `IA_Aim` 할당 | BP 디테일 |
| 10 | WBP_Crosshair 위젯 블루프린트 생성 (중앙 십자 이미지) | Content/UI/ |
| 11 | BP_EXFILCharacter에서 `CrosshairWidgetClass` → `WBP_Crosshair` 할당 | BP 디테일 |

---

## 6. 주의사항

- **서버 히트 재검증 필수:** 클라이언트 히트를 그대로 신뢰하면 안 됨
- **NetExecutionPolicy = LocalPredicted:** 클라이언트에서 즉시 디버그 라인, 서버에서 데미지 확정
- **FVector_NetQuantize:** 히트 위치/노멀 네트워크 전송 시 대역폭 절약
- **Multicast는 Unreliable:** 히트 이펙트 누락돼도 게임플레이 무관
- **인벤토리 열려있으면 발사 안 함:** bIsInventoryOpen 체크
- **조준 모드가 아니면 발사 안 함:** bIsAiming 체크. R키로 토글
- **인벤토리 열 때 조준 자동 해제:** bIsAiming 상태에서 I키 누르면 OnAimToggled() 호출
- **GE는 BP로 생성, GA도 BP 래퍼 필요할 수 있음**
- **generated.h 마지막 include**

---

## 7. 검증 기준 (Done Criteria)

### Phase A: 슈팅
- [x] 무기 미장착 → 좌클릭 → 발사 안 됨
- [x] Pistol 장착 → 조준 모드에서 좌클릭 → 라인 트레이스 발사
- [x] 상대 히트 → 피격 Overlay 플래시 (M_HitOverlay)
- [x] 서버 재검증 → 타겟 Health -20 감소 → StatsBar HP 바 감소
- [x] HP 0 → 래그돌 → 3초 후 Destroy
- [x] 인벤토리 열린 상태 → 좌클릭 → 발사 안 됨
- [x] R키 → 어깨 너머 카메라 줌인 + 크로스헤어 표시
- [x] R키 다시 → 카메라 원복 + 크로스헤어 숨김
- [x] 일반 모드에서 좌클릭 → 발사 안 됨
- [x] 조준 모드에서 좌클릭 → 발사
- [x] 조준 시 캐릭터가 카메라 방향으로 회전
- [x] 2P 상호 사격 → 서버 크래시 없음

### Phase A 보너스
- [x] 피격 Overlay Material (M_HitOverlay) — Lyra 몽타주 대신 채택
- [ ] ~~Lyra 발사 몽타주~~ — 스켈레톤 불일치로 스킵

### Phase B: 코드 클린업
- [x] LogTemp → LogEXFIL 전환 (Warning/Error 14건), 디버그 로그 22건 삭제
- [x] CellStride < 1.f 시 RefreshIcons/NativePaint 스킵 (이미 구현 확인)
- [x] GetWorld() null 체크 추가 (사망 Destroy 시 크래시 방지)
- [x] 인벤토리 풀 시 화면 메시지 (Client_ShowNotification)

---

## ✅ 완료 보고

### Phase A: 라인 트레이스 슈팅 시스템
- GA_Fire (LocalPredicted) + ECC_Pawn 라인 트레이스 + Server_ConfirmHit 서버 재검증
- GE_Damage (Instant, Health -20) → SurvivalAttributeSet → HP 0 시 OnDeath
- R키 조준 토글: SpringArm SocketOffset 어깨 너머 시점 + FOV 60 + 크로스헤어
- 조준 시 bUseControllerRotationYaw=true, 비조준 시 OrientRotationToMovement=true
- 사망: Multicast_OnDeath (래그돌 + 캡슐 해제 + 입력 차단) → SetLifeSpan(3초)
- 피격: Multicast_PlayHitReact → SetOverlayMaterial(M_HitOverlay) 0.2초 플래시
- LocalPredicted 중복 방지: IsLocallyControlled() 가드로 Server RPC 1회만 호출

### Phase B: 코드 클린업
- LogEXFIL 카테고리 전환 (14건 Warning/Error), 디버그 LogTemp 22건 삭제
- GetWorld() null 체크 2곳 추가 (InventoryPanelWidget — 사망 Destroy 크래시 방지)
- CellStride < 1 가드 기존 구현 확인
- WorldItem 아이템 이름 텍스트(TextRenderComponent) 기본 비활성화 — 시연용 클린 비주얼
- Client_ShowNotification RPC로 인벤토리 풀 피드백 화면 메시지 구현

### Phase B-2: 추가 폴리시
- StatEntryWidget: `IconTexture` UPROPERTY(EditAnywhere) 추가 — 에디터에서 각 StatEntry 인스턴스별 아이콘 텍스처 지정 가능, NativeOnInitialized에서 SetBrushFromTexture 적용
- WorldItem: Bandage / Helmet / Pistol / SniperRifle 4종 메시 + PBR 텍스처 + 아이콘 에셋 추가
- Medkit 메시 + PBR 텍스처 에셋 추가
- DT_ItemData 업데이트 (신규 아이템 반영)
- Health / Hunger 스탯 아이콘 텍스처 에셋 추가 (Content/Assets/Textures/)

### Phase C: 시연 준비 (수현 직접 처리)
- [ ] 시연 영상 촬영
- [ ] GitHub README.md 작성
- [ ] ARCHITECTURE.md 최종 업데이트
- [x] 최종 커밋 + 태그 (v1.0-portfolio)

---

## 📌 프연기 지시 프롬프트

```
Day 8 작업 시작. Docs/DailyLogs/Day8_Design.md를 읽고 Phase A → B 순서대로 구현해줘.

Phase A가 핵심 — 라인 트레이스 슈팅 + 네트워크 데미지.
Phase A 완료 + PIE 테스트 통과 후, 시간 남으면 Phase A 보너스(Lyra 발사 몽타주) 시도.
보너스는 30분 이상 막히면 즉시 롤백.
Phase B는 로그 정리 + 소규모 수정.
Phase C(시연 영상, README)는 내가 직접 처리함.

각 Phase 완료 후 빌드 확인하고, 전체 완료되면 Day8_Design.md 하단 완료 보고 작성해줘.
```
