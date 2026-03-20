// Copyright Project EXFIL. All Rights Reserved.

#include "EXFILCharacter.h"
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GAS/SurvivalAttributeSet.h"
#include "Inventory/InventoryComponent.h"
#include "UI/InventoryViewModel.h"
#include "UI/InventoryPanelWidget.h"
#include "Blueprint/UserWidget.h"

AEXFILCharacter::AEXFILCharacter()
{
    InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));

    // GAS — AttributeSet은 반드시 생성자에서 CreateDefaultSubobject
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    SurvivalAttributes = CreateDefaultSubobject<USurvivalAttributeSet>(TEXT("SurvivalAttributes"));
}

UAbilitySystemComponent* AEXFILCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AEXFILCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // 서버 권위 초기화 (Day 6 멀티플레이어에서 안전)
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
        UE_LOG(LogTemp, Log, TEXT("EXFILCharacter: ASC InitAbilityActorInfo (PossessedBy)"));
    }
}

void AEXFILCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 단독 세션(Listen Server / Standalone)에서 PossessedBy 없이 BeginPlay만 오는 경우 대비
    if (AbilitySystemComponent && !AbilitySystemComponent->GetOwnerActor())
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }

    if (InventoryComponent)
    {
        InventoryViewModel = NewObject<UInventoryViewModel>(this);
        InventoryViewModel->Initialize(InventoryComponent);

        // ===== Day 3 PIE 테스트 — DataTable 기반 추가 =====
        InventoryComponent->TryAddItemByID(FName("Bandage"),    3);  // 1x1, 스택 3
        InventoryComponent->TryAddItemByID(FName("Pistol"));         // 2x1
        InventoryComponent->TryAddItemByID(FName("BodyArmor"));      // 2x3
        InventoryComponent->TryAddItemByID(FName("ScrapMetal"), 5);  // 1x1, 스택 5
        InventoryComponent->TryAddItemByID(FName("Medkit"));         // 1x2

        InventoryComponent->DebugPrintGrid();
        // ===================================================
    }

    // 로컬 플레이어만 UI 생성
    if (IsLocallyControlled() && InventoryViewModel && InventoryPanelWidgetClass)
    {
        APlayerController* PC = Cast<APlayerController>(GetController());
        if (PC)
        {
            InventoryPanelWidget = CreateWidget<UInventoryPanelWidget>(PC, InventoryPanelWidgetClass);
            if (InventoryPanelWidget)
            {
                InventoryPanelWidget->SetViewModel(InventoryViewModel);
            }
        }
    }
}

