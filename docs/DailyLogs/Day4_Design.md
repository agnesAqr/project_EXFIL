# Day 4 설계 지시서: UI Fix + GAS Survival Stats + Equipment

> 채팅 탭(Opus)이 상단 "설계 지시서" 섹션을 작성 → 수현이 검토 후 Docs/DailyLogs/Day4_Design.md로 저장
> 프로젝트 연결 탭이 하단 "완료 보고"를 작성 → 수현이 채팅 탭에 전달

---

## ⚠️ Day 3 완료 보고 반영사항

### 반영 1: `FSoftClassPath` → `TSoftClassPtr<UGameplayEffect>` 복원

Day 3에서 UHT가 전방 선언된 클래스를 `TSoftClassPtr` 템플릿 인자로 인식 불가하여 `FSoftClassPath`로 우회했음. Day 4에서 `GameplayAbilities` 모듈 추가 후 **반드시 복원**:

```cpp
// EXFILItemTypes.h — Before (Day 3):
UPROPERTY(...) FSoftClassPath ConsumableEffect;
UPROPERTY(...) FSoftClassPath EquipmentEffect;

// After (Day 4):
#include "GameplayEffect.h"
UPROPERTY(...) TSoftClassPtr<UGameplayEffect> ConsumableEffect;
UPROPERTY(...) TSoftClassPtr<UGameplayEffect> EquipmentEffect;
```

### 반영 2: `UItemDataSubsystem` 접근 패턴

ViewModel(UObject)에서 `GetWorld()` 직접 호출 불가. Day 3에서 확립된 패턴:
```cpp
InventoryComp->GetWorld()->GetGameInstance()->GetSubsystem<UItemDataSubsystem>()
```

### 반영 3: FieldNotify Getter/Setter 규칙 (필수)

```cpp
// float: Getter="GetHealth"
// bool: Getter="IsAlive" (Get 없이 Is/Has 사용)
// Getter 단독 사용 절대 금지
```

### 반영 4: WBP 중복 파일 정리

`Content/UI/` vs `Content/UI/Inventory/` 중복 WBP 확인 후 사용 중인 쪽 유지, 나머지 삭제.

### 반영 5: DT_ItemData 40종 확장

Day 3에서 아이템이 40종으로 확장됨. ConsumableEffect/EquipmentEffect 열은 Day 4에서 채워야 함.

---

## 📋 Day 4 설계 지시서

**작성자:** 채팅 탭 (Opus 4.6)
**WBS 분류:** GAS + 장비 슬롯 + UI 수정
**목표 시간:** 15h
**날짜:** 2026-03-20
**권장 모델:** Opus 4.6 (GAS 보일러플레이트 + 복잡한 상속 구조)

## 1. 오늘의 목표 (우선순위 순)

**Phase A: 인벤토리 UI 전면 개편 (3~4시간 예상)**
- [ ] WBP_InventoryPanel 루트를 CanvasPanel_Root로 변경 (풀스크린 보장)
- [ ] Border_Background: Anchor 0,0~1,1 반투명 검정 풀스크린 덮개
- [ ] Border_Panel: Anchor 0.03,0.03~0.97,0.97 비율 기반 인벤토리 패널
- [ ] HorizontalBox: Equipment(Fill 0.55) + Grid(Fill 0.45) 좌우 분할
- [ ] Equipment 패널: CanvasPanel + UEquipmentSlotWidget 6개 인스턴스 배치
- [ ] 멀티셀 아이콘 스패닝: Grid 영역의 Overlay 내 CanvasPanel 아이콘 레이어
- [ ] **모든 UI는 Anchor 비율 기반 배치 (고정 픽셀 SizeBox 금지)**

**Phase B: GAS 기반 시스템 (8시간 예상)**
- [ ] Build.cs에 `GameplayAbilities`, `GameplayTasks` 모듈 추가
- [ ] `EXFILItemTypes.h`의 `FSoftClassPath` → `TSoftClassPtr<UGameplayEffect>` 복원
- [ ] `USurvivalAttributeSet` 구현 (Health, MaxHealth, Hunger, MaxHunger, Thirst, MaxThirst, Stamina, MaxStamina)
- [ ] `UAbilitySystemComponent`를 캐릭터에 부착 + `USurvivalAttributeSet` 등록
- [ ] 초기값 설정용 `GE_DefaultAttributes` GameplayEffect (BP 또는 C++)
- [ ] 소비 아이템용 `UGA_UseItem` GameplayAbility 기초 구현

