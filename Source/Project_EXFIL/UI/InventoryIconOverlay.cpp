// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventoryIconOverlay.h"
#include "CoreMinimal.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/Texture2D.h"
#include "UI/InventoryViewModel.h"
#include "UI/InventorySlotViewModel.h"
#include "UI/ItemContextMenuWidget.h"
#include "UI/InventoryDragDropOp.h"
#include "UI/InventoryPanelWidget.h"
#include "Inventory/InventoryComponent.h"
#include "Data/Equipment/EquipmentComponent.h"
#include "Data/ItemDataSubsystem.h"
#include "Core/EXFILLog.h"

void UInventoryIconOverlay::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    // The overlay must stay Visible so it can receive mouse events directly.
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

void UInventoryIconOverlay::RefreshIcons(UInventoryViewModel* InViewModel,
                                          UUniformGridPanel* InGridPanel,
                                          int32 InGridWidth, int32 InGridHeight,
                                          const TSet<int32>& DirtyIndices)
{
    // Refresh caches used for hit testing and drag math.
    CachedViewModel  = InViewModel;
    CachedGridPanel  = InGridPanel;
    CachedGridWidth  = InGridWidth;
    CachedGridHeight = InGridHeight;

    if (!IconCanvas || !InViewModel || !InGridPanel)
    {
        return;
    }

    UWidget* FirstSlot = InGridPanel->GetChildAt(0);
    if (!FirstSlot)
    {
        UE_LOG(LogEXFIL, Error, TEXT("RefreshIcons: FirstSlot is null - grid panel has no children"));
        return;
    }

    const FVector2D SlotSize = FirstSlot->GetCachedGeometry().GetLocalSize();
    const FVector2D GridLocalSize = InGridPanel->GetCachedGeometry().GetLocalSize();
    const FVector2D CellStride(GridLocalSize.X / InGridWidth, GridLocalSize.Y / InGridHeight);

    if (GridLocalSize.IsNearlyZero() || CellStride.X < 1.f || CellStride.Y < 1.f)
    {
        return;
    }

    // Rebuild every icon when the slot stride changes.
    const bool bStrideChanged = !CachedCellStride.Equals(CellStride, 0.5f);
    if (bStrideChanged)
    {
        CachedCellStride = CellStride;
        IconCanvas->ClearChildren();
        IconImageCache.Empty();
        StackTextCache.Empty();
    }

    // Resolve cached item icons through the item data subsystem.
    UItemDataSubsystem* ItemSub = nullptr;
    if (UGameInstance* GI = GetGameInstance())
    {
        ItemSub = GI->GetSubsystem<UItemDataSubsystem>();
    }

    // Collect unique root items affected by the dirty slot set.
    TSet<FGuid> DirtyRootItems;

    for (int32 SlotIndex : DirtyIndices)
    {
        const FIntPoint Pos(SlotIndex % InGridWidth, SlotIndex / InGridWidth);
        UInventorySlotViewModel* SlotVM = InViewModel->GetSlotAt(Pos);
        if (!SlotVM) continue;

        if (!SlotVM->IsEmpty())
        {
            const FGuid ItemID = SlotVM->GetItemInstanceID();
            if (ItemID.IsValid())
            {
                DirtyRootItems.Add(ItemID);
            }
        }
        else
        {
            // Empty dirty slots are handled during the cache reconciliation pass below.
        }
    }

    // Refresh each dirty root item once.
    for (const FGuid& ItemID : DirtyRootItems)
    {
        // Resolve the root slot for the item.
        const FIntPoint RootPos = InViewModel->GetItemRootPosition(ItemID);
        if (RootPos.X < 0) continue;

        UInventorySlotViewModel* RootSlotVM = InViewModel->GetSlotAt(RootPos);
        if (!RootSlotVM || RootSlotVM->IsEmpty() || !RootSlotVM->IsRootSlot()) continue;

        const FVector2D IconPos(RootPos.X * CellStride.X, RootPos.Y * CellStride.Y);
        const FVector2D IconSize(
            RootSlotVM->GetItemSizeX() * CellStride.X,
            RootSlotVM->GetItemSizeY() * CellStride.Y);

        // Update the icon widget.
        const TSoftObjectPtr<UTexture2D> IconPtr = RootSlotVM->GetIcon();
        UImage** ExistingImage = IconImageCache.Find(ItemID);

        if (!IconPtr.IsNull())
        {
            UTexture2D* IconTex = ItemSub ? ItemSub->GetCachedTexture(IconPtr) : IconPtr.LoadSynchronous();
            if (IconTex)
            {
                const bool bRotated = RootSlotVM->IsRotated();
                const FVector2D TexSize(IconTex->GetSizeX(), IconTex->GetSizeY());
                const float Scale = bRotated
                    ? FMath::Min(IconSize.X / TexSize.Y, IconSize.Y / TexSize.X)
                    : FMath::Min(IconSize.X / TexSize.X, IconSize.Y / TexSize.Y);
                const FVector2D FinalSize = TexSize * Scale;
                const FVector2D Offset = (IconSize - FinalSize) * 0.5f;

                if (ExistingImage && *ExistingImage)
                {
                    UImage* Img = *ExistingImage;
                    FSlateBrush Brush;
                    Brush.SetResourceObject(IconTex);
                    Brush.DrawAs = ESlateBrushDrawType::Image;
                    Img->SetBrush(Brush);
                    Img->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
                    Img->SetRenderTransformAngle(bRotated ? 90.f : 0.f);

                    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Img->Slot))
                    {
                        CanvasSlot->SetPosition(IconPos + Offset);
                        CanvasSlot->SetSize(FinalSize);
                    }
                }
                else
                {
                    UImage* IconImage = NewObject<UImage>(this);
                    IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);

                    FSlateBrush Brush;
                    Brush.SetResourceObject(IconTex);
                    Brush.DrawAs = ESlateBrushDrawType::Image;
                    IconImage->SetBrush(Brush);
                    IconImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
                    IconImage->SetRenderTransformAngle(bRotated ? 90.f : 0.f);

                    UCanvasPanelSlot* CanvasSlot = IconCanvas->AddChildToCanvas(IconImage);
                    if (CanvasSlot)
                    {
                        CanvasSlot->SetPosition(IconPos + Offset);
                        CanvasSlot->SetSize(FinalSize);
                        CanvasSlot->SetAutoSize(false);
                    }
                    IconImageCache.Add(ItemID, IconImage);
                }
            }
        }
        else if (ExistingImage && *ExistingImage)
        {
            (*ExistingImage)->RemoveFromParent();
            IconImageCache.Remove(ItemID);
        }

        // Update the stack count widget.
        UTextBlock** ExistingText = StackTextCache.Find(ItemID);

        if (RootSlotVM->GetStackCount() > 1)
        {
            if (ExistingText && *ExistingText)
            {
                (*ExistingText)->SetText(FText::AsNumber(RootSlotVM->GetStackCount()));
                if (UCanvasPanelSlot* TextSlot = Cast<UCanvasPanelSlot>((*ExistingText)->Slot))
                {
                    TextSlot->SetPosition(FVector2D(
                        IconPos.X + IconSize.X - SlotSize.X * 0.3f,
                        IconPos.Y + IconSize.Y - SlotSize.Y * 0.3f));
                }
            }
            else
            {
                UTextBlock* StackText = NewObject<UTextBlock>(this);
                StackText->SetVisibility(ESlateVisibility::HitTestInvisible);
                StackText->SetText(FText::AsNumber(RootSlotVM->GetStackCount()));

                FSlateFontInfo FontInfo = StackText->GetFont();
                FontInfo.Size = 12;
                StackText->SetFont(FontInfo);
                StackText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.9f)));

                UCanvasPanelSlot* TextSlot = IconCanvas->AddChildToCanvas(StackText);
                if (TextSlot)
                {
                    TextSlot->SetAutoSize(true);
                    TextSlot->SetPosition(FVector2D(
                        IconPos.X + IconSize.X - SlotSize.X * 0.3f,
                        IconPos.Y + IconSize.Y - SlotSize.Y * 0.3f));
                }
                StackTextCache.Add(ItemID, StackText);
            }
        }
        else if (ExistingText && *ExistingText)
        {
            (*ExistingText)->RemoveFromParent();
            StackTextCache.Remove(ItemID);
        }
    }

    // Remove stale widgets that no longer map to an item in the view model.
    TArray<FGuid> StaleIDs;
    for (auto& Pair : IconImageCache)
    {
        const FIntPoint RootPos = InViewModel->GetItemRootPosition(Pair.Key);
        if (RootPos.X < 0)
        {
            // The view model no longer knows about this item.
            StaleIDs.Add(Pair.Key);
        }
    }
    for (const FGuid& ID : StaleIDs)
    {
        if (UImage** Img = IconImageCache.Find(ID))
        {
            if (*Img) (*Img)->RemoveFromParent();
        }
        if (UTextBlock** Txt = StackTextCache.Find(ID))
        {
            if (*Txt) (*Txt)->RemoveFromParent();
        }
        IconImageCache.Remove(ID);
        StackTextCache.Remove(ID);
    }

    if (DirtyRootItems.Num() > 0 || StaleIDs.Num() > 0)
    {
        Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
    }
}

