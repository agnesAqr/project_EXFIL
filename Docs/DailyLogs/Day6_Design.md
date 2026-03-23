# Day 6 설계 지시서: Replication 1 — Inventory · Equipment · Crafting 네트워크 동기화

> 채팅 탭(Opus)이 상단 "설계 지시서" 섹션을 작성 → 수현이 검토 후 Docs/DailyLogs/Day6_Design.md로 저장
> 프로젝트 연결 탭이 하단 "완료 보고"를 작성 → 수현이 채팅 탭에 전달

---

## ⚠️ Day 5 반영사항

### 반영 1: Day 5에서 완료된 사항

- [x] WBP_InventoryPanel 전면 개편 (CanvasPanel_Root + Anchor 비율 + HBox Equipment/Grid)
- [x] WBP_EquipmentSlot 6개 + WBP_StatEntry 4개 배치
- [x] UCraftingComponent: CanCraft, StartCraft, CancelCraft
- [x] UGA_Craft GameplayAbility
- [x] UInventoryComponent: ConsumeItemByID, GetItemCountByID 추가
- [x] WBP_CraftingPanel + WBP_CraftingRecipe + WidgetSwitcher 탭 전환
- [x] BP 토글: UIOnly 모드 + NativeOnKeyDown I키 닫기

### 반영 2: 이미 Replication-Ready한 코드

Day 4에서 `USurvivalAttributeSet`에 이미 `ReplicatedUsing` + `GAMEPLAYATTRIBUTE_REPNOTIFY` + `GetLifetimeReplicatedProps`를 구현 완료함. Day 6에서는 **별도 수정 없이** Dedicated Server 2P에서 어트리뷰트 동기화가 동작해야 함.

`AEXFILCharacter::PossessedBy()`에서 `InitAbilityActorInfo()` 호출 구조도 준비됨.

### 반영 4: Dedicated Server 아키텍처 채택

**Listen Server가 아닌 Dedicated Server를 기준으로 구현합니다.**

선택 이유:
- 타르코프 스타일 PvPvE에서 Listen Server는 호스트에게 레이턴시 0 + 서버 데이터 직접 접근이라는 불공정한 이점 발생
- 상용 멀티플레이어 게임은 거의 전부 Dedicated Server 사용
- Nexon 낙원 채용공고의 "네트워크 코드" 우대사항에서 Dedicated Server 경험이 더 높은 평가

**Dedicated Server vs Listen Server 코드 차이점:**

| 항목 | Listen Server | Dedicated Server |
|------|-------------|-----------------|
| 서버에 화면 | 있음 (호스트 플레이어) | **없음** — UI 코드 실행 시 크래시 |
| OnRep 호출 | 호스트에서 미호출 (서버+클라이언트 겸용) | **모든 클라이언트에서 정상 호출** |
| UI 생성 가드 | 선택적 | **IsLocallyControlled() 필수** |
| PIE 테스트 | Net Mode = Play As Listen Server | **Net Mode = Play As Client** |

**추가 구현 사항 (30분~1시간):**
- `AEXFILCharacter::BeginPlay()`에서 UI 위젯 생성 코드를 `if (IsLocallyControlled())` 로 감싸기
- `OnInventoryUpdated` 등 델리게이트에 UI 위젯 바인딩하는 코드도 클라이언트에서만 실행
- 서버에서 UI 관련 코드가 실행되지 않도록 전체 검증

### 반영 3: 현재 컴포넌트 리플리케이션 상태

| 컴포넌트 | Replicated | Server RPC | 현재 상태 |
|---------|-----------|-----------|---------|
| USurvivalAttributeSet | ✅ 완료 | GAS 내부 | Day 4에서 완료 |
| UInventoryComponent | ❌ 로컬 전용 | ❌ | Day 6에서 구현 |
| UEquipmentComponent | ❌ 로컬 전용 | ❌ | Day 6에서 구현 |
| UCraftingComponent | ❌ 로컬 전용 | ❌ | Day 6에서 구현 |

---

## 📋 Day 6 설계 지시서

**작성자:** 채팅 탭 (Opus 4.6)
**WBS 분류:** Replication 1
**목표 시간:** 15h
**날짜:** 2026-03-23
**권장 모델:** Opus 4.6 (Replication/RPC 패턴은 복잡한 상속 구조 + 서버 권한 로직. GAS Replication Mode 설정 포함)

## 1. 오늘의 목표 (3개 페이즈)

**Phase A: 인벤토리 리플리케이션 (5~6h)**
- [x] `UInventoryComponent`에 `GetLifetimeReplicatedProps` 추가
- [x] `FInventoryItemInstance` 리플리케이션 가능 구조로 변환
- [x] `TArray<FInventoryItemInstance> Items` → `DOREPLIFETIME_CONDITION` (COND_OwnerOnly) 등록
- [x] `TArray<FInventorySlot> GridSlots` → `DOREPLIFETIME_CONDITION` (COND_OwnerOnly) 등록
- [x] 인벤토리 변경 → Server RPC 패턴 적용
- [x] `OnRep_Items()` → 클라이언트 UI 갱신