**Phase C: 장비 시스템 (3시간 예상)**
- [ ] `UEquipmentComponent` 구현 (Head, Face, Eyewear, Body, Weapon1, Weapon2 슬롯)
- [ ] `UEquipmentSlotWidget` 구현 (모든 슬롯 공유 UI 위젯)
- [ ] WBP_EquipmentSlot 생성 + CanvasPanel_Equipment에 6개 인스턴스 배치
- [ ] 장착 시 Infinite Duration GE Apply / 해제 시 Remove
- [ ] 그리드 ↔ 장비슬롯 드래그앤드롭 (UInventoryDragDropOp 재사용)
- [ ] `UStatEntryWidget` + WBP_StatEntry 구현 (StatsBar 하단 HP/Hunger/Thirst/Stamina)

**Phase D: 선택적 — 시간 여유 시 (1시간)**
- [ ] `USurvivalViewModel` + UI HUD 기초

---

## 2. 생성할 파일 목록

### Phase A: UI 수정

| 파일 경로 | 용도 | 신규/수정 |
|-----------|------|----------|
| `Source/Project_EXFIL/UI/InventoryIconOverlay.h` | CanvasPanel 기반 아이콘 오버레이 위젯 | 신규 |
| `Source/Project_EXFIL/UI/InventoryIconOverlay.cpp` | 오버레이 구현 | 신규 |
| `Source/Project_EXFIL/UI/InventoryPanelWidget.h/.cpp` | CanvasPanel 오버레이 통합, 팝업 형태 수정 | 수정 |
| `Source/Project_EXFIL/UI/InventorySlotWidget.h/.cpp` | 루트 슬롯 아이콘 표시 제거 (오버레이로 이동) | 수정 |
| WBP_InventoryPanel | 레이아웃 수정 | 수정 |

### Phase B: GAS

| 파일 경로 | 용도 | 신규/수정 |
|-----------|------|----------|
| `Source/Project_EXFIL/GAS/SurvivalAttributeSet.h` | 생존 어트리뷰트 | 신규 |
| `Source/Project_EXFIL/GAS/SurvivalAttributeSet.cpp` | AttributeSet 구현 | 신규 |
| `Source/Project_EXFIL/GAS/GA_UseItem.h` | 아이템 사용 GameplayAbility | 신규 |
| `Source/Project_EXFIL/GAS/GA_UseItem.cpp` | 구현 | 신규 |
| `Source/Project_EXFIL/Data/EXFILItemTypes.h` | FSoftClassPath → TSoftClassPtr 복원 | 수정 |
| `Source/Project_EXFIL/Core/EXFILCharacter.h/.cpp` | ASC + AttributeSet 부착, 초기화 | 수정 |
| `Source/Project_EXFIL/Project_EXFIL.Build.cs` | GameplayAbilities, GameplayTasks 추가 | 수정 |

### Phase C: 장비

| 파일 경로 | 용도 | 신규/수정 |
|-----------|------|----------|
| `Source/Project_EXFIL/Equipment/EquipmentTypes.h` | EEquipmentSlot 열거형(6종), FEquipmentSlotData | 신규 |
| `Source/Project_EXFIL/Equipment/EquipmentComponent.h` | 장비 컴포넌트 | 신규 |
| `Source/Project_EXFIL/Equipment/EquipmentComponent.cpp` | 구현 | 신규 |
| `Source/Project_EXFIL/UI/EquipmentSlotWidget.h` | 장비 슬롯 UI 위젯 (모든 슬롯 공유) | 신규 |
| `Source/Project_EXFIL/UI/EquipmentSlotWidget.cpp` | 구현 | 신규 |
| `Content/UI/Inventory/WBP_EquipmentSlot.uasset` | UEquipmentSlotWidget 서브클래스 WBP | 신규 |
| `Source/Project_EXFIL/UI/StatEntryWidget.h/.cpp` | 재사용 스탯 바 위젯 (HP/Hunger/Thirst/Stamina) | 신규 |
| `Content/UI/Inventory/WBP_StatEntry.uasset` | UStatEntryWidget 서브클래스 WBP | 신규 |

---

## 3. Phase A: 인벤토리 UI 전면 개편

### 3-1. WBP_InventoryPanel 전체 레이아웃

