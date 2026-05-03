// Copyright Project EXFIL. All Rights Reserved.

#include "UI/EXFILUIManager.h"
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Crafting/CraftingComponent.h"
#include "Data/Equipment/EquipmentComponent.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "UI/InventoryPanelWidget.h"
#include "UI/InventoryViewModel.h"
#include "UI/EquipmentViewModel.h"
#include "UI/CraftingViewModel.h"
#include "GAS/SurvivalViewModel.h"

void UEXFILUIManager::Initialize(APlayerController* InPC,
                                  TSubclassOf<UInventoryPanelWidget> InInventoryClass,
                                  TSubclassOf<UUserWidget> InCrosshairClass)
{
	OwningPC = InPC;
	if (!InPC) return;

	if (InInventoryClass)
	{
		InventoryPanel = CreateWidget<UInventoryPanelWidget>(InPC, InInventoryClass);
		if (InventoryPanel)
		{
			InventoryPanel->AddToPlayerScreen();
			InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (InCrosshairClass)
	{
		CrosshairWidget = CreateWidget<UUserWidget>(InPC, InCrosshairClass);
		if (CrosshairWidget)
		{
			CrosshairWidget->AddToPlayerScreen();
			CrosshairWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
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

	if (InventoryPanel)
	{
		InventoryPanel->SetViewModel(InventoryViewModel);
		InventoryPanel->SetEquipmentViewModel(EquipmentViewModel);
		InventoryPanel->SetCraftingViewModel(CraftingViewModel);
		InventoryPanel->BindStatsToViewModel(SurvivalViewModel);
	}
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
	if (!InventoryPanel) return;

	InventoryPanel->SetVisibility(ESlateVisibility::Visible);
	InventoryPanel->NotifyPanelShown();
	InventoryPanel->SetKeyboardFocus();
	SetInputModeUIOnly();
	UpdateCrosshairVisibility();
}

void UEXFILUIManager::HideInventory()
{
	if (!InventoryPanel) return;

	InventoryPanel->NotifyPanelHidden();
	InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);
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
	if (!CrosshairWidget) return;

	const bool bShouldShow = bWantsCrosshairVisible && !IsInventoryVisible();
	CrosshairWidget->SetVisibility(
		bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UEXFILUIManager::SetInputModeGame()
{
	APlayerController* PC = OwningPC.Get();
	if (!PC) return;

	FInputModeGameOnly InputMode;
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = false;
}

void UEXFILUIManager::SetInputModeUIOnly()
{
	APlayerController* PC = OwningPC.Get();
	if (!PC) return;

	FInputModeUIOnly InputMode;
	if (InventoryPanel)
	{
		InputMode.SetWidgetToFocus(InventoryPanel->TakeWidget());
	}
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;
}
