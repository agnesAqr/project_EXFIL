// Copyright Project EXFIL. All Rights Reserved.

#include "Core/EXFILPlayerController.h"
#include "CoreMinimal.h"
#include "UI/EXFILUIManager.h"

void AEXFILPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		UIManager = NewObject<UEXFILUIManager>(this);
		UIManager->Initialize(this, InventoryPanelWidgetClass, CrosshairWidgetClass);
	}
}
