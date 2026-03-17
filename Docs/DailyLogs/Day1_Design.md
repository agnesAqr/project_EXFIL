# Day 1 설계 지시서: Inventory Core — Grid Allocation Algorithm

> 채팅 탭(Opus)이 상단 "설계 지시서" 섹션을 작성 → 수현이 검토 후 Docs/DailyLogs/Day1_Design.md로 저장
> 프로젝트 연결 탭(Opus)이 하단 "완료 보고"를 작성 → 수현이 채팅 탭에 전달

---

**작성자:** 채팅 탭 (Opus 4.6)
**WBS 분류:** Inventory Core
**목표 시간:** 15h
**날짜:** 2026-03-17

## 1. 오늘의 목표

- 타르코프 스타일 2D 그리드 인벤토리의 **핵심 데이터 구조**(구조체)를 C++로 정의한다
- `UInventoryComponent`에 **공간 할당 알고리즘**(다중 크기 아이템 배치, 빈칸 검증, 좌표 계산)을 구현한다
- UE_LOG 기반 **검증 로그**로 그리드 점유/해제가 정확히 동작함을 증명한다
- Day 2(MVVM UI)와 Day 6(Replication)을 고려한 **확장 가능한 설계**를 한다

## 2. 생성할 파일 목록

| 파일 경로 | 용도 | 신규/수정 |
|-----------|------|----------|
| `Source/EXFIL/Inventory/EXFILInventoryTypes.h` | 인벤토리 관련 모든 구조체 정의 | 신규 |
| `Source/EXFIL/Inventory/InventoryComponent.h` | UInventoryComponent 헤더 | 신규 |
| `Source/EXFIL/Inventory/InventoryComponent.cpp` | UInventoryComponent 구현 | 신규 |
| `Source/EXFIL/EXFIL.Build.cs` | 모듈 의존성 추가 (필요 시) | 수정 |
| `Source/EXFIL/Core/EXFILCharacter.h/.cpp` | 테스트용 InventoryComponent 부착 | 수정 |

## 3. 구조체 정의 (EXFILInventoryTypes.h)

### 3-1. FItemSize — 아이템이 그리드에서 차지하는 크기

```cpp
// EXFILInventoryTypes.h
#pragma once
#include "CoreMinimal.h"
#include "EXFILInventoryTypes.generated.h"

USTRUCT(BlueprintType)
struct EXFIL_API FItemSize
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Width = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Height = 1;

    FItemSize() = default;
    FItemSize(int32 InWidth, int32 InHeight)
        : Width(InWidth), Height(InHeight) {}

    // 회전 시 Width/Height 스왑된 사이즈 반환
    FItemSize GetRotated() const { return FItemSize(Height, Width); }

    bool operator==(const FItemSize& Other) const
    {
        return Width == Other.Width && Height == Other.Height;
    }
};
```

### 3-2. FInventoryItemInstance — 인벤토리에 존재하는 아이템 런타임 인스턴스

```cpp
USTRUCT(BlueprintType)
struct EXFIL_API FInventoryItemInstance
{
    GENERATED_BODY()

    // 고유 인스턴스 ID (같은 종류 아이템도 각각 다른 ID)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGuid InstanceID;

    // 아이템 정의 ID (Day 3에서 DataTable RowName으로 연결)
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ItemDataID;

    // 그리드 내 루트 위치 (좌상단 기준)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FIntPoint RootPosition = FIntPoint::ZeroValue;

    // 그리드 점유 크기
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FItemSize ItemSize;

    // 현재 적용 중인 크기 (회전 반영)
    FItemSize GetEffectiveSize() const
    {
        return bIsRotated ? ItemSize.GetRotated() : ItemSize;
    }

    // 회전 여부
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bIsRotated = false;

    // 스택 수량
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 StackCount = 1;

    // 최대 스택 수량 (Day 3에서 DataTable에서 로드)
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MaxStackCount = 1;

    bool IsValid() const { return InstanceID.IsValid(); }

    bool operator==(const FInventoryItemInstance& Other) const
    {
        return InstanceID == Other.InstanceID;
    }
};
```

### 3-3. FInventorySlot — 그리드의 개별 셀

