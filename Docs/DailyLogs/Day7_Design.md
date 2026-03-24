# Day 7 설계 지시서: Replication 2 — WorldItem 스폰/소멸 동기화 + 아이템 픽업/드롭

> 채팅 탭(Opus)이 상단 "설계 지시서" 섹션을 작성 → 수현이 검토 후 Docs/DailyLogs/Day7_Design.md로 저장
> 프로젝트 연결 탭이 하단 "완료 보고"를 작성 → 수현이 채팅 탭에 전달

---

## ⚠️ Day 6 반영사항

### 반영 1: Day 6에서 완료된 사항

- [x] Dedicated Server 권한 모델 전체 적용 (Inventory/Equipment/Crafting)
- [x] DOREPLIFETIME_CONDITION(COND_OwnerOnly) 전 컴포넌트 적용
- [x] Server RPC 총 10개 (Inventory 4 + Equipment 4 + Crafting 2)
- [x] ASC Replication Mode = Mixed
- [x] IsLocallyControlled() 가드 전수 적용
- [x] 인벤토리 ↔ 장비슬롯 드래그 장착/해제 (핫픽스 A)
- [x] TryAddItemByID 스택 병합 (핫픽스 B)
- [x] DecrementStack API, 장비슬롯 비주얼 Clear

### 반영 2: Day 6 완료 보고에서 이월된 항목

- 클라이언트 예측(Prediction) + 롤백: Day 8 폴리시로 이관
- IconOverlay 윈도우 리사이즈 대응: Day 8 폴리시로 이관

### 반영 3: 현재 네트워크 아키텍처 상태

| 컴포넌트 | Replicated | Server RPC | 상태 |
|---------|-----------|-----------|------|
| USurvivalAttributeSet | ✅ | GAS 내부 | Day 4 완료 |
| UInventoryComponent | ✅ COND_OwnerOnly | 4개 + DecrementStack | Day 6 완료 |
| UEquipmentComponent | ✅ COND_OwnerOnly | 4개 (Equip/Unequip/EquipFromInv/UnequipToInv) | Day 6 완료 |
| UCraftingComponent | ✅ COND_OwnerOnly | 2개 (Start/Cancel) | Day 6 완료 |
| AWorldItem | ❌ | ❌ | **Day 7에서 구현** |

### 반영 4: ARCHITECTURE.md 참조

ARCHITECTURE.md 섹션 2.6에 `AWorldItem`이 Day 7 클래스로 정의됨:
- 역할: 월드에 드랍된 아이템 액터. 네트워크 스폰/소멸.
- 위치: `Source/Project_EXFIL/World/WorldItem.h/.cpp`

ARCHITECTURE.md 섹션 3.1 아이템 획득 흐름:
```
Player가 WorldItem에 Interact
  → [Client] Server RPC 호출: RequestPickupItem(WorldItemID)
  → [Server] 검증: WorldItem 존재? 거리 유효?
  → [Server] UInventoryComponent::TryAddItemByID() 호출
  → [Server] WorldItem Destroy (리플리케이트)
  → [Client] OnRep → ViewModel → View 갱신
```

---

## 📋 Day 7 설계 지시서

**작성자:** 채팅 탭 (Opus 4.6)
**WBS 분류:** Replication 2
**목표 시간:** 15h
**날짜:** 2026-03-23
**권장 모델:** Sonnet 4.6 (설계서가 상세하고 패턴 반복. Day 6 Server RPC 패턴과 동일한 구조)

## 1. 오늘의 목표 (3개 페이즈)

**Phase A: AWorldItem 액터 구현 (4~5h)**
- [ ] `AWorldItem` 클래스 생성 (Replicated Actor)
- [ ] 메시/콜리전/인터랙션 컴포넌트 구성
- [ ] 아이템 데이터 리플리케이션
- [ ] 서버 스폰/소멸 로직

**Phase B: 아이템 픽업 인터랙션 (4~5h)**
- [ ] 인터랙션 시스템 (라인 트레이스 + 입력)
- [ ] Server RPC: 픽업 요청 → 서버 검증 → 인벤토리 추가 → WorldItem Destroy
- [ ] 픽업 시 UI 피드백

**Phase C: 아이템 드롭 + 컨텍스트 메뉴 + 통합 테스트 (5~6h)**
- [ ] 우클릭 컨텍스트 메뉴 위젯 (Use / Equip / Unequip / Drop)
- [ ] 인벤토리/장비에서 월드로 아이템 드롭 (Server RPC)
- [ ] 서버에서 WorldItem 스폰 → 모든 클라이언트에 리플리케이션
- [ ] Dedicated Server 2P 전체 시나리오 테스트

---

## 2. 생성할 파일 목록

| 파일 경로 | 용도 | 신규/수정 |
|-----------|------|----------|
| `Source/Project_EXFIL/World/WorldItem.h` | 월드 드롭 아이템 액터 | **신규** |
| `Source/Project_EXFIL/World/WorldItem.cpp` | 월드 드롭 아이템 액터 구현 | **신규** |
| `Source/Project_EXFIL/UI/ItemContextMenuWidget.h` | 우클릭 컨텍스트 메뉴 (Use/Equip/Drop) | **신규** |
| `Source/Project_EXFIL/UI/ItemContextMenuWidget.cpp` | 컨텍스트 메뉴 구현 | **신규** |
| `Source/Project_EXFIL/Core/EXFILCharacter.h` | 인터랙션 입력 + 라인트레이스 | 수정 |
| `Source/Project_EXFIL/Core/EXFILCharacter.cpp` | 픽업 Server RPC | 수정 |
| `Source/Project_EXFIL/Inventory/InventoryComponent.h` | DropItem Server RPC 추가 | 수정 |
| `Source/Project_EXFIL/Inventory/InventoryComponent.cpp` | DropItem 구현 | 수정 |
| `Source/Project_EXFIL/Equipment/EquipmentComponent.h` | DropEquippedItem Server RPC 추가 | 수정 |
| `Source/Project_EXFIL/Equipment/EquipmentComponent.cpp` | DropEquippedItem 구현 | 수정 |
| `Source/Project_EXFIL/UI/IconOverlay.cpp` | 우클릭 → 컨텍스트 메뉴 호출 | 수정 |
| `Source/Project_EXFIL/UI/EquipmentSlotWidget.cpp` | 우클릭 → 컨텍스트 메뉴 호출 | 수정 |

---

## 3. Phase A: AWorldItem 액터

### 3-1. 클래스 설계

