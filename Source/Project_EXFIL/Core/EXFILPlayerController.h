// Copyright Project EXFIL. All Rights Reserved.
// EXFILPlayerController.h — EXFIL PlayerController: UIManager 소유, UI 위젯 클래스 설정

#pragma once

#include "CoreMinimal.h"
#include "Project_EXFILPlayerController.h"
#include "EXFILPlayerController.generated.h"

class UEXFILUIManager;
class UInventoryPanelWidget;
class UUserWidget;

UCLASS()
class PROJECT_EXFIL_API AEXFILPlayerController : public AProject_EXFILPlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	UEXFILUIManager* GetUIManager() const { return UIManager; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UInventoryPanelWidget> InventoryPanelWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> CrosshairWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UEXFILUIManager> UIManager;
};
