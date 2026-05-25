// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "EXFILUIManager.generated.h"

class UInventoryPanelWidget;
class UInventoryViewModel;
class UInventoryComponent;
class UEquipmentViewModel;
class UEquipmentComponent;
class UCraftingViewModel;
class UCraftingComponent;
class USurvivalViewModel;
class UAbilitySystemComponent;
class UUserWidget;

UCLASS()
class PROJECT_EXFIL_API UEXFILUIManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(APlayerController* InPC,
	                TSubclassOf<UInventoryPanelWidget> InInventoryClass,
	                TSubclassOf<UUserWidget> InCrosshairClass);

	void BindModels(UInventoryComponent* InInventoryComponent,
	                UEquipmentComponent* InEquipmentComponent,
	                UCraftingComponent* InCraftingComponent,
	                UAbilitySystemComponent* InAbilitySystemComponent);

	void UnbindModels();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowInventory();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideInventory();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	bool IsInventoryVisible() const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetCrosshairVisible(bool bVisible);

	UInventoryPanelWidget* GetInventoryPanel() const { return InventoryPanel; }

private:
	TWeakObjectPtr<APlayerController> OwningPC;

	UPROPERTY()
	TObjectPtr<UInventoryPanelWidget> InventoryPanel;

	UPROPERTY()
	TObjectPtr<UUserWidget> CrosshairWidget;

	UPROPERTY()
	TSubclassOf<UInventoryPanelWidget> InventoryPanelClass;

	UPROPERTY()
	TSubclassOf<UUserWidget> CrosshairWidgetClass;

	TWeakObjectPtr<UInventoryComponent> BoundInventoryComponent;
	TWeakObjectPtr<UEquipmentComponent> BoundEquipmentComponent;
	TWeakObjectPtr<UCraftingComponent> BoundCraftingComponent;
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UInventoryViewModel> InventoryViewModel;

	UPROPERTY()
	TObjectPtr<UEquipmentViewModel> EquipmentViewModel;

	UPROPERTY()
	TObjectPtr<UCraftingViewModel> CraftingViewModel;

	UPROPERTY()
	TObjectPtr<USurvivalViewModel> SurvivalViewModel;

	bool bWantsCrosshairVisible = false;

	bool EnsureInventoryPanelReady();
	void ApplyViewModelsToInventoryPanel();
	bool EnsureCrosshairWidgetReady();

	void UpdateCrosshairVisibility();

	void SetInputModeGame();

	void SetInputModeUIOnly();

	void LogUIState(const TCHAR* Context) const;
};
