# Day 2 설계 지시서: MVVM + Common UI Inventory Panel

> 채팅 탭(Opus)이 상단 "설계 지시서" 섹션을 작성 → 수현이 검토 후 Docs/DailyLogs/Day2_Design.md로 저장
> 프로젝트 연결 탭이 하단 "완료 보고"를 작성 → 수현이 채팅 탭에 전달

---

## ⚠️ Day 2 시작 전 패치 (Day 1 완료 후 수정사항)

### 패치 1: CLAUDE.md / ARCHITECTURE.md 모듈명 수정

프로젝트 연결 탭에서 아래 명령 실행:

```
CLAUDE.md와 Docs/ARCHITECTURE.md에서 다음을 찾아 바꿔줘:
- EXFIL_API → PROJECT_EXFIL_API
- 모듈명: EXFIL → 모듈명: Project_EXFIL
- Source/EXFIL/ → Source/Project_EXFIL/
- EXFIL.Build.cs → Project_EXFIL.Build.cs
- EXFIL.uproject → Project_EXFIL.uproject
```

### 패치 2: CLAUDE.md에 모델 선택 가이드 추가

"작업 흐름" 섹션 아래에 추가:

```markdown
## 모델 선택 가이드

- **기본:** Sonnet 4.6 (설계서가 상세하므로 구현에 충분)
- **Opus 4.6 전환 시점:**
  - GAS 연동에서 복잡한 상속/보일러플레이트 문제 발생 시
  - Replication/RPC 디버깅에서 원인 파악이 어려울 시
  - 아키텍처 수준의 판단이 필요한 예외 상황 발생 시
- 설계 변경이 필요하면 구현 탭에서 해결하지 말고 채팅 탭(Opus)에 문의
```

### 패치 3: BeginPlay 테스트 코드 삭제

`InventoryComponent.cpp`의 BeginPlay에서 Day 1 테스트 코드(TryAddItem, DebugPrintGrid 호출부)를 삭제. `InitializeGrid()` 호출만 유지.

---

## 📋 Day 2 설계 지시서

**작성자:** 채팅 탭 (Opus 4.6)
**WBS 분류:** UI (MVVM + Common UI)
**목표 시간:** 15h
**날짜:** 2026-03-18

## 1. 오늘의 목표

- UE5 **UMG ViewModel 플러그인**(`UMVVMViewModelBase`) 기반 `UInventoryViewModel` 구현
- `UCommonActivatableWidget` 기반 **인벤토리 패널** C++ 구현
- **드래그앤드롭** UI 로직 및 Day 1 `UInventoryComponent` 배열 상태 동기화
- MVVM 단방향 흐름 증명: Model → ViewModel → View (역방향 금지)
- 게임패드/키보드 **입력 자동 전환** (Common UI Input Routing) 기초 세팅

## 2. 사전 준비 (Build.cs + 플러그인)

### 2-1. 플러그인 활성화 (.uproject)

`Project_EXFIL.uproject`의 Plugins 섹션에 추가:

```json
{"Name": "CommonUI", "Enabled": true},
{"Name": "ModelViewViewModel", "Enabled": true}
```

