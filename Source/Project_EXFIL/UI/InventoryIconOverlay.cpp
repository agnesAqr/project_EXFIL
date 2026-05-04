// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventoryIconOverlay.h"
#include "CoreMinimal.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/Texture2D.h"
#include "UI/EquipmentViewModel.h"
#include "UI/InventoryDragDropOp.h"
#include "UI/InventoryPanelWidget.h"
#include "UI/ItemContextMenuWidget.h"

namespace
{
    FIntPoint LocalPosToGridCell(
        const FVector2D& LocalPos, const FVector2D& GridLocalSize,
        int32 GridW, int32 GridH)
    {
        const FVector2D CellStride(GridLocalSize.X / GridW, GridLocalSize.Y / GridH);
        return FIntPoint(
            FMath::FloorToInt(LocalPos.X / CellStride.X),
            FMath::FloorToInt(LocalPos.Y / CellStride.Y));
    }
}

void UInventoryIconOverlay::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::Visible);
}

void UInventoryIconOverlay::CloseContextMenuIfOpen()
{
    if (ContextMenuWidget &&
        ContextMenuWidget->GetVisibility() == ESlateVisibility::Visible)
    {
        ContextMenuWidget->CloseMenu();
    }
}

void UInventoryIconOverlay::ClearIcons()
{
    if (IconCanvas)
    {
        IconCanvas->ClearChildren();
    }

    CachedViewModel = nullptr;
    CachedEquipmentViewModel = nullptr;
    CachedGridPanel.Reset();
    CachedGridWidth = 0;
    CachedGridHeight = 0;
    PendingDragInstanceID.Invalidate();
    PendingDragSource = FInventoryDragSourceViewData();
    ActiveDragOperation = nullptr;
    bHasCachedPreview = false;
    IconImageCache.Empty();
    StackTextCache.Empty();
    CachedCellStride = FVector2D::ZeroVector;
    CloseContextMenuIfOpen();
}

