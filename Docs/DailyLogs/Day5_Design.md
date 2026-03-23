# Day 5 설계 지시서: UI Fix + Crafting System

> 채팅 탭(Opus)이 상단 "설계 지시서" 섹션을 작성 → 수현이 검토 후 Docs/DailyLogs/Day5_Design.md로 저장
> 프로젝트 연결 탭이 하단 "완료 보고"를 작성 → 수현이 채팅 탭에 전달

---

## ⚠️ Day 4 반영사항

### 반영 1: 미해결 UI 문제 3건 (Phase A에서 해결)

| # | 문제 | 근본 원인 |
|---|------|----------|
| 1 | Panel이 전체 화면을 가리지 않음 | Border가 루트일 때 자식 콘텐츠 크기에 맞춰짐. CanvasPanel 안의 Anchor가 필요 |
| 2 | 각 Slot이 늘어나 화면을 넘어섬 | SetAnchorsInViewport(0,0~1,1)이 위젯 전체를 화면 크기로 늘림 |
| 3 | Slot에 구멍(투명 영역) 뚫림 | 슬롯이 비정상적으로 늘어나면서 패딩이 과도하게 확대 |

### 반영 2: Day 4에서 완료된 사항

- [x] GAS: `USurvivalAttributeSet` (8개 어트리뷰트) 구현 완료
- [x] GAS: `AEXFILCharacter`에 ASC + AttributeSet 부착 완료
- [x] GAS: PIE에서 showdebug abilitysystem 확인 완료 (전 어트리뷰트 100)
- [x] `UGA_UseItem` 기초 구현 완료
- [x] `UEquipmentComponent` 구현 완료
- [x] `UInventoryIconOverlay` C++ 구현 완료
- [x] `TSoftClassPtr<UGameplayEffect>` 복원 완료
- [x] WBP_InventoryIconOverlay 생성 완료
- [x] WBP_InventorySlot SizeBox 60×60 고정 완료

### 반영 3: 현재 WBP 계층 (Day 4 에디터 실제 상태)

```
[WBP_InventoryPanel] — 현재 상태 (수정 전)
└── [Border]
    └── [Overlay]
        ├── GridPanel
        └── IconOverlay

[WBP_InventorySlot] — 수정 전
└── [Border_Black]
    └── [SlotBorder]
        └── [Overlay]
            ├── ItemIcon
            └── [StackCountText]

[WBP_InventorySlot] — Day 5 최종
└── [Border_Black]
    └── [SlotBorder]
        (자식 없음 — StackCountText는 IconOverlay에서 렌더링)
```

### 반영 4: 크래프팅 데이터 준비 완료 (Day 3)

- `FCraftingRecipe` 구조체 정의 완료 (EXFILItemTypes.h)
- `DT_CraftingRecipe.uasset` 생성 완료 (Recipe_Bandage, Recipe_Medkit)
- `UItemDataSubsystem::GetCraftingRecipe()` / `GetAllRecipeIDs()` 구현 완료

### 반영 5: 반응형 UI 규칙 (프로젝트 전역)

- **고정 픽셀(SizeBox) 사용 금지** — 전체 패널 레이아웃은 Anchor 비율 기반
- **예외:** 컴포넌트 내부의 시각적 최소 크기 보장용 (예: StatsBar Track Height 6px)
- **FieldNotify Getter 명시적 지정 필수:** `Getter="GetterName"` 형식

---

## 📋 Day 5 설계 지시서

**작성자:** 채팅 탭 (Opus 4.6)
**WBS 분류:** UI 수정 + 크래프팅 시스템
**목표 시간:** 15h
**날짜:** 2026-03-21
**권장 모델:** Sonnet 4.6 (설계서가 상세함. 막히면 Opus 전환)

## 1. 오늘의 목표 (2개 페이즈)

**Phase A: 인벤토리 UI 전면 개편 (6~7h)**
- [x] WBP_InventoryPanel 루트를 CanvasPanel_Root로 변경
- [x] Border_Background: Anchor 0,0~1,1 반투명 검정 풀스크린
- [x] Border_Panel: Anchor 0.03,0.03~0.97,0.97 비율 기반
- [x] HorizontalBox: Equipment(Fill 0.55) + Grid(Fill 0.45)
- [x] CanvasPanel_Equipment에 WBP_EquipmentSlot 6개 인스턴스 Anchor 배치
- [x] UEquipmentSlotWidget C++ 구현 (RefreshSlot, 드래그앤드롭, SlotLabel 자동 설정)
- [x] WBP_EquipmentSlot 생성 (Border_Slot > Overlay > ScaleBox > Image_ItemIcon + TextBlock_SlotLabel + TextBlock_ItemName)
- [x] StatsBar 구현: Border_StatsBar + HBox_Stats + WBP_StatEntry 4개 인스턴스
- [x] UStatEntryWidget C++ 구현 (UpdateStat, StatType별 색상, StatLabel)
- [x] WBP_StatEntry 생성 (HBox_Entry > Image_Icon + TextBlock_StatLabel + SizeBox_Track + TextBlock_Value)
- [x] 멀티셀 아이콘 스패닝 동작 확인 (IconOverlay geometry 기반)
- [x] BP 토글 수정: UIOnly + NativeOnKeyDown I키 닫기
- [x] **모든 UI는 Anchor 비율 기반 배치 (고정 픽셀 SizeBox 금지)**

**Phase B: 크래프팅 시스템 (7~8h)**
- [x] `UCraftingComponent` 구현
- [x] `UGA_Craft` GameplayAbility 구현
- [x] 크래프팅 UI: `UCraftingPanelWidget` + `UCraftingRecipeWidget`
- [x] 인벤토리 패널에 크래프팅 탭/버튼 추가
- [x] DT_CraftingRecipe 연동 테스트

---

## 2. 생성할 파일 목록

### Phase A: UI

| 파일 경로 | 용도 | 신규/수정 |
|-----------|------|----------|
| `Source/Project_EXFIL/UI/EquipmentSlotWidget.h/.cpp` | 장비 슬롯 UI 위젯 | 신규 |
| `Source/Project_EXFIL/UI/StatEntryWidget.h/.cpp` | 스탯 바 엔트리 위젯 | 신규 |
| `Source/Project_EXFIL/UI/InventoryPanelWidget.h/.cpp` | CanvasPanel_Root 구조로 전면 개편 | 수정 |
| `Content/UI/Inventory/WBP_InventoryPanel.uasset` | 레이아웃 전면 수정 | 수정 |
| `Content/UI/Inventory/WBP_EquipmentSlot.uasset` | 장비 슬롯 WBP | 신규 |
| `Content/UI/Inventory/WBP_StatEntry.uasset` | 스탯 바 엔트리 WBP | 신규 |

### Phase B: 크래프팅