**Phase B: 장비 + 크래프팅 리플리케이션 (5~6h)**
- [x] `UEquipmentComponent`에 리플리케이션 추가
- [x] 장착/해제 → Server RPC
- [x] `UCraftingComponent` 서버 권한 검증
- [x] 크래프팅 시작/취소 → Server RPC

**Phase C: ASC Replication Mode + Dedicated Server 테스트 (3~4h)**
- [x] ASC Replication Mode 설정 (Mixed)
- [ ] Dedicated Server 테스트 환경 구축 (PIE: Play As Client) — 빌드 후 테스트 필요
- [x] `IsLocallyControlled()` 가드 추가 (UI 생성, 델리게이트 바인딩)
- [ ] 전체 시스템 네트워크 검증 — 빌드 후 PIE 테스트 필요

---

## 2. 생성할 파일 목록

| 파일 경로 | 용도 | 신규/수정 |
|-----------|------|----------|
| `Source/Project_EXFIL/Inventory/InventoryComponent.h/.cpp` | DOREPLIFETIME_CONDITION(COND_OwnerOnly) + Server RPC 추가 | 수정 |
| `Source/Project_EXFIL/Inventory/EXFILInventoryTypes.h` | FInventoryItemInstance에 USTRUCT 리플리케이션 지원 확인 | 수정 |
| `Source/Project_EXFIL/Equipment/EquipmentComponent.h/.cpp` | 리플리케이션 + Server RPC 추가 | 수정 |
| `Source/Project_EXFIL/Crafting/CraftingComponent.h/.cpp` | Server RPC 추가 | 수정 |
| `Source/Project_EXFIL/Core/EXFILCharacter.h/.cpp` | ASC ReplicationMode, 컴포넌트 Replicate 설정 | 수정 |
| `Source/Project_EXFIL/Project_EXFIL.Build.cs` | NetCore 모듈 확인 (보통 불필요) | 확인 |

---

## 3. Phase A: 인벤토리 리플리케이션

### 3-1. 리플리케이션 아키텍처 원칙

**서버 권한(Server Authority) 모델:**
- 모든 인벤토리 변경(Add, Remove, Move, Consume)은 **서버에서만 실행**
- 클라이언트는 Server RPC를 통해 **요청**만 함
- 서버가 검증 후 실행 → 리플리케이션으로 클라이언트에 결과 전파
- 클라이언트 UI는 `OnRep` 콜백에서 갱신

```
[Client]                     [Server]
   │                            │
   │── Server_TryAddItem() ───→ │
   │                            │── TryAddItemByID() (실제 실행)
   │                            │── Items/GridSlots 변경
   │←── OnRep_Items() ─────────│ (Replicated)
   │── UI 갱신                   │
```

### 3-2. UInventoryComponent 수정

**헤더 추가:**
```cpp
// InventoryComponent.h 수정

// === Replicated 프로퍼티 ===
UPROPERTY(ReplicatedUsing = OnRep_Items)
TArray<FInventoryItemInstance> Items;

UPROPERTY(Replicated)
TArray<int32> GridSlots;

// === Server RPCs ===
UFUNCTION(Server, Reliable, WithValidation)
void Server_TryAddItemByID(FName ItemDataID, int32 StackCount);

UFUNCTION(Server, Reliable, WithValidation)
void Server_RemoveItem(FGuid ItemInstanceID);

UFUNCTION(Server, Reliable, WithValidation)
void Server_MoveItem(FGuid ItemInstanceID, int32 NewRootIndex);

UFUNCTION(Server, Reliable, WithValidation)
void Server_ConsumeItemByID(FName ItemDataID, int32 Count);

// === OnRep ===
UFUNCTION()
void OnRep_Items();

// === Replication ===
virtual void GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const override;
```

### 3-3. GetLifetimeReplicatedProps 구현

```cpp
#include "Net/UnrealNetwork.h"

void UInventoryComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 인벤토리는 소유자 클라이언트에게만 전파 — 다른 플레이어는 내 인벤토리를 볼 필요 없음
    DOREPLIFETIME_CONDITION(UInventoryComponent, Items, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UInventoryComponent, GridSlots, COND_OwnerOnly);
}
```

**COND_OwnerOnly 선택 근거:**
- 인벤토리 데이터는 해당 캐릭터를 소유한 클라이언트에게만 필요
- 4인 세션 기준, 무조건 전파(COND_None) 대비 **네트워크 대역폭 75% 절감** (4명 → 1명에게만 전송)
- 다른 플레이어의 인벤토리를 보는 기능(거래 시스템 등)이 필요하면 별도 RPC로 요청하는 구조가 더 효율적

### 3-4. Server RPC 구현 패턴

**Server_TryAddItemByID:**
```cpp
bool UInventoryComponent::Server_TryAddItemByID_Validate(
    FName ItemDataID, int32 StackCount)
{
    // 기본 검증: 유효한 아이템 ID인지
    return !ItemDataID.IsNone() && StackCount > 0;
}

void UInventoryComponent::Server_TryAddItemByID_Implementation(
    FName ItemDataID, int32 StackCount)
{
    // 서버에서만 실행되는 기존 로직 호출
    TryAddItemByID(ItemDataID, StackCount);
    // Items/GridSlots가 Replicated이므로 자동으로 클라이언트에 전파
}
```

