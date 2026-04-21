// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "EXFILUIManager.generated.h"

class UInventoryPanelWidget;
class UInventoryViewModel;
class UCraftingComponent;
class UInventoryComponent;
class USurvivalViewModel;
class UUserWidget;

UCLASS()
class PROJECT_EXFIL_API UEXFILUIManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(APlayerController* InPC,
	                TSubclassOf<UInventoryPanelWidget> InInventoryClass,
	                TSubclassOf<UUserWidget> InCrosshairClass);

	void BindPawnUI(UInventoryViewModel* InInventoryVM,
	                UCraftingComponent* InCraftingComp,
	                UInventoryComponent* InInventoryComp);

	void BindSurvivalStats(USurvivalViewModel* InSurvivalVM);

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

	bool bWantsCrosshairVisible = false;

	void UpdateCrosshairVisibility();

	void SetInputModeGame();

	void SetInputModeGameAndUI();
};
