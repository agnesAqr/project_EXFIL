# Project EXFIL

**UE5 C++ Dedicated Server 기반 멀티플레이어 서바이벌 시스템 프로토타입**

타르코프 스타일 그리드 인벤토리, GAS 통합 전투, 크래프팅 시스템을 UE 5.6 C++ Dedicated Server 아키텍처로 구현한 포트폴리오 프로젝트.

---

## 🎬 시연 영상

[![시연 영상](https://img.shields.io/badge/YouTube-시연_영상-red?logo=youtube)](https://youtu.be/OF0snQA3hdw?si=EU7B3wh0OLt1pxQ9)

---

## 📌 개요

| | |
|---|---|
| **엔진** | Unreal Engine 5.6 |
| **언어** | C++ (100%, 블루프린트 미사용) |
| **아키텍처** | Dedicated Server / Server Authority |
| **개발 기간** | 2026.03.18 - 2026.04.27 |
| **작업 인원** | 1명 |
| **주요 모듈** | GameplayAbilities · ModelViewViewModel · CommonUI · NetCore |

---

## 📁 프로젝트 구조

```
  Source/Project_EXFIL/
  ├── Core/              EXFILCharacter · EXFILGameMode · EXFILPlayerController + EXFILLog
  ├── Inventory/         InventoryComponent + FastArray/슬롯/아이템 구조체
  ├── GAS/               SurvivalAttributeSet · GA_Fire · GA_Craft · SurvivalViewModel
  ├── Crafting/          CraftingComponent
  ├── Data/              ItemDataSubsystem + Item/Crafting 데이터 타입
  │   └── Equipment/     EquipmentComponent + Equipment 슬롯 타입
  ├── UI/                MVVM ViewModel/View / 드래그드롭 / UIManager
  ├── World/             WorldItem
  ├── Project_EXFILCharacter.h
  ├── Project_EXFILGameMode.h
  ├── Project_EXFILPlayerController.h
  └── Project_EXFIL.h
```

> Core 클래스(`AEXFILCharacter` 등)는 Epic 템플릿 베이스(`AProject_EXFILCharacter` 등)를 상속해 EXFIL 도메인 로직을 확장합니다.

---

## 📊 기술 스택

| 분류 | 기술 |
|------|------|
| 엔진 | UE 5.6 |
| 언어 | C++ |
| GAS | AbilitySystemComponent (Mixed Mode), AttributeSet, GameplayAbility, GameplayEffect |
| 네트워크 | Dedicated Server, Server RPC, OnRep, Multicast, **FFastArraySerializer (NetDeltaSerialize)** |
| UI | CommonUI (UCommonActivatableWidget), **MVVM (ModelViewViewModel · FieldNotify)**, Slate NativePaint |
| 데이터 | UDataTable + CSV, UItemDataSubsystem (GameInstanceSubsystem), TSoftObjectPtr / TSoftClassPtr 지연 로드 |

---

## 🎯 핵심 시스템

| 시스템 | 설명 |
|--------|------|
| **Grid Inventory (10×12)** | 다중 크기 아이템 배치, 회전, 드래그앤드롭, 스택 병합, 비트맵 기반 O(H) 슬롯 검색 |
| **FastArray Replication** | `FInventoryFastArray` + `NetDeltaSerialize`, 변경 인덱스 단위 PostReplicatedAdd/Change/Remove 콜백 |
| **Crafting** | DataTable 레시피 기반, 원자적 재료 소모 + 실패 시 롤백, 타이머 + GA_Craft |
| **Equipment** | 6슬롯 (Head/Face/Eyewear/Body/Weapon1/Weapon2), 장착 시 GE Apply / 해제 시 Remove |
| **Survival Stats** | GAS AttributeSet 8속성 (Health, Hunger, Thirst, Stamina + Max), Pre/Post 클램핑 2단계 |
| **Line Trace Shooting** | LocalPredicted GA_Fire → Server_ConfirmHit (서버 라인 트레이스 재검증) |
| **Death / Respawn** | `ERespawnPhase` Replicated enum (Alive→Dead→HiddenDead→Respawning→Alive), OnRep 단일 경로로 late-joiner 안전 |

---

## 🏗️ 아키텍처

```
View Layer (CommonUI Widgets + Drag/Drop Overlay)
    │  Delegate / FieldNotify
ViewModel Layer (InventoryViewModel · EquipmentViewModel · CraftingViewModel · SurvivalViewModel)
    │  Delegate
Model Layer (UInventoryComponent · UCraftingComponent · UEquipmentComponent)
    │
GAS Layer (USurvivalAttributeSet · GA_Fire · GA_Craft · GE_Damage 등)
    │
Data Layer (UItemDataSubsystem · UDataTable · CSV)
    │
Network Layer (FastArray · Server RPC · OnRep · COND 전략 분리)
```

### 설계 원칙

- **MVVM 단방향:** View → ViewModel → Model 방향으로만 참조. 게임플레이 상태 동기화는 Delegate / OnRep / FieldNotify 기반이며 Tick/Timer 기반 Model polling은 금지
- **Server Authority:** 모든 게임 로직은 서버에서만 실행. 클라이언트는 Request → Server RPC → `_Internal` 3계층으로 요청만 전달
- **Data-Driven:** 아이템 / 레시피 / 장비 스펙은 DataTable CSV. 텍스처·GE 클래스는 TSoftObjectPtr / TSoftClassPtr로 지연 로드 후 Subsystem 캐시
- **Incremental Cache Patch:** FastArray의 변경 인덱스만 받아 캐시(`ItemIndexMap`, `ItemCountCache`, `RowBitmap`, `GridSlots`)를 부분 갱신

---

## ✨ 핵심 기능 상세

### FEATURE 01 — 그리드 인벤토리 + 비트맵 검색 + FastArray 델타 동기화
- 1D `TArray`로 2D 그리드를 표현하는 타르코프 스타일 인벤토리 — 멀티셀 아이템 배치, 회전, 스택 병합
- `RowBitmap (TArray<uint16>)` 기반 빈 영역 검증: **O(H)** (행당 비트 AND 1회, 현재 `GridWidth <= 16`으로 제한)
- `ItemIndexMap (TMap<FGuid, int32>)`로 인스턴스 탐색: **O(1)**
- `ItemCountCache (TMap<FName, int32>)`로 ID별 수량 합계: **O(1)**
- Replicated 데이터(`FInventoryFastArray InventoryList`) + 로컬 전용 캐시(GridSlots / TMap / Bitmap) 분리
- FastArray 콜백(`PreReplicatedRemove` / `PostReplicatedAdd` / `PostReplicatedChange` / `PostReplicatedReceive`)에서 변경분만 캐시에 패치 후 ViewModel → IconOverlay까지 dirty 인덱스 전파

### FEATURE 02 — 서버 히트 재검증 슈팅
- `GA_Fire` (LocalPredicted, InstancedPerActor) → 클라이언트가 카메라 기준 라인 트레이스 (5000cm, ECC_Pawn)
- `Server_ConfirmHit (Reliable)` → 서버에서 동일 시점 라인 트레이스 재실행 → 클라가 보고한 액터와 일치할 때만 데미지 적용
- 검증 통과 시 `MakeOutgoingSpec → ApplyGameplayEffectSpecToTarget`로 `DamageEffectClass` 적용
- 피격: `Multicast_PlayHitReact` (Unreliable) → 0.2초 오버레이 머티리얼 플래시

### FEATURE 03 — GAS 통합 파이프라인
- `USurvivalAttributeSet` 8속성 (Health/MaxHealth, Hunger/MaxHunger, Thirst/MaxThirst, Stamina/MaxStamina) — 각각 `ReplicatedUsing = OnRep_*`
- 클램핑 2단계: `PreAttributeChange` (CurrentValue 클램프) + `PostGameplayEffectExecute` (BaseValue 클램프 + Health ≤ 0 시 `OnDeath()`)
- ASC Mixed Replication: GE는 Owner Client에만, GameplayCue / Tag는 전체 전파

### FEATURE 04 — Dedicated Server 리플리케이션 전략
- Server Authority 모델 — 클라는 Request → Server RPC → `_Internal` 3계층 패턴으로만 요청
- **Server RPC 12개**(Inventory 4 · Equipment 4 · Crafting 2 · Character 2)
  - 모든 Server RPC는 `_Implementation` 내부 sanity-check + early return
  - Character RPC(`Server_ConfirmHit`, `Server_RequestPickupItem`)도 동일 정책으로 정리
  - `_Validate` 실패는 클라이언트 disconnect 성격이므로, 정상 플레이에서도 발생 가능한 요청 실패는 `_Implementation`에서 서버 상태 기준으로 거부 처리
- 리플리케이션 조건 전략 분리:
  - `COND_OwnerOnly` — Inventory(`InventoryList`), Equipment(`ReplicatedSlots`), Crafting(`bIsCrafting`, `CurrentRecipeID`)
  - `COND_None` — WorldItem, AttributeSet, ERespawnPhase (전체 가시성 필요)
- 치트 방어 3계층: 파라미터 sanity check → 서버 라인 트레이스 재검증 → `HasAuthority` 가드

### FEATURE 05 — Deferred UI Refresh + Drag & Drop
- ViewModel 갱신과 Slate 레이아웃 측정의 순서가 보장되지 않는 문제를, `bLayoutReady` + `bHasPendingOverlayRefresh` 2조건이 모두 충족된 시점에서 한 번만 flush 하는 패턴으로 해결 (데이터 동기화 polling 없음, NativePaint 트리거)
- `InventoryIconOverlay`는 `UInventoryViewModel`이 만든 overlay delta(Upsert/Remove)를 소비하고, `UniformGridPanel` 위 `CanvasPanel`에 멀티셀 아이콘을 정확한 픽셀 위치로 배치
- 드래그 시 회전(R 키) + 자동 스크롤 + 빈 영역 하이라이트(초록/빨강) 지원. 배치 가능 표시는 `bPredictedPlaceable` 기반 UX hint이며 최종 성공 여부는 서버가 재검증

---

## ⚠️ 현재 한계

- `AEXFILCharacter::BeginPlay()`의 시작 로드아웃(Bandage/Pistol/BodyArmor/Painkillers/Medkit)과 `AEXFILGameMode::SpawnTestWorldItems()`의 테스트 월드 스폰은 아직 DataTable/DataAsset로 분리되지 않은 데모용 seed 코드입니다.
- `UItemDataSubsystem`의 `LoadSynchronous()` 기반 최초 로드는 현재 데이터 규모에서는 단순성을 우선한 선택입니다. 아이템/아이콘/GE 수가 늘거나 첫 상호작용 hitch가 프로파일링에서 확인되면, 핵심 에셋은 GameInstance 초기화 단계에서 warm-up하고 나머지는 `StreamableManager` 기반 async load + 캐시 완료 콜백 구조로 전환할 예정입니다.
- `TraceForWorldItem()`의 `TActorIterator` 기반 근접 검색은 테스트 월드의 소수 아이템 기준 구현입니다. 월드 아이템 수가 증가하거나 상호작용 탐색 비용이 프레임에 영향을 주는 시점에는 overlap candidate cache 또는 `OverlapMultiByObjectType`/spatial query 기반 탐색으로 교체할 예정입니다.
- GameplayEffect는 DataTable에서 SoftClassPtr로 참조하지만, 실제 modifier 수치는 GE 에셋 내부에 분산되어 있습니다. 현재 규모에서는 빠른 제작과 GAS 파이프라인 검증을 우선한 선택이며, 아이템/장비/효과 수가 늘어나면 modifier magnitude를 DataTable/DataAsset 또는 SetByCaller 기반으로 분리해 밸런스 값을 한 곳에서 관리할 예정입니다.
- `GA_Fire`는 `LocalPredicted` 정책을 사용하지만 진짜 GAS Prediction Key 발급/롤백은 구현하지 않았습니다 — 클라 즉시 활성화 + 서버 ConfirmHit 재검증 조합입니다.

---

## 📐 클래스 구조

```mermaid
classDiagram
    direction TB

    namespace Character_Actor_Layer {
        class AEXFILCharacter {
            +UAbilitySystemComponent* ASC «Mixed Mode»
            +UInventoryComponent* InventoryComp
            +UEquipmentComponent* EquipmentComp
            +UCraftingComponent* CraftingComp
            +ERespawnPhase RespawnPhase «ReplicatedUsing»
            +PossessedBy()
            +OnRep_PlayerState()
            +Server_ConfirmHit() «Server, Reliable»
            +Server_RequestPickupItem() «Server, Reliable»
            +Multicast_PlayHitReact() «Unreliable»
            +Multicast_PlayHitEffect() «Unreliable»
            +Client_ShowNotification() «Client, Reliable»
        }
    }

    namespace Model_Component_Layer {
        class UInventoryComponent {
            +FInventoryFastArray InventoryList «Replicated, NetDeltaSerialize»
            -TArray~FInventorySlot~ GridSlots «Local»
            -TMap~FGuid,int32~ ItemIndexMap «Local»
            -TMap~FName,int32~ ItemCountCache «Local»
            -TArray~uint16~ RowBitmap «Local, GridWidth<=16»
            +Server_RequestRemoveItem() «Server, Reliable»
            +Server_RequestMoveItem() «Server, Reliable»
            +Server_RequestConsumeItemByID() «Server, Reliable»
            +Server_RequestDropItem() «Server, Reliable»
            +AddItemByID_Internal()  «Authority»
            +AreSlotsFree() O(H) bitmap
        }

        class UEquipmentComponent {
            +TArray~FEquipmentSlotData~ ReplicatedSlots «ReplicatedUsing=OnRep_Slots»
            -TMap~EEquipmentSlot,int32~ SlotIndexMap «Local»
            +Server_RequestEquipFromInventory() «Server, Reliable»
            +Server_RequestUnequipToInventory() «Server, Reliable»
            +Server_RequestUnequipToInventoryAt() «Server, Reliable»
            +Server_RequestDropEquippedItem() «Server, Reliable»
            +ApplyEquipmentEffect() / RemoveEquipmentEffect()
        }

        class UCraftingComponent {
            +bool bIsCrafting «ReplicatedUsing=OnRep_CraftingState»
            +FName CurrentRecipeID «Replicated»
            -TArray~FConsumedIngredient~ ConsumedIngredients
            +Server_RequestStartCraft() «Server, Reliable»
            +Server_RequestCancelCraft() «Server, Reliable»
            +Client_NotifyCraftStartFailed() «Client, Reliable»
        }
    }

    namespace ViewModel_Layer {
        class UInventoryViewModel {
            +TArray~UInventorySlotViewModel~ SlotViewModels
            +HandleInventoryUpdated(TSet~int32~)
            +OnViewModelRefreshed «Native Multicast Delegate»
        }

        class UInventorySlotViewModel {
            +GridPosition / ItemDataID / StackCount «FieldNotify»
            +ItemInstanceID / Icon / Size / Rotated «FieldNotify»
        }

        class UEquipmentViewModel {
            +GetSlotViewData()
            +OnEquipmentSlotChanged «Native Multicast Delegate»
        }

        class UCraftingViewModel {
            +BuildInitialRecipeListDelta()
            +OnRecipeListDeltaChanged «Native Multicast Delegate»
            +OnCraftingProgressStarted «Native Multicast Delegate»
        }

        class USurvivalViewModel {
            +OnStatChanged(EExfilStatType, Current, Max) «Dynamic Multicast Delegate»
            +InitializeWithASC()
        }
    }

    namespace Data_GAS_Layer {
        class USurvivalAttributeSet {
            +Health / MaxHealth «ReplicatedUsing»
            +Hunger / MaxHunger «ReplicatedUsing»
            +Thirst / MaxThirst «ReplicatedUsing»
            +Stamina / MaxStamina «ReplicatedUsing»
            +PreAttributeChange()
            +PostGameplayEffectExecute()
        }

        class UItemDataSubsystem {
            +UDataTable* ItemDataTable
            +UDataTable* CraftingRecipeTable
            -TMap TextureCache
            -TMap EffectClassCache
            +GetItemData(FName) FItemData*
            +GetCachedTexture() UTexture2D*
            +GetCachedEffect() UClass*
        }
    }

    AEXFILCharacter *-- UInventoryComponent
    AEXFILCharacter *-- UEquipmentComponent
    AEXFILCharacter *-- UCraftingComponent
    AEXFILCharacter --> USurvivalAttributeSet : ASC owns

    UInventoryComponent ..> UItemDataSubsystem : queries
    UEquipmentComponent ..> UItemDataSubsystem : queries
    UCraftingComponent ..> UInventoryComponent : consumes / adds items
    UCraftingComponent ..> UItemDataSubsystem : queries

    UInventoryComponent ..> UInventoryViewModel : OnInventoryUpdated
    UEquipmentComponent ..> UEquipmentViewModel : OnItemEquipped/Unequipped
    UCraftingComponent ..> UCraftingViewModel : OnCraftingStateChanged
    USurvivalAttributeSet ..> USurvivalViewModel : ASC AttributeChanged
    UInventoryViewModel *-- UInventorySlotViewModel : owns slot VMs
```

```mermaid
classDiagram
    direction LR

    class GA_Craft {
        +ActivateAbility()
        delegates → CraftingComponent.RequestStartCraft
    }

    class GA_Fire {
        +CanActivateAbility() — 무기 장착 검사
        +ActivateAbility() — LineTrace → Server_ConfirmHit
    }

    class GE_Consumable {
        <<Blueprint>>
        Duration: Instant
        Health/Hunger/Thirst +N
    }

    class GE_Equipment {
        <<Blueprint>>
        Duration: Infinite
        MaxHealth +N 등 (구조 구현)
    }

    class GE_Damage {
        <<Blueprint>>
        Duration: Instant
        Health -N
    }

    class USurvivalAttributeSet {
        PreAttributeChange()
        PostGameplayEffectExecute()
        HP ≤ 0 → OnDeath()
    }

    GA_Fire --> GE_Damage : Apply
    GE_Equipment --> USurvivalAttributeSet : Modify
    GE_Consumable --> USurvivalAttributeSet : Modify
    GE_Damage --> USurvivalAttributeSet : Modify
```

---

## 🔄 네트워크 흐름

```mermaid
sequenceDiagram
    participant C as Client
    participant S as Dedicated Server
    participant T as Other Client

    C->>S: Server RPC (Request → Server_Request)
    S->>S: Sanity check + 게임 로직 실행 (Authority)

    S-->>C: NetDeltaSerialize / OnRep (COND_OwnerOnly)
    Note over C: Inventory(FastArray) / Equipment / Crafting

    S-->>C: OnRep (COND_None)
    S-->>T: OnRep (COND_None)
    Note over C,T: WorldItem / AttributeSet / ERespawnPhase

    S->>C: Multicast (Unreliable)
    S->>T: Multicast (Unreliable)
    Note over C,T: PlayHitReact / PlayHitEffect

    S->>C: Client RPC (Reliable)
    Note over C: ShowNotification / NotifyCraftStartFailed
```

---

## 🔧 빌드 및 실행

### 요구사항
- Unreal Engine 5.6
- Visual Studio 2022 또는 JetBrains Rider
- Windows 11

### 설정
```bash
git clone https://github.com/agnesAqr/project_EXFIL.git
```
1. `Project_EXFIL.uproject` 우클릭 → Generate Visual Studio project files
2. Development Editor로 빌드
3. 에디터에서 PIE → 2 Players → Net Mode: Play As Listen Server / Dedicated Server

---

## 📄 관련 시각 자료 (`docs/Portfolio/`)

인터랙티브 HTML 다이어그램:

- [Shooting 네트워크 흐름](docs/Portfolio/ShootingNetworkFlow.html)
- [GAS 통합 파이프라인](docs/Portfolio/GASPipeline.html)
- [Replication 아키텍처](docs/Portfolio/ReplicationArch.html)
- [그리드 + Bitmap 시각화](docs/Portfolio/GridBitmapVisualization.html)
- [캐시 분리 아키텍처 (FastArray + 로컬 캐시)](docs/Portfolio/CacheSeparationArch.html)
- [ASC 초기화 타이밍](docs/Portfolio/ASCTimingDiagram.html)
- [최적화 비교표](docs/Portfolio/OptimizationTable.html)

---

## 📝 라이선스

본 프로젝트는 포트폴리오 목적으로 제작되었습니다.

---

*UE 5.6 · C++ · Dedicated Server · GAS · MVVM · FastArraySerializer*