```cpp
USTRUCT(BlueprintType)
struct EXFIL_API FInventorySlot
{
    GENERATED_BODY()

    // 이 슬롯을 점유한 아이템의 InstanceID (Invalid = 빈 슬롯)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGuid OccupyingItemID;

    // 이 슬롯이 아이템의 루트(좌상단) 위치인지
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bIsRootSlot = false;

    bool IsEmpty() const { return !OccupyingItemID.IsValid(); }

    void Clear()
    {
        OccupyingItemID.Invalidate();
        bIsRootSlot = false;
    }

    void Occupy(const FGuid& ItemID, bool bIsRoot)
    {
        OccupyingItemID = ItemID;
        bIsRootSlot = bIsRoot;
    }
};
```

## 4. 클래스 헤더 시그니처 (InventoryComponent.h)

```cpp
// InventoryComponent.h
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EXFILInventoryTypes.h"
#include "InventoryComponent.generated.h"

// 델리게이트 선언 (Day 2 ViewModel 바인딩용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnItemAdded, const FInventoryItemInstance&, AddedItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnItemRemoved, const FGuid&, RemovedItemID);

UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent))
class EXFIL_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInventoryComponent();

    // ========== 설정 ==========
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Config")
    int32 GridWidth = 10;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Config")
    int32 GridHeight = 6;

    // ========== 핵심 API ==========

    // 자동 빈칸 탐색 후 아이템 추가
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool TryAddItem(FName ItemDataID, FItemSize Size,
                    int32 StackCount = 1, int32 MaxStack = 1);

    // 지정 위치에 아이템 배치
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool TryAddItemAt(FName ItemDataID, FItemSize Size,
                      FIntPoint Position, bool bRotated = false,
                      int32 StackCount = 1, int32 MaxStack = 1);

    // 아이템 제거
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItem(const FGuid& InstanceID);

    // 아이템 이동
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool MoveItem(const FGuid& InstanceID, FIntPoint NewPosition,
                  bool bNewRotated = false);

    // ========== 쿼리 API ==========

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    bool CanPlaceItemAt(FIntPoint Position, FItemSize Size) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    bool FindFirstAvailableSlot(FItemSize Size,
                                FIntPoint& OutPosition) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    bool GetItemAt(FIntPoint Position,
                   FInventoryItemInstance& OutItem) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    bool GetItemByID(const FGuid& InstanceID,
                     FInventoryItemInstance& OutItem) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    TArray<FInventoryItemInstance> GetAllItems() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    bool IsEmpty() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
    int32 GetItemCount(FName ItemDataID) const;

    // ========== 유틸리티 ==========

    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    void DebugPrintGrid() const;

    // ========== 델리게이트 ==========
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnInventoryUpdated OnInventoryUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemAdded OnItemAdded;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemRemoved OnItemRemoved;

protected:
    virtual void BeginPlay() override;

private:
    // ========== 내부 데이터 ==========

    // 1D 배열로 표현한 2D 그리드 (Index = Y * GridWidth + X)
    UPROPERTY()
    TArray<FInventorySlot> GridSlots;

    // 인벤토리에 존재하는 모든 아이템 인스턴스
    UPROPERTY()
    TMap<FGuid, FInventoryItemInstance> ItemInstances;

    // ========== 내부 헬퍼 ==========
    bool IsValidGridPosition(FIntPoint Position) const;
    int32 GridPositionToIndex(FIntPoint Position) const;
    FIntPoint IndexToGridPosition(int32 Index) const;

    bool AreSlotsFree(FIntPoint Position, FItemSize Size) const;
    void OccupySlots(FIntPoint Position, FItemSize Size,
                     const FGuid& ItemID);
    void FreeSlots(const FInventoryItemInstance& Item);
    void InitializeGrid();
};
```

## 5. 핵심 함수 구현 가이드

### `InitializeGrid()`
- **목적:** BeginPlay에서 호출. GridSlots 배열을 GridWidth * GridHeight 크기로 초기화
- **핵심 로직:** `GridSlots.SetNum(GridWidth * GridHeight)` → 모든 슬롯 Clear
- **주의:** GridWidth, GridHeight가 0 이하인 경우 방어 처리 필수