### 2-2. Build.cs 의존성 추가

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    // 기존 Day 1
    "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
    // Day 2 추가
    "UMG", "SlateCore", "Slate",
    "CommonUI", "CommonInput",
    "ModelViewViewModel"
});
```

## 3. 생성할 파일 목록

| 파일 경로 | 용도 | 신규/수정 |
|-----------|------|----------|
| `Source/Project_EXFIL/UI/InventoryViewModel.h` | ViewModel — 그리드 데이터를 UI 친화적 형태로 변환 | 신규 |
| `Source/Project_EXFIL/UI/InventoryViewModel.cpp` | ViewModel 구현 | 신규 |
| `Source/Project_EXFIL/UI/InventorySlotViewModel.h` | 개별 슬롯 ViewModel | 신규 |
| `Source/Project_EXFIL/UI/InventorySlotViewModel.cpp` | 슬롯 ViewModel 구현 | 신규 |
| `Source/Project_EXFIL/UI/InventoryPanelWidget.h` | View — 인벤토리 패널 위젯 (CommonUI) | 신규 |
| `Source/Project_EXFIL/UI/InventoryPanelWidget.cpp` | 패널 위젯 구현 | 신규 |
| `Source/Project_EXFIL/UI/InventorySlotWidget.h` | View — 개별 슬롯 위젯 | 신규 |
| `Source/Project_EXFIL/UI/InventorySlotWidget.cpp` | 슬롯 위젯 구현 | 신규 |
| `Source/Project_EXFIL/UI/InventoryDragDropOp.h` | 드래그앤드롭 오퍼레이션 | 신규 |
| `Source/Project_EXFIL/UI/InventoryDragDropOp.cpp` | 드래그 오퍼레이션 구현 | 신규 |
| `Source/Project_EXFIL/Inventory/InventoryComponent.cpp` | BeginPlay 테스트 코드 삭제 | 수정 |
| `Source/Project_EXFIL/Project_EXFIL.Build.cs` | UI/MVVM 모듈 의존성 추가 | 수정 |
| `Project_EXFIL.uproject` | 플러그인 활성화 | 수정 |

## 4. MVVM 아키텍처 — 데이터 흐름

```
[UInventoryComponent]  ← Model (Day 1)
       │
       │ OnItemAdded / OnItemRemoved / OnInventoryUpdated
       ▼
[UInventoryViewModel : UMVVMViewModelBase]  ← ViewModel
       │
       │ FieldNotify + UE_MVVM_SET_PROPERTY_VALUE
       ▼
[UInventoryPanelWidget : UCommonActivatableWidget]  ← View
       │
       │ 그리드 UniformGridPanel
       ▼
[UInventorySlotWidget × (GridWidth × GridHeight)]  ← View (개별 셀)
       │
       │ Bind to UInventorySlotViewModel
       ▼
   아이콘 표시, 드래그앤드롭 처리
```

**핵심 규칙:**
- View → ViewModel: 읽기(바인딩) + Request(사용자 입력 전달)
- ViewModel → Model: 함수 호출 (TryAddItem, MoveItem 등)
- Model → ViewModel: 델리게이트 콜백
- **View → Model 직접 참조: 절대 금지**

## 5. 클래스별 헤더 시그니처

### 5-1. UInventorySlotViewModel (개별 슬롯 ViewModel)

```cpp
// InventorySlotViewModel.h
#pragma once
#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "InventorySlotViewModel.generated.h"

UCLASS()
class PROJECT_EXFIL_API UInventorySlotViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

    friend class UInventoryViewModel; // Model이 데이터 세팅

public:
    // === Getters (View가 바인딩) ===
    UFUNCTION(BlueprintPure, FieldNotify)
    bool IsEmpty() const { return bEmpty; }

    UFUNCTION(BlueprintPure, FieldNotify)
    FName GetItemDataID() const { return ItemDataID; }

    UFUNCTION(BlueprintPure, FieldNotify)
    int32 GetStackCount() const { return StackCount; }

    UFUNCTION(BlueprintPure, FieldNotify)
    bool IsRootSlot() const { return bIsRootSlot; }

    UFUNCTION(BlueprintPure, FieldNotify)
    FIntPoint GetGridPosition() const { return GridPosition; }

    UFUNCTION(BlueprintPure, FieldNotify)
    FGuid GetItemInstanceID() const { return ItemInstanceID; }

    // === Request (View → ViewModel → Model) ===
    UFUNCTION(BlueprintCallable, Category = "Inventory|Request")
    void RequestDrop();

protected:
    // === Setters (ViewModel이 호출, View 접근 불가) ===
    void SetEmpty(bool bNewValue);
    void SetItemDataID(FName NewValue);
    void SetStackCount(int32 NewValue);
    void SetIsRootSlot(bool bNewValue);
    void SetGridPosition(FIntPoint NewValue);
    void SetItemInstanceID(FGuid NewValue);