// ========== Context Menu ==========

FReply UInventoryIconOverlay::NativeOnMouseButtonDown(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    (void)InGeometry;

    if (!CachedGridPanel.IsValid())
    {
        return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    }

    const FVector2D GridLocalPos = CachedGridPanel->GetCachedGeometry().AbsoluteToLocal(
        InMouseEvent.GetScreenSpacePosition());

    // Right mouse button opens the context menu.
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        FGuid HitInstanceID;
        FName HitItemDataID;
        const bool bHitItem = FindItemAtPosition(GridLocalPos, HitInstanceID, HitItemDataID);

        // Always close an existing menu before opening a new one.
        if (ContextMenuWidget &&
            ContextMenuWidget->GetVisibility() == ESlateVisibility::Visible)
        {
            ContextMenuWidget->CloseMenu();
        }

        // Open the menu when the cursor is over an item.
        if (bHitItem)
        {
            UItemContextMenuWidget* Menu = GetOrCreateContextMenu();
            if (Menu)
            {
                Menu->ShowForInventoryItem(HitInstanceID, HitItemDataID);
                Menu->SetMenuPosition(InMouseEvent.GetScreenSpacePosition());
            }
        }
        // Empty space only closes the menu.

        return FReply::Handled();
    }

    // The overlay owns left-click drag detection because it intercepts hit tests.
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        FGuid HitInstanceID;
        FName HitItemDataID;
        const bool bHit = FindItemAtPosition(GridLocalPos, HitInstanceID, HitItemDataID);
        if (bHit)
        {
            PendingDragInstanceID    = HitInstanceID;
            PendingDragClickLocalPos = GridLocalPos;
            // TakeWidget forces the underlying Slate widget to exist before drag detection.
            return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventoryIconOverlay::NativeOnDragDetected(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    if (!PendingDragInstanceID.IsValid())
    {
        UE_LOG(LogEXFIL, Error, TEXT("IconOverlay NativeOnDragDetected: PendingDragInstanceID invalid - canceling drag"));
        return;
    }

    // Resolve the authoritative inventory item so drag math uses the true item size.
    APlayerController* PC = GetOwningPlayer();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    UInventoryComponent* InvComp = Pawn
        ? Pawn->FindComponentByClass<UInventoryComponent>()
        : nullptr;

    if (!InvComp)
    {
        PendingDragInstanceID.Invalidate();
        return;
    }

    FInventoryItemInstance ItemInstance;
    if (!InvComp->GetItemByID(PendingDragInstanceID, ItemInstance))
    {
        PendingDragInstanceID.Invalidate();
        return;
    }

    const bool bCanRotate = !ItemInstance.ItemSize.IsSquare();
    const bool bInitialRotated = bCanRotate ? ItemInstance.bIsRotated : false;

    UInventoryDragDropOp* DragOp = NewObject<UInventoryDragDropOp>(this);
    DragOp->DraggedItemInstanceID = ItemInstance.InstanceID;
    DragOp->ItemDataID             = ItemInstance.ItemDataID;
    DragOp->OriginalPosition       = ItemInstance.RootPosition;
    DragOp->bOriginalRotated       = bInitialRotated;
    DragOp->ItemSize               = ItemInstance.ItemSize;
    DragOp->DragItemSize           = ItemInstance.GetEffectiveSize();
    DragOp->bIsRotated             = bInitialRotated;
    DragOp->bFromEquipment         = false;
    DragOp->Pivot                  = EDragPivot::TopLeft;

    // DragOffset keeps drops aligned when dragging from any occupied cell.
    if (CachedGridPanel.IsValid() && CachedGridWidth > 0 && CachedGridHeight > 0)
    {
        const FVector2D GridLocalSize = CachedGridPanel->GetCachedGeometry().GetLocalSize();
        if (!GridLocalSize.IsNearlyZero())
        {
            const FVector2D CellStride(GridLocalSize.X / CachedGridWidth,
                                        GridLocalSize.Y / CachedGridHeight);
            const FIntPoint ClickedCell(
                FMath::FloorToInt(PendingDragClickLocalPos.X / CellStride.X),
                FMath::FloorToInt(PendingDragClickLocalPos.Y / CellStride.Y));
            FIntPoint Offset = ClickedCell - ItemInstance.RootPosition;
            // Clamp the offset inside the current item bounds.
            const FItemSize EffSize = ItemInstance.GetEffectiveSize();
            Offset.X = FMath::Clamp(Offset.X, 0, EffSize.Width  - 1);
            Offset.Y = FMath::Clamp(Offset.Y, 0, EffSize.Height - 1);
            DragOp->DragOffset = Offset;
        }
    }

    // Use highlight feedback only. No separate drag visual is spawned.
    DragOp->DefaultDragVisual = nullptr;

    ActiveDragOperation = DragOp;
    CachedDragLocalPos = PendingDragClickLocalPos;
    CachedPreviewRootPos = ItemInstance.RootPosition;
    bCachedPreviewCanPlace = true;
    bHasCachedPreview = false;

    UE_LOG(LogEXFIL, Log,
        TEXT("[InventoryDrag] Begin Item=%s DataID=%s Root=(%d,%d) DragOffset=(%d,%d) Rotated=%s"),
        *ItemInstance.InstanceID.ToString(),
        *ItemInstance.ItemDataID.ToString(),
        ItemInstance.RootPosition.X,
        ItemInstance.RootPosition.Y,
        DragOp->DragOffset.X,
        DragOp->DragOffset.Y,
        DragOp->bIsRotated ? TEXT("true") : TEXT("false"));

    OutOperation = DragOp;
    PendingDragInstanceID.Invalidate();
}

bool UInventoryIconOverlay::FindItemAtPosition(const FVector2D& LocalPos,
    FGuid& OutInstanceID, FName& OutItemDataID) const
{
    if (!CachedViewModel || !CachedGridPanel.IsValid() || CachedGridWidth <= 0 || CachedGridHeight <= 0)
    {
        return false;
    }

    const FVector2D GridLocalSize = CachedGridPanel->GetCachedGeometry().GetLocalSize();
    if (GridLocalSize.IsNearlyZero()) return false;

    const FVector2D CellStride(GridLocalSize.X / CachedGridWidth,
                                GridLocalSize.Y / CachedGridHeight);

    // Convert directly from local coordinates to a grid cell.
    const int32 Col = FMath::FloorToInt(LocalPos.X / CellStride.X);
    const int32 Row = FMath::FloorToInt(LocalPos.Y / CellStride.Y);

    if (Col < 0 || Col >= CachedGridWidth || Row < 0 || Row >= CachedGridHeight)
    {
        return false;
    }

    UInventorySlotViewModel* SlotVM = CachedViewModel->GetSlotAt(FIntPoint(Col, Row));
    if (!SlotVM || SlotVM->IsEmpty())
    {
        return false;
    }

    OutInstanceID = SlotVM->GetItemInstanceID();
    OutItemDataID = SlotVM->GetItemDataID();
    return true;
}

// ========== Parent Panel Wiring ==========

void UInventoryIconOverlay::SetParentPanel(UInventoryPanelWidget* InPanel)
{
    ParentPanel = InPanel;
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


// ========== Drop / Highlight ==========

/** Convert local overlay coordinates into a grid cell index. */
static FIntPoint LocalPosToGridCell(const FVector2D& LocalPos,
    const FVector2D& GridLocalSize, int32 GridW, int32 GridH)
{
    const FVector2D CellStride(GridLocalSize.X / GridW, GridLocalSize.Y / GridH);
    return FIntPoint(FMath::FloorToInt(LocalPos.X / CellStride.X),
                     FMath::FloorToInt(LocalPos.Y / CellStride.Y));
}

void UInventoryIconOverlay::UpdateDragPreview(UInventoryDragDropOp* DragOp, const FVector2D& LocalPos)
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

    const FIntPoint DroppedCell = LocalPosToGridCell(LocalPos, GridLocalSize,
        CachedGridWidth, CachedGridHeight);
    const FIntPoint RootPos = DroppedCell - DragOp->DragOffset;
    CachedPreviewRootPos = RootPos;
    bHasCachedPreview = true;

    const int32 GridW = CachedViewModel->GetGridWidth();
    const int32 GridH = CachedViewModel->GetGridHeight();

    ParentPanel->ClearAreaHighlights();

    if (RootPos.X < 0 || RootPos.Y < 0 ||
        RootPos.X + DragOp->DragItemSize.Width > GridW ||
        RootPos.Y + DragOp->DragItemSize.Height > GridH)
    {
        bCachedPreviewCanPlace = false;
        return;
    }

    bool bCanPlace = true;
    for (int32 Y = RootPos.Y; Y < RootPos.Y + DragOp->DragItemSize.Height && bCanPlace; ++Y)
    {
        for (int32 X = RootPos.X; X < RootPos.X + DragOp->DragItemSize.Width && bCanPlace; ++X)
        {
            UInventorySlotViewModel* TargetSlot = CachedViewModel->GetSlotAt(FIntPoint(X, Y));
            if (!TargetSlot ||
                (!TargetSlot->IsEmpty() &&
                 TargetSlot->GetItemInstanceID() != DragOp->DraggedItemInstanceID))
            {
                bCanPlace = false;
            }
        }
    }

    bCachedPreviewCanPlace = bCanPlace;
    ParentPanel->HighlightArea(RootPos, DragOp->DragItemSize, bCanPlace);
}

bool UInventoryIconOverlay::NativeOnDrop(const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    (void)InGeometry;

    UInventoryDragDropOp* DragOp = Cast<UInventoryDragDropOp>(InOperation);
    if (!DragOp) return false;

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
    if (GridLocalSize.IsNearlyZero()) return false;

    const FVector2D GridLocalPos = CachedGridPanel->GetCachedGeometry().AbsoluteToLocal(
        InDragDropEvent.GetScreenSpacePosition());
    CachedDragLocalPos = GridLocalPos;
    // Use the actual pointer location at drop time as the authoritative input.
    // The cached drag-over preview is only for highlight feedback.
    const FIntPoint DroppedCell = LocalPosToGridCell(GridLocalPos, GridLocalSize,
        CachedGridWidth, CachedGridHeight);
    const FIntPoint NewRootPos = DroppedCell - DragOp->DragOffset;

    bHasCachedPreview = false;
    bCachedPreviewCanPlace = false;

    if (DragOp->bFromEquipment)
    {
        APlayerController* PC = GetOwningPlayer();
        APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        UEquipmentComponent* EquipComp = Pawn
            ? Pawn->FindComponentByClass<UEquipmentComponent>() : nullptr;
        if (!EquipComp)
        {
            return false;
        }

        UE_LOG(LogEXFIL, Log,
            TEXT("[InventoryDrag] UnequipDrop Item=%s Slot=%d To=(%d,%d) Rotated=%s"),
            *DragOp->DraggedItemInstanceID.ToString(),
            static_cast<int32>(DragOp->SourceEquipmentSlot),
            NewRootPos.X,
            NewRootPos.Y,
            DragOp->bIsRotated ? TEXT("true") : TEXT("false"));

        EquipComp->RequestUnequipToInventoryAt(
            DragOp->SourceEquipmentSlot, NewRootPos, DragOp->bIsRotated);
        return true;
    }

    if (NewRootPos == DragOp->OriginalPosition &&
        DragOp->bIsRotated == DragOp->bOriginalRotated)
    {
        return false;
    }

    UE_LOG(LogEXFIL, Log,
        TEXT("[InventoryDrag] Drop Item=%s From=(%d,%d) To=(%d,%d) Rotated=%s"),
        *DragOp->DraggedItemInstanceID.ToString(),
        DragOp->OriginalPosition.X,
        DragOp->OriginalPosition.Y,
        NewRootPos.X,
        NewRootPos.Y,
        DragOp->bIsRotated ? TEXT("true") : TEXT("false"));

    return ParentPanel->ForwardMoveRequest(
        DragOp->DraggedItemInstanceID, NewRootPos, DragOp->bIsRotated);
}

bool UInventoryIconOverlay::NativeOnDragOver(const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
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

void UInventoryIconOverlay::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent,
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

void UInventoryIconOverlay::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent,
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
        if (!PC) return nullptr;

        ContextMenuWidget = CreateWidget<UItemContextMenuWidget>(PC, ContextMenuWidgetClass);
        if (ContextMenuWidget)
        {
            ContextMenuWidget->AddToViewport(100); // Keep the menu above the inventory panel.
            ContextMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    return ContextMenuWidget;
}