bool UInventoryIconOverlay::RefreshIcons(
    UInventoryViewModel* InViewModel,
    UUniformGridPanel* InGridPanel,
    int32 InGridWidth,
    int32 InGridHeight,
    const FInventoryOverlayDeltaViewData& Delta)
{
    CachedViewModel  = InViewModel;
    CachedGridPanel  = InGridPanel;
    CachedGridWidth  = InGridWidth;
    CachedGridHeight = InGridHeight;

    if (!IconCanvas || !InViewModel || !InGridPanel)
    {
        return false;
    }

    UWidget* FirstSlot = InGridPanel->GetChildAt(0);
    if (!FirstSlot)
    {
        return false;
    }

    const FVector2D SlotSize = FirstSlot->GetCachedGeometry().GetLocalSize();
    const FVector2D GridLocalSize = InGridPanel->GetCachedGeometry().GetLocalSize();
    const FVector2D CellStride(GridLocalSize.X / InGridWidth, GridLocalSize.Y / InGridHeight);

    if (GridLocalSize.IsNearlyZero() || CellStride.X < 1.f || CellStride.Y < 1.f)
    {
        return false;
    }

    const bool bStrideChanged = !CachedCellStride.Equals(CellStride, 0.5f);
    if (Delta.bFullRefresh || bStrideChanged)
    {
        CachedCellStride = CellStride;
        IconCanvas->ClearChildren();
        IconImageCache.Empty();
        StackTextCache.Empty();
    }

    for (const FGuid& RemovedID : Delta.RemovedItemInstanceIDs)
    {
        if (UImage** Img = IconImageCache.Find(RemovedID))
        {
            if (*Img)
            {
                (*Img)->RemoveFromParent();
            }
        }
        if (UTextBlock** Txt = StackTextCache.Find(RemovedID))
        {
            if (*Txt)
            {
                (*Txt)->RemoveFromParent();
            }
        }
        IconImageCache.Remove(RemovedID);
        StackTextCache.Remove(RemovedID);
    }

    for (const FInventoryIconViewData& ItemViewData : Delta.UpsertItems)
    {
        if (!ItemViewData.InstanceID.IsValid())
        {
            continue;
        }

        const FVector2D IconPos(
            ItemViewData.RootGridPosition.X * CellStride.X,
            ItemViewData.RootGridPosition.Y * CellStride.Y);
        const FVector2D IconSize(
            ItemViewData.GridSpan.Width * CellStride.X,
            ItemViewData.GridSpan.Height * CellStride.Y);

        UImage** ExistingImage = IconImageCache.Find(ItemViewData.InstanceID);
        UTexture2D* IconTex = ItemViewData.Icon.Get();

        if (IconTex)
        {
            const FIntPoint ImportedSize = IconTex->GetImportedSize();
            const FVector2D TexSize(
                ImportedSize.X > 0 ? static_cast<float>(ImportedSize.X) : static_cast<float>(IconTex->GetSizeX()),
                ImportedSize.Y > 0 ? static_cast<float>(ImportedSize.Y) : static_cast<float>(IconTex->GetSizeY()));
            const float Scale = ItemViewData.bRotated
                ? FMath::Min(IconSize.X / TexSize.Y, IconSize.Y / TexSize.X)
                : FMath::Min(IconSize.X / TexSize.X, IconSize.Y / TexSize.Y);
            const FVector2D FinalSize = TexSize * Scale;
            const FVector2D Offset = (IconSize - FinalSize) * 0.5f;

            UImage* IconImage = ExistingImage ? *ExistingImage : nullptr;
            if (!IconImage)
            {
                IconImage = NewObject<UImage>(this);
                IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
                IconImageCache.Add(ItemViewData.InstanceID, IconImage);

                UCanvasPanelSlot* NewCanvasSlot = IconCanvas->AddChildToCanvas(IconImage);
                if (NewCanvasSlot)
                {
                    NewCanvasSlot->SetAutoSize(false);
                }
            }

            FSlateBrush Brush;
            Brush.SetResourceObject(IconTex);
            Brush.DrawAs = ESlateBrushDrawType::Image;
            IconImage->SetBrush(Brush);
            IconImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
            IconImage->SetRenderTransformAngle(ItemViewData.bRotated ? 90.f : 0.f);

            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(IconImage->Slot))
            {
                CanvasSlot->SetPosition(IconPos + Offset);
                CanvasSlot->SetSize(FinalSize);
            }
        }
        else if (ExistingImage && *ExistingImage)
        {
            (*ExistingImage)->RemoveFromParent();
            IconImageCache.Remove(ItemViewData.InstanceID);
        }

        UTextBlock** ExistingText = StackTextCache.Find(ItemViewData.InstanceID);   
        if (ItemViewData.StackCount > 1)
        {
            UTextBlock* StackText = ExistingText ? *ExistingText : nullptr;
            if (!StackText)
            {
                StackText = NewObject<UTextBlock>(this);
                StackText->SetVisibility(ESlateVisibility::HitTestInvisible);

                FSlateFontInfo FontInfo = StackText->GetFont();     
                FontInfo.Size = 12;
                StackText->SetFont(FontInfo);
                StackText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.9f)));
                StackTextCache.Add(ItemViewData.InstanceID, StackText);

                if (UCanvasPanelSlot* TextSlot = IconCanvas->AddChildToCanvas(StackText))
                {
                    TextSlot->SetAutoSize(true);
                }
            }

            StackText->SetText(FText::AsNumber(ItemViewData.StackCount));
            if (UCanvasPanelSlot* TextSlot = Cast<UCanvasPanelSlot>(StackText->Slot))
            {
                TextSlot->SetPosition(FVector2D(
                    IconPos.X + IconSize.X - SlotSize.X * 0.3f,
                    IconPos.Y + IconSize.Y - SlotSize.Y * 0.3f));
            }
        }
        else if (ExistingText && *ExistingText)
        {
            (*ExistingText)->RemoveFromParent();
            StackTextCache.Remove(ItemViewData.InstanceID);
        }
    }

    if (Delta.bFullRefresh ||
        Delta.UpsertItems.Num() > 0 ||
        Delta.RemovedItemInstanceIDs.Num() > 0)
    {
        Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
    }

    return true;
}

