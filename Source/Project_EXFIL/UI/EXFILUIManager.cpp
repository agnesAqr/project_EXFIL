// Copyright Project EXFIL. All Rights Reserved.

#include "UI/EXFILUIManager.h"
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SWidget.h"
#include "Core/EXFILLog.h"
#include "Crafting/CraftingComponent.h"
#include "Data/Equipment/EquipmentComponent.h"
#include "GAS/SurvivalViewModel.h"
#include "Inventory/InventoryComponent.h"
#include "UI/CraftingViewModel.h"
#include "UI/EquipmentViewModel.h"
#include "UI/InventoryPanelWidget.h"
#include "UI/InventoryViewModel.h"

namespace
{
const TCHAR* LexBoolForEXFILUIFlow(const bool bValue)
{
	return bValue ? TEXT("true") : TEXT("false");
}

const TCHAR* LexNetModeForEXFILUIFlow(const ENetMode NetMode)
{
	switch (NetMode)
	{
	case NM_Standalone:
		return TEXT("Standalone");
	case NM_DedicatedServer:
		return TEXT("DedicatedServer");
	case NM_ListenServer:
		return TEXT("ListenServer");
	case NM_Client:
		return TEXT("Client");
	default:
		return TEXT("Unknown");
	}
}

FString LexSlateVisibilityForEXFILUIFlow(const ESlateVisibility Visibility)
{
	if (const UEnum* Enum = StaticEnum<ESlateVisibility>())
	{
		return Enum->GetNameStringByValue(static_cast<int64>(Visibility));
	}

	return FString::FromInt(static_cast<int32>(Visibility));
}

FString GetFocusedSlateWidgetForEXFILUIFlow()
{
	if (!FSlateApplication::IsInitialized())
	{
		return TEXT("SlateNotInitialized");
	}

	const TSharedPtr<SWidget> FocusedWidget =
		FSlateApplication::Get().GetKeyboardFocusedWidget();
	return FocusedWidget.IsValid()
		? FocusedWidget->GetTypeAsString()
		: FString(TEXT("None"));
}
}

