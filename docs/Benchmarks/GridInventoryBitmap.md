# Grid Inventory Bitmap Benchmark

## TL;DR

- `FindFirstAvailableSlot`의 기존 `GridSlots` 셀 스캔 방식과 현재 `RowBitmap` 방식을 UE Automation Test로 비교했다.
- 대표 케이스인 `10x20 Grid / Fragmented50 / 2x3 Item`에서 `27.2453ms -> 5.7104ms`, `4.77x` 개선을 확인했다.
- `10x20` 빈 슬롯 탐색 30개 케이스 평균 개선폭은 `2.91x`, 최대 개선폭은 `5.57x`였다.

## Benchmark Target

| 항목 | 내용 |
|---|---|
| 테스트 이름 | `Project.EXFIL.Inventory.GridBitmapBenchmark` |
| 테스트 파일 | `Source/Project_EXFIL/Tests/Benchmarks/Inventory/InventoryBitmapBenchmark.cpp` |
| 비교 대상 | `GridSlots` 기반 셀 스캔 vs `RowBitmap` 기반 비트마스크 검사 |
| 주요 함수 | `FindFirstAvailableSlot`, `AreSlotsFree` |
| 측정 환경 | UE Automation Test / Development Editor |
| 최근 검증 | 2026-05-18 |

## Methodology

- UE 자동화 테스트로 벤치마크를 실행했다.
- 측정 전 Before / After 결과가 동일한지 먼저 검증했다.
- 고정 시드(`3579` 시작)를 사용해 점유 패턴을 재현 가능하게 구성했다.
- 각 케이스는 워밍업 후 3회 실행하고, 최단 시간을 결과로 사용했다.
- 측정에는 `FPlatformTime::Seconds()`를 사용했다.
- 반복 실행이 최적화로 제거되지 않도록 accumulator sink를 사용했다.

### Warm-up Rationale

워밍업은 본 측정 전에 같은 작업을 미리 실행해 첫 실행에서 발생하는 노이즈를 줄이기 위한 절차다.

- 첫 호출 시점에는 CPU cache, branch predictor, 메모리 접근 패턴이 아직 안정되지 않아 시간이 튈 수 있다.
- 벤치마크 대상 함수가 매우 짧기 때문에, 이런 초기 실행 비용이 결과에 섞이면 Before / After 비교가 왜곡될 수 있다.
- 따라서 본 측정 시간에는 포함하지 않는 사전 반복을 수행한 뒤, 이후 3회 측정 중 최단 시간을 사용했다.

## Scenario Matrix

| 축 | 값 |
|---|---|
| 그리드 크기 | `10x20`, `10x10`, `16x20` |
| 점유 패턴 | `Empty`, `Sparse25`, `Fragmented50`, `Dense75`, `NearFull`, `WorstNoFit` |
| 아이템 크기 | `1x1`, `2x1`, `2x3`, `4x2`, `1x4` |
| 측정 함수 | `FindFirstAvailableSlot`, `AreSlotsFree` |
| 총 측정값 | `180`개 |

## Representative Result

포트폴리오 메인 수치로 사용하기 좋은 대표 케이스다.

| 항목 | 값 |
|---|---:|
| 함수 | `FindFirstAvailableSlot` |
| 시나리오 | `10x20 Grid / Fragmented50 / 2x3 Item` |
| 반복 횟수 | `100,000` |
| Before | `27.2453ms` |
| After | `5.7104ms` |
| Speedup | `4.77x` |
| 시간 감소율 | `79.0%` |

포트폴리오 표기용:

```text
10x20 Grid / Fragmented50 / 2x3 Item
FindFirstAvailableSlot 100,000 iterations
27.25ms -> 5.71ms
4.77x faster / -79.0% search time
```

## 10x20 FindFirstAvailableSlot Highlights

| Pattern | Item | Iter | Before | After | Speedup | 감소율 |
|---|---:|---:|---:|---:|---:|---:|
| `WorstNoFit` | `4x2` | `10,000` | `3.3679ms` | `0.6042ms` | `5.57x` | `82.1%` |
| `Fragmented50` | `4x2` | `100,000` | `31.8237ms` | `6.4819ms` | `4.91x` | `79.6%` |
| `WorstNoFit` | `2x1` | `10,000` | `2.9420ms` | `0.6158ms` | `4.78x` | `79.1%` |
| `Fragmented50` | `2x3` | `100,000` | `27.2453ms` | `5.7104ms` | `4.77x` | `79.0%` |
| `NearFull` | `1x1` | `25,000` | `7.6219ms` | `1.6594ms` | `4.59x` | `78.2%` |

## Summary

| 범위 | Count | Avg Speedup | Min | Max |
|---|---:|---:|---:|---:|
| `10x20 / FindFirstAvailableSlot` | `30` | `2.91x` | `0.97x` | `5.57x` |
| `10x20 / AreSlotsFree` | `30` | `1.37x` | `1.11x` | `2.97x` |
| `10x10 / FindFirstAvailableSlot` | `30` | `2.76x` | `0.97x` | `5.74x` |
| `10x10 / AreSlotsFree` | `30` | `1.36x` | `1.13x` | `2.83x` |
| `16x20 / FindFirstAvailableSlot` | `30` | `3.49x` | `0.97x` | `7.63x` |
| `16x20 / AreSlotsFree` | `30` | `1.36x` | `1.12x` | `2.82x` |

## Interpretation

- `AreSlotsFree` 단독 검사는 한 좌표의 footprint만 확인하므로 개선폭이 상대적으로 작다.
- 실제 병목에 가까운 경로는 그리드 전체 후보 위치를 순회하는 `FindFirstAvailableSlot`이다.
- `1x1` 아이템이나 매우 초기에 배치 가능한 케이스에서는 bitmap 방식의 이점이 작거나 약간 느릴 수 있다.
- 단편화되었거나 배치 실패 후보를 많이 훑는 케이스에서는 `RowBitmap` 방식이 후보별 셀 순회를 줄여 뚜렷한 개선을 보인다.

## Insight

- 핵심 개선 지점은 단일 좌표 검사가 아니라, 배치 가능한 첫 위치를 찾기 위해 여러 후보 좌표를 순회하는 search path다.
- 기존 방식은 후보 좌표마다 아이템 footprint 내부 셀을 다시 순회하므로, 단편화된 그리드에서 실패 후보가 많아질수록 비용이 커진다.
- `RowBitmap` 방식은 각 row의 점유 상태를 bitmask로 압축하고, `AND` 연산으로 충돌 여부를 확인해 후보별 셀 접근을 줄인다.
- 개선폭은 grid가 크거나, item footprint가 넓거나, 앞쪽 후보가 자주 실패하는 시나리오에서 더 크게 나타난다.
- 반대로 `Empty / 1x1`처럼 첫 후보에서 바로 성공하는 케이스는 Before도 이미 충분히 짧아서 개선폭이 작다.
- 포트폴리오에서는 전체 평균보다 `FindFirstAvailableSlot`의 대표 케이스와 주요 개선 케이스를 중심으로 설명하는 것이 적절하다.

## Portfolio Copy

```text
고정 시드 기반의 다양한 그리드 점유 패턴에서 빈 슬롯 탐색 알고리즘을 비교했다.
대표 케이스인 10x20 Fragmented50 / 2x3 아이템 탐색에서 27.25ms -> 5.71ms로 감소해 4.77x 개선됐다.
10x20 placement-search 30개 케이스 평균 개선폭은 2.91x였다.
```