### `CanPlaceItemAt(Position, Size)` / `AreSlotsFree(Position, Size)`
- **목적:** 지정 위치에 아이템을 배치할 수 있는지 검증
- **핵심 로직:**
  1. Position + Size가 그리드 범위를 초과하는지 확인 (`Position.X + Size.Width > GridWidth`)
  2. 해당 영역의 모든 슬롯을 순회하며 `IsEmpty()` 검증
  3. 이중 for 루프: `for (Y = Position.Y; Y < Position.Y + Size.Height; Y++)` → `for (X = ...)`
  4. 하나라도 점유되어 있으면 false
- **주의:** **경계 체크를 루프 밖에서 먼저 수행** (루프 내부 매번 체크 비효율)

### `FindFirstAvailableSlot(Size, OutPosition)`
- **목적:** 좌상단부터 순회하여 아이템이 들어갈 첫 번째 빈 공간을 찾음
- **핵심 로직:**
  1. `for (Y = 0; Y <= GridHeight - Size.Height; Y++)` → `for (X = 0; X <= GridWidth - Size.Width; X++)`
  2. 각 위치에서 `AreSlotsFree(FIntPoint(X, Y), Size)` 호출
  3. 성공 시 OutPosition에 좌표 설정 후 true 반환
- **주의:** 루프 범위는 `<=` 아닌 **`GridHeight - Size.Height`까지** (초과 방지)

### `TryAddItem(ItemDataID, Size, StackCount, MaxStack)`
- **목적:** 자동으로 빈 슬롯을 찾아 아이템 추가
- **핵심 로직:**
  1. `FindFirstAvailableSlot()` 호출
  2. 성공 시 `TryAddItemAt()` 위임
  3. 실패 시 false + 로그 경고
- **주의:** 스택 병합 로직은 Day 3에서 추가 (현재는 항상 새 인스턴스 생성)

### `TryAddItemAt(ItemDataID, Size, Position, bRotated, StackCount, MaxStack)`
- **목적:** 지정 좌표에 아이템 인스턴스를 생성하고 그리드에 배치
- **핵심 로직:**
  1. 회전 상태 반영: `EffectiveSize = bRotated ? Size.GetRotated() : Size`
  2. `AreSlotsFree(Position, EffectiveSize)` 검증
  3. `FInventoryItemInstance` 생성 (FGuid::NewGuid()로 InstanceID 발급)
  4. `OccupySlots(Position, EffectiveSize, InstanceID)` 호출
  5. `ItemInstances.Add(InstanceID, NewItem)`
  6. 델리게이트 브로드캐스트: `OnItemAdded`, `OnInventoryUpdated`
- **주의:** **반드시 검증 → 생성 → 점유 → 등록 → 이벤트 순서** 유지

### `OccupySlots(Position, Size, ItemID)`
- **목적:** 그리드 슬롯들을 아이템으로 점유 처리
- **핵심 로직:** 영역 순회하며 각 슬롯에 `Occupy(ItemID, bIsRoot)` 호출. 루트는 Position 자체에 해당하는 슬롯만
- **주의:** `bIsRootSlot`은 `(X == Position.X && Y == Position.Y)` 일 때만 true

### `RemoveItem(InstanceID)`
- **목적:** 아이템 제거 및 슬롯 해제
- **핵심 로직:**
  1. `ItemInstances.Find(InstanceID)` → 없으면 false
  2. `FreeSlots(Item)` → 점유 해제
  3. `ItemInstances.Remove(InstanceID)`
  4. 델리게이트 브로드캐스트

### `FreeSlots(Item)`
- **목적:** 아이템이 차지한 모든 슬롯을 해제
- **핵심 로직:** `GetEffectiveSize()` 기준으로 RootPosition부터 영역 순회 → 각 슬롯 `Clear()`

### `MoveItem(InstanceID, NewPosition, bNewRotated)`
- **목적:** 아이템을 새 위치로 이동 (드래그앤드롭의 백엔드)
- **핵심 로직:**
  1. 기존 아이템 정보 조회
  2. `FreeSlots()` → 기존 위치 해제
  3. 새 사이즈 계산 (회전 반영)
  4. `AreSlotsFree(NewPosition, NewSize)` → 실패 시 **롤백** (기존 위치 재점유)
  5. 성공 시 `OccupySlots()` + 아이템 정보 갱신
