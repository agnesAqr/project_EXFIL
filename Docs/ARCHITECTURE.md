# ARCHITECTURE.md — Project: EXFIL 전체 아키텍처 설계서

> 이 문서는 프로젝트의 **전체 구조**를 정의합니다.
> 모든 Day의 설계 지시서는 이 아키텍처를 기반으로 작성됩니다.
> 새로운 클래스나 시스템 추가 시 반드시 이 문서를 먼저 참조하세요.

---

## 1. 시스템 전체 구조

```
┌─────────────────────────────────────────────────────────┐
│                    View Layer (Day 2)                     │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────────┐ │
│  │ Inventory    │ │ Crafting UI  │ │ Survival Status  │ │
│  │ Panel        │ │              │ │ HUD              │ │
│  │ (CommonUI)   │ │ (CommonUI)   │ │ (CommonUI)       │ │
│  └──────┬───────┘ └──────┬───────┘ └────────┬─────────┘ │
│         │                │                   │           │
├─────────┼────────────────┼───────────────────┼───────────┤
│         ▼                ▼                   ▼           │
│              ViewModel Layer (Day 2)                      │
│  ┌──────────────────┐  ┌─────────────────────────────┐  │
│  │ UInventory       │  │ USurvivalViewModel           │  │
│  │ ViewModel        │  │ (AttributeSet → UI 변환)     │  │
│  └──────┬───────────┘  └──────────┬──────────────────┘  │
│         │ Delegate                │ Delegate             │
├─────────┼─────────────────────────┼──────────────────────┤
│         ▼                         ▼                      │
│                  Model Layer (Day 1, 3, 4, 5)             │
│  ┌──────────────┐ ┌────────────┐ ┌────────────────────┐ │
│  │ UInventory   │ │ UCrafting  │ │ UEquipment         │ │
│  │ Component    │ │ Component  │ │ Component          │ │
│  │ (그리드 관리) │ │ (제작 로직)│ │ (장비 슬롯)        │ │
│  └──────┬───────┘ └─────┬──────┘ └──────┬─────────────┘ │
│         │               │               │                │
├─────────┼───────────────┼───────────────┼────────────────┤
│         ▼               ▼               ▼                │
│                  GAS Layer (Day 4-5)                      │
│  ┌───────────────────────────────────────────────────┐  │
│  │ USurvivalAttributeSet (Hunger, Thirst, Stamina)   │  │
│  │ UGameplayEffect (Instant / Infinite Duration)     │  │
│  │ UGA_Craft (GameplayAbility)                       │  │
│  └───────────────────────────────────────────────────┘  │
│                                                          │
├──────────────────────────────────────────────────────────┤
│                Data Layer (Day 3)                         │
│  ┌──────────────┐ ┌───────────────┐ ┌────────────────┐  │
│  │ UDataTable   │ │ FCraftingRecipe│ │ FEquipmentData│  │
│  │ + FItemData  │ │ DataTable     │ │ DataTable     │  │
│  │ (CSV 연동)   │ │               │ │               │  │
│  └──────────────┘ └───────────────┘ └────────────────┘  │
│                                                          │
├──────────────────────────────────────────────────────────┤
│              Network Layer (Day 6-7)                      │
│  ┌──────────────────────────────────────────────────┐   │
│  │ Replication + Server RPC + Client Prediction     │   │
│  │ (모든 레이어를 감싸는 네트워크 동기화)             │   │
│  └──────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

---

## 2. 핵심 클래스 목록 및 책임

### 2.1 데이터 구조체 (EXFILInventoryTypes.h)

| 구조체 | 역할 | Day |
|--------|------|-----|
| `FItemSize` | 아이템 그리드 크기 (Width × Height), 회전 지원 | 1 |
| `FInventorySlot` | 그리드 개별 셀. 점유 아이템 ID + 루트 여부 | 1 |
| `FInventoryItemInstance` | 아이템 런타임 인스턴스. InstanceID, 위치, 크기, 스택 | 1 |
| `FItemData` | 아이템 정적 정의 (DataTable Row). 이름, 아이콘, GE 태그 | 3 |
| `FCraftingRecipe` | 크래프팅 레시피 (DataTable Row). 재료, 결과물, 제작 시간 | 3 |
| `FEquipmentSlotInfo` | 장비 슬롯 정보. 슬롯 타입, 적용 GE 클래스 | 4 |

### 2.2 컴포넌트 (Model Layer)

| 클래스 | 부착 대상 | 역할 | Day |
|--------|----------|------|-----|
| `UInventoryComponent` | Character | 2D 그리드 인벤토리 관리. 추가/제거/이동/쿼리 | 1 |
| `UCraftingComponent` | Character | 레시피 검증, 재료 소모, 결과물 생성 | 5 |
| `UEquipmentComponent` | Character | 장비 슬롯 관리, GE Apply/Remove | 4 |

### 2.3 GAS 클래스

| 클래스 | 역할 | Day |
|--------|------|-----|
| `USurvivalAttributeSet` | Hunger, Thirst, Stamina, Health 어트리뷰트 | 4 |
| `UGE_ConsumableEffect` | 소비 아이템 사용 시 즉시 스탯 변화 (Instant) | 4 |
| `UGE_EquipmentBonus` | 장비 착용 중 지속 스탯 보너스 (Infinite Duration) | 4 |
| `UGA_Craft` | 크래프팅 GameplayAbility. 재료 검증 → 소모 → 생성 | 5 |
| `UGA_UseItem` | 아이템 사용 GameplayAbility. GE 적용 | 4 |

### 2.4 ViewModel (MVVM Layer)

| 클래스 | 구독 대상 | 역할 | Day |
|--------|----------|------|-----|
| `UInventoryViewModel` | `UInventoryComponent` 델리게이트 | 인벤토리 데이터를 UI 친화적 형태로 변환 | 2 |
| `USurvivalViewModel` | `USurvivalAttributeSet` OnAttributeChanged | 어트리뷰트 변경을 UI에 전달 | 4 |

### 2.5 View (UI Layer)

| 클래스 | 부모 클래스 | 역할 | Day |
|--------|-----------|------|-----|
| `UInventoryPanelWidget` | `UCommonActivatableWidget` | 그리드 인벤토리 패널 + 드래그앤드롭 | 2 |
| `UInventorySlotWidget` | `UCommonActivatableWidget` | 개별 그리드 셀 위젯 | 2 |
| `UCraftingPanelWidget` | `UCommonActivatableWidget` | 크래프팅 레시피 브라우저 | 5 |
| `USurvivalStatusWidget` | `UCommonActivatableWidget` | 생존 스탯 게이지 HUD | 4 |
| `UEquipmentPanelWidget` | `UCommonActivatableWidget` | 장비 슬롯 패널 | 4 |

### 2.6 Core 클래스

| 클래스 | 역할 | Day |
|--------|------|-----|
| `AEXFILCharacter` | 플레이어 캐릭터. 컴포넌트 부착 대상 | 1 |
| `AEXFILGameMode` | 게임 모드. 데이터 로딩, 월드 아이템 관리 | 3 |
| `AWorldItem` | 월드에 드랍된 아이템 액터. 네트워크 스폰/소멸 | 7 |

---

## 3. 데이터 흐름 (Data Flow)

### 3.1 아이템 획득 흐름

```
Player가 WorldItem에 Interact
  → [Client] Server RPC 호출: RequestPickupItem(WorldItemID)
  → [Server] 검증: WorldItem 존재? 거리 유효?
  → [Server] UInventoryComponent::TryAddItem() 호출
    → FindFirstAvailableSlot() → 빈칸 탐색
    → AreSlotsFree() → 공간 검증
    → OccupySlots() → 그리드 점유
    → ItemInstances에 등록
  → [Server] OnItemAdded 델리게이트 브로드캐스트
  → [Server] WorldItem Destroy (리플리케이트)
  → [Client] OnRep 또는 Client RPC → ViewModel → View 갱신
