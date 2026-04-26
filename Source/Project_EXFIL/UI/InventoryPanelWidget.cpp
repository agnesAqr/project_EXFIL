// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventoryPanelWidget.h"
#include "CoreMinimal.h"
#include "Core/EXFILPlayerController.h"
#include "UI/EXFILUIManager.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/InventoryViewModel.h"
#include "UI/EquipmentViewModel.h"
#include "UI/CraftingViewModel.h"
#include "UI/InventorySlotWidget.h"
#include "UI/InventoryIconOverlay.h"
#include "UI/EquipmentSlotWidget.h"
#include "UI/CraftingPanelWidget.h"
#include "UI/StatEntryWidget.h"
#include "GAS/SurvivalViewModel.h"
#include "Input/CommonUIInputTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"

void UInventoryPanelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(true);
    if (Button_InventoryTab)
    {
        Button_InventoryTab->OnClicked.AddDynamic(this, &UInventoryPanelWidget::OnInventoryTabClicked);
    }
    if (Button_CraftingTab)
    {
        Button_CraftingTab->OnClicked.AddDynamic(this, &UInventoryPanelWidget::OnCraftingTabClicked);
    }
    UpdateTabStyles(0);
}

void UInventoryPanelWidget::NativeDestruct()
{
    StopDragAutoScroll();

    if (ViewModel && ViewModelRefreshedHandle.IsValid())
    {
        ViewModel->OnViewModelRefreshed.Remove(ViewModelRefreshedHandle);
        ViewModelRefreshedHandle.Reset();
    }

    Super::NativeDestruct();
}

void UInventoryPanelWidget::SetViewModel(UInventoryViewModel* InViewModel)
{
    if (ViewModel)
    {
        ViewModel->OnViewModelRefreshed.Remove(ViewModelRefreshedHandle);
        ViewModelRefreshedHandle.Reset();
    }

    ViewModel = InViewModel;

    if (ViewModel)
    {
        ViewModelRefreshedHandle = ViewModel->OnViewModelRefreshed.AddUObject(
            this, &UInventoryPanelWidget::HandleViewModelRefreshed);
        BuildGrid();
    }
}

void UInventoryPanelWidget::SetEquipmentViewModel(UEquipmentViewModel* InViewModel)
{
    EquipmentViewModel = InViewModel;

    if (IconOverlay)
    {
        IconOverlay->SetEquipmentViewModel(InViewModel);
    }

    if (!WidgetTree)
    {
        return;
    }

    WidgetTree->ForEachWidget([InViewModel](UWidget* Widget)
    {
        if (UEquipmentSlotWidget* EquipmentSlot = Cast<UEquipmentSlotWidget>(Widget))
        {
            EquipmentSlot->SetViewModel(InViewModel);
        }
    });
}

void UInventoryPanelWidget::SetCraftingViewModel(UCraftingViewModel* InViewModel)
{
    CraftingViewModel = InViewModel;
    if (CraftingPanel)
    {
        CraftingPanel->SetViewModel(InViewModel);
    }
}

void UInventoryPanelWidget::NotifyPanelShown()
{

    if (!CraftingPanel)
    {
        return;
    }

    const bool bCraftingTabActive =
        WidgetSwitcher_Content && WidgetSwitcher_Content->GetActiveWidgetIndex() == 1;
    if (bCraftingTabActive)
    {
        CraftingPanel->NotifyPanelShown();
    }
    else
    {
        CraftingPanel->NotifyPanelHidden();
    }
}

void UInventoryPanelWidget::NotifyPanelHidden()
{

    if (CraftingPanel)
    {
        CraftingPanel->NotifyPanelHidden();
    }
}

void UInventoryPanelWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    bNeedsCellSquareFix = true;
    bLayoutReady = false;
    CachedCellStride = FVector2D::ZeroVector;
    CachedSquareCellSize = 0.f;
    NotifyPanelShown();
}

void UInventoryPanelWidget::NativeOnDeactivated()
{
    Super::NativeOnDeactivated();
    NotifyPanelHidden();
}