```cpp
// WorldItem.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldItem.generated.h"

UCLASS()
class PROJECT_EXFIL_API AWorldItem : public AActor
{
    GENERATED_BODY()

public:
    AWorldItem();

    // === 초기화 ===
    void InitializeItem(FName InItemDataID, int32 InStackCount);

    // === 접근자 ===
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WorldItem")
    FName GetItemDataID() const { return ItemDataID; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WorldItem")
    int32 GetStackCount() const { return StackCount; }

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    // === 컴포넌트 ===
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USphereComponent> InteractionSphere;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UTextRenderComponent> ItemNameText;

    // === Replicated 데이터 ===
    UPROPERTY(ReplicatedUsing = OnRep_ItemData)
    FName ItemDataID;

    UPROPERTY(Replicated)
    int32 StackCount = 1;

    // === OnRep ===
    UFUNCTION()
    void OnRep_ItemData();

    // === 비주얼 갱신 ===
    void UpdateVisual();
};
```

### 3-2. 생성자 — 컴포넌트 구성

```cpp
// WorldItem.cpp
#include "WorldItem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "Net/UnrealNetwork.h"

AWorldItem::AWorldItem()
{
    bReplicates = true;
    bAlwaysRelevant = false;
    SetReplicatingMovement(true);

    PrimaryActorTick.bCanEverTick = false;

    // 메시 (루트)
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    SetRootComponent(MeshComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    MeshComponent->SetSimulatePhysics(true);
    MeshComponent->SetEnableGravity(true);

    // 인터랙션 스피어
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(150.f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // 아이템 이름 텍스트 (큐브 위에 표시)
    ItemNameText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ItemNameText"));
    ItemNameText->SetupAttachment(RootComponent);
    ItemNameText->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
    ItemNameText->SetHorizontalAlignment(EHTA_Center);
    ItemNameText->SetVerticalAlignment(EVRTA_TextCenter);
    ItemNameText->SetWorldSize(20.f);
    ItemNameText->SetTextRenderColor(FColor::White);
}
```

**설계 결정:**
- `bReplicates = true`: Actor 자체가 서버에서 스폰되면 모든 클라이언트에 자동 생성
- `SetReplicatingMovement(true)`: 물리 시뮬레이션 결과(낙하 후 위치)가 클라이언트에 동기화
- `bAlwaysRelevant = false`: 거리 기반 Relevancy 사용 — 멀리 있는 아이템은 전송하지 않아 대역폭 절약
- `InteractionSphere` 반경 150cm: 플레이어가 가까이 가야 픽업 가능
- `PrimaryActorTick.bCanEverTick = false`: Tick 불필요, 이벤트 기반 설계

### 3-3. Replication 설정

```cpp
void AWorldItem::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 모든 클라이언트에게 전파 — 월드 아이템은 모든 플레이어가 볼 수 있어야 함
    DOREPLIFETIME(AWorldItem, ItemDataID);
    DOREPLIFETIME(AWorldItem, StackCount);
}
```

**COND_None (모든 클라이언트 전파) 선택 근거:**
- 인벤토리와 달리, 월드에 드롭된 아이템은 **모든 플레이어가 시각적으로 확인하고 경쟁적으로 획득**할 수 있어야 함
- 타르코프 스타일: 바닥에 떨어진 아이템을 다른 플레이어가 먼저 줍는 상황이 핵심 게임플레이

### 3-4. 초기화 + 비주얼

```cpp
void AWorldItem::InitializeItem(FName InItemDataID, int32 InStackCount)
{
    // 서버에서만 호출 (스폰 직후)
    checkf(HasAuthority(), TEXT("InitializeItem must be called on server"));

    ItemDataID = InItemDataID;
    StackCount = InStackCount;

    UpdateVisual();
}

void AWorldItem::BeginPlay()
{
    Super::BeginPlay();
    UpdateVisual();
}

void AWorldItem::OnRep_ItemData()
{
    // 클라이언트에서 아이템 데이터가 리플리케이션되면 비주얼 갱신
    UpdateVisual();
}

void AWorldItem::UpdateVisual()
{
    if (ItemDataID.IsNone()) return;

    // UItemDataSubsystem에서 FItemData 조회 → WorldMesh 또는 기본 메시 적용
    // Day 3에서 만든 UItemDataSubsystem 활용
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UItemDataSubsystem* DataSub = GI->GetSubsystem<UItemDataSubsystem>();
    if (!DataSub) return;

    const FItemData* Data = DataSub->FindItemData(ItemDataID);
    if (Data && Data->WorldMesh)
    {
        MeshComponent->SetStaticMesh(Data->WorldMesh);
    }
    // WorldMesh가 없으면 기본 메시(큐브 등)를 폴백으로 사용

    // 아이템 이름 텍스트 갱신
    if (ItemNameText)
    {
        if (Data)
        {
            ItemNameText->SetText(FText::FromName(Data->ItemName));
        }
        else
        {
            ItemNameText->SetText(FText::FromName(ItemDataID));
        }
    }
}
```

### 3-5. FItemData에 WorldMesh 필드 추가

```cpp
// ItemData.h (또는 EXFILInventoryTypes.h 내 FItemData)

// 기존 필드에 추가:
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
TObjectPtr<UStaticMesh> WorldMesh = nullptr;
```

**DT_ItemData 에디터 작업:** 각 아이템 행에 `WorldMesh` 컬럼 추가. 초기에는 `nullptr`(기본 큐브 폴백)로 두고, Day 8 폴리시에서 실제 메시 세팅.

---

## 4. Phase B: 아이템 픽업 인터랙션

### 4-1. 인터랙션 설계 — 라인 트레이스 방식

```
[Client] 매 프레임 아님 — 입력(F키) 시점에 1회 라인 트레이스
  → 카메라 Forward 방향으로 라인 트레이스 (MaxDistance = 300cm)
  → 히트된 Actor가 AWorldItem인지 체크
  → 유효하면 Server RPC 호출
```

**Overlap 방식이 아닌 라인 트레이스 선택 이유:**
- 타르코프 스타일: 플레이어가 아이템을 **조준하고** 의도적으로 줍는 행위
- Overlap은 지나가기만 해도 자동 픽업 → 게임플레이 의도와 불일치
- 라인 트레이스는 입력 시점에 1회만 실행 → Tick 비용 제로

### 4-2. AEXFILCharacter — 인터랙션 입력