```
CanvasPanel_Root (루트)
├── Border_Background (Anchor 0,0~1,1 = 반투명 검정 풀스크린 덮개)
│
└── Border_Panel (Anchor 0.03,0.03~0.97,0.97 = 화면 94% 비율 패널)
    └── HorizontalBox
        ├── Border_Equipment (Fill: 0.55 = 좌측 55%)
        │   └── CanvasPanel_Equipment
        │       ├── 캐릭터 실루엣 이미지 (Anchor 0.25,0.1~0.75,0.7 중앙)
        │       ├── EquipSlot_Head (Anchor 비율 배치)
        │       ├── EquipSlot_Face
        │       ├── EquipSlot_Eyewear
        │       ├── EquipSlot_Body
        │       ├── EquipSlot_Weapon1
        │       ├── EquipSlot_Weapon2
        │       └── StatsBar (Anchor 0.03,0.88~0.97,0.98)
        │
        └── Border_Grid (Fill: 0.45 = 우측 45%)
            └── Overlay
                ├── GridPanel (UniformGridPanel — 슬롯 배경/하이라이트)
                └── IconOverlay (WBP_InventoryIconOverlay — 아이콘 레이어)
```

### 3-2. 위젯별 Anchor/설정 상세

| 위젯 | 타입 | Anchor Min/Max | 주요 설정 |
|------|------|----------------|----------|
| CanvasPanel_Root | CanvasPanel | — | 루트 위젯 |
| Border_Background | Border | 0,0 ~ 1,1 | Color: (0,0,0,0.6), 자식 없음, Hit Testable |
| Border_Panel | Border | 0.03,0.03 ~ 0.97,0.97 | Color: (0.12,0.12,0.12,0.95), Align: 0.5,0.5, Pad: 8 |
| HorizontalBox | HorizontalBox | — | 좌/우 분할 |
| Border_Equipment | Border | HBox Fill: 0.55 | Pad: 8, Color: (0.10,0.10,0.10,1) |
| Border_Grid | Border | HBox Fill: 0.45 | Pad: 4, Color: (0.08,0.08,0.08,1) |
| Overlay | Overlay | — | GridPanel + IconOverlay 겹침 |
| GridPanel | UniformGridPanel | Align: Fill/Fill | 슬롯 배경, 드래그 대상 |
| IconOverlay | WBP_IconOverlay | Align: Fill/Fill | Not Hit-Testable, 아이콘만 |

### 3-3. 반응형 UI 규칙 (프로젝트 전역)

- **고정 픽셀(SizeBox) 사용 금지:** 모든 UI는 Anchor 비율 기반 배치
- **CanvasPanel + Anchor Min/Max로 화면 비율 유지**
- **셀 크기도 비율 기반:** 슬롯 크기는 부모 패널에서 GridWidth/GridHeight로 계산
- **테스트 기준:** 1080p, 1440p, 4K에서 비율 동일 유지
- **이 규칙은 Day 5 이후 모든 UI에도 적용**

### 3-4. 멀티셀 아이콘 스패닝

**핵심 로직:**
- `UInventoryIconOverlay` C++ 클래스: `RefreshIcons()` 함수에서 ViewModel의 모든 루트 아이템 순회
- 각 아이콘: `CanvasSlot->SetPosition(FVector2D(rootX * CellSize, rootY * CellSize))`
- 각 아이콘: `CanvasSlot->SetSize(FVector2D(itemW * CellSize, itemH * CellSize))`
- `InventorySlotWidget`에서 아이콘 표시 로직 제거 → 슬롯은 배경/하이라이트/드롭만 담당
- `OnInventoryUpdated` 델리게이트에서 `RefreshIcons()` 호출

### 3-5. BP 토글 수정

기존 `SetAnchorsInViewport`, `SetPositionInViewport` 호출을 **삭제**. CanvasPanel_Root가 풀스크린을 처리:

```
I 키 (Pressed)
    └── IsInViewport?
        ├── True  → RemoveFromParent → SetInputMode(GameOnly) → ShowCursor(false)
        └── False → AddToViewport(ZOrder: 10) → SetInputMode(GameAndUI, DoNotLock) → ShowCursor(true)
```

---

## 4. Phase B: GAS 기반 시스템

### 4-1. Build.cs 수정

```csharp
// Day 4 추가
"GameplayAbilities",
"GameplayTasks"
```

### 4-2. USurvivalAttributeSet