```

### 3.2 아이템 사용 흐름 (GAS 연동)

```
Player가 인벤토리에서 아이템 사용
  → [Client] Server RPC: RequestUseItem(InstanceID)
  → [Server] UInventoryComponent에서 아이템 조회
  → [Server] FItemData에서 GameplayEffect 클래스 참조
  → [Server] AbilitySystemComponent::ApplyGameplayEffect()
  → [Server] USurvivalAttributeSet 값 변경
    → PreAttributeChange() → 클램핑
    → PostGameplayEffectExecute() → 후처리
  → [Server] UInventoryComponent::RemoveItem() (소비)
  → [Both] OnAttributeChanged → USurvivalViewModel → UI 갱신
```

### 3.3 장비 착탈 흐름

```
Player가 장비 슬롯에 아이템 장착
  → [Server] UEquipmentComponent::EquipItem(SlotType, ItemInstance)
  → [Server] FItemData에서 Equipment GE 클래스 참조
  → [Server] ApplyGameplayEffectToSelf(GE, Infinite Duration)
    → ActiveGEHandle 저장
  → [Server] 슬롯 상태 리플리케이트
  → [Client] UI 갱신 (장비 아이콘 표시, 스탯 변화)

Player가 장비 해제
  → [Server] UEquipmentComponent::UnequipItem(SlotType)
  → [Server] RemoveActiveGameplayEffect(SavedHandle)
  → [Server] 인벤토리로 아이템 복귀
