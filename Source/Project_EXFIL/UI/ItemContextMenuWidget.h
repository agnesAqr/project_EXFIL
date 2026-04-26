// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "UI/InventoryViewModel.h"
#include "ItemContextMenuWidget.generated.h"

class UButton;
class UEquipmentViewModel;

UCLASS(Abstract)
class PROJECT_EXFIL_API UItemContextMenuWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    void Show(const FItemContextMenuViewData& InViewData,
              UInventoryViewModel* InInventoryViewModel,
              UEquipmentViewModel* InEquipmentViewModel);

    
    void CloseMenu();

    
    void SetMenuPosition(const FVector2D& ScreenPosition);

protected:
    virtual void NativeConstruct() override;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Use;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Equip;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Unequip;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Drop;

private:
    FItemContextMenuViewData CachedViewData;

    UPROPERTY()
    TWeakObjectPtr<UInventoryViewModel> InventoryViewModel;

    UPROPERTY()
    TWeakObjectPtr<UEquipmentViewModel> EquipmentViewModel;

    UFUNCTION()
    void OnUseClicked();

    UFUNCTION()
    void OnEquipClicked();

    UFUNCTION()
    void OnUnequipClicked();

    UFUNCTION()
    void OnDropClicked();
};
