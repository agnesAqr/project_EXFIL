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

		// ===== PIE 테스트용 더미 아이템 =====
		InventoryComponent->TryAddItem(FName("TestBullet"),   FItemSize(1, 1), 5, 30);
		InventoryComponent->TryAddItem(FName("TestMagazine"), FItemSize(2, 1), 1, 1);
		InventoryComponent->TryAddItem(FName("TestPistol"),   FItemSize(2, 3), 1, 1);
		// ====================================
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
