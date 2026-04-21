// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "InventorySlotWidget.generated.h"

class UInventorySlotViewModel;
class UInventoryPanelWidget;
class UImage;
class UBorder;

UCLASS(Abstract)
class PROJECT_EXFIL_API UInventorySlotWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void SetSlotViewModel(UInventorySlotViewModel* InSlotVM);

    
    void SetParentPanel(UInventoryPanelWidget* InPanel);

protected:
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UBorder> SlotBorder;

public:
    
    void SetHighlight(bool bHighlighted, bool bIsValid = true);

protected:

private:
    UPROPERTY()
    TObjectPtr<UInventorySlotViewModel> SlotVM;

    
    UPROPERTY()
    TWeakObjectPtr<UInventoryPanelWidget> ParentPanel;

    
    void RefreshVisuals();

    
    void OnSlotFieldChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId);
};