bool UInventoryPanelWidget::NativeOnHandleBackAction()
{
    return false;
}

FReply UInventoryPanelWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (IconOverlay)
    {
        IconOverlay->CloseContextMenuIfOpen();
    }
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        return FReply::Handled().CaptureMouse(TakeWidget());
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UInventoryPanelWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HasMouseCapture())
    {
        return FReply::Handled().ReleaseMouseCapture();
    }

    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UInventoryPanelWidget::NativeOnMouseMove(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (HasMouseCapture() && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        return FReply::Handled();
    }

    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UInventoryPanelWidget::NativeOnKeyDown(const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::R && IconOverlay && IconOverlay->RotateActiveDragItem())
    {
        return FReply::Handled();
    }

    if (InKeyEvent.GetKey() == EKeys::Tab)
    {
        if (AEXFILPlayerController* PC = Cast<AEXFILPlayerController>(GetOwningPlayer()))
        {
            if (UEXFILUIManager* UIManager = PC->GetUIManager())
            {
                UIManager->ToggleInventory();
                return FReply::Handled();
            }
        }
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

TOptional<FUIInputConfig> UInventoryPanelWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UInventoryPanelWidget::BuildGrid()
{
    if (!ViewModel || !SlotWidgetClass || !GridPanel)
    {
        return;
    }

    ClearGrid();

    const int32 Width = ViewModel->GetGridWidth();
    const int32 Height = ViewModel->GetGridHeight();

    SlotWidgets.Reserve(Width * Height);
    bNeedsCellSquareFix = true;
    CachedSquareCellSize = 0.f;

    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            UInventorySlotWidget* SlotWidget = CreateWidget<UInventorySlotWidget>(this, SlotWidgetClass);
            if (!SlotWidget)
            {
                continue;
            }

            UInventorySlotViewModel* SlotVM = ViewModel->GetSlotAt(FIntPoint(X, Y));
            SlotWidget->SetParentPanel(this);
            SlotWidget->SetSlotViewModel(SlotVM);
            UUniformGridSlot* GridSlot = GridPanel->AddChildToUniformGrid(SlotWidget, Y, X);
            GridSlot->SetHorizontalAlignment(HAlign_Fill);
            GridSlot->SetVerticalAlignment(VAlign_Fill);
            SlotWidgets.Add(SlotWidget);
        }
    }
    if (IconOverlay)
    {
        IconOverlay->SetParentPanel(this);
        IconOverlay->SetEquipmentViewModel(EquipmentViewModel);
    }
    bHasPendingOverlayRefresh = true;
}

bool UInventoryPanelWidget::ForwardMoveRequest(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated)
{
    if (!ViewModel)
    {
        return false;
    }
    ViewModel->RequestMoveItem(ItemInstanceID, NewPosition, bNewRotated);
    return true;
}

void UInventoryPanelWidget::ClearGrid()
{
    if (GridPanel)
    {
        GridPanel->ClearChildren();
    }
    SlotWidgets.Empty();
}

void UInventoryPanelWidget::HighlightArea(FIntPoint RootPos, FItemSize ItemSize, bool bIsValid)
{
    if (!ViewModel)
    {
        return;
    }

    const int32 GridWidth = ViewModel->GetGridWidth();
    const int32 GridHeight = ViewModel->GetGridHeight();

    for (int32 Y = RootPos.Y; Y < RootPos.Y + ItemSize.Height; ++Y)
    {
        for (int32 X = RootPos.X; X < RootPos.X + ItemSize.Width; ++X)
        {
            if (X < 0 || Y < 0 || X >= GridWidth || Y >= GridHeight)
            {
                continue;
            }

            const int32 Index = Y * GridWidth + X;
            if (SlotWidgets.IsValidIndex(Index))
            {
                SlotWidgets[Index]->SetHighlight(true, bIsValid);
            }
        }
    }
}