FReply UInventoryIconOverlay::NativeOnMouseButtonDown(
    const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    (void)InGeometry;

    if (!CachedViewModel || !CachedGridPanel.IsValid())
    {
        return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    }

    const FVector2D GridLocalPos = CachedGridPanel->GetCachedGeometry().AbsoluteToLocal(
        InMouseEvent.GetScreenSpacePosition());
    const FVector2D GridLocalSize = CachedGridPanel->GetCachedGeometry().GetLocalSize();
    if (GridLocalSize.IsNearlyZero() || CachedGridWidth <= 0 || CachedGridHeight <= 0)
    {
        return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    }

    const FIntPoint Cell = LocalPosToGridCell(
        GridLocalPos, GridLocalSize, CachedGridWidth, CachedGridHeight);

    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        if (ContextMenuWidget &&
            ContextMenuWidget->GetVisibility() == ESlateVisibility::Visible)
        {
            ContextMenuWidget->CloseMenu();
        }

        FItemContextMenuViewData MenuViewData;
        if (CachedViewModel->TryGetItemContextMenuViewDataAtCell(Cell, MenuViewData))
        {
            UItemContextMenuWidget* Menu = GetOrCreateContextMenu();
            if (Menu)
            {
                Menu->Show(MenuViewData, CachedViewModel, CachedEquipmentViewModel);
                Menu->SetMenuPosition(InMouseEvent.GetScreenSpacePosition());
            }
        }

        return FReply::Handled();
    }

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        FInventoryDragSourceViewData DragSource;
        if (CachedViewModel->TryBuildDragSourceAtCell(Cell, Cell, DragSource))
        {
            PendingDragSource = DragSource;
            PendingDragInstanceID = DragSource.InstanceID;
            PendingDragClickLocalPos = GridLocalPos;
            return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventoryIconOverlay::NativeOnDragDetected(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent,
    UDragDropOperation*& OutOperation)
{
    if (!PendingDragInstanceID.IsValid())
    {
        return;
    }

    UInventoryDragDropOp* DragOp = NewObject<UInventoryDragDropOp>(this);
    DragOp->DraggedItemInstanceID = PendingDragSource.InstanceID;
    DragOp->ItemDataID = PendingDragSource.ItemDataID;
    DragOp->OriginalPosition = PendingDragSource.OriginalRootPosition;
    DragOp->bOriginalRotated = PendingDragSource.bOriginalRotated;
    DragOp->ItemSize = PendingDragSource.BaseItemSize;
    DragOp->DragItemSize = PendingDragSource.InitialDragItemSize;
    DragOp->bIsRotated = PendingDragSource.bOriginalRotated;
    DragOp->bFromEquipment = false;
    DragOp->DragOffset = PendingDragSource.DragOffset;
    DragOp->Pivot = EDragPivot::TopLeft;
    DragOp->DefaultDragVisual = nullptr;

    ActiveDragOperation = DragOp;
    CachedDragLocalPos = PendingDragClickLocalPos;
    CachedPreviewRootPos = PendingDragSource.OriginalRootPosition;
    bCachedPreviewCanPlace = true;
    bHasCachedPreview = false;

    OutOperation = DragOp;
    PendingDragInstanceID.Invalidate();
    PendingDragSource = FInventoryDragSourceViewData();
}

void UInventoryIconOverlay::SetParentPanel(UInventoryPanelWidget* InPanel)
{
    ParentPanel = InPanel;
}

void UInventoryIconOverlay::SetEquipmentViewModel(UEquipmentViewModel* InViewModel)
{
    CachedEquipmentViewModel = InViewModel;
}

bool UInventoryIconOverlay::RotateActiveDragItem()
{
    if (!ActiveDragOperation || ActiveDragOperation->bFromEquipment ||
        ActiveDragOperation->ItemSize.IsSquare())
    {
        return false;
    }

    ActiveDragOperation->bIsRotated = !ActiveDragOperation->bIsRotated;
    ActiveDragOperation->DragItemSize = ActiveDragOperation->bIsRotated
        ? ActiveDragOperation->ItemSize.GetRotated()
        : ActiveDragOperation->ItemSize;

    ActiveDragOperation->DragOffset.X = FMath::Clamp(
        ActiveDragOperation->DragOffset.X,
        0,
        ActiveDragOperation->DragItemSize.Width - 1);
    ActiveDragOperation->DragOffset.Y = FMath::Clamp(
        ActiveDragOperation->DragOffset.Y,
        0,
        ActiveDragOperation->DragItemSize.Height - 1);

    UpdateDragPreview(ActiveDragOperation, CachedDragLocalPos);
    return true;
}

void UInventoryIconOverlay::UpdateDragPreview(
    UInventoryDragDropOp* DragOp, const FVector2D& LocalPos)
{
    if (!DragOp || !ParentPanel.IsValid()
        || !CachedViewModel || !CachedGridPanel.IsValid()
        || CachedGridWidth <= 0 || CachedGridHeight <= 0)
    {
        return;
    }

    const FVector2D GridLocalSize = CachedGridPanel->GetCachedGeometry().GetLocalSize();
    if (GridLocalSize.IsNearlyZero())
    {
        return;
    }

    CachedDragLocalPos = LocalPos;

    const FIntPoint DroppedCell = LocalPosToGridCell(
        LocalPos, GridLocalSize, CachedGridWidth, CachedGridHeight);
    const FGuid IgnoredInstanceID = DragOp->bFromEquipment
        ? FGuid()
        : DragOp->DraggedItemInstanceID;
    const FInventoryPlacementHintViewData Hint =
        CachedViewModel->BuildPlacementHint(
            DroppedCell, DragOp->DragOffset, IgnoredInstanceID, DragOp->DragItemSize);

    CachedPreviewRootPos = Hint.PreviewRootPosition;
    bCachedPreviewCanPlace = Hint.bPredictedPlaceable;
    bHasCachedPreview = true;

    ParentPanel->ClearAreaHighlights();
    ParentPanel->HighlightArea(
        Hint.PreviewRootPosition, Hint.PreviewSize, Hint.bPredictedPlaceable);
}

bool UInventoryIconOverlay::NativeOnDrop(
    const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    (void)InGeometry;

    UInventoryDragDropOp* DragOp = Cast<UInventoryDragDropOp>(InOperation);
    if (!DragOp)
    {
        return false;
    }

    ActiveDragOperation = nullptr;

    if (ParentPanel.IsValid())
    {
        ParentPanel->ClearAreaHighlights();
        ParentPanel->StopDragAutoScroll();
    }

    if (!ParentPanel.IsValid() || !CachedGridPanel.IsValid()
        || CachedGridWidth <= 0 || CachedGridHeight <= 0)
    {
        return false;
    }

    const FVector2D GridLocalSize = CachedGridPanel->GetCachedGeometry().GetLocalSize();
    if (GridLocalSize.IsNearlyZero())
    {
        return false;
    }

    const FVector2D GridLocalPos = CachedGridPanel->GetCachedGeometry().AbsoluteToLocal(
        InDragDropEvent.GetScreenSpacePosition());
    CachedDragLocalPos = GridLocalPos;
    const FIntPoint DroppedCell = LocalPosToGridCell(
        GridLocalPos, GridLocalSize, CachedGridWidth, CachedGridHeight);
    const FIntPoint NewRootPos = DroppedCell - DragOp->DragOffset;

    bHasCachedPreview = false;
    bCachedPreviewCanPlace = false;

    if (DragOp->bFromEquipment)
    {
        if (!CachedEquipmentViewModel)
        {
            return false;
        }

        CachedEquipmentViewModel->RequestUnequipAt(
            DragOp->SourceEquipmentSlot, NewRootPos, DragOp->bIsRotated);
        return true;
    }

    if (NewRootPos == DragOp->OriginalPosition &&
        DragOp->bIsRotated == DragOp->bOriginalRotated)
    {
        return false;
    }

    return ParentPanel->ForwardMoveRequest(
        DragOp->DraggedItemInstanceID, NewRootPos, DragOp->bIsRotated);
}

bool UInventoryIconOverlay::NativeOnDragOver(
    const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    (void)InGeometry;

    UInventoryDragDropOp* DragOp = Cast<UInventoryDragDropOp>(InOperation);
    if (!DragOp || !ParentPanel.IsValid()
        || !CachedViewModel || !CachedGridPanel.IsValid()
        || CachedGridWidth <= 0 || CachedGridHeight <= 0)
    {
        return false;
    }

    const FVector2D GridLocalPos = CachedGridPanel->GetCachedGeometry().AbsoluteToLocal(
        InDragDropEvent.GetScreenSpacePosition());
    UpdateDragPreview(DragOp, GridLocalPos);
    ParentPanel->UpdateDragAutoScroll(InDragDropEvent.GetScreenSpacePosition());
    return true;
}

void UInventoryIconOverlay::NativeOnDragLeave(
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    bHasCachedPreview = false;
    bCachedPreviewCanPlace = false;

    if (ParentPanel.IsValid())
    {
        ParentPanel->ClearAreaHighlights();
        ParentPanel->StopDragAutoScroll();
    }
}

void UInventoryIconOverlay::NativeOnDragCancelled(
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);

    ActiveDragOperation = nullptr;
    bHasCachedPreview = false;
    bCachedPreviewCanPlace = false;

    if (ParentPanel.IsValid())
    {
        ParentPanel->ClearAreaHighlights();
        ParentPanel->StopDragAutoScroll();
    }
}

UItemContextMenuWidget* UInventoryIconOverlay::GetOrCreateContextMenu()
{
    if (!ContextMenuWidget && ContextMenuWidgetClass)
    {
        APlayerController* PC = GetOwningPlayer();
        if (!PC)
        {
            return nullptr;
        }

        ContextMenuWidget = CreateWidget<UItemContextMenuWidget>(PC, ContextMenuWidgetClass);
        if (ContextMenuWidget)
        {
            ContextMenuWidget->AddToViewport(100);
            ContextMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    return ContextMenuWidget;
}