```cpp
// SurvivalAttributeSet.h
#pragma once
#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "SurvivalAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class PROJECT_EXFIL_API USurvivalAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    USurvivalAttributeSet();

    // === Health ===
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Health")
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Health")
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, MaxHealth)

    // === Hunger ===
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Hunger, Category = "Attributes|Survival")
    FGameplayAttributeData Hunger;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, Hunger)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHunger, Category = "Attributes|Survival")
    FGameplayAttributeData MaxHunger;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, MaxHunger)

    // === Thirst ===
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Thirst, Category = "Attributes|Survival")
    FGameplayAttributeData Thirst;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, Thirst)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxThirst, Category = "Attributes|Survival")
    FGameplayAttributeData MaxThirst;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, MaxThirst)

    // === Stamina ===
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Attributes|Survival")
    FGameplayAttributeData Stamina;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, Stamina)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Attributes|Survival")
    FGameplayAttributeData MaxStamina;
    ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, MaxStamina)

    // === Replication ===
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute,
        float& NewValue) override;
    virtual void PostGameplayEffectExecute(
        const FGameplayEffectModCallbackData& Data) override;

private:
    UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldHealth);
    UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
    UFUNCTION() void OnRep_Hunger(const FGameplayAttributeData& OldHunger);
    UFUNCTION() void OnRep_MaxHunger(const FGameplayAttributeData& OldMaxHunger);
    UFUNCTION() void OnRep_Thirst(const FGameplayAttributeData& OldThirst);
    UFUNCTION() void OnRep_MaxThirst(const FGameplayAttributeData& OldMaxThirst);
    UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldStamina);
    UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);
};
```

### 4-3. 핵심 함수 구현 가이드

**`PreAttributeChange(Attribute, NewValue)`:**
- 목적: 어트리뷰트 변경 전 클램핑
- 핵심 로직:
  - Health: `NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth())`
  - Hunger, Thirst, Stamina 동일
- 주의: `PreAttributeChange`는 CurrentValue만 클램핑. BaseValue는 별도 처리.

**`PostGameplayEffectExecute(Data)`:**
- 목적: GE 적용 후 후처리 (사망 판정, 스탯 분배 등)
- 핵심 로직:
  - Health가 0 이하 → 사망 이벤트 발화 (현재는 로그만)
  - 각 어트리뷰트 Final Clamp

**`GetLifetimeReplicatedProps`:**
- 각 어트리뷰트에 `DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, Health, COND_None, REPNOTIFY_Always)`

**OnRep 함수들:**
- `GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, Health, OldHealth)` 매크로 호출

### 4-4. 캐릭터에 ASC + AttributeSet 부착

```cpp
// AEXFILCharacter.h 에 추가
#include "AbilitySystemInterface.h"

class PROJECT_EXFIL_API AEXFILCharacter : public ACharacter, public IAbilitySystemInterface
{
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<USurvivalAttributeSet> SurvivalAttributes;

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
};
```

생성자에서:
```cpp
AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>("AbilitySystemComponent");
SurvivalAttributes = CreateDefaultSubobject<USurvivalAttributeSet>("SurvivalAttributes");
```

`PossessedBy` 또는 `BeginPlay`에서:
```cpp
AbilitySystemComponent->InitAbilityActorInfo(this, this);
```

### 4-5. 소비 아이템 연동 개념

Day 3의 `FItemData`에 `ConsumableEffect` 필드가 있음. 아이템 사용 시:
1. `UItemDataSubsystem`에서 `FItemData` 조회
2. `ConsumableEffect` 클래스 로드
3. `AbilitySystemComponent->ApplyGameplayEffectToSelf()` 호출
4. `UInventoryComponent::RemoveItem()` (소비)

Day 4에서는 `UGA_UseItem`의 기초 구조만 만들고, 주요 로직은 직접 C++로 처리.

---

## 5. Phase C: 장비 시스템

### 5-1. EEquipmentSlot

```cpp
UENUM(BlueprintType)
enum class EEquipmentSlot : uint8
{
    None      UMETA(DisplayName = "None"),
    Head      UMETA(DisplayName = "Head"),
    Face      UMETA(DisplayName = "Face"),
    Eyewear   UMETA(DisplayName = "Eyewear"),
    Body      UMETA(DisplayName = "Body"),
    Weapon1   UMETA(DisplayName = "Weapon1"),
    Weapon2   UMETA(DisplayName = "Weapon2")
};
```

### 5-1b. UEquipmentSlotWidget — 장비 슬롯 UI 위젯

모든 장비 슬롯이 동일한 기본 구조를 공유하므로 **하나의 C++ 위젯을 재사용**.

**WBP_EquipmentSlot 계층:**
```
WBP_EquipmentSlot (UEquipmentSlotWidget 서브클래스)
└─ Border_Slot (테두리 + 배경색)
    └─ Overlay
        ├─ Image_ItemIcon (장착된 아이템 아이콘, 미장착 시 Hidden)
        ├─ TextBlock_SlotLabel (좌상단, "Head" / "Body" 등)
        └─ TextBlock_ItemName (하단, 아이템명, 미장착 시 Hidden)
```