private:
    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter="SetEmpty",
              meta=(AllowPrivateAccess=true))
    bool bEmpty = true;

    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter="SetItemDataID",
              meta=(AllowPrivateAccess=true))
    FName ItemDataID;

    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter="SetStackCount",
              meta=(AllowPrivateAccess=true))
    int32 StackCount = 0;

    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter="SetIsRootSlot",
              meta=(AllowPrivateAccess=true))
    bool bIsRootSlot = false;

    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter="SetGridPosition",
              meta=(AllowPrivateAccess=true))
    FIntPoint GridPosition = FIntPoint::ZeroValue;

    UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter="SetItemInstanceID",
              meta=(AllowPrivateAccess=true))
    FGuid ItemInstanceID;
};
```

### 5-2. UInventoryViewModel (전체 인벤토리 ViewModel)

```cpp
// InventoryViewModel.h
#pragma once
#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "InventorySlotViewModel.h"
#include "InventoryViewModel.generated.h"

class UInventoryComponent;

UCLASS()
class PROJECT_EXFIL_API UInventoryViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    // Model(InventoryComponent)에 바인딩
    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void Initialize(UInventoryComponent* InInventoryComponent);

    // 슬롯 ViewModel 조회
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    UInventorySlotViewModel* GetSlotAt(FIntPoint Position) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    TArray<UInventorySlotViewModel*> GetAllSlots() const;

    // 그리드 사이즈
    UFUNCTION(BlueprintPure, FieldNotify)
    int32 GetGridWidth() const { return GridWidth; }

    UFUNCTION(BlueprintPure, FieldNotify)
    int32 GetGridHeight() const { return GridHeight; }

    // === 사용자 액션 (View → ViewModel → Model) ===
    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    bool RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition,
                         bool bNewRotated = false);

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    bool RequestRemoveItem(FGuid ItemInstanceID);

private:
    UPROPERTY()
    TWeakObjectPtr<UInventoryComponent> InventoryComp;

    UPROPERTY()
    TArray<TObjectPtr<UInventorySlotViewModel>> SlotViewModels;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter,
              meta=(AllowPrivateAccess=true))
    int32 GridWidth = 0;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter,
              meta=(AllowPrivateAccess=true))
    int32 GridHeight = 0;

    // Model 델리게이트 콜백
    UFUNCTION()
    void OnInventoryUpdated();

    UFUNCTION()
    void OnItemAdded(const FInventoryItemInstance& AddedItem);

    UFUNCTION()
    void OnItemRemoved(const FGuid& RemovedItemID);

    // 전체 그리드 상태를 SlotViewModels에 반영
    void RefreshAllSlots();

    // 헬퍼
    int32 PositionToIndex(FIntPoint Position) const;
};
```

### 5-3. UInventoryPanelWidget (인벤토리 패널 View)

```cpp
// InventoryPanelWidget.h
#pragma once
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "InventoryPanelWidget.generated.h"

class UInventoryViewModel;
class UInventorySlotWidget;
class UUniformGridPanel;

UCLASS(Abstract)
class PROJECT_EXFIL_API UInventoryPanelWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void SetViewModel(UInventoryViewModel* InViewModel);

protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual bool NativeOnHandleBackAction() override;
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UUniformGridPanel> GridPanel;

    UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI")
    TSubclassOf<UInventorySlotWidget> SlotWidgetClass;

private:
    UPROPERTY()
    TObjectPtr<UInventoryViewModel> ViewModel;

    UPROPERTY()
    TArray<TObjectPtr<UInventorySlotWidget>> SlotWidgets;

    void BuildGrid();
    void ClearGrid();
};
```

### 5-4. UInventorySlotWidget (개별 슬롯 View)

```cpp
// InventorySlotWidget.h
#pragma once
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "InventorySlotWidget.generated.h"

class UInventorySlotViewModel;
class UImage;
class UTextBlock;
class UBorder;

UCLASS(Abstract)
class PROJECT_EXFIL_API UInventorySlotWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void SetSlotViewModel(UInventorySlotViewModel* InSlotVM);