void UInventoryPanelWidget::HandleViewModelRefreshed(const TSet<int32>& DirtyIndices)
{
    if (DirtyIndices.Num() == 0)
    {
        return;
    }

    bHasPendingOverlayRefresh = true;

    FlushOverlayDelta();
}

void UInventoryPanelWidget::HandleLayoutMeasured(const FGeometry& AllottedGeometry)
{
    if (!GridPanel || !ViewModel) return;
    const FVector2D GridSize = GridPanel->GetCachedGeometry().GetLocalSize();
    bLayoutReady = (GridSize.X > 1.f && GridSize.Y > 1.f);

    if (!bLayoutReady) return;
    const int32 GridW = ViewModel->GetGridWidth();
    const int32 GridH = ViewModel->GetGridHeight();
    const FVector2D NewStride(GridSize.X / GridW, GridSize.Y / GridH);
    const bool bStrideChanged = !CachedCellStride.Equals(NewStride, 0.5f);

    if (bStrideChanged)
    {
        CachedCellStride = NewStride;
        RebuildOverlayFull();
        return;
    }

    FlushOverlayDelta();
}

void UInventoryPanelWidget::FlushOverlayDelta()
{
    if (!bLayoutReady) return;
    if (!bHasPendingOverlayRefresh) return;
    if (!IconOverlay || !ViewModel || !GridPanel) return;

    const int32 GridW = ViewModel->GetGridWidth();
    const int32 GridH = ViewModel->GetGridHeight();

    FInventoryOverlayDeltaViewData Delta;
    if (!ViewModel->ConsumePendingOverlayDelta(Delta))
    {
        bHasPendingOverlayRefresh = false;
        return;
    }

    IconOverlay->RefreshIcons(ViewModel, GridPanel, GridW, GridH, Delta);

    bHasPendingOverlayRefresh = false;
}

void UInventoryPanelWidget::RebuildOverlayFull()
{
    if (!bLayoutReady) return;
    if (!IconOverlay || !ViewModel || !GridPanel) return;

    const int32 GridW = ViewModel->GetGridWidth();
    const int32 GridH = ViewModel->GetGridHeight();
    FInventoryOverlayDeltaViewData Delta;
    if (ViewModel->BuildFullOverlayDelta(Delta))
    {
        IconOverlay->RefreshIcons(ViewModel, GridPanel, GridW, GridH, Delta);
    }

    bHasPendingOverlayRefresh = false;
}

int32 UInventoryPanelWidget::NativePaint(const FPaintArgs& Args,
    const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId,
    const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    int32 Result = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
                                       OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    auto* MutableThis = const_cast<UInventoryPanelWidget*>(this);
    if (GridPanel && GridPanel->GetChildrenCount() > 0 && ViewModel)
    {
        const int32 GridWidth = FMath::Max(1, ViewModel->GetGridWidth());
        const float GridPixelWidth = GridPanel->GetCachedGeometry().GetLocalSize().X;
        const float DesiredCellSize = GridPixelWidth / GridWidth;

        if (DesiredCellSize > 1.f &&
            (bNeedsCellSquareFix || !FMath::IsNearlyEqual(CachedSquareCellSize, DesiredCellSize, 0.5f)))
        {
            MutableThis->bNeedsCellSquareFix = false;
            MutableThis->CachedSquareCellSize = DesiredCellSize;
            MutableThis->GridPanel->SetMinDesiredSlotWidth(DesiredCellSize);
            MutableThis->GridPanel->SetMinDesiredSlotHeight(DesiredCellSize);
            return Result;
        }
    }
    MutableThis->HandleLayoutMeasured(AllottedGeometry);

    return Result;
}

void UInventoryPanelWidget::ClearAreaHighlights()
{
    for (UInventorySlotWidget* SlotWidget : SlotWidgets)
    {
        if (SlotWidget)
        {
            SlotWidget->SetHighlight(false);
        }
    }
}

