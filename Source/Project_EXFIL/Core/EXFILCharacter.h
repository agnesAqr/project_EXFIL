// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Project_EXFILCharacter.h"
#include "EXFILCharacter.generated.h"

class UInventoryComponent;
class UInventoryViewModel;
class UInventoryPanelWidget;

/**
 * AEXFILCharacter — EXFIL 프로젝트의 플레이어 캐릭터
 * 인벤토리, 장비, 크래프팅 등 게임 시스템 컴포넌트의 부착 대상
 */
UCLASS()
class PROJECT_EXFIL_API AEXFILCharacter : public AProject_EXFILCharacter
{
	GENERATED_BODY()

public:
	AEXFILCharacter();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
	UInventoryViewModel* GetInventoryViewModel() const { return InventoryViewModel; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInventoryComponent> InventoryComponent;

	/** BP에서 WBP_InventoryPanel 클래스 지정 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI")
	TSubclassOf<UInventoryPanelWidget> InventoryPanelWidgetClass;

	UPROPERTY()
	TObjectPtr<UInventoryViewModel> InventoryViewModel;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInventoryPanelWidget> InventoryPanelWidget;
};