```cpp
// EXFILCharacter.h 추가

// === 인터랙션 ===
UPROPERTY(EditAnywhere, Category = "Interaction")
float InteractionDistance = 300.f;

// 입력 액션 (Enhanced Input)
UPROPERTY(EditAnywhere, Category = "Input")
TObjectPtr<UInputAction> IA_Interact;

void OnInteractPressed();

// 라인 트레이스로 바라보고 있는 WorldItem 탐색
AWorldItem* TraceForWorldItem() const;

// === Server RPC ===
UFUNCTION(Server, Reliable, WithValidation)
void Server_RequestPickupItem(AWorldItem* TargetItem);
```

### 4-3. 라인 트레이스 구현

```cpp
// EXFILCharacter.cpp

AWorldItem* AEXFILCharacter::TraceForWorldItem() const
{
    // 카메라 위치 + 방향 기준 라인 트레이스
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return nullptr;

    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    const FVector TraceStart = CameraLocation;
    const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * InteractionDistance;

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    if (GetWorld()->LineTraceSingleByChannel(
            HitResult, TraceStart, TraceEnd, ECC_Visibility, Params))
    {
        return Cast<AWorldItem>(HitResult.GetActor());
    }

    return nullptr;
}
```

### 4-4. 입력 바인딩

```cpp
// EXFILCharacter.cpp — SetupPlayerInputComponent 내부

// Enhanced Input 바인딩 (기존 패턴 따름)
if (UEnhancedInputComponent* EnhancedInput =
        Cast<UEnhancedInputComponent>(PlayerInputComponent))
{
    // 기존 이동/카메라 바인딩 유지...

    // 인터랙션 (F키)
    if (IA_Interact)
    {
        EnhancedInput->BindAction(
            IA_Interact, ETriggerEvent::Started, this,
            &AEXFILCharacter::OnInteractPressed);
    }
}
```

**에디터 작업:** `IA_Interact` InputAction 에셋 생성 (F키 매핑). 기존 InputMappingContext에 추가.

### 4-5. 픽업 요청 흐름

```cpp
void AEXFILCharacter::OnInteractPressed()
{
    AWorldItem* Target = TraceForWorldItem();
    if (!Target) return;

    // Server RPC 호출 — Authority에서 호출해도 UE가 _Implementation 직접 실행
    Server_RequestPickupItem(Target);
}

bool AEXFILCharacter::Server_RequestPickupItem_Validate(AWorldItem* TargetItem)
{
    return TargetItem != nullptr;
}

void AEXFILCharacter::Server_RequestPickupItem_Implementation(AWorldItem* TargetItem)
{
    ExecutePickup(TargetItem);
}
```

### 4-6. 서버 측 픽업 실행 — ExecutePickup

```cpp
// EXFILCharacter.h 추가
void ExecutePickup(AWorldItem* TargetItem);

// EXFILCharacter.cpp
void AEXFILCharacter::ExecutePickup(AWorldItem* TargetItem)
{
    // 서버에서만 실행
    if (!HasAuthority()) return;

    // 1. WorldItem 유효성 검증
    if (!TargetItem || TargetItem->IsPendingKillPending()) return;

    // 2. 거리 검증 (치트 방지)
    const float Distance = FVector::Dist(GetActorLocation(), TargetItem->GetActorLocation());
    constexpr float MaxPickupDistance = 400.f; // InteractionDistance보다 약간 넉넉하게 (네트워크 지연 보상)
    if (Distance > MaxPickupDistance)
    {
        UE_LOG(LogTemp, Warning, TEXT("Pickup rejected: distance %.1f > max %.1f"),
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
        UE_LOG(LogTemp, Warning, TEXT("Pickup rejected: inventory full"));
        // TODO: 클라이언트에 "인벤토리 가득 참" 피드백 (Day 8)
        return;
    }

    // 4. WorldItem 제거 — Destroy는 리플리케이션됨 (bReplicates=true Actor의 Destroy)
    TargetItem->Destroy();
}
```

**서버 검증 체크리스트:**
- WorldItem 존재 여부 (다른 플레이어가 먼저 주웠을 수 있음)
- 거리 검증 (치트/핵 방지, 네트워크 지연 보상으로 여유값 부여)
- 인벤토리 공간 검증 (TryAddItemByID 반환값)

---

## 5. Phase C: 아이템 드롭 (인벤토리 → 월드)

### 5-1. 드롭 흐름

```
[Client] 인벤토리 아이템 우클릭 → 컨텍스트 메뉴 → "Drop" 선택
  → [Client] Server RPC: Server_DropItem(ItemInstanceID)
  → [Server] 검증: 아이템 존재? 인벤토리에 있는가?
  → [Server] UInventoryComponent::RemoveItem(ItemInstanceID)
  → [Server] AWorldItem 스폰 (캐릭터 전방 100cm)
  → [Server] WorldItem.InitializeItem(ItemDataID, StackCount)
  → [All Clients] Actor 리플리케이션으로 WorldItem 자동 생성
  → [Owner Client] OnRep_Items로 인벤토리 UI 갱신
```

### 5-2. UInventoryComponent — 드롭 Server RPC

```cpp
// InventoryComponent.h 추가

UFUNCTION(Server, Reliable, WithValidation)
void Server_DropItem(FGuid ItemInstanceID);
```

```cpp
// InventoryComponent.cpp

bool UInventoryComponent::Server_DropItem_Validate(FGuid ItemInstanceID)
{
    return ItemInstanceID.IsValid();
}

void UInventoryComponent::Server_DropItem_Implementation(FGuid ItemInstanceID)
{
    // 1. 아이템 조회
    const FInventoryItemInstance* Item = FindItemByInstanceID(ItemInstanceID);
    if (!Item) return;

    // 드롭에 필요한 정보 캐싱 (RemoveItem 전에)
    const FName DropItemDataID = Item->ItemDataID;
    const int32 DropStackCount = Item->StackCount;

    // 2. 인벤토리에서 제거
    RemoveItem(ItemInstanceID);

    // 3. 캐릭터 전방에 WorldItem 스폰
    AActor* Owner = GetOwner();
    if (!Owner) return;

    const FVector SpawnLocation =
        Owner->GetActorLocation()
        + Owner->GetActorForwardVector() * 100.f
        + FVector(0.f, 0.f, 50.f); // 약간 위에서 스폰 (바닥 관통 방지)

    const FRotator SpawnRotation = FRotator::ZeroRotator;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = Owner;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AWorldItem* DroppedItem = GetWorld()->SpawnActor<AWorldItem>(
        AWorldItem::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);

    if (DroppedItem)
    {
        DroppedItem->InitializeItem(DropItemDataID, DropStackCount);
    }
}
```

