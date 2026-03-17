# Project EXFIL

> GAS 기반 멀티플레이어 서바이벌 시스템 — 타르코프 스타일 그리드 인벤토리 & 크래프팅

## 프로젝트 개요

Escape from Tarkov에서 영감을 받은 **2D 그리드 인벤토리 시스템**과 **서바이벌 메카닉**을 Unreal Engine 5.6 C++로 구현한 포트폴리오 프로젝트입니다.

- **엔진:** Unreal Engine 5.6
- **언어:** C++ 전용 (블루프린트 미사용)
- **네트워크:** Listen Server 기반 멀티플레이어
- **기간:** 8일 스프린트

## 핵심 시스템

| 시스템 | 설명 |
|--------|------|
| **Grid Inventory** | 다중 크기 아이템 배치, 회전, 드래그앤드롭, 스택 |
| **Crafting** | DataTable 기반 레시피, GameplayAbility로 제작 실행 |
| **Equipment** | 장비 슬롯 관리, GameplayEffect로 스탯 보너스 적용/해제 |
| **Survival Stats** | GAS AttributeSet (Hunger, Thirst, Stamina, Health) |
| **Replication** | 서버 권한 모델, 클라이언트 예측, RPC 기반 동기화 |

## 기술 스택

- **GAS (Gameplay Ability System)** — 어트리뷰트, 이펙트, 어빌리티
- **MVVM 아키텍처** — View(CommonUI Widget) -> ViewModel -> Model(Component) 단방향 흐름
- **CommonUI** — 크로스 플랫폼 UI 프레임워크
- **Data-Driven** — DataTable + CSV 기반 아이템/레시피/장비 데이터 관리
- **이벤트 드리븐** — Tick 배제, Delegate/Callback 기반 UI 갱신

## 아키텍처

```
View Layer (CommonUI Widgets)
    |  Delegate
ViewModel Layer (데이터 변환)
    |  Delegate
Model Layer (UInventoryComponent, UCraftingComponent, UEquipmentComponent)
    |
GAS Layer (USurvivalAttributeSet, GameplayEffects, GameplayAbilities)
    |
Data Layer (UDataTable + CSV)
    |
Network Layer (Replication + Server RPC + Client Prediction)
```

## 폴더 구조

```
Source/Project_EXFIL/
├── Core/                ← 캐릭터, 게임모드, 공통 타입
├── Inventory/           ← 그리드 인벤토리 시스템
├── Crafting/            ← 크래프팅 시스템
├── Equipment/           ← 장비 슬롯 시스템
├── GAS/                 ← AttributeSet, GameplayEffects, Abilities
├── UI/                  ← MVVM ViewModel + CommonUI Widget
└── World/               ← 월드 아이템 액터
```

## 빌드 환경

- Unreal Engine 5.6
- Visual Studio 2022 또는 Rider
- Windows 10/11
