// Copyright Project EXFIL. All Rights Reserved.

#include "UI/EXFILUIManager.h"
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Crafting/CraftingComponent.h"
#include "Data/Equipment/EquipmentComponent.h"
#include "GAS/SurvivalViewModel.h"
#include "Inventory/InventoryComponent.h"
#include "UI/CraftingViewModel.h"
#include "UI/EquipmentViewModel.h"
#include "UI/InventoryPanelWidget.h"
#include "UI/InventoryViewModel.h"

void UEXFILUIManager::Initialize(APlayerController* InPC,
                                  TSubclassOf<UInventoryPanelWidget> InInventoryClass,
                                  TSubclassOf<UUserWidget> InCrosshairClass)
{
	OwningPC = InPC;
	InventoryPanelClass = InInventoryClass;
	CrosshairWidgetClass = InCrosshairClass;
	if (!InPC) return;

	if (EnsureInventoryPanelReady())
	{
		InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (EnsureCrosshairWidgetReady())
	{
		CrosshairWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UEXFILUIManager::BindModels(UInventoryComponent* InInventoryComponent,
                                  UEquipmentComponent* InEquipmentComponent,
                                  UCraftingComponent* InCraftingComponent,
                                  UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!InInventoryComponent || !InEquipmentComponent ||
		!InCraftingComponent || !InAbilitySystemComponent)
	{
		UnbindModels();
		return;
	}

	if (BoundInventoryComponent.Get() == InInventoryComponent &&
		BoundEquipmentComponent.Get() == InEquipmentComponent &&
		BoundCraftingComponent.Get() == InCraftingComponent &&
		BoundAbilitySystemComponent.Get() == InAbilitySystemComponent)
	{
		return;
	}

	UnbindModels();

	InventoryViewModel = NewObject<UInventoryViewModel>(this);
	InventoryViewModel->Initialize(InInventoryComponent);

	EquipmentViewModel = NewObject<UEquipmentViewModel>(this);
	EquipmentViewModel->Initialize(InEquipmentComponent);

	CraftingViewModel = NewObject<UCraftingViewModel>(this);
	CraftingViewModel->Initialize(InCraftingComponent, InInventoryComponent);

	SurvivalViewModel = NewObject<USurvivalViewModel>(this);
	SurvivalViewModel->InitializeWithASC(InAbilitySystemComponent);

	BoundInventoryComponent = InInventoryComponent;
	BoundEquipmentComponent = InEquipmentComponent;
	BoundCraftingComponent = InCraftingComponent;
	BoundAbilitySystemComponent = InAbilitySystemComponent;

	ApplyViewModelsToInventoryPanel();
}

void UEXFILUIManager::UnbindModels()
{
	if (InventoryPanel)
	{
		InventoryPanel->SetViewModel(nullptr);
		InventoryPanel->SetEquipmentViewModel(nullptr);
		InventoryPanel->SetCraftingViewModel(nullptr);
		InventoryPanel->BindStatsToViewModel(nullptr);
	}

	InventoryViewModel = nullptr;
	EquipmentViewModel = nullptr;
	CraftingViewModel = nullptr;
	SurvivalViewModel = nullptr;

	BoundInventoryComponent.Reset();
	BoundEquipmentComponent.Reset();
	BoundCraftingComponent.Reset();
	BoundAbilitySystemComponent.Reset();
}

void UEXFILUIManager::ToggleInventory()
{
	if (IsInventoryVisible())
	{
		HideInventory();
	}
	else
	{
		ShowInventory();
	}
}

void UEXFILUIManager::ShowInventory()
{
	if (!EnsureInventoryPanelReady())
	{
		SetInputModeGame();
		UpdateCrosshairVisibility();
		return;
	}

	InventoryPanel->SetVisibility(ESlateVisibility::Visible);
	InventoryPanel->NotifyPanelShown();
	InventoryPanel->SetKeyboardFocus();
	SetInputModeUIOnly();
	UpdateCrosshairVisibility();
}

void UEXFILUIManager::HideInventory()
{
	if (!InventoryPanel)
	{
		SetInputModeGame();
		UpdateCrosshairVisibility();
		return;
	}

	if (InventoryPanel->IsInViewport())
	{
		InventoryPanel->NotifyPanelHidden();
		InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	SetInputModeGame();
	UpdateCrosshairVisibility();
}

bool UEXFILUIManager::IsInventoryVisible() const
{
	return InventoryPanel &&
	       InventoryPanel->IsInViewport() &&
	       InventoryPanel->GetVisibility() == ESlateVisibility::Visible;
}

void UEXFILUIManager::SetCrosshairVisible(bool bVisible)
{
	bWantsCrosshairVisible = bVisible;
	UpdateCrosshairVisibility();
}

void UEXFILUIManager::UpdateCrosshairVisibility()
{
	if (!EnsureCrosshairWidgetReady()) return;

	const bool bShouldShow = bWantsCrosshairVisible && !IsInventoryVisible();
	CrosshairWidget->SetVisibility(
		bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UEXFILUIManager::SetInputModeGame()
{
	APlayerController* PC = OwningPC.Get();
	if (!PC)
	{
		return;
	}

	FInputModeGameOnly InputMode;
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = false;
}

void UEXFILUIManager::SetInputModeUIOnly()
{
	APlayerController* PC = OwningPC.Get();
	if (!PC)
	{
		return;
	}

	if (!InventoryPanel || !InventoryPanel->IsInViewport())
	{
		SetInputModeGame();
		return;
	}

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(InventoryPanel->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;
}

bool UEXFILUIManager::EnsureInventoryPanelReady()
{
	APlayerController* PC = OwningPC.Get();
	if (!PC)
	{
		return false;
	}

	if (!InventoryPanel)
	{
		if (!InventoryPanelClass)
		{
			return false;
		}

		InventoryPanel = CreateWidget<UInventoryPanelWidget>(PC, InventoryPanelClass);
		ApplyViewModelsToInventoryPanel();
	}

	if (!InventoryPanel)
	{
		return false;
	}

	if (!InventoryPanel->IsInViewport())
	{
		InventoryPanel->AddToPlayerScreen();
		ApplyViewModelsToInventoryPanel();
	}

	return InventoryPanel->IsInViewport();
}

void UEXFILUIManager::ApplyViewModelsToInventoryPanel()
{
	if (!InventoryPanel)
	{
		return;
	}

	InventoryPanel->SetViewModel(InventoryViewModel);
	InventoryPanel->SetEquipmentViewModel(EquipmentViewModel);
	InventoryPanel->SetCraftingViewModel(CraftingViewModel);
	InventoryPanel->BindStatsToViewModel(SurvivalViewModel);
}

bool UEXFILUIManager::EnsureCrosshairWidgetReady()
{
	APlayerController* PC = OwningPC.Get();
	if (!PC)
	{
		return false;
	}

	if (!CrosshairWidget)
	{
		if (!CrosshairWidgetClass)
		{
			return false;
		}

		CrosshairWidget = CreateWidget<UUserWidget>(PC, CrosshairWidgetClass);
	}

	if (!CrosshairWidget)
	{
		return false;
	}

	if (!CrosshairWidget->IsInViewport())
	{
		CrosshairWidget->AddToPlayerScreen();
	}

	return CrosshairWidget->IsInViewport();
}