**C++ 클래스:**
```cpp
UCLASS()
class PROJECT_EXFIL_API UEquipmentSlotWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    // 에디터에서 슬롯 타입 지정 (WBP 인스턴스마다 다르게 설정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
    EEquipmentSlot SlotType = EEquipmentSlot::None;

    // EquipmentComponent 연동
    void RefreshSlot(const FEquipmentSlotData& SlotData);

protected:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> Border_Slot;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_ItemIcon;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TextBlock_SlotLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TextBlock_ItemName;

    // 드래그앤드롭 (인벤토리 ↔ 장비 슬롯)
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
};
```

**배치:** WBP_EquipmentSlot 하나를 만들고, CanvasPanel_Equipment 안에 6개 인스턴스를 배치. 각 인스턴스의 디테일 패널에서 `SlotType`만 다르게 설정.

| 인스턴스 | SlotType | Anchor Min → Max |
|----------|----------|------------------|
| EquipSlot_Head | Head | (0.02, 0.05) → (0.22, 0.25) |
| EquipSlot_Face | Face | (0.78, 0.05) → (0.98, 0.25) |
| EquipSlot_Eyewear | Eyewear | (0.78, 0.28) → (0.98, 0.48) |
| EquipSlot_Body | Body | (0.02, 0.28) → (0.22, 0.58) |
| EquipSlot_Weapon1 | Weapon1 | (0.02, 0.62) → (0.48, 0.85) |
| EquipSlot_Weapon2 | Weapon2 | (0.52, 0.62) → (0.98, 0.85) |

**드래그앤드롭:** 기존 `UInventoryDragDropOp`을 그대로 재사용. 그리드 → 장비슬롯 드롭 시 SlotType과 아이템의 EquipmentSlotTag 일치 확인 후 EquipItem() 호출. 장비슬롯 → 그리드 드롭 시 UnequipItem() 호출.

### 5-1c. WBP_EquipmentSlot 내부 위젯 상세 설정

**계층 구조:**
```
Border_Slot (root)
└─ Overlay
    ├─ Image_ItemIcon
    ├─ TextBlock_SlotLabel
    └─ TextBlock_ItemName
```

**Border_Slot (root)**

| 설정 | 값 |
|------|-----|
| 타입 | Border |
| Background Color | (0.08, 0.08, 0.08, 1.0) |
| Padding | 4px all |
| Brush → Border Color | (1, 1, 1, 0.1) |
| Is Variable | true (check) |

**Image_ItemIcon**

| 설정 | 값 |
|------|-----|
| 타입 | Image |
| Overlay Slot H-Align | Fill |
| Overlay Slot V-Align | Fill |
| Overlay Slot Padding | 8px all |
| Stretch | Scale to Fit (비율 유지) |
| Visibility | Hidden (default, 미장착) |
| Is Variable | true (check) |
| BindWidget | 필수 |

**TextBlock_SlotLabel**

| 설정 | 값 |
|------|-----|
| 타입 | TextBlock |
| Text | "HEAD" (기본값, C++에서 SlotType에 따라 변경) |
| Font Size | 10 |
| Font Weight | Bold |
| Color | (1, 1, 1, 0.35) |
| Overlay Slot H-Align | Left |
| Overlay Slot V-Align | Top |
| Overlay Slot Padding | L:4 T:2 R:0 B:0 |
| Is Variable | true (check) |
| BindWidget | 필수 |

**TextBlock_ItemName**

| 설정 | 값 |
|------|-----|
| 타입 | TextBlock |
| Text | "" (비어있음) |
| Font Size | 10 |
| Color | (1, 1, 1, 0.6) |
| Overlay Slot H-Align | Center |
| Overlay Slot V-Align | Bottom |
| Overlay Slot Padding | L:0 T:0 R:0 B:4 |
| Visibility | Hidden (default, 미장착) |
| Is Variable | true (check) |
| BindWidget | BindWidgetOptional |

**상태별 Border_Slot 색상 (C++ RefreshSlot에서 적용):**

| 상태 | Background Color | Border Color |
|------|-----------------|--------------|
| Empty | (0.08, 0.08, 0.08, 1.0) | (1, 1, 1, 0.1) |
| Equipped | (0.05, 0.25, 0.15, 1.0) | (0.12, 0.63, 0.43, 0.4) |
| Drag hover (valid) | (0.08, 0.18, 0.3, 1.0) | (0.22, 0.54, 0.87, 0.5) |
| Drag hover (invalid) | (0.25, 0.08, 0.08, 1.0) | (0.87, 0.18, 0.18, 0.4) |