**Server_MoveItem:**
```cpp
bool UInventoryComponent::Server_MoveItem_Validate(
    FGuid ItemInstanceID, int32 NewRootIndex)
{
    return ItemInstanceID.IsValid() && NewRootIndex >= 0;
}

void UInventoryComponent::Server_MoveItem_Implementation(
    FGuid ItemInstanceID, int32 NewRootIndex)
{
    // 서버에서 MoveItem 실행
    MoveItem(ItemInstanceID, NewRootIndex);
}
```

**Server_RemoveItem / Server_ConsumeItemByID:** 동일 패턴.

### 3-5. OnRep_Items — 클라이언트 UI 갱신

```cpp
void UInventoryComponent::OnRep_Items()
{
    // 기존 OnInventoryUpdated 델리게이트 브로드캐스트
    OnInventoryUpdated.Broadcast();
}
```

**핵심:** 기존 싱글플레이어 로직에서 `OnInventoryUpdated.Broadcast()`를 직접 호출하던 부분은 유지. 서버에서는 직접 호출(서버 로직 내부), 클라이언트에서는 `OnRep_Items()`를 통해 호출.

**Dedicated Server 참고:** Dedicated Server에서는 모든 플레이어가 리모트 클라이언트이므로 `OnRep`이 정상적으로 호출됩니다. 서버 프로세스에는 로컬 플레이어가 없으므로, 서버에서의 `Broadcast`는 UI가 아닌 서버 사이드 로직(예: AI 반응)에만 영향을 줍니다.

### 3-6. 기존 함수 수정 — Authority 검사 추가

기존 `TryAddItemByID`, `RemoveItem`, `MoveItem`, `ConsumeItemByID`에 Authority 검사를 추가:

```cpp
bool UInventoryComponent::TryAddItemByID(FName ItemDataID, int32 StackCount)
{
    // 클라이언트에서 호출 시 → Server RPC로 포워딩
    if (!GetOwner()->HasAuthority())
    {
        Server_TryAddItemByID(ItemDataID, StackCount);
        return true; // 낙관적 반환 (실제 결과는 OnRep에서)
    }

    // === 기존 서버 로직 (변경 없음) ===
    // ...
    
    OnInventoryUpdated.Broadcast(); // 서버에서 직접 UI 갱신
    return true;
}
```

**이 패턴의 장점:** 기존 코드에서 `TryAddItemByID`를 호출하는 모든 곳(BeginPlay 테스트, UseItem, CraftingComponent 등)이 **자동으로** 서버 권한 경로를 탐. 호출자가 서버/클라이언트를 구분할 필요 없음.

### 3-7. FInventoryItemInstance 리플리케이션 확인

`USTRUCT(BlueprintType)`에 `GENERATED_BODY()`가 있으면 자동으로 리플리케이션 가능. 내부의 모든 `UPROPERTY()`가 Replicated 대상.

**확인 사항:** `FInventoryItemInstance`의 모든 멤버가 `UPROPERTY()`로 선언되어 있는지 검증. `FGuid`는 기본 리플리케이션 지원됨.

---

## 4. Phase B: 장비 + 크래프팅 리플리케이션

### 4-1. UEquipmentComponent 수정

```cpp
// EquipmentComponent.h 수정

// === Replicated 프로퍼티 ===
UPROPERTY(ReplicatedUsing = OnRep_Slots)
TArray<FEquipmentSlotData> ReplicatedSlots;
// 주의: TMap은 리플리케이션 불가 → TArray로 변환

// === Server RPCs ===
UFUNCTION(Server, Reliable, WithValidation)
void Server_EquipItem(EEquipmentSlot Slot, FGuid ItemInstanceID);

UFUNCTION(Server, Reliable, WithValidation)
void Server_UnequipItem(EEquipmentSlot Slot);

// === OnRep ===
UFUNCTION()
void OnRep_Slots();

// === Replication ===
virtual void GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const override;
```

### 4-2. TMap → TArray 변환

**문제:** `TMap<EEquipmentSlot, FEquipmentSlotData>`는 UE에서 직접 리플리케이션할 수 없음.

**해결:** 내부 저장소를 `TArray<FEquipmentSlotData>`로 변경. `FEquipmentSlotData`에 이미 `SlotType` 필드가 있으므로, 배열 인덱스 대신 `SlotType`으로 검색.

```cpp
// EquipmentComponent.cpp

// 기존 TMap 접근:
// FEquipmentSlotData& Slot = Slots[SlotType];

// 변경 후 TArray 접근:
FEquipmentSlotData* UEquipmentComponent::FindSlotData(EEquipmentSlot SlotType)
{
    return ReplicatedSlots.FindByPredicate(
        [SlotType](const FEquipmentSlotData& S) { return S.SlotType == SlotType; });
}
```

