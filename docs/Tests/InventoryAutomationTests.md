# Inventory Automation Unit Tests

## TL;DR

- `UInventoryComponent`의 리팩터링 회귀를 빠르게 잡기 위한 UE Automation Unit Test 12개를 추가했다.
- 테스트는 `Project.EXFIL.Inventory.Unit.*` prefix 아래에 등록되며, `RowBitmap`, grid math, cache, inventory ops를 검증한다.
- 성능 측정용 `GridBitmapBenchmark`와 분리하기 위해 코드는 `Source/Project_EXFIL/Tests/Unit/Inventory`에 둔다.

## Test Target

| 항목 | 내용 |
|---|---|
| 테스트 prefix | `Project.EXFIL.Inventory.Unit` |
| 테스트 파일 | `Source/Project_EXFIL/Tests/Unit/Inventory/*.cpp` |
| Fixture | `Source/Project_EXFIL/Tests/Unit/Inventory/InventoryTestFixture.*` |
| 대상 컴포넌트 | `UInventoryComponent` |
| 목적 | Inventory 알고리즘/캐시 정합성 회귀 검증 |
| 실행 환경 | UE Automation Test / Development Editor |

## Test Cases

| 그룹 | 테스트 수 | 테스트 이름 |
|---|---:|---|
| Grid Math | 3 | `PositionToIndexRoundTrip`, `IsValidGridPosition_BoundaryAccept`, `IsValidGridPosition_BoundaryReject` |
| Bitmap | 2 | `EmptyGridAlwaysFree`, `OccupiedSlotsBlockOverlap` |
| Cache | 3 | `AddIncrementsCount`, `RemoveDecrementsCountAndKeepsIndexMap`, `StackOverflowCreatesSecondStack` |
| Ops | 4 | `AddThenRemove_LeavesEmpty`, `AddItemAtBlocked_ReturnsFalse`, `MoveItem_UpdatesPosition`, `AddAutoRotatesWhenOnlyRotatedFits` |

## Execution

```bash
UnrealEditor-Cmd.exe Project_EXFIL.uproject ^
  -ExecCmds="Automation RunTests Project.EXFIL.Inventory.Unit;Quit" ^
  -unattended -nullrhi -log
```

Editor UI:

1. `Window > Test Automation`을 연다.
2. `Project.EXFIL.Inventory.Unit.*` 테스트 12개를 선택한다.
3. `Start Tests`로 실행한다.

## Notes

- 테스트 fixture는 in-memory `UDataTable`과 `UItemDataSubsystem`을 사용해 `BeginPlay()` 의존 없이 `UInventoryComponent`를 초기화한다.
- 테스트 전용 API는 모두 `WITH_DEV_AUTOMATION_TESTS` 가드 안에 있어 Shipping 빌드에는 포함되지 않는다.
- `GridPositionToIndex`는 valid 좌표에서만 round-trip을 검증한다. Out-of-bounds 방어는 `IsValidGridPosition`의 책임으로 테스트한다.
- `StackOverflowCreatesSecondStack`은 `MaxStackCount`를 초과하는 추가 요청이 여러 stack으로 분산되는지 확인한다.

## Latest Result

| 날짜 | 결과 | 비고 |
|---|---|---|
| 2026-05-18 | 12 / 12 Passed | `Project.EXFIL.Inventory.Unit` commandlet 실행, exit code 0 |