**SlotLabel 자동 설정 (NativeOnInitialized):**
```cpp
static const TMap<EEquipmentSlot, FText> SlotLabels = {
    {EEquipmentSlot::Head, NSLOCTEXT("Equip", "Head", "HEAD")},
    {EEquipmentSlot::Face, NSLOCTEXT("Equip", "Face", "FACE")},
    {EEquipmentSlot::Eyewear, NSLOCTEXT("Equip", "Eye", "EYEWEAR")},
    {EEquipmentSlot::Body, NSLOCTEXT("Equip", "Body", "BODY")},
    {EEquipmentSlot::Weapon1, NSLOCTEXT("Equip", "W1", "WEAPON 1")},
    {EEquipmentSlot::Weapon2, NSLOCTEXT("Equip", "W2", "WEAPON 2")},
};
if (const FText* Label = SlotLabels.Find(SlotType))
    TextBlock_SlotLabel->SetText(*Label);
```

**RefreshSlot() 로직:**
```cpp
void UEquipmentSlotWidget::RefreshSlot(const FEquipmentSlotData& Data)
{
    if (Data.IsEmpty())
    {
        Image_ItemIcon->SetVisibility(ESlateVisibility::Hidden);
        if (TextBlock_ItemName) TextBlock_ItemName->SetVisibility(ESlateVisibility::Hidden);
        Border_Slot->SetBrushColor(EmptyColor);  // (0.08, 0.08, 0.08, 1.0)
    }
    else
    {
        // UItemDataSubsystem에서 아이콘 + 이름 조회
        Image_ItemIcon->SetBrushFromTexture(LoadedTex);
        Image_ItemIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (TextBlock_ItemName)
        {
            TextBlock_ItemName->SetText(ItemData.DisplayName);
            TextBlock_ItemName->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        Border_Slot->SetBrushColor(EquippedColor);  // (0.05, 0.25, 0.15, 1.0)
    }
}
```

### 5-1d. StatsBar — 생존 스탯 표시 바

CanvasPanel_Equipment 하단에 Anchor (0.03, 0.88) ~ (0.97, 0.98)로 배치. Border_Equipment의 자식인 CanvasPanel_Equipment 안에 위치.

**구조:**
```
Border_StatsBar (Anchor 0.03,0.88 ~ 0.97,0.98)
└─ HBox_Stats
    ├─ StatEntry_Health (WBP_StatEntry, Fill 1.0, Padding R:4)
    ├─ StatEntry_Hunger (WBP_StatEntry, Fill 1.0, Padding L:4 R:4)
    ├─ StatEntry_Thirst (WBP_StatEntry, Fill 1.0, Padding L:4 R:4)
    └─ StatEntry_Stamina (WBP_StatEntry, Fill 1.0, Padding L:4)
```

**Border_StatsBar:**

| 설정 | 값 |
|------|-----|
| Background | (0.06, 0.06, 0.06, 0.95) |
| Padding | 6px all |
| Corner Radius | 4 |

**WBP_StatEntry — 재사용 스탯 바 위젯:**

```
HBox_Entry (root)
├─ Image_Icon (14×14, V-Align Center, BindWidget 필수)
├─ SizeBox_Track (Fill 1.0, Height Override 6, V-Align Center)
│  └─ Overlay_Track
│     ├─ Image_TrackBg (Color: (1,1,1,0.08), Fill/Fill)
│     └─ Image_TrackFill (H-Align Left, V-Align Fill, BindWidget 필수)
└─ TextBlock_Value ("100/100", Font 9, Color (1,1,1,0.5), Min Width 40, BindWidget 필수)
```

**WBP_StatEntry 위젯 상세 설정:**

| 위젯 | 타입 | 주요 설정 |
|------|------|----------|
| HBox_Entry | HorizontalBox | H-Align: Fill |
| Image_Icon | Image | Size: 14×14, HBox V-Align: Center, Padding R:4, Auto Size: true, Tint: C++에서 StatType별 설정 |
| SizeBox_Track | SizeBox | HBox Fill: 1.0, V-Align: Center, Height Override: 6, Min Desired Width: 40 |
| Overlay_Track | Overlay | — |
| Image_TrackBg | Image | Color: (1,1,1,0.08), Overlay Align: Fill/Fill |
| Image_TrackFill | Image | Color: C++에서 StatType별 설정, Overlay H-Align: Left, V-Align: Fill |
| TextBlock_Value | TextBlock | Text: "100/100", Font: 9, Color: (1,1,1,0.5), HBox V-Align: Center, Padding L:6, Justification: Right, Min Width: 40 |

**StatType별 색상:**