**InitializeSlots() 수정:**
```cpp
void UEquipmentComponent::InitializeSlots()
{
    ReplicatedSlots.Empty();
    ReplicatedSlots.Add(FEquipmentSlotData{EEquipmentSlot::Head});
    ReplicatedSlots.Add(FEquipmentSlotData{EEquipmentSlot::Face});
    ReplicatedSlots.Add(FEquipmentSlotData{EEquipmentSlot::Eyewear});
    ReplicatedSlots.Add(FEquipmentSlotData{EEquipmentSlot::Body});
    ReplicatedSlots.Add(FEquipmentSlotData{EEquipmentSlot::Weapon1});
    ReplicatedSlots.Add(FEquipmentSlotData{EEquipmentSlot::Weapon2});
}
```

**GetLifetimeReplicatedProps:**
```cpp
void UEquipmentComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 장비 슬롯도 소유자에게만 — 다른 플레이어에게는 외형 메시로 간접 표시
    DOREPLIFETIME_CONDITION(UEquipmentComponent, ReplicatedSlots, COND_OwnerOnly);
}
```

### 4-3. EquipItem Authority 패턴

```cpp
bool UEquipmentComponent::EquipItem(EEquipmentSlot Slot, FGuid ItemInstanceID)
{
    if (!GetOwner()->HasAuthority())
    {
        Server_EquipItem(Slot, ItemInstanceID);
        return true;
    }

    // === 기존 서버 로직 ===
    FEquipmentSlotData* SlotData = FindSlotData(Slot);
    if (!SlotData || !SlotData->IsEmpty()) return false;

    // ... (기존 GE Apply 로직 유지)

    OnEquipmentChanged.Broadcast(Slot, ItemInstanceID);
    return true;
}
```

### 4-4. FEquipmentSlotData 리플리케이션 제약

**ActiveGEHandle 제외:** `FActiveGameplayEffectHandle`은 서버에서만 유의미. 클라이언트에서는 GAS가 자체적으로 GE를 동기화함. `ActiveGEHandle`은 `UPROPERTY()`에서 `NotReplicated` 또는 리플리케이션 대상에서 제외.

```cpp
USTRUCT(BlueprintType)
struct FEquipmentSlotData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EEquipmentSlot SlotType = EEquipmentSlot::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGuid EquippedItemID;

    // 서버 전용 — 리플리케이션 제외
    UPROPERTY(NotReplicated)
    FActiveGameplayEffectHandle ActiveGEHandle;

    bool IsEmpty() const { return !EquippedItemID.IsValid(); }
};
```

### 4-5. UCraftingComponent Server RPC

```cpp
// CraftingComponent.h 추가

UFUNCTION(Server, Reliable, WithValidation)
void Server_StartCraft(FName RecipeID);

UFUNCTION(Server, Reliable, WithValidation)
void Server_CancelCraft();

// 크래프팅 상태 리플리케이션
UPROPERTY(ReplicatedUsing = OnRep_CraftingState)
bool bIsCrafting = false;

UPROPERTY(Replicated)
FName CurrentRecipeID;

UFUNCTION()
void OnRep_CraftingState();

// === Replication ===
virtual void GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const override;
```

**GetLifetimeReplicatedProps:**
```cpp
void UCraftingComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 크래프팅 상태는 소유자에게만 — 다른 플레이어는 내 크래프팅 진행 상태를 볼 필요 없음
    DOREPLIFETIME_CONDITION(UCraftingComponent, bIsCrafting, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UCraftingComponent, CurrentRecipeID, COND_OwnerOnly);
}
```

**StartCraft Authority 패턴:**
```cpp
bool UCraftingComponent::StartCraft(FName RecipeID)
{
    if (!GetOwner()->HasAuthority())
    {
        Server_StartCraft(RecipeID);
        return true;
    }

    // === 기존 서버 로직 ===
    if (!CanCraft(RecipeID)) return false;
    // 재료 소비, 타이머 시작...
    
    bIsCrafting = true; // Replicated → 클라이언트에 자동 전파
    OnCraftingStateChanged.Broadcast(true, Recipe.CraftDuration);
    return true;
}
```

**OnRep_CraftingState:**
```cpp
void UCraftingComponent::OnRep_CraftingState()
{
    // 클라이언트에서 UI 갱신
    if (bIsCrafting)
    {
        const FCraftingRecipe* Recipe = /* ... */;
        float Duration = Recipe ? Recipe->CraftDuration : 0.f;
        OnCraftingStateChanged.Broadcast(true, Duration);
    }
    else
    {
        OnCraftingStateChanged.Broadcast(false, 0.f);
    }
}
```

---

## 5. Phase C: ASC Replication Mode + 테스트

### 5-1. ASC Replication Mode 설정

```cpp
// EXFILCharacter.cpp — 생성자
AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(
    TEXT("AbilitySystemComponent"));
AbilitySystemComponent->SetIsReplicated(true);
AbilitySystemComponent->SetReplicationMode(
    EGameplayEffectReplicationMode::Mixed);
```

