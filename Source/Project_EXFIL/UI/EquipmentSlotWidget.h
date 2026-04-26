// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "UI/EquipmentViewModel.h"
#include "EquipmentSlotWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UInventoryDragDropOp;
class UItemContextMenuWidget;

UCLASS(Abstract)
class PROJECT_EXFIL_API UEquipmentSlotWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
    EEquipmentSlot SlotType = EEquipmentSlot::None;

    
    UFUNCTION(BlueprintCallable, Category = "Equipment|UI")
    void SetViewModel(UEquipmentViewModel* InViewModel);

    
    UFUNCTION(BlueprintCallable, Category = "Equipment|UI")
    void RefreshSlot(const FEquipmentSlotViewData& SlotData);

    
    UFUNCTION(BlueprintCallable, Category = "Equipment|UI")
    void SetDragHighlight(bool bVisible, bool bIsValid);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                           const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry,
                                      const FPointerEvent& InMouseEvent,
                                      UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry,
                               const FDragDropEvent& InDragDropEvent,
                               UDragDropOperation* InOperation) override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> Border_Slot;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_ItemIcon;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TextBlock_SlotLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TextBlock_ItemName;

    
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UItemContextMenuWidget> ContextMenuWidgetClass;

private:
    
    FEquipmentSlotViewData CachedSlotData;

    
    UPROPERTY()
    TObjectPtr<UItemContextMenuWidget> ContextMenuWidget;

    
    UPROPERTY()
    TWeakObjectPtr<UEquipmentViewModel> BoundViewModel;

    FDelegateHandle EquipmentSlotChangedHandle;

    void HandleEquipmentSlotChanged(
        EEquipmentSlot InSlot, const FEquipmentSlotViewData& InViewData);

    
    void ApplyEmptyStyle();
    void ApplyEquippedStyle();
    void ApplyDragHoverStyle(bool bIsValid);
};