**스폰 위치 계산:**
- 캐릭터 전방 100cm + 높이 50cm에서 스폰
- 물리 시뮬레이션(SetSimulatePhysics=true)이 켜져 있으므로 자연스럽게 바닥으로 낙하
- `AdjustIfPossibleButAlwaysSpawn`: 벽 안에 스폰되는 경우 위치 보정, 불가능해도 강제 스폰

### 5-3. 드롭 트리거 — 우클릭 컨텍스트 메뉴

인벤토리가 풀스크린이므로 "패널 밖으로 드래그" 방식은 불가. **우클릭 컨텍스트 메뉴**로 드롭을 트리거한다.

**컨텍스트 메뉴 동작 흐름:**
```
인벤토리 아이템 우클릭
  → 컨텍스트 메뉴 팝업 (Use / Equip / Drop)
  → "Drop" 선택 → Server_DropItem(ItemInstanceID)
  → 메뉴 닫힘

장비슬롯 아이템 우클릭
  → 컨텍스트 메뉴 팝업 (Unequip / Drop)
  → "Drop" 선택 → Server_DropEquippedItem(SlotType)
  → 메뉴 닫힘
```

### 5-4. WBP_ItemContextMenu 위젯

**신규 파일:**
- `Source/Project_EXFIL/UI/ItemContextMenuWidget.h/.cpp`

```cpp
// ItemContextMenuWidget.h
#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ItemContextMenuWidget.generated.h"

UCLASS()
class PROJECT_EXFIL_API UItemContextMenuWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    // 인벤토리 아이템용 메뉴 열기
    void ShowForInventoryItem(FGuid InItemInstanceID, FName InItemDataID);

    // 장비슬롯 아이템용 메뉴 열기
    void ShowForEquippedItem(EEquipmentSlot InSlot, FName InItemDataID);

protected:
    virtual void NativeConstruct() override;

private:
    // === UI 요소 (BindWidget) ===
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Use;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Equip;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Unequip;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Drop;

    // === 상태 ===
    FGuid CachedItemInstanceID;
    FName CachedItemDataID;
    EEquipmentSlot CachedEquipmentSlot = EEquipmentSlot::None;
    bool bIsEquippedItem = false;

    // === 버튼 핸들러 ===
    UFUNCTION()
    void OnUseClicked();

    UFUNCTION()
    void OnEquipClicked();

    UFUNCTION()
    void OnUnequipClicked();

    UFUNCTION()
    void OnDropClicked();

    void CloseMenu();
};
```

### 5-5. 컨텍스트 메뉴 구현

```cpp
// ItemContextMenuWidget.cpp

void UItemContextMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Button_Use) Button_Use->OnClicked.AddDynamic(this, &UItemContextMenuWidget::OnUseClicked);
    if (Button_Equip) Button_Equip->OnClicked.AddDynamic(this, &UItemContextMenuWidget::OnEquipClicked);
    if (Button_Unequip) Button_Unequip->OnClicked.AddDynamic(this, &UItemContextMenuWidget::OnUnequipClicked);
    if (Button_Drop) Button_Drop->OnClicked.AddDynamic(this, &UItemContextMenuWidget::OnDropClicked);
}

void UItemContextMenuWidget::ShowForInventoryItem(FGuid InItemInstanceID, FName InItemDataID)
{
    bIsEquippedItem = false;
    CachedItemInstanceID = InItemInstanceID;
    CachedItemDataID = InItemDataID;

    // 아이템 타입에 따라 버튼 가시성 결정
    // UItemDataSubsystem에서 FItemData 조회
    const FItemData* Data = /* DataSubsystem->FindItemData(InItemDataID) */;

    const bool bIsConsumable = Data && Data->bIsConsumable;
    const bool bIsEquipable = Data && Data->EquipmentSlotTag != NAME_None;

    Button_Use->SetVisibility(bIsConsumable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    Button_Equip->SetVisibility(bIsEquipable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    Button_Unequip->SetVisibility(ESlateVisibility::Collapsed);
    Button_Drop->SetVisibility(ESlateVisibility::Visible); // 항상 표시

    SetVisibility(ESlateVisibility::Visible);
}

void UItemContextMenuWidget::ShowForEquippedItem(EEquipmentSlot InSlot, FName InItemDataID)
{
    bIsEquippedItem = true;
    CachedEquipmentSlot = InSlot;
    CachedItemDataID = InItemDataID;

    Button_Use->SetVisibility(ESlateVisibility::Collapsed);
    Button_Equip->SetVisibility(ESlateVisibility::Collapsed);
    Button_Unequip->SetVisibility(ESlateVisibility::Visible);
    Button_Drop->SetVisibility(ESlateVisibility::Visible);

    SetVisibility(ESlateVisibility::Visible);
}

void UItemContextMenuWidget::OnUseClicked()
{
    // 기존 UseItem 로직 호출 (Day 4의 GA_UseItem 경유)
    if (UInventoryComponent* Inv = /* ... */)
    {
        Inv->Server_ConsumeItemByID(CachedItemDataID, 1);
    }
    CloseMenu();
}

void UItemContextMenuWidget::OnEquipClicked()
{
    // Day 6 핫픽스 A의 Server_EquipFromInventory 호출
    if (UEquipmentComponent* Equip = /* ... */)
    {
        Equip->Server_EquipFromInventory(CachedItemInstanceID);
    }
    CloseMenu();
}

void UItemContextMenuWidget::OnUnequipClicked()
{
    if (UEquipmentComponent* Equip = /* ... */)
    {
        Equip->Server_UnequipToInventory(CachedEquipmentSlot);
    }
    CloseMenu();
}

void UItemContextMenuWidget::OnDropClicked()
{
    if (bIsEquippedItem)
    {
        // 장비 해제 + 월드 드롭 원자적 처리
        if (UEquipmentComponent* Equip = /* ... */)
        {
            Equip->Server_DropEquippedItem(CachedEquipmentSlot);
        }
    }
    else
    {
        // 인벤토리에서 월드 드롭
        if (UInventoryComponent* Inv = /* ... */)
        {
            Inv->Server_DropItem(CachedItemInstanceID);
        }
    }
    CloseMenu();
}

void UItemContextMenuWidget::CloseMenu()
{
    SetVisibility(ESlateVisibility::Collapsed);
}
```

### 5-6. 우클릭 수신 — IconOverlay + EquipmentSlotWidget