| 파일 경로 | 용도 | 신규/수정 |
|-----------|------|----------|
| `Source/Project_EXFIL/Crafting/CraftingComponent.h/.cpp` | 크래프팅 로직 컴포넌트 | 신규 |
| `Source/Project_EXFIL/GAS/GA_Craft.h/.cpp` | 크래프팅 GameplayAbility | 신규 |
| `Source/Project_EXFIL/UI/CraftingPanelWidget.h/.cpp` | 크래프팅 패널 UI | 신규 |
| `Source/Project_EXFIL/UI/CraftingRecipeWidget.h/.cpp` | 개별 레시피 엔트리 UI | 신규 |
| `Source/Project_EXFIL/Core/EXFILCharacter.h/.cpp` | CraftingComponent 부착 | 수정 |

---

## 3. Phase A: 인벤토리 UI 전면 개편

### 3-1. WBP_InventoryPanel 최종 레이아웃

```
CanvasPanel_Root (루트 — 새로 추가)
├── Border_Background (Anchor 0,0~1,1 = 반투명 검정 풀스크린)
│
└── Border_Panel (Anchor 0.03,0.03~0.97,0.97 = 비율 기반 패널)
    └── HorizontalBox
        ├── Border_Equipment (Fill: 0.55)
        │   └── CanvasPanel_Equipment
        │       ├── 캐릭터 실루엣 이미지 (Anchor 0.25,0.1~0.75,0.7)
        │       ├── EquipSlot_Head (Anchor 0.02,0.05~0.22,0.25)
        │       ├── EquipSlot_Face (Anchor 0.78,0.05~0.98,0.25)
        │       ├── EquipSlot_Eyewear (Anchor 0.78,0.28~0.98,0.48)
        │       ├── EquipSlot_Body (Anchor 0.02,0.28~0.22,0.58)
        │       ├── EquipSlot_Weapon1 (Anchor 0.02,0.62~0.48,0.85)
        │       ├── EquipSlot_Weapon2 (Anchor 0.52,0.62~0.98,0.85)
        │       └── Border_StatsBar (Anchor 0.03,0.88~0.97,0.98)
        │
        └── Border_Grid (Fill: 0.45)
            └── VerticalBox_GridContent
                ├── HorizontalBox_Tabs (Auto Size)
                │   ├── Button_InventoryTab (Fill 1.0, "INVENTORY")
                │   └── Button_CraftingTab (Fill 1.0, "CRAFTING")
                └── WidgetSwitcher_Content (Fill 1.0)
                    ├── [Index 0] Overlay
                    │   ├── GridPanel (UniformGridPanel, H:Fill V:Top)
                    │   └── IconOverlay (WBP_InventoryIconOverlay, H:Fill V:Top)
                    └── [Index 1] WBP_CraftingPanel
```

### 3-2. 위젯별 Anchor/설정 상세

| 위젯 | 타입 | Anchor Min/Max 또는 HBox 설정 | 주요 설정 |
|------|------|------|----------|
| CanvasPanel_Root | CanvasPanel | — | 루트 위젯 |
| Border_Background | Border | 0,0 ~ 1,1 | Color: (0,0,0,0.6), 자식 없음, Hit Testable |
| Border_Panel | Border | 0.03,0.03 ~ 0.97,0.97 | Color: (0.12,0.12,0.12,0.95), Align: 0.5,0.5, Pad: 8 |
| HorizontalBox | HorizontalBox | — | 좌/우 분할 |
| Border_Equipment | Border | Fill: 0.55 | Pad: 8, Color: (0.10,0.10,0.10,1) |
| Border_Grid | Border | Fill: 0.45 | Pad: 4, Color: (0.08,0.08,0.08,1) |
| VerticalBox_GridContent | VerticalBox | — | Tabs + WidgetSwitcher 분할 |
| HorizontalBox_Tabs | HorizontalBox | Auto Size | 탭 버튼 컨테이너 |
| WidgetSwitcher_Content | WidgetSwitcher | Fill 1.0 | Is Variable ✓, Index 0=Grid, Index 1=Crafting |
| Overlay | Overlay | — | GridPanel + IconOverlay 겹침 (Index 0) |
| GridPanel | UniformGridPanel | Align: Fill/Top | 슬롯 배경, 드래그 대상 |
| IconOverlay | WBP_IconOverlay | Align: Fill/Top | HitTestInvisible, 아이콘+StackCount |

### 3-3. WBP_EquipmentSlot 내부 위젯 설정

```
Border_Slot (root, Draw As=Rounded Box, Outline Settings으로 테두리)
└─ Overlay
    ├─ ScaleBox_Icon (Stretch: Scale to Fit, Fill/Fill, Pad 8)
    │   └─ Image_ItemIcon (Hidden, BindWidget 필수)
    ├─ TextBlock_SlotLabel (Left/Top, Pad L:4 T:2, Font 10 Bold, Color (1,1,1,0.35), BindWidget 필수)
    └─ TextBlock_ItemName (Center/Bottom, Pad B:4, Font 10, Color (1,1,1,0.6), Hidden, BindWidgetOptional)
```

**Border_Slot:** Draw As=Rounded Box, Tint (0.08,0.08,0.08,1.0), Outline Color (1,1,1,0.1), Width 1, Corner Radii (4,4,4,4), Content Padding 4px

**상태별 색상:**

| 상태 | Background | Border Color |
|------|-----------|-------------|
| Empty | (0.08, 0.08, 0.08, 1.0) | (1, 1, 1, 0.1) |
| Equipped | (0.05, 0.25, 0.15, 1.0) | (0.12, 0.63, 0.43, 0.4) |
| Drag hover (valid) | (0.08, 0.18, 0.3, 1.0) | (0.22, 0.54, 0.87, 0.5) |
| Drag hover (invalid) | (0.25, 0.08, 0.08, 1.0) | (0.87, 0.18, 0.18, 0.4) |

**UEquipmentSlotWidget C++ 핵심:**
```cpp
UCLASS()
class PROJECT_EXFIL_API UEquipmentSlotWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
    EEquipmentSlot SlotType = EEquipmentSlot::None;

    void RefreshSlot(const FEquipmentSlotData& SlotData);

protected:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> Border_Slot;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> Image_ItemIcon;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TextBlock_SlotLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TextBlock_ItemName;

    virtual void NativeOnInitialized() override; // SlotLabel 자동 설정
    virtual FReply NativeOnMouseButtonDown(...) override;
    virtual void NativeOnDragDetected(...) override;
    virtual bool NativeOnDrop(...) override;
};
```

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

### 3-4. StatsBar + WBP_StatEntry 설정

**위치:** CanvasPanel_Equipment 내부, Anchor (0.03, 0.88) ~ (0.97, 0.98)

```
Border_StatsBar (Background: (0.06,0.06,0.06,0.95), Pad 6, Radius 4)
└─ HBox_Stats
    ├─ StatEntry_Health (Fill 1.0, Pad R:4)
    ├─ StatEntry_Hunger (Fill 1.0, Pad L:4 R:4)
    ├─ StatEntry_Thirst (Fill 1.0, Pad L:4 R:4)
    └─ StatEntry_Stamina (Fill 1.0, Pad L:4)
```

