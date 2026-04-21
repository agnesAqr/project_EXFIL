// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "InventoryIconOverlay.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class UInventoryViewModel;
class UInventorySlotViewModel;
class UUniformGridPanel;
class UItemContextMenuWidget;
class UInventoryDragDropOp;
class UInventoryPanelWidget;
class UItemDataSubsystem;

UCLASS(Abstract)
class PROJECT_EXFIL_API UInventoryIconOverlay : public UUserWidget
{
    GENERATED_BODY()

public:
    
    void RefreshIcons(UInventoryViewModel* InViewModel, UUniformGridPanel* InGridPanel,
                      int32 InGridWidth, int32 InGridHeight,
                      const TSet<int32>& DirtyIndices);

    
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UItemContextMenuWidget> ContextMenuWidgetClass;

    
    void CloseContextMenuIfOpen();

    
    void SetParentPanel(UInventoryPanelWidget* InPanel);

    
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
    TWeakObjectPtr<UItemDataSubsystem> CachedItemSub;

    UPROPERTY()
    TWeakObjectPtr<UUniformGridPanel> CachedGridPanel;

    int32 CachedGridWidth = 0;
    int32 CachedGridHeight = 0;

    
    UPROPERTY()
    TObjectPtr<UItemContextMenuWidget> ContextMenuWidget;

    
    FGuid PendingDragInstanceID;

    
    FVector2D PendingDragClickLocalPos = FVector2D::ZeroVector;

    
    UPROPERTY()
    TWeakObjectPtr<UInventoryPanelWidget> ParentPanel;

    
    bool FindItemAtPosition(const FVector2D& LocalPos,
                            FGuid& OutInstanceID, FName& OutItemDataID) const;

    
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