**IconOverlay (인벤토리 아이템 우클릭):**
```cpp
// IconOverlay의 NativeOnMouseButtonDown 오버라이드
virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent) override;

// 구현
FReply UIconOverlay::NativeOnMouseButtonDown(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        // 클릭 위치에서 아이템 탐색
        const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        const FInventoryItemInstance* HitItem = FindItemAtPosition(LocalPos);

        if (HitItem && ContextMenuWidget)
        {
            ContextMenuWidget->ShowForInventoryItem(HitItem->InstanceID, HitItem->ItemDataID);
            // 메뉴 위치를 클릭 좌표에 배치
            ContextMenuWidget->SetPositionInViewport(InMouseEvent.GetScreenSpacePosition());
            return FReply::Handled();
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
```

**EquipmentSlotWidget (장비 아이템 우클릭):**
```cpp
// EquipmentSlotWidget의 NativeOnMouseButtonDown 오버라이드
FReply UEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        if (!CurrentSlotData.IsEmpty() && ContextMenuWidget)
        {
            ContextMenuWidget->ShowForEquippedItem(SlotType, CurrentSlotData.EquippedItemDataID);
            ContextMenuWidget->SetPositionInViewport(InMouseEvent.GetScreenSpacePosition());
            return FReply::Handled();
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
```

### 5-7. Server_DropEquippedItem — 장비 해제 + 월드 드롭 원자적 처리

```cpp
// EquipmentComponent.h 추가

UFUNCTION(Server, Reliable, WithValidation)
void Server_DropEquippedItem(EEquipmentSlot InSlot);
```

```cpp
// EquipmentComponent.cpp

bool UEquipmentComponent::Server_DropEquippedItem_Validate(EEquipmentSlot InSlot)
{
    return InSlot != EEquipmentSlot::None;
}

void UEquipmentComponent::Server_DropEquippedItem_Implementation(EEquipmentSlot InSlot)
{
    FEquipmentSlotData* SlotData = FindSlotData(InSlot);
    if (!SlotData || SlotData->IsEmpty()) return;

    // 장착 아이템 정보 캐싱
    const FName DropItemDataID = SlotData->EquippedItemDataID;

    // GE 제거
    if (UAbilitySystemComponent* ASC = /* ... */)
    {
        if (SlotData->ActiveGEHandle.IsValid())
        {
            ASC->RemoveActiveGameplayEffect(SlotData->ActiveGEHandle);
        }
    }

    // 슬롯 비우기
    SlotData->EquippedItemDataID = NAME_None;
    SlotData->EquippedItemID = FGuid();
    SlotData->ActiveGEHandle = FActiveGameplayEffectHandle();

    // 월드에 스폰
    AActor* Owner = GetOwner();
    if (!Owner) return;

    const FVector SpawnLocation =
        Owner->GetActorLocation()
        + Owner->GetActorForwardVector() * 100.f
        + FVector(0.f, 0.f, 50.f);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = Owner;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AWorldItem* DroppedItem = GetWorld()->SpawnActor<AWorldItem>(
        AWorldItem::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParams);

    if (DroppedItem)
    {
        DroppedItem->InitializeItem(DropItemDataID, 1); // 장비는 항상 1개
    }
}
```

---

## 6. 에디터 수동 작업 목록

Day 7 코드 구현 후, 에디터에서 수동으로 처리해야 할 항목:

| # | 작업 | 위치 | 비고 |
|---|------|------|------|
| 1 | `IA_Interact` InputAction 에셋 생성 | Content/Input/ | Trigger: Pressed, F키 |
| 2 | 기존 InputMappingContext에 `IA_Interact` 매핑 추가 | Content/Input/IMC_Default | F키 바인딩 |
| 3 | BP_EXFILCharacter에서 `IA_Interact` 에셋 할당 | BP_EXFILCharacter 디테일 | UPROPERTY(EditAnywhere) |
| 4 | DT_ItemData에 `WorldMesh` 컬럼 확인 | DT_ItemData | nullptr이면 기본 큐브 폴백 |
| 5 | InteractionSphere 콜리전 프리셋 확인 | WorldItem BP 또는 C++ 기본값 | Pawn만 Overlap |
| 6 | 테스트용 WorldItem 맵 배치 | 테스트 레벨 | BeginPlay에서 서버 스폰 또는 수동 배치 |

---

## 7. 테스트용 초기 월드 아이템 스폰

서버 BeginPlay에서 테스트용 WorldItem을 스폰하여 픽업 테스트:

```cpp
// EXFILGameMode.cpp — BeginPlay 또는 별도 함수

void AEXFILGameMode::SpawnTestWorldItems()
{
    // GameMode는 서버에서만 존재 → HasAuthority() 불필요

    struct FTestSpawn
    {
        FName ItemDataID;
        int32 StackCount;
        FVector Location;
    };

    const TArray<FTestSpawn> TestItems = {
        { FName("Bandage"), 3, FVector(200.f, 0.f, 100.f) },
        { FName("WaterBottle"), 1, FVector(300.f, 100.f, 100.f) },
        { FName("Pistol"), 1, FVector(400.f, -100.f, 100.f) },
        { FName("Helmet"), 1, FVector(500.f, 0.f, 100.f) },
    };

    for (const FTestSpawn& Spawn : TestItems)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        AWorldItem* Item = GetWorld()->SpawnActor<AWorldItem>(
            AWorldItem::StaticClass(), Spawn.Location, FRotator::ZeroRotator, Params);

        if (Item)
        {
            Item->InitializeItem(Spawn.ItemDataID, Spawn.StackCount);
        }
    }
}
```

`BeginPlay()`에서 `SpawnTestWorldItems()` 호출. Day 8에서 레벨 디자인으로 교체.

---

## 8. 의존성 & 연결 관계

```
AWorldItem (신규)
  ├── UItemDataSubsystem (Day 3) — FItemData 조회, WorldMesh 참조
  ├── UStaticMeshComponent — 비주얼
  ├── UTextRenderComponent — 아이템 이름 표시
  └── USphereComponent — 인터랙션 범위

AEXFILCharacter (수정)
  ├── AWorldItem — 라인 트레이스 타겟
  ├── UInventoryComponent — TryAddItemByID (픽업)
  └── IA_Interact — Enhanced Input 입력 액션

UInventoryComponent (수정)
  └── Server_DropItem (신규 RPC) — AWorldItem 스폰

UEquipmentComponent (수정)
  └── Server_DropEquippedItem (신규 RPC) — 장비 해제 + AWorldItem 스폰

UItemContextMenuWidget (신규)
  ├── UInventoryComponent — Use, Drop 호출
  ├── UEquipmentComponent — Equip, Unequip, DropEquipped 호출
  └── UItemDataSubsystem — 아이템 타입 조회 (버튼 가시성 결정)

FItemData (수정)
  └── WorldMesh (신규 필드) — UStaticMesh 참조
```