- **주의:** **반드시 롤백 로직 구현** — 이동 실패 시 원래 상태 복구 필수

### `DebugPrintGrid()`
- **목적:** 그리드 상태를 텍스트로 시각화 (UE_LOG)
- **핵심 로직:** 각 행을 문자열로 변환. 빈 슬롯 = `.`, 루트 슬롯 = `#`, 점유 슬롯 = `X`
- **출력 예시:**
```
[Inventory Grid 10x6]
. . X X . . . . . .
. . X X . . . . . .
. . . . . # X . . .
. . . . . . . . . .
. . . . . . . . . .
. . . . . . . . . .
```

## 6. 의존성 & 연결 관계

- `UInventoryComponent`는 `UActorComponent`를 상속 — 별도 모듈 의존성 불필요
- `EXFILInventoryTypes.h`는 순수 데이터 구조체만 포함 — 다른 클래스에서 include 가능
- Day 2: `UInventoryViewModel`이 `OnInventoryUpdated` 델리게이트에 바인딩
- Day 6: `GridSlots`와 `ItemInstances`에 `GetLifetimeReplicatedProps` 추가 예정 (현재는 비리플리케이션)
- `EXFILCharacter`에 `UInventoryComponent`를 `CreateDefaultSubobject`로 부착하여 테스트

## 7. 주의사항 & 제약

- **`*.generated.h` include 순서:** 반드시 마지막 include로 배치
- **TMap과 Replication:** `TMap<FGuid, FInventoryItemInstance>`는 기본적으로 리플리케이션이 안 됨. Day 6에서 `TArray` 기반 리플리케이션 래퍼 또는 `FFastArraySerializer`로 교체 예정. 현재는 로컬 전용
- **FGuid 직렬화:** `FGuid`는 `UPROPERTY()`로 선언하면 자동 직렬화됨. 별도 처리 불필요
- **1D↔2D 변환 공식:**
  - `Index = Y * GridWidth + X`
  - `X = Index % GridWidth`, `Y = Index / GridWidth`
  - 이 공식이 모든 좌표 변환의 기반이므로 절대 혼동 금지
- **경계값 테스트:** 그리드 모서리(0,0), 우하단(Width-1, Height-1), 그리드 밖(-1, 넘침) 반드시 테스트
- **스택 병합은 Day 3으로 미룸:** 현재는 같은 종류 아이템도 무조건 새 인스턴스로 생성

## 8. 검증 기준 (Done Criteria)

- [ ] UBT 빌드 성공 (에러 0, 경고 최소화)
- [ ] 1x1 아이템 추가 → 그리드 정상 점유 확인 (DebugPrintGrid)
- [ ] 2x2 아이템 추가 → 4칸 정확히 점유 확인
- [ ] 1x3 아이템 추가 → 3칸 수직 점유 확인
- [ ] 그리드 가득 찼을 때 TryAddItem false 반환 확인
- [ ] 아이템 제거 후 해당 슬롯 비워짐 확인
- [ ] MoveItem으로 이동 후 기존/신규 슬롯 상태 정확성 확인
- [ ] MoveItem 실패(공간 부족) 시 롤백 정상 동작 확인
- [ ] 회전(bIsRotated) 시 Width/Height 스왑 후 배치 정확성 확인
- [ ] 경계값 테스트: 그리드 모서리, 범위 밖 좌표에서 올바른 false 반환
- [ ] UE 에디터 PIE에서 BeginPlay 후 테스트 아이템 자동 추가 → 로그 확인

## 9. BeginPlay 테스트 코드 가이드

