// Copyright Project EXFIL. All Rights Reserved.

#include "EXFILCharacter.h"
#include "InventoryComponent.h"
#include "UI/InventoryViewModel.h"
#include "UI/InventoryPanelWidget.h"
#include "Blueprint/UserWidget.h"

AEXFILCharacter::AEXFILCharacter()
{
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
}

void AEXFILCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (InventoryComponent)
	{
		InventoryViewModel = NewObject<UInventoryViewModel>(this);
		InventoryViewModel->Initialize(InventoryComponent);

		// ===== Day 3 PIE 테스트 — DataTable 기반 추가 (완료 후 삭제) =====
		InventoryComponent->TryAddItemByID(FName("Bandage"),    3);  // 1x1, 스택 3
		InventoryComponent->TryAddItemByID(FName("Pistol"));         // 2x1
		InventoryComponent->TryAddItemByID(FName("BodyArmor"));      // 2x3
		InventoryComponent->TryAddItemByID(FName("ScrapMetal"), 5);  // 1x1, 스택 5
		InventoryComponent->TryAddItemByID(FName("Medkit"));         // 1x2

		InventoryComponent->DebugPrintGrid();
		// =================================================================
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