**Mixed 모드 선택 이유:**
- `Full`: 모든 GE가 모든 클라이언트에 리플리케이션 — MMORPG 수백 명에서 비용 과다
- `Mixed`: GE는 Owner 클라이언트에만, Tag/Cue는 모두에게 — **PvPvE 서바이벌에 적합**
- `Minimal`: GE 리플리케이션 없음 — NPC/AI 전용

**Mixed 모드 주의사항:** `OwnerActor`의 Owner가 Controller여야 함. `ACharacter`는 `PossessedBy()`에서 자동으로 Owner가 Controller로 설정되므로 기본 동작에서 문제없음.

### 5-2. 컴포넌트 Replicate 설정

**AEXFILCharacter 생성자에서:**
```cpp
// 이미 있는 컴포넌트 → SetIsReplicated(true) 추가
InventoryComponent->SetIsReplicated(true);
EquipmentComponent->SetIsReplicated(true);
CraftingComponent->SetIsReplicated(true);
```

**AEXFILCharacter::GetLifetimeReplicatedProps:**
```cpp
void AEXFILCharacter::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    // 캐릭터 자체의 리플리케이션 프로퍼티가 있으면 여기에 추가
}
```

### 5-3. bReplicates 설정

```cpp
// EXFILCharacter.cpp 생성자
bReplicates = true;
// ACharacter는 기본적으로 bReplicates = true이지만 명시적으로 확인
```

### 5-4. Dedicated Server 전용: IsLocallyControlled 가드

Dedicated Server 프로세스에는 화면이 없으므로, 서버에서 UI 위젯을 생성하거나 접근하면 크래시합니다.

**AEXFILCharacter::BeginPlay 수정:**
```cpp
void AEXFILCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocallyControlled()) // Dedicated Server에서 서버 프로세스 제외
    {
        // 인벤토리 위젯 생성
        // ViewModel 초기화 + UI 델리게이트 바인딩
        // 카메라/입력 설정 등 로컬 전용 코드
    }

    // 서버 전용 로직 (아이템 초기 지급 등)은 HasAuthority()로 감싸기
    if (HasAuthority())
    {
        if (UInventoryComponent* Inv = FindComponentByClass<UInventoryComponent>())
        {
            Inv->TryAddItemByID(FName("Bandage"), 3);
            // ...
        }
    }
}
```

**델리게이트 바인딩도 클라이언트에서만:**
```cpp
// OnInventoryUpdated → UI 갱신은 클라이언트에서만 바인딩
// OnCraftingCompleted → UI 갱신은 클라이언트에서만 바인딩
// SurvivalViewModel 초기화도 클라이언트에서만
```

### 5-5. Dedicated Server 2P 테스트 체크리스트

PIE 설정: `Number of Players = 2`, **`Net Mode = Play As Client`**
(에디터가 백그라운드에서 Dedicated Server를 자동으로 실행하고, 2개 클라이언트 창이 열림)

| 테스트 항목 | 검증 방법 | 예상 결과 |
|-----------|---------|---------|
| 서버 크래시 없음 | PIE 실행 시 서버 프로세스 Output Log 확인 | UI 관련 크래시 없음 |
| 어트리뷰트 동기화 | 클라이언트에서 `showdebug abilitysystem` | 양쪽 클라이언트 동일한 값 |
| 아이템 추가 | 클라이언트에서 아이템 획득 | 서버 인벤토리에 반영 |
| 아이템 이동 | 클라이언트에서 드래그&드롭 | 서버 GridSlots 변경 |
| 장비 장착 | 클라이언트에서 장비 슬롯에 드롭 | 서버 GE Apply + 슬롯 동기화 |
| 장비 해제 | 클라이언트에서 장비 해제 | 서버 GE Remove + 인벤토리 복귀 |
| 크래프팅 시작 | 클라이언트에서 레시피 클릭 | 서버에서 재료 소비 + 타이머 |
| 크래프팅 완료 | 서버 타이머 완료 | 클라이언트 인벤토리에 결과물 |
| 크래프팅 취소 | 클라이언트에서 취소 | 서버에서 재료 복구 |
| 크로스 클라이언트 | Client1에서 아이템 사용 | Client1만 인벤토리 변경, Client2 무관 |

---

## 6. 의존성 & 연결 관계

- `UInventoryComponent` → `Net/UnrealNetwork.h` (DOREPLIFETIME_CONDITION 매크로)
- `UEquipmentComponent` → `TMap` → `TArray` 변환 (리플리케이션 호환)
- `UCraftingComponent` → `UInventoryComponent` Server RPC 경유 (서버에서 ConsumeItemByID 직접 호출)
- `AEXFILCharacter` → ASC `SetReplicationMode(Mixed)` + 컴포넌트 `SetIsReplicated(true)`
- Day 7: 맵 드롭 아이템(Actor) 스폰/소멸 네트워크 동기화, 아이템 획득 인터랙션 Server RPC

---

## 7. 주의사항 & 제약

