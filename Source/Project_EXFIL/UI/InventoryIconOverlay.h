// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "UI/InventoryViewModel.h"
#include "InventoryIconOverlay.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class UUniformGridPanel;
class UItemContextMenuWidget;
class UInventoryDragDropOp;
class UInventoryPanelWidget;
class UEquipmentViewModel;

UCLASS(Abstract)
class PROJECT_EXFIL_API UInventoryIconOverlay : public UUserWidget
{
    GENERATED_BODY()

public:

    bool RefreshIcons(UInventoryViewModel* InViewModel,
                      UUniformGridPanel* InGridPanel,
                      int32 InGridWidth,
                      int32 InGridHeight,
                      const FInventoryOverlayDeltaViewData& Delta);


    void ClearIcons();


    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UItemContextMenuWidget> ContextMenuWidgetClass;


    void CloseContextMenuIfOpen();


    void SetParentPanel(UInventoryPanelWidget* InPanel);

    
    void SetEquipmentViewModel(UEquipmentViewModel* InViewModel);

    
    bool RotateActiveDragItem();

protected:
    virtual void NativeOnInitialized() override;

    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCanvasPanel> IconCanvas;

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    virtual void NativeOnDragDetected(const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

    virtual bool NativeOnDrop(const FGeometry& InGeometry,
        const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    virtual bool NativeOnDragOver(const FGeometry& InGeometry,
        const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent,
        UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent,
        UDragDropOperation* InOperation) override;

private:
    UPROPERTY()
    TObjectPtr<UInventoryViewModel> CachedViewModel;

    UPROPERTY()
    TObjectPtr<UEquipmentViewModel> CachedEquipmentViewModel;

    UPROPERTY()
    TWeakObjectPtr<UUniformGridPanel> CachedGridPanel;

    int32 CachedGridWidth = 0;
    int32 CachedGridHeight = 0;

    
    UPROPERTY()
    TObjectPtr<UItemContextMenuWidget> ContextMenuWidget;

    
    FGuid PendingDragInstanceID;

    FInventoryDragSourceViewData PendingDragSource;

    
    FVector2D PendingDragClickLocalPos = FVector2D::ZeroVector;

    
    UPROPERTY()
    TWeakObjectPtr<UInventoryPanelWidget> ParentPanel;

    
    UItemContextMenuWidget* GetOrCreateContextMenu();

    UPROPERTY()
    TObjectPtr<UInventoryDragDropOp> ActiveDragOperation;

    FVector2D CachedDragLocalPos = FVector2D::ZeroVector;
    FIntPoint CachedPreviewRootPos = FIntPoint::ZeroValue;
    bool bCachedPreviewCanPlace = false;
    bool bHasCachedPreview = false;

    void UpdateDragPreview(UInventoryDragDropOp* DragOp, const FVector2D& LocalPos);

    
    UPROPERTY()
    TMap<FGuid, UImage*> IconImageCache;

    
    UPROPERTY()
    TMap<FGuid, UTextBlock*> StackTextCache;

    
    FVector2D CachedCellStride = FVector2D::ZeroVector;
};
