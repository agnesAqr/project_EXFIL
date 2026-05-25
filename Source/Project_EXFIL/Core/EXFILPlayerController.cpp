// Copyright Project EXFIL. All Rights Reserved.

#include "Core/EXFILPlayerController.h"
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "Core/EXFILCharacter.h"
#include "Core/EXFILLog.h"
#include "Crafting/CraftingComponent.h"
#include "Data/Equipment/EquipmentComponent.h"
#include "Inventory/InventoryComponent.h"
#include "UI/EXFILUIManager.h"
#include "UI/InventoryPanelWidget.h"

namespace
{
const TCHAR* LexBoolForEXFILUIFlow(const bool bValue)
{
	return bValue ? TEXT("true") : TEXT("false");
}
}

void AEXFILPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void AEXFILPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UE_LOG(LogEXFIL, Log,
		TEXT("[UIFlow][PC SetupInputComponent] PC=%s Local=%s InputComponent=%s IA_ToggleInventory=%s Pawn=%s Cursor=%s"),
		*GetNameSafe(this),
		LexBoolForEXFILUIFlow(IsLocalPlayerController()),
		*GetNameSafe(InputComponent),
		*GetNameSafe(IA_ToggleInventory),
		*GetNameSafe(GetPawn()),
		LexBoolForEXFILUIFlow(bShowMouseCursor));

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (IA_ToggleInventory)
		{
			EnhancedInput->BindAction(
				IA_ToggleInventory,
				ETriggerEvent::Started,
				this,
				&AEXFILPlayerController::OnToggleInventoryPressed);

			UE_LOG(LogEXFIL, Log,
				TEXT("[UIFlow][PC SetupInputComponent] Bound IA_ToggleInventory. PC=%s Action=%s"),
				*GetNameSafe(this),
				*GetNameSafe(IA_ToggleInventory));
		}
		else
		{
			UE_LOG(LogEXFIL, Warning,
				TEXT("[UIFlow][PC SetupInputComponent] IA_ToggleInventory is null. PC=%s"),
				*GetNameSafe(this));
		}
	}
	else
	{
		UE_LOG(LogEXFIL, Warning,
			TEXT("[UIFlow][PC SetupInputComponent] InputComponent is not UEnhancedInputComponent. PC=%s InputComponent=%s"),
			*GetNameSafe(this),
			*GetNameSafe(InputComponent));
	}
}

void AEXFILPlayerController::AcknowledgePossession(APawn* pawn)
{
	Super::AcknowledgePossession(pawn);

	UE_LOG(LogEXFIL, Log,
		TEXT("[UIFlow][PC AcknowledgePossession] PC=%s Local=%s PawnArg=%s CurrentPawn=%s Cursor=%s"),
		*GetNameSafe(this),
		LexBoolForEXFILUIFlow(IsLocalPlayerController()),
		*GetNameSafe(pawn),
		*GetNameSafe(GetPawn()),
		LexBoolForEXFILUIFlow(bShowMouseCursor));

	if (AEXFILCharacter* EXFILCharacter = Cast<AEXFILCharacter>(pawn))
	{
		EXFILCharacter->InitAbilityActorInfoForClient();
	}

	EnsureUIManager();
	BindCurrentPawnUI();
}