| StatType | Icon/Fill Color | Low Warning (20% 이하) |
|----------|----------------|----------------------|
| Health | #E24B4A | blink animation |
| Hunger | #EF9F27 | fill → #E24B4A |
| Thirst | #378ADD | fill → #E24B4A |
| Stamina | #1D9E75 | fill → #EF9F27 |

**C++ 클래스: UStatEntryWidget**

```cpp
UENUM(BlueprintType)
enum class ESurvivalStatType : uint8
{
    Health,
    Hunger,
    Thirst,
    Stamina
};

UCLASS()
class PROJECT_EXFIL_API UStatEntryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    ESurvivalStatType StatType = ESurvivalStatType::Health;

    void UpdateStat(float Current, float Max);

protected:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_Icon;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_TrackFill;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TextBlock_Value;

    virtual void NativeOnInitialized() override;

private:
    FLinearColor GetStatColor() const;
    FLinearColor GetLowWarningColor() const;
};
```

**UpdateStat() 로직:**
```cpp
void UStatEntryWidget::UpdateStat(float Current, float Max)
{
    float Percent = (Max > 0.f) ? FMath::Clamp(Current / Max, 0.f, 1.f) : 0.f;

    // 프로그레스 바: RenderTransform Scale.X로 너비 제어
    Image_TrackFill->SetRenderScale(FVector2D(Percent, 1.f));

    // 수치 텍스트
    TextBlock_Value->SetText(FText::FromString(
        FString::Printf(TEXT("%d/%d"), FMath::RoundToInt(Current), FMath::RoundToInt(Max))
    ));

    // 20% 이하 경고 색상
    if (Percent <= 0.2f)
        Image_TrackFill->SetColorAndOpacity(GetLowWarningColor());
    else
        Image_TrackFill->SetColorAndOpacity(GetStatColor());
}
```

**SurvivalViewModel 연동:** `USurvivalViewModel`의 `OnAttributeChanged`에서 각 StatEntry의 `UpdateStat()`을 호출.

**SizeBox 사용 이유:** `SizeBox_Track`은 프로그레스 바의 최소 높이(6px)를 보장하기 위한 것. 반응형 UI 규칙의 "고정 픽셀 SizeBox 금지"는 전체 패널 레이아웃에 해당하며, 컴포넌트 내부의 시각적 최소 크기 보장용은 예외.

**생성 파일:**
- `Source/Project_EXFIL/UI/StatEntryWidget.h/.cpp`
- `Content/UI/Inventory/WBP_StatEntry.uasset`

### 5-2. UEquipmentComponent

```cpp
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class PROJECT_EXFIL_API UEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEquipmentComponent();

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool EquipItem(EEquipmentSlot Slot, const FInventoryItemInstance& ItemInstance);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool UnequipItem(EEquipmentSlot Slot);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
    bool GetEquippedItem(EEquipmentSlot Slot, FInventoryItemInstance& OutItem) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
    bool IsSlotOccupied(EEquipmentSlot Slot) const;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
        FOnEquipmentChanged, EEquipmentSlot, Slot,
        const FInventoryItemInstance&, Item);

    UPROPERTY(BlueprintAssignable, Category = "Equipment|Events")
    FOnEquipmentChanged OnItemEquipped;

    UPROPERTY(BlueprintAssignable, Category = "Equipment|Events")
    FOnEquipmentChanged OnItemUnequipped;

private:
    UPROPERTY()
    TMap<EEquipmentSlot, FInventoryItemInstance> EquippedItems;

    UPROPERTY()
    TMap<EEquipmentSlot, FActiveGameplayEffectHandle> ActiveEffectHandles;

    void ApplyEquipmentEffect(EEquipmentSlot Slot, const FInventoryItemInstance& Item);
    void RemoveEquipmentEffect(EEquipmentSlot Slot);
};
```

**`EquipItem` 로직:**
1. 해당 슬롯이 이미 점유 시 `UnequipItem` 먼저 호출
2. `EquippedItems.Add(Slot, ItemInstance)`
3. `ApplyEquipmentEffect` → `FItemData`에서 `EquipmentEffect` 로드 → `ASC->ApplyGameplayEffectToSelf()` (Infinite Duration)
4. `ActiveEffectHandles.Add(Slot, Handle)`
5. `OnItemEquipped.Broadcast()`

**`UnequipItem` 로직:**
1. `RemoveEquipmentEffect` → `ASC->RemoveActiveGameplayEffect(Handle)`
2. `EquippedItems.Remove(Slot)`
3. `OnItemUnequipped.Broadcast()`

---

## 6. 의존성 & 연결 관계

