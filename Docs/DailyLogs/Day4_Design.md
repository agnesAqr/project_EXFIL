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

**Phase A: UI 수정 (3시간 예상)**
- [ ] 멀티셀 아이콘 스패닝: `CanvasPanel` 오버레이 레이어로 아이템 크기에 맞게 아이콘 표시
- [ ] 인벤토리 팝업 형태 복원: 화면 전체를 덮지 않는 팝업 오버레이

**Phase B: GAS 기반 시스템 (8시간 예상)**
- [ ] Build.cs에 `GameplayAbilities`, `GameplayTasks` 모듈 추가
- [ ] `EXFILItemTypes.h`의 `FSoftClassPath` → `TSoftClassPtr<UGameplayEffect>` 복원
- [ ] `USurvivalAttributeSet` 구현 (Health, MaxHealth, Hunger, MaxHunger, Thirst, MaxThirst, Stamina, MaxStamina)
- [ ] `UAbilitySystemComponent`를 캐릭터에 부착 + `USurvivalAttributeSet` 등록
- [ ] 초기값 설정용 `GE_DefaultAttributes` GameplayEffect (BP 또는 C++)
- [ ] 소비 아이템용 `UGA_UseItem` GameplayAbility 기초 구현

**Phase C: 장비 시스템 (3시간 예상)**
- [ ] `UEquipmentComponent` 구현 (Head, Body, Weapon 슬롯)
- [ ] 장착 시 Infinite Duration GE Apply / 해제 시 Remove

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
| `Source/Project_EXFIL/Equipment/EquipmentTypes.h` | EEquipmentSlot 열거형 | 신규 |
| `Source/Project_EXFIL/Equipment/EquipmentComponent.h` | 장비 컴포넌트 | 신규 |
| `Source/Project_EXFIL/Equipment/EquipmentComponent.cpp` | 구현 | 신규 |

---

## 3. Phase A: 멀티셀 아이콘 스패닝 구현

### 3-1. 문제 상황

현재 `UniformGridPanel`의 각 슬롯이 독립 위젯이라, 2x3 같은 대형 아이템의 아이콘이 루트 슬롯(1칸)에만 표시됨.

### 3-2. 해결 방안: CanvasPanel 오버레이

`WBP_InventoryPanel`의 레이아웃을 다음과 같이 변경:

```
CanvasPanel (Root)
├─ UniformGridPanel         ← 기존 슬롯 그리드 (배경 + 하이라이트 + 드롭 영역)
└─ CanvasPanel (IconOverlay) ← 아이콘 전용 레이어 (새로 추가)
    ├─ ItemIcon_0: Image at (rootX×cellSize, rootY×cellSize), size (W×cellSize, H×cellSize)
    ├─ ItemIcon_1: ...
    └─ ...
```

**핵심 로직:**
- `UInventoryIconOverlay` C++ 클래스 생성 (`UUserWidget` 상속)
- `RefreshIcons()` 함수: InventoryViewModel의 모든 아이템을 순회하며 루트 슬롯에만 아이콘 Image 위젯 생성
- 각 아이콘: `CanvasSlot->SetPosition(FVector2D(rootX * CellSize, rootY * CellSize))`
- 각 아이콘: `CanvasSlot->SetSize(FVector2D(itemW * CellSize, itemH * CellSize))`
- `InventorySlotWidget`에서는 아이콘 표시 로직 제거 (오버레이에서 처리)
- 슬롯 위젯은 배경색 + 하이라이트 + 드래그드롭 영역만 담당
- `OnInventoryUpdated` 델리게이트에서 `RefreshIcons()` 호출

**CellSize:** `InventorySlotWidget`에 이미 있는 `CellPixelSize` (60.f) 사용.

### 3-3. 인벤토리 팝업 형태 복원

`WBP_InventoryPanel`이 화면 전체를 덮지 않고 팝업으로 뜨도록:

1. WBP 루트: `CanvasPanel` (Full Screen Anchor)
2. 반투명 배경: `Border` (`LinearColor(0, 0, 0, 0.5)` 페이드)
3. 인벤토리 패널: `SizeBox` (고정 크기, 예: 700×500) → 중앙 정렬
4. 그리드 + 오버레이는 SizeBox 내부에 배치