void UInventoryPanelWidget::BindStatsToViewModel(USurvivalViewModel* InSurvivalViewModel)
{
    if (StatEntry_HP) StatEntry_HP->BindToViewModel(InSurvivalViewModel, EExfilStatType::Health);
    if (StatEntry_HU) StatEntry_HU->BindToViewModel(InSurvivalViewModel, EExfilStatType::Hunger);
    if (StatEntry_TH) StatEntry_TH->BindToViewModel(InSurvivalViewModel, EExfilStatType::Thirst);
    if (StatEntry_ST) StatEntry_ST->BindToViewModel(InSurvivalViewModel, EExfilStatType::Stamina);
}

void UInventoryPanelWidget::OnInventoryTabClicked()
{
    if (WidgetSwitcher_Content)
    {
        WidgetSwitcher_Content->SetActiveWidgetIndex(0);
    }
    UpdateTabStyles(0);
    NotifyPanelHidden();
}

void UInventoryPanelWidget::OnCraftingTabClicked()
{
    if (WidgetSwitcher_Content)
    {
        WidgetSwitcher_Content->SetActiveWidgetIndex(1);
    }
    UpdateTabStyles(1);
    if (CraftingPanel)
    {
        CraftingPanel->NotifyPanelShown();
    }
}

void UInventoryPanelWidget::UpdateTabStyles(int32 ActiveIndex)
{
    auto ApplyTabColor = [](UButton* Btn, FLinearColor Color)
    {
        if (!Btn) return;
        if (UWidget* Child = Btn->GetChildAt(0))
        {
            if (UTextBlock* TB = Cast<UTextBlock>(Child))
            {
                TB->SetColorAndOpacity(FSlateColor(Color));
            }
        }
    };

    ApplyTabColor(Button_InventoryTab,
        ActiveIndex == 0 ? FLinearColor(1.f, 1.f, 1.f, 0.8f) : FLinearColor(1.f, 1.f, 1.f, 0.35f));
    ApplyTabColor(Button_CraftingTab,
        ActiveIndex == 1 ? FLinearColor(1.f, 1.f, 1.f, 0.8f) : FLinearColor(1.f, 1.f, 1.f, 0.35f));
}

void UInventoryPanelWidget::UpdateDragAutoScroll(const FVector2D& ScreenSpacePosition)
{
    if (!InventoryScrollBox) return;

    const FGeometry& ScrollGeo = InventoryScrollBox->GetCachedGeometry();
    const FVector2D LocalPos = ScrollGeo.AbsoluteToLocal(ScreenSpacePosition);
    const FVector2D ScrollSize = ScrollGeo.GetLocalSize();
    if (LocalPos.Y < ScrollEdgeZone)
    {
        AutoScrollSpeed = -ScrollRate;
    }
    else if (LocalPos.Y > ScrollSize.Y - ScrollEdgeZone)
    {
        AutoScrollSpeed = ScrollRate;
    }
    else
    {
        AutoScrollSpeed = 0.f;
    }
    if (AutoScrollSpeed != 0.f && !GetWorld()->GetTimerManager().IsTimerActive(AutoScrollTimerHandle))
    {
        GetWorld()->GetTimerManager().SetTimer(
            AutoScrollTimerHandle, this,
            &UInventoryPanelWidget::TickAutoScroll,
            AutoScrollUpdateInterval, true);
    }
    else if (AutoScrollSpeed == 0.f)
    {
        GetWorld()->GetTimerManager().ClearTimer(AutoScrollTimerHandle);
    }
}

void UInventoryPanelWidget::StopDragAutoScroll()
{
    AutoScrollSpeed = 0.f;
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AutoScrollTimerHandle);
    }
}

void UInventoryPanelWidget::TickAutoScroll()
{
    if (!InventoryScrollBox || AutoScrollSpeed == 0.f)
    {
        StopDragAutoScroll();
        return;
    }

    const float CurrentOffset = InventoryScrollBox->GetScrollOffset();
    InventoryScrollBox->SetScrollOffset(CurrentOffset + AutoScrollSpeed);
}