**WBP_StatEntry 내부:**
```
HBox_Entry (root)
├─ Image_Icon (14×14, V-Align Center, Pad R:4, Auto Size, BindWidget 필수)
├─ TextBlock_StatLabel ("HP", Font 9 Bold, Color (1,1,1,0.35), Pad R:4, BindWidgetOptional)
├─ SizeBox_Track (Fill 1.0, V-Align Center, Height Override 6, Min Width 40)
│  └─ Overlay_Track
│     ├─ Image_TrackBg (Color (1,1,1,0.08), Fill/Fill)
│     └─ Image_TrackFill (H-Align Left, V-Align Fill, BindWidget 필수)
└─ TextBlock_Value ("100/100", Font 9, Color (1,1,1,0.5), Pad L:6, Min Width 40, BindWidget 필수)
```

**StatType별 색상:**

| StatType | Fill Color | Low Warning (≤20%) |
|----------|-----------|-------------------|
| Health | #E24B4A | blink |
| Hunger | #EF9F27 | → #E24B4A |
| Thirst | #378ADD | → #E24B4A |
| Stamina | #1D9E75 | → #EF9F27 |

### 3-5. BP 토글 수정

**기존 (삭제할 것):**
```
AddToViewport → SetAnchorsInViewport(Min 0,0 / Max 1,1) → SetPositionInViewport(0,0)
```

**수정 후:**
```
I 키 (Pressed) — 캐릭터 BP (열기 전용)
    └── IsInViewport?
        ├── True  → (아무것도 안 함 — UIOnly라서 여기 안 옴)
        └── False → AddToViewport(ZOrder: 10) → SetInputMode(UIOnly) → ShowCursor(true)

I 키 — 위젯 NativeOnKeyDown (닫기 전용)
    → RemoveFromParent → SetInputMode(GameOnly) → ShowCursor(false)
```

---

## 4. Phase B: 크래프팅 시스템

### 4-1. UCraftingComponent

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnCraftingStateChanged, bool, bIsCrafting, float, RemainingTime);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnCraftingCompleted, FName, RecipeID);

UCLASS(ClassGroup=(Crafting), meta=(BlueprintSpawnableComponent))
class PROJECT_EXFIL_API UCraftingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCraftingComponent();

    // 크래프팅 가능 여부 확인 (재료 보유 체크)
    UFUNCTION(BlueprintCallable, Category = "Crafting")
    bool CanCraft(FName RecipeID) const;

    // 크래프팅 시작 (재료 소비 → 타이머 시작 → 결과물 생성)
    UFUNCTION(BlueprintCallable, Category = "Crafting")
    bool StartCraft(FName RecipeID);

    // 크래프팅 취소
    UFUNCTION(BlueprintCallable, Category = "Crafting")
    void CancelCraft();

    // 현재 크래프팅 중인지
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting")
    bool IsCrafting() const { return bIsCrafting; }

    // 사용 가능한 레시피 목록 (재료 보유 여부 포함)
    UFUNCTION(BlueprintCallable, Category = "Crafting")
    TArray<FName> GetAvailableRecipes() const;

    UPROPERTY(BlueprintAssignable, Category = "Crafting|Events")
    FOnCraftingStateChanged OnCraftingStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Crafting|Events")
    FOnCraftingCompleted OnCraftingCompleted;

protected:
    virtual void BeginPlay() override;

private:
    bool bIsCrafting = false;
    FName CurrentRecipeID;
    FTimerHandle CraftTimerHandle;

    void OnCraftTimerComplete();

    // 캐시된 참조
    UPROPERTY()
    TWeakObjectPtr<UInventoryComponent> InventoryComp;
};
```

### 4-2. 핵심 로직

**CanCraft():**
1. `UItemDataSubsystem`에서 `FCraftingRecipe` 조회
2. `FCraftingRecipe::Ingredients` 순회
3. 각 재료의 `ItemDataID`와 `RequiredCount`를 `UInventoryComponent`의 아이템과 대조
4. 모든 재료가 충분하면 true

**StartCraft():**
1. `CanCraft()` 확인
2. 재료 소비: 각 Ingredient에 대해 `InventoryComp->ConsumeItemByID(ItemDataID, Count)`
   - **주의:** `ConsumeItemByID`는 기존 API에 없음 → Day 5에서 `UInventoryComponent`에 추가 필요
3. `bIsCrafting = true`
4. `GetWorld()->GetTimerManager().SetTimer(CraftTimerHandle, this, &UCraftingComponent::OnCraftTimerComplete, Recipe.CraftDuration)`
5. `OnCraftingStateChanged.Broadcast(true, Recipe.CraftDuration)`

**OnCraftTimerComplete():**
1. `InventoryComp->TryAddItemByID(Recipe.ResultItemID, Recipe.ResultCount)`
2. `bIsCrafting = false`
3. `OnCraftingCompleted.Broadcast(CurrentRecipeID)`
4. `OnCraftingStateChanged.Broadcast(false, 0.f)`

**CancelCraft():**
1. 타이머 클리어
2. 소비한 재료 복구 (각 Ingredient를 `TryAddItemByID`로 되돌림)
3. `bIsCrafting = false`
4. `OnCraftingStateChanged.Broadcast(false, 0.f)`

### 4-3. UInventoryComponent 추가 API

```cpp
// InventoryComponent.h에 추가
UFUNCTION(BlueprintCallable, Category = "Inventory")
bool ConsumeItemByID(FName ItemDataID, int32 Count = 1);

UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
int32 GetItemCountByID(FName ItemDataID) const;
```

**ConsumeItemByID():**
- 인벤토리에서 해당 ItemDataID를 가진 아이템을 찾아 Count만큼 스택 감소
- 스택이 0이 되면 아이템 제거
- 여러 슬롯에 분산된 경우 순서대로 소비

**GetItemCountByID():**
- 인벤토리 내 해당 ItemDataID의 총 스택 수 합산 반환

### 4-4. UGA_Craft — 크래프팅 GameplayAbility

```cpp
UCLASS()
class PROJECT_EXFIL_API UGA_Craft : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Craft();

protected:
    virtual void ActivateAbility(...) override;
    virtual void EndAbility(...) override;

    UPROPERTY(EditDefaultsOnly, Category = "Craft")
    FName RecipeID;
};
```

**ActivateAbility 핵심 로직:**
1. `UCraftingComponent` 획득 (OwnerActor에서)
2. `CraftingComp->StartCraft(RecipeID)` 호출
3. `OnCraftingCompleted` 델리게이트에 바인딩하여 완료 시 `EndAbility()` 호출
4. 실패 시 즉시 `EndAbility()` + `CancelAbility()`

**참고:** 간단한 크래프팅은 GA 없이 `UCraftingComponent::StartCraft()`를 직접 호출해도 동작함. GA는 GAS 태그 기반 제어(크래프팅 중 이동 불가 등)가 필요할 때 사용.

### 4-5. 크래프팅 UI

크래프팅 UI는 인벤토리 패널의 **Grid 영역 상단에 탭으로 추가**하거나, **별도 패널로 분리**할 수 있음. Day 5에서는 **간단한 레시피 목록 패널**을 구현.

**WBP_CraftingPanel 구조:**
```
Border_CraftingPanel (root)
└── VerticalBox_Content
    ├── Border_CraftProgress (크래프팅 진행 바, 미크래프팅 시 Hidden)
    │   └── VerticalBox
    │       ├── HBox_ProgressInfo
    │       │   ├── TextBlock_CraftingLabel ("Crafting: Bandage")
    │       │   └── TextBlock_CraftingTime ("1.9s / 3.0s")
    │       └── SizeBox_ProgressTrack (Height Override 4)
    │           └── Overlay
    │               ├── Image_ProgressBg (Fill/Fill)
    │               └── Image_ProgressFill (Left/Fill)
    │
    └── ScrollBox_Recipes (Fill 1.0)
        ├── WBP_CraftingRecipe (동적 생성)
        ├── WBP_CraftingRecipe
        └── ...