- `USurvivalAttributeSet` → `AbilitySystemComponent.h`, `AttributeSet.h`
- `UEquipmentComponent` → `UAbilitySystemComponent` (캐릭터에서 획득), `UItemDataSubsystem`
- `AEXFILCharacter` → `IAbilitySystemInterface` 인터페이스 구현 필수
- `InventoryIconOverlay` → `UInventoryViewModel` (아이템 목록 조회)
- Day 5: `UGA_Craft`가 `UCraftingComponent` + `UInventoryComponent` 연동
- Day 6: `USurvivalAttributeSet`의 `ReplicatedUsing`이 네트워크에서 자동 동기화

## 7. 주의사항 & 제약

- **`*.generated.h`는 반드시 마지막 include**
- **`IAbilitySystemInterface` 구현 필수:** 캐릭터에 `GetAbilitySystemComponent()` 오버라이드 없으면 GAS가 ASC를 찾지 못함
- **`InitAbilityActorInfo` 호출 시점:** `PossessedBy` 또는 `BeginPlay`에서 호출. Day 6 멀티플레이어에서는 `PossessedBy`가 더 안전
- **AttributeSet 생성 위치:** 반드시 캐릭터 생성자에서 `CreateDefaultSubobject`. BeginPlay에서 생성하면 ASC가 인식 못함
- **FieldNotify Getter 명시적 지정 (Day 2-3 규칙):** Day 4 ViewModel에도 동일 적용
- **ForwardMoveRequest 패턴 유지:** 아이템 사용/장비 요청도 PanelWidget 중계
- **`ActiveEffectHandles` 저장 필수:** 장비 해제 시 `RemoveActiveGameplayEffect`에 핸들이 필요
- **`DOREPLIFETIME_CONDITION_NOTIFY` 사용:** Day 6 Replication을 바로 지원하도록 설계

## 8. 검증 기준 (Done Criteria)

### Phase A: UI
- [ ] 멀티셀 아이템(2x1, 2x3 등) 아이콘이 여러 칸에 걸쳐 표시
- [ ] 인벤토리가 팝업 형태로 뜨고 반투명 배경 뒤로 게임 월드가 보임
- [ ] 드래그앤드롭이 여전히 정상 동작

### Phase B: GAS
- [ ] UBT 빌드 성공
- [ ] `EXFILItemTypes.h`의 `TSoftClassPtr<UGameplayEffect>` 복원 확인
- [ ] PIE에서 `showdebug abilitysystem` 명령어로 AttributeSet 확인
- [ ] Health=100, Hunger=100, Thirst=100, Stamina=100 초기값 확인
- [ ] 테스트: Bandage 사용 시 Health 변화 확인 (UE_LOG)

### Phase C: 장비
- [ ] Pistol 장착 시 `EquipItem(Weapon1, ...)` 성공
- [ ] 장착 중 `showdebug abilitysystem`에서 Infinite Duration GE 확인
- [ ] 장비 해제 시 GE 제거 확인
- [ ] 동일 슬롯에 재장착 시 기존 GE 제거 후 새 GE 적용 확인
- [ ] 6개 EquipmentSlotWidget 인스턴스가 각각 올바른 SlotLabel 표시
- [ ] 장착 시 EquipmentSlotWidget이 Equipped 상태(초록 테두리) 표시
- [ ] 해제 시 Empty 상태로 복귀
- [ ] 그리드 → 장비슬롯 드래그앤드롭 동작
- [ ] StatsBar에 4개 스탯이 정상 표시 (HP/Hunger/Thirst/Stamina)
- [ ] GE 적용 시 StatsBar 수치가 실시간 업데이트

---

## ✅ 완료 보고

**작성자:** 프로젝트 연결 탭 → 수현이 채팅 탭에 전달
**작성 시점:** 당일 저녁 작업 종료 후

### 1. 완료 항목
- [ ] (구현 완료된 항목 체크)

### 2. 미완료 / 내일로 이월
- (미완료 항목 + 이유)

### 3. 트러블슈팅
| 문제 | 원인 | 해결 |
|------|------|------|
| (증상) | (근본 원인) | (적용한 해결책) |

### 4. 파일 구조 변경사항
| 파일 | 변경 유형 | 변경 내용 요약 |
|------|----------|---------------|
| `Source/.../파일명.h` | 신규 생성 | (설명) |

### 5. 다음 Day 참고사항
- (내일 작업에 영향을 줄 수 있는 사항)

### 6. Git 커밋
- `feat(Day4): GAS SurvivalAttributeSet + EquipmentComponent + UI multi-cell icon` — (커밋 해시)