---

## 9. 주의사항 & 제약

- **AWorldItem은 서버에서만 스폰/Destroy:** 클라이언트가 직접 SpawnActor/Destroy 호출 절대 금지. 모든 스폰은 Server RPC 또는 GameMode에서만.
- **Actor Replication과 COND_OwnerOnly 혼동 금지:** WorldItem의 ItemDataID/StackCount는 `DOREPLIFETIME` (COND_None). 인벤토리의 COND_OwnerOnly와 다름.
- **IsPendingKillPending() 체크 필수:** 두 플레이어가 동시에 같은 아이템을 줍는 경우, 서버에서 첫 번째 요청이 Destroy한 뒤 두 번째 요청이 들어올 수 있음.
- **라인 트레이스 채널:** `ECC_Visibility` 사용. WorldItem의 MeshComponent는 `Visibility` 채널에 Block 응답이어야 함. `PhysicsOnly`로 설정된 현재 상태에서는 라인 트레이스에 안 잡힐 수 있음 → **MeshComponent 콜리전을 `QueryAndPhysics`로 변경 필요**.
- **SetReplicatingMovement(true):** 물리 낙하 후 최종 위치가 클라이언트에 동기화됨. 단, 물리 시뮬레이션 자체는 서버에서만 실행되고 결과만 전파.
- **SpawnParams.Owner 설정:** 드롭한 플레이어를 Owner로 설정하지만, 이것이 Relevancy에는 영향 없음 (bOnlyRelevantToOwner = false 기본값).
- **generated.h는 반드시 마지막 include**
- **매직 넘버 금지:** InteractionDistance, MaxPickupDistance, SpawnOffset 등은 UPROPERTY 또는 constexpr로 노출
- **장비 스왑 시 인벤토리 공간 우선 확인:** 기존 장비를 인벤토리로 복귀시킬 공간이 없으면 스왑 자체를 거부해야 함. 복귀 먼저 → 장착 순서.

---

## 9-1. 장비 스왑 로직 — Server_EquipFromInventory 수정

Day 6에서 구현한 `Server_EquipFromInventory`는 슬롯이 비어있을 때만 장착을 허용함. 이미 장착된 아이템이 있으면 **기존 장비를 인벤토리로 복귀 후 새 장비 장착 (스왑)** 처리 필요.

**수정 대상:** `EquipmentComponent.cpp` — `Server_EquipFromInventory_Implementation`

```cpp
void UEquipmentComponent::Server_EquipFromInventory_Implementation(
    FGuid ItemInstanceID, FName ItemDataID)
{
    // 1. 아이템 데이터에서 슬롯 타입 결정
    EEquipmentSlot TargetSlot = SlotTagToEnum(/* FItemData::EquipmentSlotTag */);
    FEquipmentSlotData* SlotData = FindSlotData(TargetSlot);
    if (!SlotData) return;

    UInventoryComponent* Inventory = /* Owner의 InventoryComponent */;
    if (!Inventory) return;

    // 2. 기존 장비가 있으면 스왑 처리
    if (!SlotData->IsEmpty())
    {
        const FName OldItemDataID = SlotData->EquippedItemDataID;

        // 인벤토리에 공간이 있는지 먼저 확인
        // TryAddItemByID가 실패하면 스왑 거부
        const bool bCanReturn = Inventory->TryAddItemByID(OldItemDataID, 1);
        if (!bCanReturn)
        {
            UE_LOG(LogTemp, Warning, TEXT("Equip swap rejected: inventory full"));
            return;
        }

        // 기존 장비 GE 제거
        if (UAbilitySystemComponent* ASC = /* ... */)
        {
            if (SlotData->ActiveGEHandle.IsValid())
            {
                ASC->RemoveActiveGameplayEffect(SlotData->ActiveGEHandle);
            }
        }

        // 슬롯 비우기
        SlotData->EquippedItemDataID = NAME_None;
        SlotData->EquippedItemID = FGuid();
        SlotData->ActiveGEHandle = FActiveGameplayEffectHandle();
    }

    // 3. 인벤토리에서 새 아이템 차감 (DecrementStack)
    Inventory->DecrementStack(ItemInstanceID);

    // 4. 새 장비 장착 + GE 적용
    SlotData->EquippedItemDataID = ItemDataID;
    SlotData->EquippedItemID = FGuid::NewGuid();
    // GE Apply 로직...
}
```

**핵심 순서:**
1. 기존 장비 인벤토리 복귀 시도 (실패 시 전체 거부)
2. 기존 GE 제거
3. 새 아이템 인벤토리에서 차감
4. 새 장비 장착 + GE 적용

이 순서가 중요한 이유: 복귀 먼저 확인하지 않으면 기존 장비가 소실될 수 있음.

---

## 10. 검증 기준 (Done Criteria)

### Phase A: WorldItem
- [ ] AWorldItem 클래스 빌드 성공
- [ ] 서버에서 SpawnTestWorldItems → 모든 클라이언트에 WorldItem 표시
- [ ] WorldItem 물리 낙하 → 바닥에 정착 → 클라이언트 위치 동기화
- [ ] OnRep_ItemData → UpdateVisual 호출 확인 (UE_LOG로)

### Phase B: 픽업
- [ ] F키 입력 → 라인 트레이스 → WorldItem 감지 (UE_LOG로 확인)
- [ ] Server_RequestPickupItem → 서버 검증 통과 → 인벤토리에 아이템 추가
- [ ] 픽업 후 WorldItem Destroy → 모든 클라이언트에서 소멸
- [ ] 인벤토리 가득 찬 상태에서 픽업 시도 → 거부 + WorldItem 유지
- [ ] 두 클라이언트가 동시에 같은 아이템 픽업 → 한 명만 획득, 다른 한 명 거부