C++ 코드에서 `NativeOnActivated`에서 입력 모드를 `GameAndUI`로 설정하는 기존 로직 유지.

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
    None     UMETA(DisplayName = "None"),
    Head     UMETA(DisplayName = "Head"),
    Body     UMETA(DisplayName = "Body"),
    Weapon   UMETA(DisplayName = "Weapon")
};
```

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
- [ ] Pistol 장착 시 `EquipItem(Weapon, ...)` 성공
- [ ] 장착 중 `showdebug abilitysystem`에서 Infinite Duration GE 확인
- [ ] 장비 해제 시 GE 제거 확인
- [ ] 동일 슬롯에 재장착 시 기존 GE 제거 후 새 GE 적용 확인

---

## ✅ 완료 보고

**작성자:** 프로젝트 연결 탭 (Claude Sonnet 4.6)
**작성 시점:** 2026-03-20

### 1. 완료 항목

**Pre-patch**
- [x] `EXFILItemTypes.h`: `FSoftClassPath` → `TSoftClassPtr<UGameplayEffect>` 복원 (`#include "GameplayEffect.h"` 추가)
- [x] `Project_EXFIL.Build.cs`: `GameplayAbilities`, `GameplayTasks` 모듈 추가; `Data/`, `GAS/`, `Equipment/` include 경로 추가

**Phase A: UI 수정**
- [x] `InventoryIconOverlay.h/.cpp` 신규 생성 — CanvasPanel 기반 멀티셀 아이콘 오버레이
- [x] `InventoryViewModel`에 `FOnInventoryViewModelRefreshed OnViewModelRefreshed` 델리게이트 추가, `RefreshAllSlots()` 끝에서 브로드캐스트
- [x] `InventoryPanelWidget`: `IconOverlay`(BindWidgetOptional) 추가, `SetViewModel()`에서 델리게이트 구독, `BuildGrid()` 후 `RefreshIconOverlay()` 호출
- [x] `InventorySlotWidget`: `ItemIcon` → `BindWidgetOptional`로 변경, `RefreshVisuals()`에서 아이콘 표시 로직 제거 (항상 Hidden)

**Phase B: GAS**
- [x] `GAS/SurvivalAttributeSet.h/.cpp` 신규 생성 — Health/MaxHealth, Hunger/MaxHunger, Thirst/MaxThirst, Stamina/MaxStamina, ATTRIBUTE_ACCESSORS, PreAttributeChange 클램핑, PostGameplayEffectExecute, GetLifetimeReplicatedProps(DOREPLIFETIME_CONDITION_NOTIFY), 생성자 초기값 100
- [x] `GAS/GA_UseItem.h/.cpp` 신규 생성 — ActivateAbility에서 ConsumableEffect 로드→ApplyGameplayEffectSpecToSelf→InventoryComponent::RemoveItem
- [x] `Core/EXFILCharacter.h` 수정 — `IAbilitySystemInterface` 구현, `UAbilitySystemComponent` + `USurvivalAttributeSet` 추가
- [x] `Core/EXFILCharacter.cpp` 수정 — 생성자에서 `CreateDefaultSubobject`, `PossessedBy`에서 `InitAbilityActorInfo`

**Phase C: 장비**
- [x] `Equipment/EquipmentTypes.h` 신규 생성 — `EEquipmentSlot` (None/Head/Body/Weapon)
- [x] `Equipment/EquipmentComponent.h/.cpp` 신규 생성 — EquipItem/UnequipItem/GetEquippedItem/IsSlotOccupied, ApplyEquipmentEffect(Infinite GE), RemoveEquipmentEffect, FActiveGameplayEffectHandle 저장

### 2. 미완료 / 내일로 이월

- **Phase A 인벤토리 팝업 형태 복원** — WBP 레이아웃(반투명 Border 배경, SizeBox 700×500 중앙 정렬)은 에디터에서 WBP 직접 수정 필요. C++ 로직(`NativeOnActivated` 입력 모드)은 기존 유지
- **`IconOverlay`의 WBP 연결** — `WBP_InventoryPanel`에 `UInventoryIconOverlay` 서브클래스(`WBP_InventoryIconOverlay`) 추가 및 `IconCanvas` CanvasPanel BindWidget 필요. 에디터 작업
- **Phase D: `USurvivalViewModel` + HUD 기초** — 시간 내 미구현, Day 5 이월

### 3. 트러블슈팅

| 문제 | 원인 | 해결 |
|------|------|------|
| `FSoftClassPath` → `TSoftClassPtr<UGameplayEffect>`: Day 3에서 UHT가 템플릿 인자의 전방선언 인식 불가로 우회 | `GameplayAbilities` 모듈이 Build.cs에 없어 `UGameplayEffect` 정의를 UHT가 못 찾음 | Build.cs에 모듈 추가 후 `#include "GameplayEffect.h"` + TSoftClassPtr 복원 |
| `UInventoryViewModel`(UObject)에서 직접 `GetWorld()` 불가 | UObject는 World context 없음 | `InventoryComp->GetWorld()->GetGameInstance()->GetSubsystem<>()` 패턴 유지 (Day 3 확립) |