void AEXFILPlayerController::ToggleInventoryUI()
{
	UE_LOG(LogEXFIL, Log,
		TEXT("[UIFlow][PC ToggleInventoryUI:Before] PC=%s Local=%s UIManager=%s Visible=%s Pawn=%s Cursor=%s MoveIgnored=%s LookIgnored=%s"),
		*GetNameSafe(this),
		LexBoolForEXFILUIFlow(IsLocalPlayerController()),
		*GetNameSafe(UIManager),
		LexBoolForEXFILUIFlow(IsInventoryVisible()),
		*GetNameSafe(GetPawn()),
		LexBoolForEXFILUIFlow(bShowMouseCursor),
		LexBoolForEXFILUIFlow(IsMoveInputIgnored()),
		LexBoolForEXFILUIFlow(IsLookInputIgnored()));

	if (UIManager)
	{
		UIManager->ToggleInventory();
	}
	else
	{
		UE_LOG(LogEXFIL, Warning,
			TEXT("[UIFlow][PC ToggleInventoryUI] UIManager is null. PC=%s Local=%s"),
			*GetNameSafe(this),
			LexBoolForEXFILUIFlow(IsLocalPlayerController()));
	}

	UE_LOG(LogEXFIL, Log,
		TEXT("[UIFlow][PC ToggleInventoryUI:After] PC=%s UIManager=%s Visible=%s Cursor=%s MoveIgnored=%s LookIgnored=%s"),
		*GetNameSafe(this),
		*GetNameSafe(UIManager),
		LexBoolForEXFILUIFlow(IsInventoryVisible()),
		LexBoolForEXFILUIFlow(bShowMouseCursor),
		LexBoolForEXFILUIFlow(IsMoveInputIgnored()),
		LexBoolForEXFILUIFlow(IsLookInputIgnored()));
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
		UE_LOG(LogEXFIL, Log,
			TEXT("[UIFlow][PC EnsureUIManager] Skip non-local controller. PC=%s"),
			*GetNameSafe(this));
		return;
	}

	if (UIManager)
	{
		UE_LOG(LogEXFIL, Log,
			TEXT("[UIFlow][PC EnsureUIManager] Reusing existing UIManager. PC=%s UIManager=%s"),
			*GetNameSafe(this),
			*GetNameSafe(UIManager));
		return;
	}

	UE_LOG(LogEXFIL, Log,
		TEXT("[UIFlow][PC EnsureUIManager] Creating UIManager. PC=%s InventoryPanelClass=%s CrosshairClass=%s"),
		*GetNameSafe(this),
		*GetNameSafe(InventoryPanelWidgetClass.Get()),
		*GetNameSafe(CrosshairWidgetClass.Get()));

	UIManager = NewObject<UEXFILUIManager>(this);
	UIManager->Initialize(this, InventoryPanelWidgetClass, CrosshairWidgetClass);
}

void AEXFILPlayerController::BindCurrentPawnUI()
{
	if (!IsLocalPlayerController() || !UIManager)
	{
		UE_LOG(LogEXFIL, Log,
			TEXT("[UIFlow][PC BindCurrentPawnUI] Skip. PC=%s Local=%s UIManager=%s"),
			*GetNameSafe(this),
			LexBoolForEXFILUIFlow(IsLocalPlayerController()),
			*GetNameSafe(UIManager));
		return;
	}

	AEXFILCharacter* EXFILPawn = Cast<AEXFILCharacter>(GetPawn());
	if (!EXFILPawn)
	{
		UE_LOG(LogEXFIL, Warning,
			TEXT("[UIFlow][PC BindCurrentPawnUI] No EXFIL pawn. PC=%s Pawn=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetPawn()));
		UIManager->UnbindModels();
		return;
	}

	UE_LOG(LogEXFIL, Log,
		TEXT("[UIFlow][PC BindCurrentPawnUI] Binding pawn UI. PC=%s Pawn=%s Inventory=%s Equipment=%s Crafting=%s ASC=%s"),
		*GetNameSafe(this),
		*GetNameSafe(EXFILPawn),
		*GetNameSafe(EXFILPawn->GetInventoryComponent()),
		*GetNameSafe(EXFILPawn->GetEquipmentComponent()),
		*GetNameSafe(EXFILPawn->GetCraftingComponent()),
		*GetNameSafe(EXFILPawn->GetAbilitySystemComponent()));

	UIManager->BindModels(
		EXFILPawn->GetInventoryComponent(),
		EXFILPawn->GetEquipmentComponent(),
		EXFILPawn->GetCraftingComponent(),
		EXFILPawn->GetAbilitySystemComponent());
}

void AEXFILPlayerController::OnToggleInventoryPressed()
{
	UE_LOG(LogEXFIL, Log,
		TEXT("[UIFlow][PC OnToggleInventoryPressed] PC=%s Local=%s Visible=%s Pawn=%s Cursor=%s"),
		*GetNameSafe(this),
		LexBoolForEXFILUIFlow(IsLocalPlayerController()),
		LexBoolForEXFILUIFlow(IsInventoryVisible()),
		*GetNameSafe(GetPawn()),
		LexBoolForEXFILUIFlow(bShowMouseCursor));

	ToggleInventoryUI();
}
