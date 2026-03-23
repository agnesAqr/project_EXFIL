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
#include "Blueprint/UserWidget.h"

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
    if (IsLocallyControlled())
    {
        if (InventoryComponent)
        {
            InventoryViewModel = NewObject<UInventoryViewModel>(this);
            InventoryViewModel->Initialize(InventoryComponent);
        }

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
    }
}
