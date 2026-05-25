// Copyright Project EXFIL. All Rights Reserved.

#include "Core/EXFILPlayerController.h"
#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "Core/EXFILCharacter.h"
#include "UI/EXFILUIManager.h"

void AEXFILPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void AEXFILPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (IA_ToggleInventory)
		{
			EnhancedInput->BindAction(
				IA_ToggleInventory,
				ETriggerEvent::Started,
				this,
				&AEXFILPlayerController::OnToggleInventoryPressed);
		}
	}
}

void AEXFILPlayerController::AcknowledgePossession(APawn* pawn)
{
	Super::AcknowledgePossession(pawn);

	if (AEXFILCharacter* EXFILCharacter = Cast<AEXFILCharacter>(pawn))
	{
		EXFILCharacter->InitAbilityActorInfoForClient();
	}

	EnsureUIManager();
	BindCurrentPawnUI();
}

void AEXFILPlayerController::ToggleInventoryUI()
{
	if (UIManager)
	{
		UIManager->ToggleInventory();
	}
}

void AEXFILPlayerController::SetCrosshairVisible(bool bVisible)
{
	if (UIManager)
	{
		UIManager->SetCrosshairVisible(bVisible);
	}
}

bool AEXFILPlayerController::IsInventoryVisible() const
{
	return UIManager && UIManager->IsInventoryVisible();
}

void AEXFILPlayerController::EnsureUIManager()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UIManager)
	{
		return;
	}

	UIManager = NewObject<UEXFILUIManager>(this);
	UIManager->Initialize(this, InventoryPanelWidgetClass, CrosshairWidgetClass);
}

void AEXFILPlayerController::BindCurrentPawnUI()
{
	if (!IsLocalPlayerController() || !UIManager)
	{
		return;
	}

	AEXFILCharacter* EXFILPawn = Cast<AEXFILCharacter>(GetPawn());
	if (!EXFILPawn)
	{
		UIManager->UnbindModels();
		return;
	}

	UIManager->BindModels(
		EXFILPawn->GetInventoryComponent(),
		EXFILPawn->GetEquipmentComponent(),
		EXFILPawn->GetCraftingComponent(),
		EXFILPawn->GetAbilitySystemComponent());
}

void AEXFILPlayerController::OnToggleInventoryPressed()
{
	ToggleInventoryUI();
}
