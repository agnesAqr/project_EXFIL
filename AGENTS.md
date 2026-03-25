# AGENTS.md — Project: EXFIL

> GAS 기반 멀티플레이어 서바이벌 시스템 — 타르코프 스타일 그리드 인벤토리 & 크래프팅

## 프로젝트 개요

- **엔진:** Unreal Engine 5.6, C++ 전용 (블루프린트 금지)
- **프로젝트명:** EXFIL
- **모듈명:** Project_EXFIL (Build.cs: `PROJECT_EXFIL_API`)
- **목적:** Krafton Orion + Nexon 낙원 포트폴리오
- **기간:** 8일 스프린트 (하루 15시간)

## 폴더 구조

```
EXFIL/
├── AGENTS.md                  ← 이 파일 (자동 로드)
├── MEMORY.md                  ← 세션 간 자동 기억
├── Source/Project_EXFIL/
│   ├── Inventory/             ← UInventoryComponent, FInventorySlot
│   ├── GAS/                   ← USurvivalAttributeSet, GameplayEffects
│   ├── Crafting/              ← UCraftingComponent, UGA_Craft
│   ├── Equipment/             ← UEquipmentComponent
│   ├── UI/                    ← MVVM ViewModels + Common UI Widgets
│   └── Core/                  ← Character, GameMode, 공통 유틸
├── Docs/
│   ├── ARCHITECTURE.md        ← 전체 아키텍처 설계서
│   ├── WBS.md                 ← 일정 + 진행 상태
│   └── DailyLogs/
│       └── DayN_Design.md     ← 일일 설계 지시서 + 완료 보고
├── Config/
└── Project_EXFIL.uproject
```

## 작업 흐름 (필수 준수)

1. **설계서 먼저 읽는다:** `Docs/DailyLogs/DayN_Design.md`를 반드시 먼저 읽고 이해
2. **현재 코드 파악:** `Source/` 폴더 구조와 기존 코드를 확인
3. **설계서에 따라 구현:** 설계서에 명시된 클래스, 함수, UPROPERTY 지정자를 그대로 따름
4. **완료 후 보고:** `DayN_Design.md`의 "완료 보고" 섹션을 작성

## 모델 선택 가이드

- **기본:** Sonnet 4.6 (설계서가 상세하므로 구현에 충분)
- **Opus 4.6 전환 시점:**
  - GAS 연동에서 복잡한 상속/보일러플레이트 문제 발생 시
  - Replication/RPC 디버깅에서 원인 파악이 어려울 시
  - 아키텍처 수준의 판단이 필요한 예외 상황 발생 시
- 설계 변경이 필요하면 구현 탭에서 해결하지 말고 채팅 탭(Opus)에 문의

## 코딩 컨벤션

### 네이밍
- UE C++ 접두사: `U`(UObject), `A`(AActor), `F`(구조체/값타입), `E`(enum), `I`(인터페이스)
- 멤버 변수: `bIsRotated`(bool), `GridWidth`(일반), PascalCase
- 함수: PascalCase, 동사 시작 (`TryAddItem`, `FindFirstAvailableSlot`)
- 파일명 = 클래스명 (접두사 제외): `InventoryComponent.h`

### Include 순서 (엄격)
```cpp
#include "InventoryComponent.h"    // 1. 자기 자신 헤더
#include "CoreMinimal.h"           // 2. 엔진 코어
#include "Components/..."          // 3. 엔진 모듈 헤더
#include "EXFILInventoryTypes.h"   // 4. 프로젝트 헤더
#include "InventoryComponent.generated.h"  // 5. 반드시 마지막
```

### UPROPERTY / UFUNCTION
- 데이터 노출: `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "카테고리")`
- 런타임 전용: `UPROPERTY(VisibleAnywhere, BlueprintReadOnly)`
- 리플리케이션 대상: `UPROPERTY(Replicated)` 또는 `UPROPERTY(ReplicatedUsing=OnRep_XXX)`
- API 함수: `UFUNCTION(BlueprintCallable, Category = "카테고리")`
- 읽기 전용 쿼리: `UFUNCTION(BlueprintCallable, BlueprintPure, Category = "카테고리")`
- 서버 RPC: `UFUNCTION(Server, Reliable, WithValidation)`
- 클라이언트 RPC: `UFUNCTION(Client, Reliable)`

## 아키텍처 규칙

### MVVM 패턴 (엄격)
- **View(Widget) → ViewModel → Model(Component)** 단방향 의존
- View에서 Model 직접 참조 **절대 금지**
- Model 변경 → Delegate 브로드캐스트 → ViewModel → View 갱신
- Tick 기반 UI 갱신 **절대 금지** (이벤트 드리븐만 허용)

### 서버 권한 (Day 6~7)
- 인벤토리 조작(추가/제거/이동)은 서버에서 검증
- 클라이언트는 요청만, 서버가 승인 후 실행
- 클라이언트 예측은 UI 반응성 용도로만 사용

### Data-Driven (Day 3~)
- 하드코딩 데이터 **절대 금지**
- 모든 아이템/레시피/장비 스펙은 `UDataTable` + CSV로 관리
- `PrimaryAssetId` 기반 아이템 참조

## 금지 사항

1. **설계서에 없는 아키텍처 변경 금지** — 새 클래스 추가, 상속 구조 변경 등은 반드시 채팅 탭 승인 필요
2. **`*.generated.h` 파일 수정 금지** — UBT가 자동 생성하는 파일
3. **Include 순서 임의 변경 금지** — `generated.h`는 반드시 마지막
4. **UPROPERTY 지정자 임의 변경 금지** — 설계서 명시 지정자 그대로 사용
5. **블루프린트 코드 작성 금지** — C++ 전용 프로젝트
6. **Tick 기반 UI 갱신 금지** — 이벤트 기반만 허용
7. **매직 넘버 금지** — 상수는 constexpr 또는 UPROPERTY로 노출

## 빌드 & 테스트

- **빌드 확인:** 코드 수정 후 반드시 UBT 빌드 성공 확인
- **에러 0 목표:** 경고도 최소화
- **테스트 방법:** BeginPlay 또는 콘솔 커맨드로 UE_LOG 기반 검증
- **PIE 모드:** Standalone 또는 Listen Server 2P

## Day별 핵심 의존성

| Day | 시스템 | 의존 모듈 (Build.cs) |
|-----|--------|---------------------|
| 1   | Inventory Core | Core, CoreUObject, Engine |
| 2   | MVVM + Common UI | UMG, CommonUI, CommonInput, SlateCore, Slate |
| 3   | Data-Driven | Core (DataTable은 Engine 내장) |
| 4   | GAS + Equipment | GameplayAbilities, GameplayTags, GameplayTasks |
| 5   | Crafting | GameplayAbilities |
| 6-7 | Replication | NetCore |
| 8   | Polish | — |

## 커밋 컨벤션

```
feat(DayN): 기능 설명
fix(DayN): 버그 수정 설명
refactor(DayN): 리팩토링 설명
docs(DayN): 문서 수정
```