```

### 3.4 크래프팅 흐름

```
Player가 크래프팅 UI에서 레시피 선택 후 제작
  → [Client] Server RPC: RequestCraft(RecipeID)
  → [Server] UCraftingComponent::CanCraft(RecipeID) 검증
    → FCraftingRecipe에서 필요 재료 목록 확인
    → UInventoryComponent에서 각 재료 수량 확인
  → [Server] UCraftingComponent::ExecuteCraft(RecipeID)
    → UGA_Craft Ability 활성화
    → 재료 아이템들 UInventoryComponent::RemoveItem()
    → 결과물 UInventoryComponent::TryAddItem()
    → 쿨타임 적용 (GameplayEffect)
  → [Both] 인벤토리 변경 → ViewModel → UI 갱신
```

---

## 4. 네트워크 아키텍처 (Day 6-7)

### 4.1 권한 모델

```
서버 (Authority)                   클라이언트 (Autonomous Proxy)
─────────────────                  ───────────────────────────
모든 인벤토리 조작 실행              UI 표시 + 입력 수집
아이템 존재/수량 검증                Server RPC로 요청 전달
GameplayEffect Apply               클라이언트 예측 (UI 즉시 반영)
WorldItem 스폰/소멸                 서버 응답 후 확정/롤백
```

### 4.2 리플리케이션 전략

| 데이터 | 방식 | 비고 |
|--------|------|------|
| 생존 스탯 (AttributeSet) | `GAMEPLAYATTRIBUTE_REPNOTIFY` | GAS 내장 리플리케이션 |
| 인벤토리 그리드 | `FFastArraySerializer` 또는 `TArray` + `DOREPLIFETIME` | TMap은 직접 리플리케이션 불가 |
| 장비 슬롯 상태 | `DOREPLIFETIME` | 간단한 구조체 배열 |
| 월드 아이템 | Actor Replication | `bReplicates = true` |

### 4.3 RPC 목록 (예정)

| RPC | 방향 | 용도 |
|-----|------|------|
| `ServerRequestPickupItem` | Client → Server | 아이템 획득 요청 |
| `ServerRequestDropItem` | Client → Server | 아이템 투기 요청 |
| `ServerRequestMoveItem` | Client → Server | 인벤토리 내 이동 요청 |
| `ServerRequestUseItem` | Client → Server | 아이템 사용 요청 |
| `ServerRequestEquipItem` | Client → Server | 장비 착용 요청 |
| `ServerRequestUnequipItem` | Client → Server | 장비 해제 요청 |
| `ServerRequestCraft` | Client → Server | 크래프팅 요청 |
| `ClientConfirmAction` | Server → Client | 서버 승인 후 확정 알림 |

---

## 5. 폴더별 파일 매핑 (예정)

```
Source/EXFIL/
├── Core/
│   ├── EXFILCharacter.h/.cpp          ← 플레이어 캐릭터
│   ├── EXFILGameMode.h/.cpp           ← 게임 모드
│   └── EXFILTypes.h                   ← 프로젝트 공통 타입
│
├── Inventory/
│   ├── EXFILInventoryTypes.h          ← 인벤토리 구조체 (Day 1)
│   ├── InventoryComponent.h/.cpp      ← 그리드 인벤토리 (Day 1)
│   └── InventoryViewModel.h/.cpp      ← MVVM ViewModel (Day 2)
│
├── GAS/
│   ├── SurvivalAttributeSet.h/.cpp    ← 생존 어트리뷰트 (Day 4)
│   ├── SurvivalViewModel.h/.cpp       ← 스탯 ViewModel (Day 4)
│   ├── GE_ConsumableEffect.h/.cpp     ← 소비 아이템 GE (Day 4)
│   ├── GE_EquipmentBonus.h/.cpp       ← 장비 보너스 GE (Day 4)
│   ├── GA_UseItem.h/.cpp              ← 아이템 사용 Ability (Day 4)
│   └── GA_Craft.h/.cpp                ← 크래프팅 Ability (Day 5)
│
├── Crafting/
│   ├── CraftingTypes.h                ← FCraftingRecipe (Day 3)
│   └── CraftingComponent.h/.cpp       ← 크래프팅 로직 (Day 5)
│
├── Equipment/
│   ├── EquipmentTypes.h               ← FEquipmentSlotInfo (Day 4)
│   └── EquipmentComponent.h/.cpp      ← 장비 슬롯 관리 (Day 4)
│
├── Data/
│   ├── ItemData.h                     ← FItemData DataTable Row (Day 3)
│   └── DataTables/                    ← CSV 파일들
│
├── UI/
│   ├── InventoryPanelWidget.h/.cpp    ← 인벤토리 패널 (Day 2)
│   ├── InventorySlotWidget.h/.cpp     ← 개별 슬롯 위젯 (Day 2)
│   ├── CraftingPanelWidget.h/.cpp     ← 크래프팅 UI (Day 5)
│   ├── SurvivalStatusWidget.h/.cpp    ← 스탯 HUD (Day 4)
│   └── EquipmentPanelWidget.h/.cpp    ← 장비 패널 (Day 4)
│
└── World/
    └── WorldItem.h/.cpp               ← 월드 드랍 아이템 (Day 7)