### Phase C: 드롭 + 컨텍스트 메뉴 + 통합
- [ ] 인벤토리 아이템 우클릭 → 컨텍스트 메뉴 표시 (Use/Equip/Drop 가시성 아이템 타입별 분기)
- [ ] 컨텍스트 메뉴 "Drop" 클릭 → WorldItem 스폰 (캐릭터 전방)
- [ ] 장비슬롯 아이템 우클릭 → 컨텍스트 메뉴 (Unequip/Drop)
- [ ] 장비 "Drop" → 장비 해제 + WorldItem 스폰 원자적 처리
- [ ] 컨텍스트 메뉴 "Use" → 소비 아이템 사용 동작
- [ ] 컨텍스트 메뉴 "Equip" → 장비 장착 동작
- [ ] Equip 시 해당 슬롯에 기존 장비 있으면 → 기존 장비 인벤토리 복귀 + 새 장비 장착 (스왑)
- [ ] 인벤토리 가득 찬 상태에서 스왑 시도 → 거부 (기존 장비 유지)
- [ ] 컨텍스트 메뉴 "Unequip" → 장비 해제 + 인벤토리 복귀
- [ ] 드롭된 WorldItem을 다른 클라이언트가 픽업 가능
- [ ] 드롭 → 픽업 → 드롭 사이클 반복 테스트 (메모리 릭 없음)
- [ ] 스택 아이템(Bandage×3) 드롭 → WorldItem StackCount=3으로 스폰 → 픽업 시 인벤토리에 3개 복원

### 통합 시나리오 (Dedicated Server 2P)
- [ ] 서버 크래시 없음
- [ ] Client1: 월드 아이템 픽업 → 장비 장착 → 크래프팅 → 결과물 드롭
- [ ] Client2: Client1이 드롭한 아이템 픽업
- [ ] 양 클라이언트 교차 테스트: 픽업 경쟁, 드롭 후 상대방 픽업

---

## ✅ 완료 보고

**작성일:** 2026-03-24
**빌드 결과:** ✅ Succeeded (에러 0, 경고 0)
**총 컴파일:** 55 파일

---

### Phase A: AWorldItem 액터 구현

| 항목 | 결과 |
|------|------|
| `AWorldItem` 클래스 (`World/WorldItem.h/.cpp`) | ✅ 신규 생성 |
| `bReplicates = true`, `SetReplicatingMovement(true)` | ✅ 구현 |
| `MeshComponent` 콜리전 `QueryAndPhysics` (라인 트레이스 + 물리) | ✅ 구현 |
| `InteractionSphere` 반경 150cm, Pawn Overlap | ✅ 구현 |
| `ItemDataID` `DOREPLIFETIME` (COND_None — 모든 클라이언트) | ✅ 구현 |
| `OnRep_ItemData` → `UpdateVisual()` — UE_LOG 포함 | ✅ 구현 |
| `FItemData::WorldMesh` 필드 추가 (`EXFILItemTypes.h`) | ✅ 구현 |

**설계 결정 적용:**
- `PhysicsOnly` → `QueryAndPhysics`: 라인 트레이스(`ECC_Visibility`)가 메시에 히트되도록 수정 (설계서 9번 주의사항)
- `UpdateVisual()`에서 `UItemDataSubsystem::GetItemData()` 활용, `DisplayName`으로 텍스트 갱신

---

### Phase B: 아이템 픽업 인터랙션

| 항목 | 결과 |
|------|------|
| `AEXFILCharacter::SetupPlayerInputComponent` 오버라이드 | ✅ 구현 |
| `IA_Interact` UPROPERTY (F키 — 에디터 할당) | ✅ 구현 |
| `TraceForWorldItem()` — 카메라 Forward 300cm 라인 트레이스 | ✅ 구현 |
| `Server_RequestPickupItem` Server RPC (Validate 포함) | ✅ 구현 |
| `ExecutePickup()` — 유효성·거리(400cm)·인벤토리 3단계 검증 | ✅ 구현 |
| 픽업 성공 시 `TargetItem->Destroy()` 리플리케이션 | ✅ 구현 |

**에디터 수동 작업 필요:**
- `Content/Input/`에 `IA_Interact` InputAction 에셋 생성 (F키, Trigger: Pressed)
- 기존 `IMC_Default`에 F키 매핑 추가
- `BP_EXFILCharacter` 디테일에서 `IA_Interact` 에셋 할당

---

### Phase C: 아이템 드롭 + 컨텍스트 메뉴

| 항목 | 결과 |
|------|------|
| `UInventoryComponent::Server_DropItem` RPC | ✅ 구현 |
| `UEquipmentComponent::Server_DropEquippedItem` RPC | ✅ 구현 |
| `Server_EquipFromInventory` 스왑 로직 — 인벤토리 풀 시 거부 | ✅ 구현 |
| `UItemContextMenuWidget` (`UI/ItemContextMenuWidget.h/.cpp`) | ✅ 신규 생성 |
| 컨텍스트 메뉴 버튼 가시성 — Consumable/Equipment 타입별 분기 | ✅ 구현 |
| `UInventoryIconOverlay` 우클릭 + `FindItemAtPosition` | ✅ 구현 |
| `UEquipmentSlotWidget` 우클릭 컨텍스트 메뉴 | ✅ 구현 |
| `AEXFILGameMode` + `SpawnTestWorldItems` | ✅ 신규 생성 |

**스왑 로직 핵심 순서 (설계서 9-1 반영):**
1. 슬롯 점유 확인
2. 기존 장비 → 인벤토리 복귀 시도 (실패 시 전체 거부)
3. 기존 GE 제거 + 슬롯 클리어
4. 새 아이템 인벤토리에서 차감
5. 새 장비 장착 + GE 적용

---

### 이월 항목 (Day 8)

| 항목 | 사유 |
|------|------|
| `IA_Interact` 에디터 에셋 생성 | C++ 영역 외, 에디터 수동 작업 |
| `DT_ItemData`에 `WorldMesh` 컬럼 실제 메시 세팅 | Day 8 폴리시 |
| `WBP_ItemContextMenu` 블루프린트 위젯 생성 | 에디터 UI 작업 |
| `ContextMenuWidgetClass` UPROPERTY 할당 | BP 디테일 패널 작업 |
| 인벤토리 가득 찼을 때 클라이언트 피드백 UI | Day 8 폴리시 |

---

### 신규 파일 목록

| 파일 | 용도 |
|------|------|
| `Source/Project_EXFIL/World/WorldItem.h` | WorldItem 액터 헤더 |
| `Source/Project_EXFIL/World/WorldItem.cpp` | WorldItem 액터 구현 |
| `Source/Project_EXFIL/UI/ItemContextMenuWidget.h` | 컨텍스트 메뉴 헤더 |
| `Source/Project_EXFIL/UI/ItemContextMenuWidget.cpp` | 컨텍스트 메뉴 구현 |
| `Source/Project_EXFIL/Core/EXFILGameMode.h` | 게임모드 헤더 |
| `Source/Project_EXFIL/Core/EXFILGameMode.cpp` | 게임모드 구현 |