```

### 4-5a. WBP_CraftingPanel 위젯 상세 설정

**Border_CraftingPanel (root)**

| 설정 | 값 |
|------|-----|
| 타입 | Border |
| Background | (0.08, 0.08, 0.08, 0.95) |
| Padding | 8px all |
| Is Variable | true |

**VerticalBox_Content**

| 설정 | 값 |
|------|-----|
| 타입 | VerticalBox |
| VBox Slot — Fill | 1.0 |

**Border_CraftProgress (크래프팅 진행 바)**

| 설정 | 값 |
|------|-----|
| 타입 | Border |
| Background | (0.94, 0.62, 0.15, 0.08) |
| Brush > Draw As | Rounded Box |
| Brush > Outline Settings > Color | (0.94, 0.62, 0.15, 0.2) |
| Brush > Outline Settings > Width | 1.0 |
| Brush > Outline Settings > Corner Radii | (4, 4, 4, 4) |
| Visibility | Hidden (default, 크래프팅 시작 시 Visible) |
| VBox Slot — Auto Size | true |
| VBox Slot — Padding | B:8 |
| Is Variable | true |
| BindWidget | 필수 |

**HBox_ProgressInfo**

| 설정 | 값 |
|------|-----|
| 타입 | HorizontalBox |

**TextBlock_CraftingLabel**

| 설정 | 값 |
|------|-----|
| Text | "Crafting: Bandage" |
| Font Size | 9 |
| Color | (0.94, 0.62, 0.15, 0.8) |
| HBox Fill | 1.0 |
| BindWidget | 필수 |

**TextBlock_CraftingTime**

| 설정 | 값 |
|------|-----|
| Text | "1.9s / 3.0s" |
| Font Size | 9 |
| Font Family | Mono |
| Color | (0.94, 0.62, 0.15, 0.8) |
| HBox Auto Size | true |
| BindWidget | 필수 |

**SizeBox_ProgressTrack**

| 설정 | 값 |
|------|-----|
| Height Override | 4 |
| VBox Padding | T:6 |

**Image_ProgressBg**

| 설정 | 값 |
|------|-----|
| Color | (1, 1, 1, 0.06) |
| Overlay Align | Fill/Fill |
| Corner Radius | 2 (Draw As Box) |

**Image_ProgressFill**

| 설정 | 값 |
|------|-----|
| Color | #EF9F27 (Amber 400) |
| Overlay H-Align | Left |
| Overlay V-Align | Fill |
| Render Transform > Pivot | (0, 0.5) — 왼쪽 기준 확장 |
| C++에서 | SetRenderScale(FVector2D(Percent, 1.f)) |
| BindWidget | 필수 |

**ScrollBox_Recipes**

| 설정 | 값 |
|------|-----|
| 타입 | ScrollBox |
| Orientation | Vertical |
| VBox Fill | 1.0 |
| Scroll Bar Visibility | Auto |
| BindWidget | 필수 |

### 4-5b. WBP_CraftingRecipe 위젯 상세 설정

```
Border_Recipe (root — 개별 레시피 항목, Draw As=Rounded Box)
└── HorizontalBox_Content
    ├── ScaleBox_ResultIcon (Stretch: Scale to Fit)
    │   └── Image_ResultIcon (48×48, 결과물 아이콘)
    ├── VerticalBox_Info (Fill 1.0)
    │   ├── TextBlock_RecipeName ("Bandage")
    │   └── TextBlock_Ingredients ("Cloth x2 (5) · ScrapMetal x1 (3)")
    ├── TextBlock_CraftTime ("3.0s")
    └── Button_Craft ("CRAFT")
```

**Border_Recipe (root)**

| 설정 | 값 |
|------|-----|
| 타입 | Border |
| Background | (1, 1, 1, 0.03) |
| Brush > Draw As | Rounded Box |
| Brush > Outline Settings > Color | (1, 1, 1, 0.06) |
| Brush > Outline Settings > Width | 1.0 |
| Brush > Outline Settings > Corner Radii | (4, 4, 4, 4) |
| Content > Padding | 12px all |
| Margin (ScrollBox Slot) | B:6 |
| Is Variable | true |

**HorizontalBox_Content**

| 설정 | 값 |
|------|-----|
| 타입 | HorizontalBox |

**ScaleBox_ResultIcon**

| 설정 | 값 |
|------|-----|
| Stretch | Scale to Fit |
| HBox V-Align | Center |
| HBox Auto Size | true |
| HBox Padding | R:8 |

**Image_ResultIcon**

| 설정 | 값 |
|------|-----|
| 타입 | Image |
| Brush Size | 48×48 |
| BindWidget | 필수 |

**VerticalBox_Info**

| 설정 | 값 |
|------|-----|
| HBox Fill | 1.0 |
| HBox V-Align | Center |

**TextBlock_RecipeName**

| 설정 | 값 |
|------|-----|
| Text | "Bandage" |
| Font Size | 11 |
| Font Weight | Bold |
| Color | (1, 1, 1, 0.75) |
| BindWidget | 필수 |

**TextBlock_Ingredients**

| 설정 | 값 |
|------|-----|
| Text | "Cloth x2 (5) · ScrapMetal x1 (3)" |
| Font Size | 9 |
| Color | C++에서 재료별 RichText 또는 단순 텍스트 |
| VBox Padding | T:2 |
| BindWidget | 필수 |

**재료 텍스트 색상 로직:**

| 조건 | 색상 | 표시 형식 |
|------|------|----------|
| 보유량 ≥ 필요량 | (0.12, 0.63, 0.43, 0.8) 초록 | `Cloth x2 (5)` — 이름 x필요량 (보유량) |
| 보유량 < 필요량 | (0.87, 0.18, 0.18, 0.8) 빨강 | `Alcohol x1 (0)` — 동일 형식 |

**TextBlock_CraftTime**

| 설정 | 값 |
|------|-----|
| Text | "3.0s" |
| Font Size | 9 |
| Font Family | Mono |
| Color | (1, 1, 1, 0.3) |
| HBox V-Align | Center |
| HBox Auto Size | true |
| HBox Padding | L:8 R:8 |
| Min Desired Width | 28 |
| Justification | Right |
| BindWidgetOptional | 선택 |

**Button_Craft**

| 설정 | 값 |
|------|-----|
| 타입 | Button |
| HBox V-Align | Center |
| HBox Auto Size | true |
| Is Variable | true |
| BindWidget | 필수 |

Button 내부에 TextBlock_CraftLabel을 자식으로 배치:

| 설정 | 값 |
|------|-----|
| Text | "CRAFT" |
| Font Size | 9 |
| Font Weight | Bold |
| Padding | L:10 R:10 T:4 B:4 |

**Button_Craft 상태별 스타일 (C++에서 적용):**

| 상태 | Button Background | Text Color |
|------|------------------|-----------|
| Can craft | (0.12, 0.63, 0.43, 0.2) | (0.12, 0.63, 0.43, 0.9) |
| Cannot craft | (1, 1, 1, 0.04) | (1, 1, 1, 0.2) |
| Crafting in progress | (0.94, 0.62, 0.15, 0.2) | (0.94, 0.62, 0.15, 0.9), Text → "CANCEL" |

**Border_Recipe 비활성 상태 (재료 전혀 없음):**
- `SetRenderOpacity(0.45)` — 전체 위젯 반투명
- Button_Craft는 `SetIsEnabled(false)`

### 4-5c. 탭 버튼 상세 설정 (Border_Grid 내부)

```
Border_Grid (Fill: 0.45)
└── VerticalBox_GridContent
    ├── HorizontalBox_Tabs (Auto Size, Padding B:0)
    │   ├── Button_InventoryTab (Fill 1.0)
    │   │   └── TextBlock "INVENTORY"
    │   └── Button_CraftingTab (Fill 1.0)
    │       └── TextBlock "CRAFTING"
    │
    └── WidgetSwitcher_Content (Fill 1.0)
        ├── [Index 0] Overlay (기존 Grid + IconOverlay)
        └── [Index 1] WBP_CraftingPanel