```

---

## 6. Build.cs 의존성

```csharp
// EXFIL.Build.cs
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "InputCore",
    "EnhancedInput",

    // UI (Day 2)
    "UMG",
    "SlateCore",
    "Slate",
    "CommonUI",
    "CommonInput",

    // GAS (Day 4)
    "GameplayAbilities",
    "GameplayTags",
    "GameplayTasks",

    // Network (Day 6)
    "NetCore"
});
```

> **주의:** Day 1에서는 Core, CoreUObject, Engine, InputCore, EnhancedInput만 필요.
> 나머지 모듈은 해당 Day에 도달했을 때 추가.

---

## 7. 설계 원칙 요약

1. **MVVM 단방향 흐름:** View → ViewModel → Model. 역방향 절대 금지.
2. **이벤트 드리븐:** Tick 배제. Delegate/Callback으로만 UI 갱신.
3. **서버 권한:** 모든 게임 로직은 서버에서 실행. 클라이언트는 요청만.
4. **Data-Driven:** 하드코딩 금지. DataTable + CSV로 모든 게임 데이터 관리.
5. **단일 책임:** 각 컴포넌트는 하나의 시스템만 담당. 크로스 기능은 델리게이트로 연결.
6. **확장 가능성:** Day N의 구현이 Day N+1의 변경을 최소화하도록 설계.