- **`*.generated.h`는 반드시 마지막 include**
- **TMap은 리플리케이션 불가:** `UEquipmentComponent`의 `TMap<EEquipmentSlot, FEquipmentSlotData>` → `TArray<FEquipmentSlotData>`로 변환 필수
- **`WithValidation` 필수:** `Server` RPC에는 반드시 `_Validate` 함수 구현. 치트 방지의 첫 번째 방어선.
- **OnRep은 서버 프로세스에서 호출되지 않음:** Dedicated Server에서는 모든 플레이어가 리모트 클라이언트이므로 `OnRep`이 전원에게 정상 호출됨. 서버 프로세스 자체에는 `OnRep`이 호출되지 않지만, 서버에는 UI가 없으므로 문제없음.
- **IsLocallyControlled() 가드 필수:** Dedicated Server에서 UI 생성/접근 코드가 서버 프로세스에서 실행되면 크래시. `BeginPlay`, 델리게이트 바인딩 등에서 반드시 `IsLocallyControlled()` 체크.
- **FActiveGameplayEffectHandle은 NotReplicated:** GAS가 GE를 자체적으로 리플리케이션하므로 Handle을 클라이언트에 보낼 필요 없음.
- **낙관적 반환 vs 비관적 반환:** Day 6에서는 `return true` (낙관적). Day 7~8에서 클라이언트 예측(Prediction) + 롤백 도입 시 수정.
- **DOREPLIFETIME_CONDITION 즉시 적용:** 인벤토리/장비/크래프팅은 모두 `COND_OwnerOnly` 확정. 소유자 클라이언트에게만 전파하여 4인 세션 기준 대역폭 75% 절감. 거래 등 다른 플레이어 인벤토리 열람이 필요하면 별도 RPC로 요청하는 구조.
- **UActorComponent의 리플리케이션:** `SetIsReplicated(true)` + `GetLifetimeReplicatedProps` 오버라이드 필수. 컴포넌트의 `UPROPERTY(Replicated)` / `UPROPERTY(ReplicatedUsing)`은 이 두 조건이 모두 충족되어야 동작.

---

## 8. 검증 기준 (Done Criteria)

### Phase A: 인벤토리
- [ ] UBT 빌드 성공
- [ ] Dedicated Server 2P(Play As Client)에서 서버 크래시 없음
- [ ] 클라이언트에서 아이템 이동(드래그&드롭) → 서버 GridSlots 동기화
- [ ] 클라이언트에서 아이템 사용(UseItem) → 서버 인벤토리에서 소멸
- [ ] 서버에서 아이템 추가(BeginPlay) → 클라이언트 UI 자동 갱신

### Phase B: 장비 + 크래프팅
- [ ] TMap → TArray 변환 후 기존 장비 기능 정상 동작 (단일 플레이어)
- [ ] 클라이언트에서 장비 장착 → 서버 GE Apply + 클라이언트 스탯 변경 확인
- [ ] 클라이언트에서 장비 해제 → 서버 GE Remove + 인벤토리 복귀
- [ ] 클라이언트에서 크래프팅 시작 → 서버에서 재료 소비 확인
- [ ] 서버 타이머 완료 → 클라이언트 인벤토리에 결과물 추가
- [ ] 크래프팅 취소 → 재료 복구 양쪽 확인

### Phase C: 통합
- [ ] ASC Mixed 모드에서 GE Instant (Bandage) 적용 → 클라이언트 Health 변경
- [ ] ASC Mixed 모드에서 GE Infinite (장비) → 클라이언트 스탯 반영
- [ ] `IsLocallyControlled()` 가드 적용 → Dedicated Server 프로세스에서 UI 크래시 없음
- [ ] Dedicated Server 2P에서 전체 시나리오 통과: 아이템 획득 → 장비 장착 → 아이템 사용 → 크래프팅

---

## ✅ 완료 보고

**작성자:** 프로젝트 연결 탭 (Opus 4.6)
**작성 시점:** 2026-03-23

### 1. 완료 항목

**Phase A: 인벤토리 리플리케이션**
- [x] `UInventoryComponent`에 `GetLifetimeReplicatedProps` 추가
- [x] `TMap<FGuid, FInventoryItemInstance>` → `TArray<FInventoryItemInstance> Items` 변환 (TMap 리플리케이션 불가)
- [x] `Items` → `DOREPLIFETIME_CONDITION(COND_OwnerOnly)` 등록
- [x] `GridSlots` → `DOREPLIFETIME_CONDITION(COND_OwnerOnly)` 등록
- [x] `SetIsReplicatedByDefault(true)` 생성자 설정
- [x] Server RPC 4개: `Server_TryAddItemByID`, `Server_RemoveItem`, `Server_MoveItem`, `Server_ConsumeItemByID`
- [x] 모든 Server RPC에 `_Validate` 함수 구현
- [x] `OnRep_Items()` → 클라이언트 UI 갱신 (`OnInventoryUpdated.Broadcast()`)
- [x] 기존 함수 5개에 Authority 검사 + Server RPC 포워딩 패턴 적용
- [x] TMap→TArray 전환 헬퍼: `FindItemByInstanceID`, `FindItemIndexByInstanceID`
- [x] 서버에서만 `InitializeGrid()` 실행 (BeginPlay HasAuthority 가드)