```

**HorizontalBox_Tabs**

| 설정 | 값 |
|------|-----|
| VBox Auto Size | true |
| Border-Bottom | (1, 1, 1, 0.08) — 탭 아래 구분선 |

**개별 탭 버튼 공통 설정:**

| 설정 | 값 |
|------|-----|
| HBox Fill | 1.0 (각 탭이 동일 너비) |
| Background (Normal) | Transparent |
| Text Font Size | 11 |
| Text Font Weight | Bold |
| Text Padding | T:8 B:8 |
| Text Justification | Center |

**탭 상태별 스타일:**

| 상태 | Text Color | Border-Bottom |
|------|-----------|--------------|
| Active | (1, 1, 1, 0.8) | 2px solid #EF9F27 (Amber) |
| Inactive | (1, 1, 1, 0.35) | 2px solid transparent |
| Hover (Inactive) | (1, 1, 1, 0.55) | 2px solid (1, 1, 1, 0.1) |

**C++ 탭 전환 로직 (InventoryPanelWidget에 추가):**
```cpp
void UInventoryPanelWidget::OnInventoryTabClicked()
{
    WidgetSwitcher_Content->SetActiveWidgetIndex(0);
    UpdateTabStyles(0);
}

void UInventoryPanelWidget::OnCraftingTabClicked()
{
    WidgetSwitcher_Content->SetActiveWidgetIndex(1);
    UpdateTabStyles(1);
    // 크래프팅 패널 레시피 목록 갱신
    if (CraftingPanel) CraftingPanel->RefreshRecipeList();
}
```

### 4-5d. UCraftingPanelWidget C++ 상세

```cpp
UCLASS()
class PROJECT_EXFIL_API UCraftingPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void Initialize(UCraftingComponent* InCraftingComp, UInventoryComponent* InInventoryComp);
    void RefreshRecipeList();

protected:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UScrollBox> ScrollBox_Recipes;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> Border_CraftProgress;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TextBlock_CraftingLabel;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TextBlock_CraftingTime;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> Image_ProgressFill;

    UPROPERTY(EditDefaultsOnly, Category = "Crafting")
    TSubclassOf<UCraftingRecipeWidget> RecipeWidgetClass;

private:
    TWeakObjectPtr<UCraftingComponent> CraftingComp;
    TWeakObjectPtr<UInventoryComponent> InventoryComp;

    // 크래프팅 상태 변경 콜백
    UFUNCTION() void OnCraftingStateChanged(bool bIsCrafting, float RemainingTime);
    UFUNCTION() void OnCraftingCompleted(FName RecipeID);

    // 프로그레스 바 업데이트 타이머
    FTimerHandle ProgressTimerHandle;
    float CraftStartTime = 0.f;
    float CraftDuration = 0.f;
    void UpdateProgressBar();
};
```

**Initialize() 로직:**
1. CraftingComp, InventoryComp 참조 저장
2. `CraftingComp->OnCraftingStateChanged` 바인딩 → `OnCraftingStateChanged()`
3. `CraftingComp->OnCraftingCompleted` 바인딩 → `OnCraftingCompleted()`
4. `InventoryComp->OnInventoryUpdated` 바인딩 → `RefreshRecipeList()` (인벤토리 변경 시 활성/비활성 갱신)
5. `Border_CraftProgress->SetVisibility(Hidden)` 초기 상태

**RefreshRecipeList() 로직:**
1. `ScrollBox_Recipes->ClearChildren()`
2. `UItemDataSubsystem::GetAllRecipeIDs()` 순회
3. 각 RecipeID에 대해:
   - `CreateWidget<UCraftingRecipeWidget>(this, RecipeWidgetClass)` 호출
   - `RecipeWidget->SetRecipe(RecipeID, CraftingComp->CanCraft(RecipeID))` 호출
   - `RecipeWidget->OnRecipeClicked.BindUObject(this, &UCraftingPanelWidget::OnRecipeSelected)` 바인딩
   - `ScrollBox_Recipes->AddChild(RecipeWidget)` 추가

**OnCraftingStateChanged() 로직:**
- `bIsCrafting == true`: Border_CraftProgress를 Visible로, 프로그레스 타이머 시작
- `bIsCrafting == false`: Border_CraftProgress를 Hidden으로, 타이머 클리어

**UpdateProgressBar() 로직 (0.05초 간격 타이머):**
```cpp
float Elapsed = GetWorld()->GetTimeSeconds() - CraftStartTime;
float Percent = FMath::Clamp(Elapsed / CraftDuration, 0.f, 1.f);
Image_ProgressFill->SetRenderScale(FVector2D(Percent, 1.f));
TextBlock_CraftingTime->SetText(FText::FromString(
    FString::Printf(TEXT("%.1fs / %.1fs"), Elapsed, CraftDuration)));
```

### 4-5e. UCraftingRecipeWidget C++ 상세

```cpp
UCLASS()
class PROJECT_EXFIL_API UCraftingRecipeWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetRecipe(FName InRecipeID, bool bCanCraft);

    DECLARE_DELEGATE_OneParam(FOnRecipeClicked, FName);
    FOnRecipeClicked OnRecipeClicked;