void UEXFILUIManager::Initialize(APlayerController* InPC,
                                  TSubclassOf<UInventoryPanelWidget> InInventoryClass,
                                  TSubclassOf<UUserWidget> InCrosshairClass)
{
	OwningPC = InPC;
	InventoryPanelClass = InInventoryClass;
	CrosshairWidgetClass = InCrosshairClass;
	if (!InPC) return;

	UE_LOG(LogEXFIL, Log,
		TEXT("[UIFlow][UIManager Initialize:Start] Manager=%s PC=%s InventoryClass=%s CrosshairClass=%s"),
		*GetNameSafe(this),
		*GetNameSafe(InPC),
		*GetNameSafe(InInventoryClass.Get()),
		*GetNameSafe(InCrosshairClass.Get()));

	if (EnsureInventoryPanelReady())
	{
		InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!InInventoryClass)
	{
		UE_LOG(LogEXFIL, Warning,
			TEXT("[UIFlow][UIManager Initialize] Inventory panel class is null. PC=%s"),
			*GetNameSafe(InPC));
	}

	if (EnsureCrosshairWidgetReady())
	{
		CrosshairWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	LogUIState(TEXT("Initialize:AfterCreateWidgets"));
}

void UEXFILUIManager::BindModels(UInventoryComponent* InInventoryComponent,
                                  UEquipmentComponent* InEquipmentComponent,
                                  UCraftingComponent* InCraftingComponent,
                                  UAbilitySystemComponent* InAbilitySystemComponent)
{
	LogUIState(TEXT("BindModels:Before"));

	if (!InInventoryComponent || !InEquipmentComponent ||
		!InCraftingComponent || !InAbilitySystemComponent)
	{
		UE_LOG(LogEXFIL, Warning,
			TEXT("[UIFlow][UIManager BindModels] Missing model. Inventory=%s Equipment=%s Crafting=%s ASC=%s"),
			*GetNameSafe(InInventoryComponent),
			*GetNameSafe(InEquipmentComponent),
			*GetNameSafe(InCraftingComponent),
			*GetNameSafe(InAbilitySystemComponent));
		UnbindModels();
		return;
	}

	if (BoundInventoryComponent.Get() == InInventoryComponent &&
		BoundEquipmentComponent.Get() == InEquipmentComponent &&
		BoundCraftingComponent.Get() == InCraftingComponent &&
		BoundAbilitySystemComponent.Get() == InAbilitySystemComponent)
	{
		LogUIState(TEXT("BindModels:AlreadyBound"));
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

	UE_LOG(LogEXFIL, Log,
		TEXT("[UIFlow][UIManager BindModels] Bound models. Inventory=%s Equipment=%s Crafting=%s ASC=%s InventoryVM=%s EquipmentVM=%s CraftingVM=%s SurvivalVM=%s"),
		*GetNameSafe(InInventoryComponent),
		*GetNameSafe(InEquipmentComponent),
		*GetNameSafe(InCraftingComponent),
		*GetNameSafe(InAbilitySystemComponent),
		*GetNameSafe(InventoryViewModel),
		*GetNameSafe(EquipmentViewModel),
		*GetNameSafe(CraftingViewModel),
		*GetNameSafe(SurvivalViewModel));
	LogUIState(TEXT("BindModels:After"));
}

void UEXFILUIManager::UnbindModels()
{
	LogUIState(TEXT("UnbindModels:Before"));

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

	LogUIState(TEXT("UnbindModels:After"));
}

void UEXFILUIManager::ToggleInventory()
{
	LogUIState(TEXT("ToggleInventory:Before"));

	if (IsInventoryVisible())
	{
		HideInventory();
	}
	else
	{
		ShowInventory();
	}

	LogUIState(TEXT("ToggleInventory:After"));
}

void UEXFILUIManager::ShowInventory()
{
	LogUIState(TEXT("ShowInventory:Before"));

	if (!EnsureInventoryPanelReady())
	{
		UE_LOG(LogEXFIL, Warning,
			TEXT("[UIFlow][UIManager ShowInventory] InventoryPanel is not ready; restoring game input. Manager=%s PC=%s"),
			*GetNameSafe(this),
			*GetNameSafe(OwningPC.Get()));
		SetInputModeGame();
		UpdateCrosshairVisibility();
		return;
	}

	InventoryPanel->SetVisibility(ESlateVisibility::Visible);
	InventoryPanel->NotifyPanelShown();
	InventoryPanel->SetKeyboardFocus();
	SetInputModeUIOnly();
	UpdateCrosshairVisibility();

	LogUIState(TEXT("ShowInventory:After"));
}

void UEXFILUIManager::HideInventory()
{
	LogUIState(TEXT("HideInventory:Before"));

	if (!InventoryPanel)
	{
		UE_LOG(LogEXFIL, Warning,
			TEXT("[UIFlow][UIManager HideInventory] InventoryPanel is null; restoring game input. Manager=%s PC=%s"),
			*GetNameSafe(this),
			*GetNameSafe(OwningPC.Get()));
		SetInputModeGame();
		UpdateCrosshairVisibility();
		return;
	}

	if (InventoryPanel->IsInViewport())
	{
		InventoryPanel->NotifyPanelHidden();
		InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		UE_LOG(LogEXFIL, Warning,
			TEXT("[UIFlow][UIManager HideInventory] InventoryPanel is already out of viewport. Manager=%s Panel=%s"),
			*GetNameSafe(this),
			*GetNameSafe(InventoryPanel));
	}

	SetInputModeGame();
	UpdateCrosshairVisibility();

	LogUIState(TEXT("HideInventory:After"));
}

bool UEXFILUIManager::IsInventoryVisible() const
{
	return InventoryPanel &&
	       InventoryPanel->IsInViewport() &&
	       InventoryPanel->GetVisibility() == ESlateVisibility::Visible;
}

void UEXFILUIManager::SetCrosshairVisible(bool bVisible)
{
	UE_LOG(LogEXFIL, Log,
		TEXT("[UIFlow][UIManager SetCrosshairVisible] Manager=%s bVisible=%s PreviousWants=%s"),
		*GetNameSafe(this),
		LexBoolForEXFILUIFlow(bVisible),
		LexBoolForEXFILUIFlow(bWantsCrosshairVisible));
	bWantsCrosshairVisible = bVisible;
	UpdateCrosshairVisibility();
}

void UEXFILUIManager::UpdateCrosshairVisibility()
{
	if (!EnsureCrosshairWidgetReady()) return;

	const bool bShouldShow = bWantsCrosshairVisible && !IsInventoryVisible();
	CrosshairWidget->SetVisibility(
		bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	UE_LOG(LogEXFIL, Log,
		TEXT("[UIFlow][UIManager UpdateCrosshairVisibility] Manager=%s Wants=%s InventoryVisible=%s Crosshair=%s NewVisibility=%s"),
		*GetNameSafe(this),
		LexBoolForEXFILUIFlow(bWantsCrosshairVisible),
		LexBoolForEXFILUIFlow(IsInventoryVisible()),
		*GetNameSafe(CrosshairWidget),
		*LexSlateVisibilityForEXFILUIFlow(CrosshairWidget->GetVisibility()));
}

void UEXFILUIManager::SetInputModeGame()
{
	APlayerController* PC = OwningPC.Get();
	if (!PC)
	{
		UE_LOG(LogEXFIL, Warning,
			TEXT("[UIFlow][UIManager SetInputModeGame] OwningPC is null. Manager=%s"),
			*GetNameSafe(this));
		return;
	}

	LogUIState(TEXT("SetInputModeGame:Before"));

	FInputModeGameOnly InputMode;
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = false;

	LogUIState(TEXT("SetInputModeGame:After"));
}

void UEXFILUIManager::SetInputModeUIOnly()
{
	APlayerController* PC = OwningPC.Get();
	if (!PC)
	{
		UE_LOG(LogEXFIL, Warning,
			TEXT("[UIFlow][UIManager SetInputModeUIOnly] OwningPC is null. Manager=%s"),
			*GetNameSafe(this));
		return;
	}

	LogUIState(TEXT("SetInputModeUIOnly:Before"));

	if (!InventoryPanel || !InventoryPanel->IsInViewport())
	{
		UE_LOG(LogEXFIL, Warning,
			TEXT("[UIFlow][UIManager SetInputModeUIOnly] InventoryPanel cannot receive focus. Manager=%s PC=%s Panel=%s InViewport=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PC),
			*GetNameSafe(InventoryPanel),
			InventoryPanel ? LexBoolForEXFILUIFlow(InventoryPanel->IsInViewport()) : TEXT("false"));
		SetInputModeGame();
		return;
	}

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(InventoryPanel->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;

	LogUIState(TEXT("SetInputModeUIOnly:After"));
}

bool UEXFILUIManager::EnsureInventoryPanelReady()
{
	APlayerController* PC = OwningPC.Get();
	if (!PC)
	{
		UE_LOG(LogEXFIL, Warning,
			TEXT("[UIFlow][UIManager EnsureInventoryPanelReady] OwningPC is null. Manager=%s"),
			*GetNameSafe(this));
		return false;
	}

	if (!InventoryPanel)
	{
		if (!InventoryPanelClass)
		{
			UE_LOG(LogEXFIL, Warning,
				TEXT("[UIFlow][UIManager EnsureInventoryPanelReady] InventoryPanelClass is null. Manager=%s PC=%s"),
				*GetNameSafe(this),
				*GetNameSafe(PC));
			return false;
		}

		UE_LOG(LogEXFIL, Log,
			TEXT("[UIFlow][UIManager EnsureInventoryPanelReady] Creating InventoryPanel. Manager=%s PC=%s Class=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PC),
			*GetNameSafe(InventoryPanelClass.Get()));
		InventoryPanel = CreateWidget<UInventoryPanelWidget>(PC, InventoryPanelClass);
		ApplyViewModelsToInventoryPanel();
	}

	if (!InventoryPanel)
	{
		UE_LOG(LogEXFIL, Warning,
			TEXT("[UIFlow][UIManager EnsureInventoryPanelReady] CreateWidget failed. Manager=%s PC=%s Class=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PC),
			*GetNameSafe(InventoryPanelClass.Get()));
		return false;
	}

	if (!InventoryPanel->IsInViewport())
	{
		UE_LOG(LogEXFIL, Warning,
			TEXT("[UIFlow][UIManager EnsureInventoryPanelReady] InventoryPanel was out of viewport; adding back. Manager=%s Panel=%s"),
			*GetNameSafe(this),
			*GetNameSafe(InventoryPanel));
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

void UEXFILUIManager::LogUIState(const TCHAR* Context) const
{
	const APlayerController* PC = OwningPC.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const ESlateVisibility PanelVisibility =
		InventoryPanel ? InventoryPanel->GetVisibility() : ESlateVisibility::Collapsed;

	UE_LOG(LogEXFIL, Log,
		TEXT("[UIFlow][UIManager %s] Manager=%s PC=%s NetMode=%s Local=%s Authority=%s Pawn=%s Panel=%s InViewport=%s Visibility=%s IsInventoryVisible=%s PanelKeyboardFocus=%s PanelAnyUserFocus=%s PanelMouseCapture=%s Cursor=%s MoveIgnored=%s LookIgnored=%s SlateFocus=%s BoundInventory=%s BoundEquipment=%s BoundCrafting=%s BoundASC=%s"),
		Context ? Context : TEXT("Unknown"),
		*GetNameSafe(this),
		*GetNameSafe(PC),
		World ? LexNetModeForEXFILUIFlow(World->GetNetMode()) : TEXT("NoWorld"),
		PC ? LexBoolForEXFILUIFlow(PC->IsLocalPlayerController()) : TEXT("false"),
		PC ? LexBoolForEXFILUIFlow(PC->HasAuthority()) : TEXT("false"),
		PC ? *GetNameSafe(PC->GetPawn()) : TEXT("None"),
		*GetNameSafe(InventoryPanel),
		InventoryPanel ? LexBoolForEXFILUIFlow(InventoryPanel->IsInViewport()) : TEXT("false"),
		*LexSlateVisibilityForEXFILUIFlow(PanelVisibility),
		LexBoolForEXFILUIFlow(IsInventoryVisible()),
		InventoryPanel ? LexBoolForEXFILUIFlow(InventoryPanel->HasKeyboardFocus()) : TEXT("false"),
		InventoryPanel ? LexBoolForEXFILUIFlow(InventoryPanel->HasAnyUserFocus()) : TEXT("false"),
		InventoryPanel ? LexBoolForEXFILUIFlow(InventoryPanel->HasMouseCapture()) : TEXT("false"),
		PC ? LexBoolForEXFILUIFlow(PC->bShowMouseCursor) : TEXT("false"),
		PC ? LexBoolForEXFILUIFlow(PC->IsMoveInputIgnored()) : TEXT("false"),
		PC ? LexBoolForEXFILUIFlow(PC->IsLookInputIgnored()) : TEXT("false"),
		*GetFocusedSlateWidgetForEXFILUIFlow(),
		*GetNameSafe(BoundInventoryComponent.Get()),
		*GetNameSafe(BoundEquipmentComponent.Get()),
		*GetNameSafe(BoundCraftingComponent.Get()),
		*GetNameSafe(BoundAbilitySystemComponent.Get()));
}