protected:
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry,
        const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragEnter(const FGeometry& InGeometry,
        const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent,
        UDragDropOperation* InOperation) override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UImage> ItemIcon;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UTextBlock> StackCountText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UBorder> SlotBorder;

private:
    UPROPERTY()
    TObjectPtr<UInventorySlotViewModel> SlotVM;

    void RefreshVisuals();
    void SetHighlight(bool bHighlighted);
};
```

### 5-5. UInventoryDragDropOp

```cpp
// InventoryDragDropOp.h
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "InventoryDragDropOp.generated.h"

UCLASS()
class PROJECT_EXFIL_API UInventoryDragDropOp : public UDragDropOperation
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite)
    FGuid DraggedItemInstanceID;

    UPROPERTY(BlueprintReadWrite)
    FIntPoint OriginalPosition;

    UPROPERTY(BlueprintReadWrite)
    FItemSize ItemSize;

    UPROPERTY(BlueprintReadWrite)
    bool bWasRotated = false;

    UPROPERTY(BlueprintReadWrite)
    FName ItemDataID;
};
```

## 6. 핵심 함수 구현 가이드

### `UInventoryViewModel::Initialize(InInventoryComponent)`
- **목적:** Model 참조 저장 + 델리게이트 바인딩 + SlotViewModel 배열 생성
- **핵심 로직:**
  1. `InventoryComp = InInventoryComponent`
  2. 델리게이트 바인딩: `OnInventoryUpdated`, `OnItemAdded`, `OnItemRemoved`
  3. GridWidth/GridHeight 저장
  4. `SlotViewModels.SetNum(GridWidth * GridHeight)` → 각 슬롯에 `NewObject<UInventorySlotViewModel>(this)` 생성
  5. 각 SlotVM에 `SetGridPosition()` 호출
  6. `RefreshAllSlots()` 호출
- **주의:** SlotViewModel은 ViewModel이 소유 (Outer = this)

### `UInventoryViewModel::RefreshAllSlots()`
- **목적:** Model의 전체 그리드 상태를 SlotViewModels에 동기화
- **핵심 로직:** 각 그리드 좌표를 순회하며 `InventoryComp->GetItemAt(Position)` 호출 → 결과에 따라 SlotVM의 Setter 호출
- **주의:** FieldNotify가 자동으로 View에 변경을 알림

### `UInventoryPanelWidget::BuildGrid()`
- **목적:** GridPanel에 SlotWidgetClass 인스턴스를 GridWidth × GridHeight만큼 생성
- **핵심 로직:**
  1. `ClearGrid()` 호출
  2. 이중 루프로 `CreateWidget<UInventorySlotWidget>(this, SlotWidgetClass)` 생성
  3. 각 SlotWidget에 `SetSlotViewModel(ViewModel->GetSlotAt(Position))` 호출
  4. `GridPanel->AddChildToUniformGrid(SlotWidget, Y, X)` 배치
- **주의:** SlotWidgetClass는 BP 서브클래스로 지정 (C++에서 레이아웃 정의 금지)

### `UInventorySlotWidget::NativeOnDragDetected()`
- **목적:** 아이템이 있는 슬롯에서 드래그 시작
- **핵심 로직:**
  1. SlotVM이 비어있으면 무시
  2. `UInventoryDragDropOp` 생성 → 아이템 정보 채우기
  3. `OutOperation = DragOp`
- **주의:** DefaultDragVisual을 세팅하면 드래그 중 아이콘 표시 가능

### `UInventorySlotWidget::NativeOnDrop()`
- **목적:** 드롭 시 Model에 이동 요청
- **핵심 로직:**
  1. `UInventoryDragDropOp`으로 캐스팅
  2. `SlotVM->GetGridPosition()`으로 드롭 대상 좌표 확인
  3. 부모 Panel의 ViewModel을 통해 `RequestMoveItem()` 호출
  4. 성공/실패에 따라 피드백 (하이라이트 색상 변경)
- **주의:** **실제 데이터 조작은 ViewModel → Model 경유** (View에서 직접 호출 금지)

## 7. Common UI 세팅 가이드

### 7-1. Input Routing

```cpp
TOptional<FUIInputConfig> UInventoryPanelWidget::GetDesiredInputConfig() const
{
    FUIInputConfig Config;
    Config.InputMode = ECommonInputMode::Menu;
    Config.MouseCaptureMode = EMouseCaptureMode::NoCapture;
    return Config;
}
```

### 7-2. Back 액션 처리

```cpp
bool UInventoryPanelWidget::NativeOnHandleBackAction()
{
    DeactivateWidget();
    return true;
}
```

## 8. 의존성 & 연결 관계

- `UInventoryViewModel` → `UInventoryComponent` (Day 1) 델리게이트 구독
- `UInventoryPanelWidget` → `UInventoryViewModel` (SetViewModel)
- `UInventorySlotWidget` → `UInventorySlotViewModel` (SetSlotViewModel)
- `UInventoryDragDropOp` → `EXFILInventoryTypes.h` (FGuid, FItemSize, FIntPoint)
- **Build.cs 추가:** `UMG`, `SlateCore`, `Slate`, `CommonUI`, `CommonInput`, `ModelViewViewModel`
- 캐릭터에서 ViewModel 생성 및 초기화 코드 추가 필요

## 9. 주의사항 & 제약

- **`*.generated.h`는 반드시 마지막 include**
- **UMG ViewModel 플러그인 활성화 필수:** `ModelViewViewModel` (Project Settings → Plugins)
- **CommonUI 플러그인 활성화 필수:** `CommonUI`, `CommonInput`
- **FieldNotify 매크로:** `UE_MVVM_SET_PROPERTY_VALUE(Property, NewValue)` — 값이 변경될 때만 Broadcast
- **SlotWidgetClass는 BP 서브클래스:** C++ 헤더만 정의, 실제 레이아웃은 WBP(Widget Blueprint)에서 구성
- **View에서 Model 직접 참조 절대 금지:** 모든 데이터 접근은 ViewModel 경유
- **드래그 중 롤백:** NativeOnDrop 실패 시 원래 슬롯으로 복귀 (MoveItem 롤백은 Day 1에서 이미 구현)
- **Tick 사용 금지:** 모든 UI 갱신은 FieldNotify 또는 델리게이트 기반

## 10. 검증 기준 (Done Criteria)

- [x] UBT 빌드 성공
- [x] UMG ViewModel 플러그인 + CommonUI 플러그인 활성화 확인
- [x] InventoryPanelWidget WBP 생성 → GridPanel에 슬롯 위젯이 10×6 그리드로 정상 배치
- [x] BeginPlay에서 테스트 아이템 추가 시 해당 슬롯에 시각적 변화 (아이콘/색상)
- [x] 아이템 있는 슬롯에서 드래그 시작 → 드래그 비주얼 표시 (멀티셀 포함)
- [x] 빈 슬롯에 드롭 → Model에 MoveItem 호출 → 그리드 상태 정상 갱신
- [x] 점유된 슬롯에 드롭 → 실패 피드백 (빨간 하이라이트 등)
- [x] ~~ESC/B 버튼으로 인벤토리 패널 닫기 동작~~ → **I 키 토글로 대체** (ESC는 PIE 종료 단축키 충돌)
- [x] View에서 Model 직접 참조가 없는지 코드 리뷰
- [x] UE 에디터 PIE 테스트 통과

---

## ✅ 완료 보고

**작성자:** 프로젝트 연결 탭
**작성 시점:** 2026-03-18

### 1. 완료 항목
- [x] 패치 1: CLAUDE.md, ARCHITECTURE.md 모듈명 수정 (`EXFIL_API` → `PROJECT_EXFIL_API`, 경로/파일명 일괄 수정)
- [x] 패치 2: CLAUDE.md "모델 선택 가이드" 섹션 추가
- [x] 패치 3: InventoryComponent.cpp BeginPlay 테스트 코드 삭제 — `InitializeGrid()` 호출만 유지
- [x] Project_EXFIL.uproject: CommonUI, ModelViewViewModel 플러그인 활성화
- [x] Project_EXFIL.Build.cs: SlateCore, CommonUI, CommonInput, ModelViewViewModel 모듈 추가. UI 폴더 PublicIncludePaths 추가
- [x] `UInventorySlotViewModel` 헤더/구현 생성 (FieldNotify Getter/Setter 패턴)
- [x] `UInventoryViewModel` 헤더/구현 생성 (Model 델리게이트 구독, RefreshAllSlots)
- [x] `UInventoryPanelWidget` 헤더/구현 생성 (CommonActivatableWidget, BuildGrid, Input Routing)
- [x] `UInventorySlotWidget` 헤더/구현 생성 (DragDetected, Drop, DragEnter/Leave, 하이라이트)
- [x] `UInventoryDragDropOp` 헤더/구현 생성

### 2. 미완료 / 내일로 이월
- 없음. Done Criteria 전항목 통과.

### 3. 트러블슈팅
| 문제 | 원인 | 해결 |
|------|------|------|
| UHT Getter 네이밍 오류 | `Getter` 지정자 단독 사용 시 `GetbEmpty()` 형태 자동 탐색 | UPROPERTY에 `Getter="IsEmpty"` 명시적 지정 |
| `FUIInputConfig` protected 멤버 접근 오류 | UE 5.6에서 멤버가 protected로 변경 | 생성자 `FUIInputConfig(ECommonInputMode, EMouseCaptureMode)` 방식으로 변경 |
| `GameplayTags` 링커 에러 | CommonUI 내부에서 `FUIActionTag` (GameplayTags 기반) 사용하나 Build.cs에 누락 | `"GameplayTags"` 모듈 추가 |
| PIE 시작 직후 즉시 종료 (`BeginTearingDown`) | `r.RayTracing=True` 설정으로 SBT 재초기화 중 크래시 | `DefaultEngine.ini`에서 `r.RayTracing=False`로 변경 |
| BindWidget 미연결 오류 | WBP 위젯 이름 불일치 또는 `Is Variable` 미체크 | 위젯명을 C++ 변수명과 정확히 일치시키고 자물쇠(Is Variable) 체크 |
| `CommonGameViewportClient` 경고 | CommonUI Input Routing이 기본 ViewportClient에서 미작동 | `DefaultEngine.ini`에 `GameViewportClientClassName` 추가 |
| UniformGridPanel 화면 전체 확장 | 크기 제약 없이 AddToViewport 시 풀스크린으로 확장됨 | WBP_InventoryPanel에 Canvas Panel → Border → SizeBox(676×404) → GridPanel 계층 구성 |
| 슬롯 드래그 시 원본 슬롯에 구멍 발생 | `DefaultDragVisual = this` 사용 시 원본 위젯이 Grid에서 탈착됨 | `CreateWidget(GetOwningPlayer(), GetClass())`로 복사본 생성 후 DragVisual로 사용 |
| 비루트 슬롯 드래그 시 MoveItem 범위 초과 | 드롭 위치를 아이템 루트로 그대로 전달 | DragOp에 `DragOffset` 추가, 드롭 위치에서 오프셋 역산하여 루트 위치 계산 |
| NativeOnDrop에서 ViewModel 직접 접근 불가 | SlotWidget → ViewModel 직접 참조는 MVVM 위반 | PanelWidget에 `ForwardMoveRequest()` 중계 함수 추가 |
| BP_ThirdPersonCharacter 부모 클래스 미스매치 | 기본 부모가 `AProject_EXFILCharacter`라 `InventoryPanelWidgetClass` 노출 안 됨 | Reparent Blueprint → `AEXFILCharacter`로 변경 |
| `I` 키 토글 미작동 | `Set Input Mode UI Only` 상태에서 캐릭터 이벤트 그래프 키 입력 차단 | `Set Input Mode Game And UI`로 변경 |

### 4. 파일 구조 변경사항
| 파일 | 변경 유형 | 변경 내용 요약 |
|------|----------|---------------|
| `CLAUDE.md` | 수정 | 모듈명 PROJECT_EXFIL_API로 정정, 경로 수정, 모델 선택 가이드 추가 |
| `Docs/ARCHITECTURE.md` | 수정 | Source/Project_EXFIL/ 경로 정정 |
| `Config/DefaultEngine.ini` | 수정 | `r.RayTracing=False`, `CommonGameViewportClient` 설정 추가 |
| `Source/Project_EXFIL/Inventory/InventoryComponent.cpp` | 수정 | BeginPlay 테스트 코드 삭제 |
| `Source/Project_EXFIL/Project_EXFIL.Build.cs` | 수정 | Day 2 모듈 의존성 추가 (`UMG`, `CommonUI`, `CommonInput`, `ModelViewViewModel`, `GameplayTags`) |
| `Project_EXFIL.uproject` | 수정 | CommonUI, ModelViewViewModel 플러그인 활성화 |
| `Source/Project_EXFIL/Core/EXFILCharacter.h/.cpp` | 신규 생성 | BeginPlay에서 ViewModel 생성·초기화, WBP_InventoryPanel 생성 및 Viewport 추가 |
| `Source/Project_EXFIL/UI/InventorySlotViewModel.h/.cpp` | 신규 생성 | 개별 슬롯 ViewModel (FieldNotify Getter/Setter) |
| `Source/Project_EXFIL/UI/InventoryViewModel.h/.cpp` | 신규 생성 | 전체 인벤토리 ViewModel (Model 델리게이트 구독, RefreshAllSlots) |
| `Source/Project_EXFIL/UI/InventoryPanelWidget.h/.cpp` | 신규 생성 | 인벤토리 패널 View (BuildGrid, ForwardMoveRequest, Input Routing) |
| `Source/Project_EXFIL/UI/InventorySlotWidget.h/.cpp` | 신규 생성 | 슬롯 View (DragDrop, 멀티셀 하이라이트, 드래그 비주얼 복사본 생성) |
| `Source/Project_EXFIL/UI/InventoryDragDropOp.h/.cpp` | 신규 생성 | 드래그앤드롭 오퍼레이션 데이터 (`DragOffset` 포함) |
| `Content/UI/WBP_InventorySlot.uasset` | 신규 생성 | SizeBox(64×64) → SlotBorder → Overlay → ItemIcon + StackCountText |
| `Content/UI/WBP_InventoryPanel.uasset` | 신규 생성 | Canvas Panel → Border → SizeBox(676×404) → GridPanel |

### 5. 설계서 대비 변경사항
- **`ForwardMoveRequest()` 추가:** SlotWidget에서 PanelWidget을 통해 ViewModel에 이동 요청을 중계하는 함수. MVVM 단방향 흐름 유지를 위해 추가.
- **`DragOffset` 추가:** 비루트 슬롯 드래그 시 루트 위치 역산을 위해 `UInventoryDragDropOp`에 `FIntPoint DragOffset` 추가.
- **`ParentPanel` 약참조 추가:** SlotWidget에서 PanelWidget 역참조를 위해 `TWeakObjectPtr<UInventoryPanelWidget>` 추가. `BuildGrid()`에서 `SetParentPanel(this)` 세팅.
- **ESC → I 키:** CommonUI BackAction 대신 BP에서 I 키 토글로 인벤토리 개폐 처리.

### 6. 다음 Day 참고사항
- **r.RayTracing=False 유지:** PIE 안정성을 위해 개발 기간 중 레이트레이싱 비활성화 상태 유지
- **WBP_InventorySlot 슬롯 크기:** 64×64 고정. 추후 아이템 크기(2×3 등)에 따른 멀티셀 시각화는 DragPreviewWidget 별도 구현 필요
- **ForwardMoveRequest 패턴:** Day 3 설계서 작성 시 채팅 탭에 이 패턴의 적절성 검토 요청

### 7. Git 커밋
- `feat(Day2): MVVM InventoryViewModel + CommonUI InventoryPanel` — 2026-03-19 푸시 완료