protected:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> Border_Recipe;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Craft;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> Image_ResultIcon;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TextBlock_RecipeName;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TextBlock_Ingredients;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TextBlock_CraftTime;

    virtual void NativeOnInitialized() override;

private:
    FName RecipeID;
    bool bCanCraftCached = false;

    UFUNCTION() void OnButtonClicked();
};
```

**SetRecipe() 로직:**
1. RecipeID 저장
2. `UItemDataSubsystem`에서 `FCraftingRecipe` 조회
3. `TextBlock_RecipeName->SetText(Recipe.RecipeName)`
4. 재료 텍스트 조립:
   ```cpp
   FString IngredientsStr;
   for (const auto& Ing : Recipe.Ingredients)
   {
       int32 Owned = InventoryComp->GetItemCountByID(Ing.ItemDataID);
       // 형식: "ItemName xRequired (Owned)"
       IngredientsStr += FString::Printf(TEXT("%s x%d (%d)"),
           *Ing.ItemDataID.ToString(), Ing.RequiredCount, Owned);
       // 구분자
       if (&Ing != &Recipe.Ingredients.Last())
           IngredientsStr += TEXT(" · ");
   }
   TextBlock_Ingredients->SetText(FText::FromString(IngredientsStr));
   ```
5. 재료별 색상: **단순 구현** — 전체 텍스트를 `bCanCraft ? 초록 : 빨강`으로 설정. **고급 구현(Day 8)** — UE RichTextBlock으로 재료별 개별 색상 적용.
6. `TextBlock_CraftTime->SetText(FString::Printf(TEXT("%.1fs"), Recipe.CraftDuration))`
7. `Image_ResultIcon`: `UItemDataSubsystem`에서 결과물의 아이콘 텍스처 로드
8. `bCanCraft`에 따라 Button 스타일 + Opacity 적용

**OnButtonClicked() 로직:**
```cpp
void UCraftingRecipeWidget::OnButtonClicked()
{
    if (bCanCraftCached)
        OnRecipeClicked.ExecuteIfBound(RecipeID);
}
```

---

## 5. 의존성 & 연결 관계

- `UCraftingComponent` → `UInventoryComponent` (재료 확인/소비/결과물 추가), `UItemDataSubsystem` (레시피 조회)
- `UGA_Craft` → `UCraftingComponent` (StartCraft 호출)
- `UCraftingPanelWidget` → `UCraftingComponent` (CanCraft, StartCraft), `UItemDataSubsystem` (레시피 데이터)
- `UEquipmentSlotWidget` → `UEquipmentComponent` (EquipItem/UnequipItem), `UItemDataSubsystem` (아이콘)
- `UStatEntryWidget` → `USurvivalViewModel` (UpdateStat 호출)
- Day 6: `UCraftingComponent`의 Replication 추가 (서버에서 StartCraft, 클라이언트에서 UI 업데이트)

---

## 6. 주의사항 & 제약

- **`*.generated.h`는 반드시 마지막 include**
- **FieldNotify Getter 명시적 지정 필수:** `Getter="GetterName"` 형식
- **반응형 UI:** 전체 패널 레이아웃은 Anchor 비율. SizeBox는 컴포넌트 내부 최소 크기 보장용만 허용.
- **ConsumeItemByID 추가 필요:** `UInventoryComponent`에 Day 5에서 추가. 여러 슬롯에 분산된 스택을 순서대로 소비하는 로직.
- **크래프팅 취소 시 재료 복구:** StartCraft에서 소비한 재료를 CancelCraft에서 TryAddItemByID로 복구. 인벤토리가 꽉 찬 경우 드롭 또는 실패 처리 필요 (Day 8 폴리시).
- **GE는 BP로 생성:** 크래프팅 결과물이 소비 아이템(Bandage 등)이면 Day 4에서 만든 GE_Bandage 등을 재사용.
- **WidgetSwitcher:** `UWidgetSwitcher`는 Build.cs에 추가 모듈 불필요 (UMG에 포함).

---

## 7. 검증 기준 (Done Criteria)

### Phase A: UI
- [x] 인벤토리 패널이 화면 전체를 반투명 회색으로 덮음
- [x] 패널이 화면 중앙에 비율 기반으로 표시 (1080p, 1440p 동일 비율)
- [x] 슬롯이 늘어나지 않고 정상 크기 유지
- [x] 슬롯에 투명 구멍 없음
- [x] 멀티셀 아이콘(2x1, 2x3)이 여러 셀에 걸쳐 정상 표시
- [x] 6개 EquipmentSlotWidget이 올바른 SlotLabel 표시
- [x] StatsBar에 4개 스탯 표시 (HP/Hunger/Thirst/Stamina + 라벨)
- [x] 드래그앤드롭 동작 (그리드 내부 이동 + 경계 검증)
- [x] I키로 인벤토리 열기/닫기 정상 동작 (UIOnly 모드)

### Phase B: 크래프팅
- [x] `CanCraft("Recipe_Bandage")` — 재료 있을 때 true, 없을 때 false
- [x] `StartCraft("Recipe_Bandage")` — 재료 소비 + 타이머 시작
- [x] 타이머 완료 후 결과물(Bandage) 인벤토리에 추가
- [x] `CancelCraft()` — 재료 복구
- [x] `GetItemCountByID()` — 정확한 수량 반환
- [x] `ConsumeItemByID()` — 분산 스택 정상 소비
- [x] 크래프팅 UI 레시피 목록 표시
- [x] 재료 부족 레시피 비활성 표시
- [x] 레시피 클릭 시 크래프팅 시작
- [x] WidgetSwitcher 탭 전환 동작
- [x] 프로그레스 바 정상 표시 (Pivot 수정 + 완료 보장)

---

## ✅ 실제 구현 변경사항 (설계 대비)

> Day 5 구현 중 설계서와 달라진 점, 추가된 항목, 트러블슈팅 결과를 기록.

### A. WBP_InventorySlot 구조 변경

**설계서 (변경 전):**
```
Border_Black > SlotBorder > Overlay > ItemIcon + StackCountText
```

**실제 최종 구조:**
```
Border_Black > SlotBorder (자식 없음)
```

**이유:** StackCountText의 Desired Size가 UniformGridPanel 셀 크기를 불균일하게 만듦. SizeBox(0×0) 래퍼, CanvasPanel 변경 등 시도했으나 UniformGridPanel의 Desired Size 전파 특성상 근본 해결 불가. StackCountText를 SlotWidget에서 완전히 제거하고 IconOverlay에서 렌더링하는 방식으로 변경.

### B. StackCountText → IconOverlay 이동

IconOverlay::RefreshIcons()에서 아이콘 Image 생성 시, StackCount > 1이면 TextBlock도 함께 CanvasPanel에 배치:
- 폰트 크기 12, 색상 (1,1,1,0.9)
- 위치: 루트 슬롯의 좌하단 (SlotSize * 0.3f 오프셋)

### C. IconOverlay geometry 기반 셀 좌표 계산

**설계서:** CellPixelSize(고정값) 기반 좌표 계산
**실제:** GridPanel의 LocalSize에서 직접 나누는 단순 방식

- 시그니처: `RefreshIcons(VM, float CellSize)` → `RefreshIcons(VM, UUniformGridPanel*, GridWidth, GridHeight)`
- CellStride = GridPanel LocalSize / Grid칸수 (패딩 포함 정확한 셀 간격)
- SlotSize = 첫 슬롯의 GetCachedGeometry().GetLocalSize() (패딩 제외 콘텐츠 영역)
- Origin = (0, 0) — GridPanel과 IconOverlay의 AbsolutePosition이 동일하므로
- Position은 CellStride(패딩 포함), Size는 SlotSize(패딩 미포함) 사용
- Absolute 좌표 차이 + Scale 변환 방식은 DPI Scale 문제로 폐기

```cpp
FVector2D GridLocalSize = InGridPanel->GetCachedGeometry().GetLocalSize();
FVector2D SlotSize = InGridPanel->GetChildAt(0)->GetCachedGeometry().GetLocalSize();
FVector2D CellStride(GridLocalSize.X / InGridWidth, GridLocalSize.Y / InGridHeight);
FVector2D Origin = FVector2D::ZeroVector;
```

### D. 아이콘 ScaleToFit + 중앙 정렬

텍스처 원본 비율 유지:
```cpp
float Scale = FMath::Min(IconSize.X / TexSize.X, IconSize.Y / TexSize.Y);
FVector2D FinalSize = TexSize * Scale;
FVector2D Offset = (IconSize - FinalSize) * 0.5f;
```

### E. IconOverlay Hit-Test 차단

`IconImage->SetVisibility(ESlateVisibility::HitTestInvisible)` — 아이콘이 마우스를 먹지 않도록. 텍스처가 nullptr인 아이템은 continue로 스킵.

### F. 셀 정사각형 동적 계산 (NativePaint 2단계 + 0.1초 타이머)

1. NativePaint: GridPanel 첫 슬롯의 CellWidth 읽기 → `SetMinDesiredSlotHeight(CellWidth)`
2. 0.1초 타이머: 레이아웃 전파 완료 후 `RefreshIconOverlay()` 실행

**이유:** NativePaint 안에서 높이 설정과 아이콘 배치를 동시에 하면, 세로 geometry가 아직 반영 안 된 상태에서 읽히므로 아이콘이 납작하게 그려짐. 0.1초 딜레이로 완전한 레이아웃 전파 보장.

```cpp
// .h
bool bNeedsCellSquareFix = true;
FTimerHandle IconRefreshTimerHandle;

