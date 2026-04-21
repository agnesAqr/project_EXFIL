// Copyright Project EXFIL. All Rights Reserved.

#include "UI/EXFILUIManager.h"
#include "CoreMinimal.h"
#include "UI/InventoryPanelWidget.h"
#include "UI/InventoryViewModel.h"
#include "UI/CraftingPanelWidget.h"
#include "GAS/SurvivalViewModel.h"
#include "Crafting/CraftingComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Core/EXFILLog.h"

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

	UE_LOG(LogEXFIL, Log, TEXT("UIManager initialized — InventoryPanel: %s, Crosshair: %s"),
		InventoryPanel ? TEXT("OK") : TEXT("NULL"),
		CrosshairWidget ? TEXT("OK") : TEXT("NULL"));
}

void UEXFILUIManager::BindPawnUI(UInventoryViewModel* InInventoryVM,
                                  UCraftingComponent* InCraftingComp,
                                  UInventoryComponent* InInventoryComp)
{
	if (!InventoryPanel) return;

	if (InInventoryVM)
	{
		InventoryPanel->SetViewModel(InInventoryVM);
	}

	if (UCraftingPanelWidget* CraftingPanel = InventoryPanel->GetCraftingPanel())
	{
		if (InCraftingComp && InInventoryComp)
		{
			CraftingPanel->SetupCrafting(InCraftingComp, InInventoryComp);
		}
	}
}

void UEXFILUIManager::BindSurvivalStats(USurvivalViewModel* InSurvivalVM)
{
	if (InventoryPanel && InSurvivalVM)
	{
		InventoryPanel->BindStatsToViewModel(InSurvivalVM);
	}
}

// ========== 인벤토리 토글 ==========

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
	SetInputModeGameAndUI();
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

// ========== 크로스헤어 ==========

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

// ========== 내부 ==========

void UEXFILUIManager::SetInputModeGame()
{
	APlayerController* PC = OwningPC.Get();
	if (!PC) return;

	FInputModeGameOnly InputMode;
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = false;
}

void UEXFILUIManager::SetInputModeGameAndUI()
{
	APlayerController* PC = OwningPC.Get();
	if (!PC) return;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	if (InventoryPanel)
	{
		InputMode.SetWidgetToFocus(InventoryPanel->TakeWidget());
	}
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;
}