**Phase B: 장비 리플리케이션**
- [x] `FEquipmentSlotData` 구조체 리팩토링 (bIsOccupied → EquippedItemID + IsEmpty(), ActiveGEHandle NotReplicated)
- [x] `TMap<EEquipmentSlot, FInventoryItemInstance>` → `TArray<FEquipmentSlotData> ReplicatedSlots`
- [x] `TMap<EEquipmentSlot, FActiveGameplayEffectHandle>` → `FEquipmentSlotData::ActiveGEHandle`로 통합
- [x] `DOREPLIFETIME_CONDITION(COND_OwnerOnly)` 등록
- [x] Server RPC: `Server_EquipItem`, `Server_UnequipItem` + Validate
- [x] `OnRep_Slots()` → 클라이언트 장비 UI 갱신
- [x] `EquipItem`/`UnequipItem`에 Authority 검사 + Server RPC 포워딩
- [x] `FindSlotData()` 헬퍼 (TArray 검색)
- [x] `InitializeSlots()` — 서버 BeginPlay에서 6슬롯 초기화
- [x] `EquipmentSlotWidget` bIsOccupied → IsEmpty() 전환

**Phase B: 크래프팅 리플리케이션**
- [x] `bIsCrafting` → `ReplicatedUsing = OnRep_CraftingState`, COND_OwnerOnly
- [x] `CurrentRecipeID` → `Replicated`, COND_OwnerOnly
- [x] Server RPC: `Server_StartCraft`, `Server_CancelCraft` + Validate
- [x] `OnRep_CraftingState()` → 클라이언트 UI 상태 갱신
- [x] `StartCraft`/`CancelCraft`에 Authority 검사 + Server RPC 포워딩

**Phase C: ASC + Dedicated Server**
- [x] `AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed)`
- [x] `AbilitySystemComponent->SetIsReplicated(true)`
- [x] `bReplicates = true` (ACharacter 생성자 명시)
- [x] 3개 컴포넌트 `SetIsReplicatedByDefault(true)` (각 생성자)
- [x] `IsLocallyControlled()` 가드: InventoryViewModel 생성, UI 위젯 생성, CraftingPanel 바인딩
- [x] `HasAuthority()` 가드: 아이템 초기 지급(BeginPlay), InitializeGrid, InitializeSlots
- [x] `UEquipmentComponent`를 AEXFILCharacter 생성자에 `CreateDefaultSubobject`로 추가

**핫픽스 A: 인벤토리 ↔ 장비슬롯 드래그 장착/해제**
- [x] `UInventoryDragDropOp`에 `bFromEquipment`, `SourceEquipmentSlot` 필드 추가
- [x] `WBP_EquipmentSlot`: `NativeOnDrop`(인벤→장비 장착), `NativeOnDragDetected`(장비→인벤 해제) 구현
- [x] 인벤토리 그리드: `bFromEquipment` 분기 추가 (해제 시 `Server_UnequipToInventory` RPC)
- [x] `Server_EquipFromInventory` / `Server_UnequipToInventory`: 원자적 서버 연산 + 슬롯 타입 검증
- [x] `SlotTagToEnum()` 헬퍼: `FItemData::EquipmentSlotTag`(FName) → `EEquipmentSlot` 변환
- [x] `EquipmentSlotWidget`: `NativeConstruct`에서 `EquipmentComponent` 자동 바인딩 + `OnItemEquipped`/`OnItemUnequipped` 구독
- [x] `RefreshSlot`에서 `ItemDataSubsystem` → `Icon` 텍스처 로드 → `SetBrushFromTexture` 적용

**핫픽스 B: TryAddItemByID 스택 병합**
- [x] `TryAddItemByID`에 동일 `ItemDataID` 검색 → `StackCount < MaxStackCount` 시 기존 스택에 병합
- [x] 전량 병합 완료 시 즉시 리턴, 남은 수량만 새 슬롯에 배치

**핫픽스 버그 수정 3건**
- [x] 버그 1: 장착 시 스택 전체 사라짐 → `DecrementStack` 신규 API (`StackCount > 1`이면 차감만, `==1`이면 `RemoveItem`)
- [x] 버그 2: 해제 시 장비슬롯 비주얼 잔류 → `OnRep_Slots`에서 빈 슬롯도 `OnItemUnequipped` 브로드캐스트
- [x] 버그 3: 해제 시 스택 병합 → `TryAddItemByID` 스택 병합 로직과 연동 확인 완료

### 2. PIE 테스트 결과 (2P Dedicated Server)
- [x] Pistol×2 → 장비 드래그 → 인벤토리 Pistol×1 잔여
- [x] 장비 → 인벤토리 드래그 → 슬롯 클리어 + Pistol×2 병합
- [x] StackCount=1 아이템 장착 → 인벤토리 완전 제거
- [x] 잘못된 슬롯 타입 드래그 → 거부
- [x] 크래프팅 결과물 스택 병합 동작
- [x] 양쪽 클라이언트 독립 동작, 서버 크래시 없음
- [x] LogNet HostClosedConnection → PIE 종료 시 정상 로그 확인