// NativePaint: 높이 설정만
// 0.1초 타이머: RefreshIconOverlay()
// OnInventoryUpdated: RefreshIconOverlay() 즉시 (geometry 이미 확정)
// NativeOnActivated: bNeedsCellSquareFix = true (재오픈 시 재계산)
```

### G. GridPanel 셀 최소 크기

SizeBox 래퍼 대신 UniformGridPanel 내장 API 사용:
```cpp
GridPanel->SetMinDesiredSlotWidth(0.f);   // 너비는 Fill이 결정
GridPanel->SetMinDesiredSlotHeight(CellWidth);  // 높이를 너비에 맞춤
```

GridPanel Overlay Slot: H-Align = Fill, V-Align = Top
IconOverlay Overlay Slot: H-Align = Fill, V-Align = Top (둘 다 Top으로 동일)

### H. 드래그 관련 수정 3건

1. **드래그 취소 시 색상 복구:** NativeOnDragCancelled 추가 → 빨간 하이라이트 해제
2. **드래그 하이라이트 전체 영역 검증:** NativeOnDragEnter에서 아이템 전체 영역의 모든 슬롯을 순회하여 빈 슬롯/다른 아이템 점유/자기 자신/범위 밖 검사
3. **그리드 경계 벗어남:** 경계 밖이면 ClearAreaHighlights() + return

### I. BP 토글 수정 (설계서 대비 변경)

**설계서:** SetInputMode(GameAndUI)
**실제:** SetInputMode(UIOnly) — 배경 완전 고정

I키 닫기는 캐릭터 BP가 아닌 **위젯의 NativeOnKeyDown()**에서 처리:
```cpp
FReply UInventoryPanelWidget::NativeOnKeyDown(...)
{
    if (InKeyEvent.GetKey() == EKeys::I)
    {
        RemoveFromParent();
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(...);
}
```
NativeConstruct()에서 `SetKeyboardFocus()` 호출 필수.

### J. UE 5.6 에디터 속성 수정

1. **Border 테두리:** `Brush > Draw As = Rounded Box`로 설정해야 `Outline Settings` (Color, Width, Corner Radii) 노출. Box/Image 모드에서는 안 보임.
2. **Image 스케일링:** Image에 Stretch 속성 없음. ScaleBox 래퍼로 감싸서 `Stretch = Scale to Fit` 사용.
3. **Image_ProgressFill Render Transform Pivot:** (0, 0.5) — 왼쪽 기준으로 오른쪽으로 확장.

### K. 크래프팅 UI 추가 수정

1. **프로그레스 바 Pivot:** Image_ProgressFill의 Render Transform > Pivot = (0, 0.5)
2. **OnCraftingCompleted:** 100% 표시 + 0.5초 딜레이 후 Hidden
3. **레시피 슬롯 간격:** ScrollBoxSlot->SetPadding(FMargin(0,0,0,8)) — 코드에서 처리
4. **레시피 아이콘:** ScaleBox 래퍼로 비율 유지

### L. StatsBar 라벨 추가

WBP_StatEntry에 TextBlock_StatLabel 추가 (BindWidgetOptional):
- Image_Icon과 SizeBox_Track 사이에 배치
- Font 9 Bold, Color (1,1,1,0.35)
- NativeOnInitialized에서 StatType별 라벨 설정: Health→"HP", Hunger→"HU", Thirst→"TH", Stamina→"ST"

---

## ✅ 완료 보고

**작성자:** 채팅 탭 (Opus 4.6)
**완료 시각:** 2026-03-23
**상태:** Phase A + Phase B 전체 완료

### 1. 완료 항목
- [x] CanvasPanel_Root + Border_Background + Border_Panel 구조
- [x] HorizontalBox Equipment(0.55) + Grid(0.45)
- [x] WBP_EquipmentSlot 6개 인스턴스 배치 + SlotLabel 자동 설정
- [x] WBP_StatEntry 4개 인스턴스 + StatLabel(HP/HU/TH/ST) 추가
- [x] StackCountText → IconOverlay 이동 (UniformGridPanel 셀 크기 균일화)
- [x] 셀 정사각형 동적 계산 (NativePaint + 0.1초 타이머 분리)
- [x] IconOverlay geometry 기반 stride 계산 (GridPanel LocalSize / 칸수, DPI-safe)
- [x] 아이콘 ScaleToFit + 중앙 정렬
- [x] Hit-Test 차단 (HitTestInvisible)
- [x] 드래그 하이라이트 전체 영역 검증 + 경계 체크 + 취소 복구
- [x] BP 토글: UIOnly 모드 + NativeOnKeyDown I키 닫기
- [x] INVENTORY/CRAFTING 탭 전환 (WidgetSwitcher)
- [x] UCraftingComponent + UGA_Craft 구현
- [x] ConsumeItemByID / GetItemCountByID 추가
- [x] WBP_CraftingPanel + WBP_CraftingRecipe 생성
- [x] 프로그레스 바 Pivot(0,0.5) + 완료 시 100% 보장

### 2. 미완료 / 내일로 이월
- 없음

### 3. 트러블슈팅

| 문제 | 원인 | 해결 |
|------|------|------|
| StackCountText 있는 셀만 크기 다름 | UniformGridPanel이 가장 큰 자식 Desired Size로 셀 크기 결정 | StackCountText를 IconOverlay로 이동 |
| 아이콘 위치/크기 불일치 | CellPixelSize 하드코딩이 SlotPadding 미반영 | GridPanel 자식 geometry에서 stride 동적 계산 |
| 셀이 세로로 늘어남 | GridPanel V-Align Fill → 전체 높이 균등 분배 | V-Align Top으로 변경 |
| 셀이 직사각형 | 가로는 Fill로 자동, 세로는 MinDesired 기본값 | NativePaint에서 CellWidth 읽어 MinDesiredSlotHeight 동적 설정 |
| 첫 PIE 아이콘 찌그러짐 | NativePaint 내 높이 설정 + 아이콘 배치 동시 실행 → 세로 geometry 미반영 | 높이 설정 후 0.1초 타이머로 아이콘 배치 분리 |
| 아이콘 Image가 마우스 먹음 | IconOverlay Image의 기본 Visibility가 Visible | HitTestInvisible 설정 |
| 드래그 후 빨간색 유지 | NativeOnDragCancelled 미구현 | 드래그 취소 시 하이라이트 해제 로직 추가 |
| 프로그레스 바 중심에서 양쪽 확장 | SetRenderScale Pivot 기본값 (0.5, 0.5) | Image_ProgressFill Pivot (0, 0.5)로 변경 |
| I키로 인벤토리 안 닫힘 | UIOnly 모드에서 캐릭터 BP 키 이벤트 수신 불가 | 위젯 NativeOnKeyDown에서 I키 처리 |
| UE 에디터 Border Outline 안 보임 | Draw As가 Box/Image면 Outline Settings 미노출 | Draw As = Rounded Box로 변경 |
| Image에 Stretch 없음 | UE5 Image 위젯에 Stretch 속성 없음 | ScaleBox 래퍼로 Scale to Fit 적용 |
| CellStride DPI Scale 불일치 | Absolute 좌표 차이 → Local 변환 시 Scale 문제 | Absolute 방식 폐기, GridPanel LocalSize / 칸수로 단순 계산 |

### 4. 파일 구조 변경사항

| 파일 | 변경 유형 | 변경 내용 요약 |
|------|----------|---------------|
| UI/EquipmentSlotWidget.h/.cpp | 신규 | 장비 슬롯 UI (RefreshSlot, SlotLabel, 드래그) |
| UI/StatEntryWidget.h/.cpp | 신규 | 스탯 바 엔트리 (UpdateStat, StatType별 색상, StatLabel) |
| Crafting/CraftingComponent.h/.cpp | 신규 | CanCraft, StartCraft, CancelCraft |
| GAS/GA_Craft.h/.cpp | 신규 | 크래프팅 GameplayAbility |
| UI/CraftingPanelWidget.h/.cpp | 신규 | 크래프팅 패널 (RefreshRecipeList, 프로그레스 바) |
| UI/CraftingRecipeWidget.h/.cpp | 신규 | 개별 레시피 엔트리 (SetRecipe, 버튼 스타일) |
| UI/InventoryPanelWidget.h/.cpp | 수정 | CanvasPanel 구조, 탭 전환, NativePaint 셀 정사각형, NativeOnKeyDown I키, 0.1초 타이머 |
| UI/InventoryIconOverlay.h/.cpp | 수정 | LocalSize/칸수 기반 stride, StackCountText 렌더링, HitTestInvisible, ScaleToFit |
| UI/InventorySlotWidget.h/.cpp | 수정 | StackCountText 제거, 드래그 전체 영역 검증, 경계 체크, 취소 복구 |
| Inventory/InventoryComponent.h/.cpp | 수정 | ConsumeItemByID, GetItemCountByID 추가 |
| Core/EXFILCharacter.h/.cpp | 수정 | CraftingComponent 부착 |
| Content/UI/Inventory/WBP_*.uasset | 수정/신규 | EquipmentSlot, StatEntry, CraftingPanel, CraftingRecipe, InventoryPanel 전면 개편 |

### 5. 다음 Day 참고사항
- UniformGridPanel의 셀 크기는 자식 Desired Size로 결정 → 셀 내부 위젯 설계 시 주의
- GetCachedGeometry()는 레이아웃 패스 이후에만 유효 → NativePaint에서 읽기
- SetRenderScale은 Pivot 기준으로 동작 → Pivot 설정 필수
- UIOnly 모드에서는 캐릭터 BP의 키 이벤트 수신 불가 → 위젯 NativeOnKeyDown에서 처리
- NativePaint에서 geometry 읽기 + 아이콘 배치를 동시에 하면 안 됨 → 타이머로 분리 필요
- Absolute 좌표 기반 stride 계산은 DPI Scale 변환 문제 발생 → GridPanel LocalSize / 칸수로 단순 계산이 정확

### 6. Git 커밋
```
feat(Day5): Inventory UI overhaul + Crafting system

[Phase A - UI Overhaul]
- CanvasPanel_Root + Border_Background + Border_Panel layout
- WBP_EquipmentSlot (6 instances) + WBP_StatEntry (4 instances with labels)
- StackCountText moved from SlotWidget to IconOverlay
- Dynamic square cell: NativePaint reads CellWidth → SetMinDesiredSlotHeight + 0.1s timer
- IconOverlay geometry-based cell stride (LocalSize/GridCount, DPI-safe)
- Icon ScaleToFit with center alignment + HitTestInvisible
- Drag highlight: full-area validation + boundary check + cancel restore
- BP toggle: UIOnly mode + NativeOnKeyDown for I-key close

[Phase B - Crafting System]
- UCraftingComponent: CanCraft, StartCraft, CancelCraft
- UGA_Craft: GameplayAbility for crafting
- InventoryComponent: ConsumeItemByID, GetItemCountByID
- WBP_CraftingPanel + WBP_CraftingRecipe widgets
- INVENTORY/CRAFTING tab switch via WidgetSwitcher
- Progress bar with Pivot(0, 0.5) + completion guarantee
- Ingredient text color: green(sufficient) / red(insufficient)
```
