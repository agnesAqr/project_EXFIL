// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Project_EXFILPlayerController.h"
#include "EXFILPlayerController.generated.h"

class UEXFILUIManager;
class UInventoryPanelWidget;
class UInputAction;
class UUserWidget;

UCLASS()
class PROJECT_EXFIL_API AEXFILPlayerController : public AProject_EXFILPlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	UEXFILUIManager* GetUIManager() const { return UIManager; }

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleInventoryUI();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetCrosshairVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	bool IsInventoryVisible() const;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void AcknowledgePossession(APawn* P) override;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UInventoryPanelWidget> InventoryPanelWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> CrosshairWidgetClass;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> IA_ToggleInventory;

private:
	UPROPERTY()
	TObjectPtr<UEXFILUIManager> UIManager;

	void EnsureUIManager();
	void BindCurrentPawnUI();
	void OnToggleInventoryPressed();
};
