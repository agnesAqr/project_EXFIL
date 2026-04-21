// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/Equipment/EquipmentTypes.h"
#include "ItemContextMenuWidget.generated.h"

class UButton;
class UInventoryComponent;
class UEquipmentComponent;

UCLASS(Abstract)
class PROJECT_EXFIL_API UItemContextMenuWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    
    void ShowForInventoryItem(FGuid InItemInstanceID, FName InItemDataID);

    
    void ShowForEquippedItem(EEquipmentSlot InSlot, FName InItemDataID);

    
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
    FGuid CachedItemInstanceID;
    FName CachedItemDataID;
    EEquipmentSlot CachedEquipmentSlot = EEquipmentSlot::None;
    bool bIsEquippedItem = false;
    UFUNCTION()
    void OnUseClicked();

    UFUNCTION()
    void OnEquipClicked();

    UFUNCTION()
    void OnUnequipClicked();

    UFUNCTION()
    void OnDropClicked();

    
    UInventoryComponent* GetInventoryComponent() const;

    
    UEquipmentComponent* GetEquipmentComponent() const;
};
