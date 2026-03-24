// Copyright Project EXFIL. All Rights Reserved.

#include "EXFILCharacter.h"
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GAS/SurvivalAttributeSet.h"
#include "Inventory/InventoryComponent.h"
#include "Crafting/CraftingComponent.h"
#include "Data/Equipment/EquipmentComponent.h"
#include "UI/InventoryViewModel.h"
#include "UI/InventoryPanelWidget.h"
#include "UI/CraftingPanelWidget.h"
#include "GAS/SurvivalViewModel.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EngineUtils.h"
#include "World/WorldItem.h"

AEXFILCharacter::AEXFILCharacter()
{
    // Day 6: 리플리케이션 활성화 (ACharacter 기본값이 true이지만 명시)
    bReplicates = true;

    InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
    CraftingComponent  = CreateDefaultSubobject<UCraftingComponent>(TEXT("CraftingComponent"));
    EquipmentComponent = CreateDefaultSubobject<UEquipmentComponent>(TEXT("EquipmentComponent"));

    // GAS — AttributeSet은 반드시 생성자에서 CreateDefaultSubobject
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    SurvivalAttributes = CreateDefaultSubobject<USurvivalAttributeSet>(TEXT("SurvivalAttributes"));
}

UAbilitySystemComponent* AEXFILCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AEXFILCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // 서버 권위 초기화
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
        UE_LOG(LogTemp, Log, TEXT("EXFILCharacter: ASC InitAbilityActorInfo (PossessedBy)"));
    }
}

void AEXFILCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 단독 세션(Standalone)에서 PossessedBy 없이 BeginPlay만 오는 경우 대비
    if (AbilitySystemComponent && !AbilitySystemComponent->GetOwnerActor())
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }

    // ===== 서버 전용: 아이템 초기 지급 =====
    if (HasAuthority())
    {
        if (InventoryComponent)
        {
            InventoryComponent->TryAddItemByID(FName("Bandage"),    3);  // 1x1, 스택 3
            InventoryComponent->TryAddItemByID(FName("Pistol"), 2);         // 2x1
            InventoryComponent->TryAddItemByID(FName("BodyArmor"));      // 2x3
            InventoryComponent->TryAddItemByID(FName("Painkillers"), 5);  // 1x1, 스택 5
            InventoryComponent->TryAddItemByID(FName("Medkit"));         // 1x2
 
            InventoryComponent->DebugPrintGrid();
        }
    }

    // ===== 클라이언트 전용: UI 생성 + 델리게이트 바인딩 =====
    UE_LOG(LogTemp, Warning, TEXT("[DIAG] BeginPlay: IsLocallyControlled=%d HasAuthority=%d ASC=%d"),
        IsLocallyControlled() ? 1 : 0,
        HasAuthority() ? 1 : 0,
        AbilitySystemComponent ? 1 : 0);

    if (IsLocallyControlled())
    {
        UE_LOG(LogTemp, Warning, TEXT("[DIAG] IsLocallyControlled 블록 진입 — Authority=%d"),
            HasAuthority() ? 1 : 0);

        // 1. InventoryViewModel
        if (InventoryComponent)
        {
            InventoryViewModel = NewObject<UInventoryViewModel>(this);
            InventoryViewModel->Initialize(InventoryComponent);
        }

        // 2. InventoryPanelWidget 생성 및 연결
        if (InventoryViewModel && InventoryPanelWidgetClass)
        {
            APlayerController* PC = Cast<APlayerController>(GetController());
            if (PC)
            {
                InventoryPanelWidget = CreateWidget<UInventoryPanelWidget>(PC, InventoryPanelWidgetClass);
                if (InventoryPanelWidget)
                {
                    InventoryPanelWidget->SetViewModel(InventoryViewModel);

                    // CraftingPanel이 WBP 안에 있으면 컴포넌트 연결
                    if (UCraftingPanelWidget* CraftingPanel = InventoryPanelWidget->GetCraftingPanel())
                    {
                        CraftingPanel->SetupCrafting(CraftingComponent, InventoryComponent);
                    }
                }
            }
        }

        // 3. SurvivalViewModel — Standalone에서는 BeginPlay에서 ASC가 이미 초기화됨
        InitializeViewModels();
    }
}

// ========== 인터랙션 입력 ==========

void AEXFILCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput =
            Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (IA_Interact)
        {
            EnhancedInput->BindAction(
                IA_Interact, ETriggerEvent::Started, this,
                &AEXFILCharacter::OnInteractPressed);
            UE_LOG(LogTemp, Log, TEXT("SetupInput: IA_Interact 바인딩 완료"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("SetupInput: IA_Interact is NULL — BP Details에서 할당 필요"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SetupInput: EnhancedInputComponent 캐스트 실패"));
    }
}

void AEXFILCharacter::OnInteractPressed()
{
    UE_LOG(LogTemp, Log, TEXT("OnInteractPressed 호출됨"));

    AWorldItem* Target = TraceForWorldItem();
    if (!Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("OnInteractPressed: 트레이스 실패 — 아이템 없음"));
        return;
    }

    Server_RequestPickupItem(Target);
}

AWorldItem* AEXFILCharacter::TraceForWorldItem() const
{
    // 월드의 모든 WorldItem 중 InteractionDistance 이내에서 가장 가까운 것 반환
    AWorldItem* NearestItem = nullptr;
    float NearestDist = InteractionDistance;

    for (TActorIterator<AWorldItem> It(GetWorld()); It; ++It)
    {
        const float Dist = FVector::Dist(GetActorLocation(), It->GetActorLocation());
        if (Dist < NearestDist)
        {
            NearestDist = Dist;
            NearestItem = *It;
        }
    }

    if (NearestItem)
    {
        UE_LOG(LogTemp, Log, TEXT("TraceForWorldItem: '%s' 발견 (%.0fcm)"),
            *NearestItem->GetItemDataID().ToString(), NearestDist);
    }

    return NearestItem;
}

// ========== 픽업 Server RPC ==========

bool AEXFILCharacter::Server_RequestPickupItem_Validate(AWorldItem* TargetItem)
{
    return TargetItem != nullptr;
}

void AEXFILCharacter::Server_RequestPickupItem_Implementation(AWorldItem* TargetItem)
{
    ExecutePickup(TargetItem);
}

void AEXFILCharacter::ExecutePickup(AWorldItem* TargetItem)
{
    if (!HasAuthority()) return;

    // 1. WorldItem 유효성 검증 (다른 플레이어가 이미 주웠을 수 있음)
    if (!IsValid(TargetItem)) return;

    // 2. 거리 검증 (치트 방지, 네트워크 지연 보상으로 여유값 부여)
    constexpr float MaxPickupDistance = 400.f;
    const float Distance = FVector::Dist(GetActorLocation(), TargetItem->GetActorLocation());
    if (Distance > MaxPickupDistance)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExecutePickup: 거리 초과 — %.1fcm > %.1fcm"),
            Distance, MaxPickupDistance);
        return;
    }

    // 3. 인벤토리에 추가 시도
    UInventoryComponent* Inventory = FindComponentByClass<UInventoryComponent>();
    if (!Inventory) return;

    const bool bAdded = Inventory->TryAddItemByID(
        TargetItem->GetItemDataID(), TargetItem->GetStackCount());

    if (!bAdded)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExecutePickup: 인벤토리 가득 참 — '%s' 획득 실패"),
            *TargetItem->GetItemDataID().ToString());
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("ExecutePickup: '%s' x%d 획득"),
        *TargetItem->GetItemDataID().ToString(), TargetItem->GetStackCount());

    // 4. WorldItem 제거 (bReplicates=true Actor의 Destroy는 모든 클라이언트에 전파)
    TargetItem->Destroy();
}

// ========== Dedicated Server: 클라이언트 ASC 초기화 ==========

void AEXFILCharacter::OnRep_PlayerState()
{
    UE_LOG(LogTemp, Warning, TEXT("[STAT-1] OnRep_PlayerState called"));
    Super::OnRep_PlayerState();

    // 클라이언트에서 ASC 초기화
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }

    // ViewModel 초기화 (클라이언트 전용)
    if (IsLocallyControlled())
    {
        InitializeViewModels();
    }
}

void AEXFILCharacter::InitializeViewModels()
{
    if (SurvivalViewModel) return; // 이미 초기화됨
    if (!AbilitySystemComponent) return;

    // ASC에 AttributeSet이 있는지 확인
    const USurvivalAttributeSet* AttrSet = AbilitySystemComponent->GetSet<USurvivalAttributeSet>();
    if (!AttrSet)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeViewModels: AttributeSet still null — 다음 호출에서 재시도"));
        return;
    }

    SurvivalViewModel = NewObject<USurvivalViewModel>(this);
    SurvivalViewModel->InitializeWithASC(AbilitySystemComponent);

    // StatEntry 바인딩
    if (InventoryPanelWidget)
    {
        InventoryPanelWidget->BindStatsToViewModel(SurvivalViewModel);
    }

    UE_LOG(LogTemp, Warning, TEXT("[STAT-2] InitializeViewModels: AttrSet=%s, ViewModel created, BindStats=%s"),
        AttrSet ? TEXT("valid") : TEXT("null"),
        InventoryPanelWidget ? TEXT("bound") : TEXT("no widget"));
}