### 4. 파일 구조 변경사항

| 파일 | 변경 유형 | 변경 내용 요약 |
|------|----------|---------------|
| `Data/EXFILItemTypes.h` | 수정 | FSoftClassPath → TSoftClassPtr<UGameplayEffect>, GameplayEffect.h include |
| `Project_EXFIL.Build.cs` | 수정 | GameplayAbilities/GameplayTasks 추가, Data/GAS/Equipment include 경로 추가 |
| `UI/InventoryIconOverlay.h` | 신규 | CanvasPanel 기반 멀티셀 아이콘 오버레이 위젯 |
| `UI/InventoryIconOverlay.cpp` | 신규 | RefreshIcons: SlotVM 순회 → 루트 슬롯에 Image 배치 |
| `UI/InventoryViewModel.h` | 수정 | FOnInventoryViewModelRefreshed 델리게이트 추가 |
| `UI/InventoryViewModel.cpp` | 수정 | RefreshAllSlots 끝에 OnViewModelRefreshed.Broadcast() |
| `UI/InventoryPanelWidget.h` | 수정 | IconOverlay(BindWidgetOptional) + RefreshIconOverlay() + 델리게이트 핸들 |
| `UI/InventoryPanelWidget.cpp` | 수정 | SetViewModel 델리게이트 구독, BuildGrid 후 RefreshIconOverlay 호출 |
| `UI/InventorySlotWidget.h` | 수정 | ItemIcon → BindWidgetOptional |
| `UI/InventorySlotWidget.cpp` | 수정 | RefreshVisuals에서 아이콘 표시 로직 제거 (항상 Hidden) |
| `GAS/SurvivalAttributeSet.h` | 신규 | 8개 어트리뷰트 + ATTRIBUTE_ACCESSORS + OnRep 선언 |
| `GAS/SurvivalAttributeSet.cpp` | 신규 | 생성자 초기값, PreAttributeChange 클램핑, PostGEExecute, GetLifetimeReplicatedProps |
| `GAS/GA_UseItem.h` | 신규 | 소비 아이템 GameplayAbility 선언 |
| `GAS/GA_UseItem.cpp` | 신규 | ActivateAbility: ConsumableEffect 로드→Apply→RemoveItem |
| `Core/EXFILCharacter.h` | 수정 | IAbilitySystemInterface, ASC, SurvivalAttributeSet 추가 |
| `Core/EXFILCharacter.cpp` | 수정 | 생성자 CreateDefaultSubobject, PossessedBy InitAbilityActorInfo |
| `Equipment/EquipmentTypes.h` | 신규 | EEquipmentSlot 열거형 |
| `Equipment/EquipmentComponent.h` | 신규 | UEquipmentComponent 선언 |
| `Equipment/EquipmentComponent.cpp` | 신규 | Equip/Unequip GE Apply/Remove 구현 |

### 5. 다음 Day 참고사항

- **WBP 에디터 작업 필요:** `WBP_InventoryIconOverlay` 신규 생성(`UInventoryIconOverlay` 서브클래스, `IconCanvas` CanvasPanel BindWidget), `WBP_InventoryPanel`에 추가 + `WBP_InventoryPanel` 팝업 레이아웃 수정(반투명 Border + SizeBox 중앙 정렬)
- **GE_DefaultAttributes** (BP 에셋) 생성 후 EXFILCharacter BeginPlay에서 적용하면 초기값 100 확인 가능 (`showdebug abilitysystem`)
- **AttributeSet 초기값:** 생성자에서 `InitXxx(100.f)` 직접 설정했으므로 GE_DefaultAttributes 없이도 기본값은 100
- **Day 5 Crafting:** `UGA_Craft`가 `UCraftingComponent` + `UInventoryComponent` 연동. `GA_UseItem` 패턴 재사용 가능
- **Day 6 Replication:** `DOREPLIFETIME_CONDITION_NOTIFY` 이미 설정 완료. `PossessedBy`에서 `InitAbilityActorInfo` 호출 구조도 준비됨

### 6. Git 커밋
- `feat(Day4): GAS SurvivalAttributeSet + EquipmentComponent + UI multi-cell icon` — (커밋 해시)