### 3. 미완료 / 내일로 이월
- 클라이언트 예측(Prediction) + 롤백: Day 7~8에서 도입 예정

### 4. 트러블슈팅
| 문제 | 원인 | 해결 |
|------|------|------|
| TMap 리플리케이션 불가 | UE5 TMap은 DOREPLIFETIME 미지원 | InventoryComponent: TMap→TArray 변환 + FindByPredicate 헬퍼 |
| TMap 리플리케이션 불가 (장비) | EquippedItems, ActiveEffectHandles 둘 다 TMap | TArray<FEquipmentSlotData>로 통합, GEHandle은 NotReplicated |
| FEquipmentSlotData 필드 변경 | bIsOccupied 제거 → IsEmpty() 메서드 | EquipmentSlotWidget의 3곳 bIsOccupied → !IsEmpty() 전환 |
| EquipmentComponent C++ 미생성 | BP에서만 추가되어 있었음 | AEXFILCharacter 생성자에 CreateDefaultSubobject 추가 |
| UFUNCTION 파라미터명 Slot 섀도잉 | UWidget에 Slot 멤버 존재 | 파라미터명 Slot → InSlot으로 변경 |
| ScaleBox 내 Image 늘어남 | Image_ItemIcon의 ScaleBox Slot 정렬이 Fill/Fill | HAlign/VAlign → Center/Center로 WBP에서 수정 |
| 장착 시 스택 전체 사라짐 | RemoveItem이 인스턴스 전체 제거 | DecrementStack API 신규 추가, 1개만 차감 |
| 해제 시 슬롯 비주얼 잔류 | OnRep_Slots에서 빈 슬롯 미브로드캐스트 | 빈 슬롯도 OnItemUnequipped 브로드캐스트 추가 |

### 5. 파일 구조 변경사항
| 파일 | 변경 유형 | 변경 내용 요약 |
|------|----------|---------------|
| `Inventory/InventoryComponent.h` | 수정 | Server RPC 4개, Replicated 프로퍼티, OnRep, TArray 헬퍼, DecrementStack 추가 |
| `Inventory/InventoryComponent.cpp` | 전면 수정 | TMap→TArray, Authority 가드, Server RPC, 스택 병합, DecrementStack |
| `Inventory/EXFILInventoryTypes.h` | 확인 | 모든 UPROPERTY 리플리케이션 호환 확인 |
| `Data/Equipment/EquipmentTypes.h` | 수정 | FEquipmentSlotData 리팩토링 (ActiveGEHandle NotReplicated) |
| `Data/Equipment/EquipmentComponent.h` | 수정 | Server RPC 4개, ReplicatedSlots, 복합 RPC, SlotTagToEnum |
| `Data/Equipment/EquipmentComponent.cpp` | 전면 수정 | TMap→TArray, 복합 RPC(EquipFromInventory/UnequipToInventory), OnRep 빈 슬롯 처리 |
| `Crafting/CraftingComponent.h` | 수정 | Server RPC, Replicated 상태, OnRep |
| `Crafting/CraftingComponent.cpp` | 수정 | Authority 가드, Server RPC 구현 |
| `Core/EXFILCharacter.h` | 수정 | EquipmentComponent 멤버, GetEquipmentComponent() |
| `Core/EXFILCharacter.cpp` | 수정 | bReplicates, ASC Mixed, IsLocallyControlled/HasAuthority 가드 |
| `UI/InventoryDragDropOp.h` | 수정 | bFromEquipment, SourceEquipmentSlot 필드 추가 |
| `UI/EquipmentSlotWidget.h` | 수정 | NativeConstruct/Destruct, 델리게이트 콜백, RefreshFromEquipmentComponent |
| `UI/EquipmentSlotWidget.cpp` | 수정 | EquipmentComponent 자동 바인딩, 장착/해제 드래그 로직, 아이콘 텍스처 로드 |
| `UI/InventorySlotWidget.cpp` | 수정 | NativeOnDrop bFromEquipment 분기 추가 |

### 6. 잔여 이슈 (Day 8 폴리시로 이관)
- IconOverlay 윈도우 리사이즈 시 그리드 어긋남 (고정 해상도 시연으로 회피 가능)
- 첫 PIE 아이콘 찌그러짐 (NativePaint 카운트다운)

### 7. 다음 Day 참고사항
- `EquipmentComponent`가 C++ CreateDefaultSubobject로 추가됨. 기존 BP에서 별도 추가했다면 중복 확인 필요
- 클라이언트 예측(Prediction)은 미구현. 현재는 낙관적 반환(return true) + 서버 결과 OnRep으로 갱신
- 거래 등 다른 플레이어 인벤토리 열람이 필요하면 별도 RPC로 요청하는 구조 필요 (COND_OwnerOnly 때문)
- Day 7: 맵 드롭 아이템(Actor) 스폰/소멸 네트워크 동기화, 아이템 획득 인터랙션 Server RPC

### 8. Git 커밋
- `feat(Day6): Dedicated Server replication + inventory-equipment drag equip/unequip + stack merge`