```cpp
// 테스트용 (Day 1 완료 후 삭제)
void UInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
    InitializeGrid();

    // Test 1: 1x1 아이템 3개 추가
    TryAddItem(FName("TestItem_1x1"), FItemSize(1, 1));
    TryAddItem(FName("TestItem_1x1"), FItemSize(1, 1));
    TryAddItem(FName("TestItem_1x1"), FItemSize(1, 1));
    DebugPrintGrid();

    // Test 2: 2x2 아이템 추가
    TryAddItem(FName("TestItem_2x2"), FItemSize(2, 2));
    DebugPrintGrid();

    // Test 3: 1x3 세로 아이템 추가
    TryAddItem(FName("TestItem_1x3"), FItemSize(1, 3));
    DebugPrintGrid();

    // Test 4: 특정 위치에 배치
    TryAddItemAt(FName("TestItem_2x1"), FItemSize(2, 1),
                 FIntPoint(5, 0));
    DebugPrintGrid();

    // Test 5: 이동 테스트
    // GetAllItems()로 아이템 하나 가져와서 MoveItem 호출
    DebugPrintGrid();
}
```

---

## ✅ 완료 보고

**작성자:** 프로젝트 연결 탭 (Opus 4.6)
**작성 시점:** 2026-03-18

### 1. 완료 항목

- [x] UBT 빌드 성공 (에러 0)
- [x] 1x1 아이템 추가 → 그리드 정상 점유 확인 (DebugPrintGrid)
- [x] 2x2 아이템 추가 → 4칸 정확히 점유 확인
- [x] 1x3 아이템 추가 → 3칸 수직 점유 확인
- [x] 그리드 가득 찼을 때 TryAddItem false 반환 확인
- [x] 아이템 제거 후 해당 슬롯 비워짐 확인
- [x] MoveItem으로 이동 후 기존/신규 슬롯 상태 정확성 확인
- [x] MoveItem 실패(공간 부족) 시 롤백 정상 동작 확인
- [x] 회전(bIsRotated) 시 Width/Height 스왑 후 배치 정확성 확인
- [x] 경계값 테스트: 그리드 모서리, 범위 밖 좌표에서 올바른 false 반환
- [x] UE 에디터 PIE에서 BeginPlay 후 테스트 아이템 자동 추가 → 로그 확인

### 2. 미완료 / 내일로 이월

- 없음 (Day 1 전체 완료)

### 3. 트러블슈팅

| 문제 | 원인 | 해결 |
|------|------|------|
| Rider MSBuild 빌드 실패 (SDK 찾을 수 없음) | Rider가 VS 18 BuildTools의 MSBuild를 사용, .NET SDK 미인식 | MSBuild를 VS 2022 Community (17.0)으로 변경 |

### 4. 파일 구조 변경사항

| 파일 | 변경 유형 | 변경 내용 요약 |
|------|----------|---------------|
| `Source/Project_EXFIL/Inventory/EXFILInventoryTypes.h` | 신규 생성 | FItemSize, FInventoryItemInstance, FInventorySlot 구조체 정의 |
| `Source/Project_EXFIL/Inventory/InventoryComponent.h` | 신규 생성 | UInventoryComponent 헤더 (API, 쿼리, 델리게이트) |
| `Source/Project_EXFIL/Inventory/InventoryComponent.cpp` | 신규 생성 | 그리드 할당 알고리즘 전체 구현 + BeginPlay 테스트 |
| `Source/Project_EXFIL/Core/EXFILCharacter.h` | 신규 생성 | AProject_EXFILCharacter 상속, UInventoryComponent 부착 |
| `Source/Project_EXFIL/Core/EXFILCharacter.cpp` | 신규 생성 | CreateDefaultSubobject로 컴포넌트 생성 |
| `Source/Project_EXFIL/Project_EXFIL.Build.cs` | 수정 | Core/, Inventory/ include 경로 추가 |

### 5. 다음 Day 참고사항

- 모듈명은 `Project_EXFIL`, API 매크로는 `PROJECT_EXFIL_API` (CLAUDE.md의 `EXFIL_API`와 다름)
- `AEXFILCharacter`는 `AProject_EXFILCharacter`(템플릿 베이스)를 상속
- BeginPlay 테스트 코드는 Day 2 시작 시 삭제 필요
- `TMap<FGuid, FInventoryItemInstance>`는 Day 6에서 리플리케이션 가능 구조로 교체 예정
- 스택 병합 로직은 Day 3에서 추가 예정

### 6. Git 커밋

- `feat(Day1): InventoryComponent grid allocation algorithm` — (커밋 대기)