---

## ✅ 완료 보고

**작성자:** 프로젝트 연결 탭
**작성 시점:** 2026-03-24

### 구현 완료 항목

**Phase A: AWorldItem 액터**
- [x] AWorldItem 클래스 생성 (bReplicates, SetReplicatingMovement)
- [x] UStaticMeshComponent (QueryAndPhysics, SimulatePhysics)
- [x] USphereComponent (반경 150cm, Pawn Overlap)
- [x] UTextRenderComponent (큐브 위 아이템 이름 표시)
- [x] ItemDataID (ReplicatedUsing=OnRep_ItemData), StackCount (Replicated) — COND_None
- [x] UpdateVisual(): UItemDataSubsystem 조회 → WorldMesh 또는 기본 큐브 폴백 + 이름 텍스트
- [x] FItemData에 WorldMesh 필드 추가
- [x] AEXFILGameMode::SpawnTestWorldItems (Bandage×3, WaterBottle×1, Pistol×1, Helmet×1)

**Phase B: 아이템 픽업**
- [x] IA_Interact UPROPERTY + Enhanced Input F키 바인딩
- [x] TraceForWorldItem(): 카메라 Forward 300cm 라인 트레이스
- [x] OnInteractPressed → Server_RequestPickupItem (HasAuthority 분기 없이 직접 RPC)
- [x] ExecutePickup: HasAuthority 가드 + IsPendingKillPending + 거리 검증 400cm + TryAddItemByID + Destroy

**Phase C: 드롭 + 컨텍스트 메뉴**
- [x] Server_DropItem: DecrementStack/RemoveItem 분기 + WorldItem 스폰
- [x] Server_DropEquippedItem: GE 제거 + 슬롯 비우기 + WorldItem 스폰
- [x] Server_EquipFromInventory 스왑 로직: 기존 장비 인벤토리 복귀 → 새 장비 장착
- [x] FindTargetSlot(): SlotTagToEnum 폐기, 빈 슬롯 자동 탐색 (Weapon → Weapon1/Weapon2)
- [x] UItemContextMenuWidget: Use/Equip/Unequip/Drop 버튼, 아이템 타입별 가시성 분기
- [x] IconOverlay NativeOnMouseButtonDown: 우클릭 컨텍스트 메뉴, 좌클릭 DetectDrag
- [x] EquipmentSlotWidget NativeOnMouseButtonDown: 우클릭 컨텍스트 메뉴
- [x] 메뉴 외부 클릭 / 우클릭 토글 닫힘

**Day 4 누락분 보완: SurvivalViewModel**
- [x] USurvivalViewModel 생성 (ASC 어트리뷰트 변경 델리게이트 바인딩)
- [x] OnRep_PlayerState에서 InitAbilityActorInfo + InitializeViewModels (BeginPlay 타이밍 문제 해결)
- [x] StatEntryWidget BindToViewModel + OnStatUpdated
- [x] InventoryPanelWidget::BindStatsToViewModel (HP/HU/TH/ST)
- [x] Server_ConsumeItemByID에서 GE 적용 추가

### PIE 테스트 결과 (2P Dedicated Server)

- [x] 테스트 WorldItem 4개 스폰, 양쪽 클라이언트 표시
- [x] F키 픽업 → 인벤토리 추가 + WorldItem 소멸
- [x] 인벤토리 풀 시 픽업 거부
- [x] 동시 픽업 경쟁 → 선착순 1명 획득
- [x] 컨텍스트 메뉴 전 아이템 타입 가시성 정상
- [x] Use → GE 적용 → StatsBar 갱신
- [x] Equip → 빈 슬롯 자동 탐색 + 스왑 동작
- [x] Unequip → 인벤토리 복귀 (스택 병합)
- [x] Drop → DecrementStack + WorldItem 스폰
- [x] 장비 Drop → 해제 + WorldItem 스폰 + 슬롯 클리어
- [x] 드롭 → 픽업 → 드롭 사이클 정상
- [x] 드래그&드롭 기존 기능 유지
- [x] 서버 크래시 없음

### 트러블슈팅

| 문제 | 원인 | 해결 |
|------|------|------|
| Pistol Equip 안 됨 | SlotTagToEnum이 "Weapon" 매핑 실패 | FindTargetSlot으로 교체, 빈 슬롯 자동 탐색 |
| Drop 시 전체 스택 소멸 | RemoveItem이 인스턴스 전체 제거 | DecrementStack 분기 적용 |
| 컨텍스트 메뉴 인벤토리에서 안 열림 | IconOverlay Visibility + 참조 미연결 | Visible + ContextMenu 참조 연결 |
| 드래그 안 됨 | NativeOnMouseButtonDown이 좌클릭도 Handled | 좌클릭 DetectDrag, 우클릭만 Handled |
| 멀티셀 아이템 하단 클릭 불가 | IconOverlay V-Align Top으로 높이 부족 | V-Align Fill로 변경 |
| 메뉴 위치 어긋남 | ScreenSpace → CanvasPanel 로컬 변환 누락 | AbsoluteToLocal 변환 적용 |
| StatsBar 갱신 안 됨 (GE 미적용) | ConsumeItemByID가 GE 적용 안 함 | ApplyGameplayEffectSpecToSelf 추가 |
| StatsBar 갱신 안 됨 (ViewModel null) | BeginPlay 시점 ASC 미초기화 | OnRep_PlayerState로 타이밍 이동 |
| StatsBar 갱신 안 됨 (위젯 이름) | BindWidget 이름 불일치 | WBP 위젯 이름 C++과 일치 |
| StatsBar 갱신 안 됨 (델리게이트 안 발동) | 초기값 100 = MAX로 값 변화 없음 | 초기 현재값 50으로 설정 |
| TrackFill 바 표시 오류 | Pivot 0.5 + Alignment 불일치 | Fill + Pivot (0.0, 0.5) |

### 잔여 이슈 (Day 8 폴리시로 이관)

- IconOverlay 윈도우 리사이즈 시 그리드 어긋남
- 첫 PIE 아이콘 찌그러짐 (NativePaint 카운트다운)
- 초기 스탯값 밸런싱 (현재 HP/HU/TH/ST 전부 50/100)
- WorldItem 실제 메시 교체 (현재 기본 큐브)

### Git 커밋

- `feat(Day7): WorldItem pickup/drop + context menu + SurvivalViewModel + equipment swap`
