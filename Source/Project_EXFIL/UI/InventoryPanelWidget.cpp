// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventoryPanelWidget.h"
#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/CommonUIInputTypes.h"
#include "Widgets/SWidget.h"
#include "Core/EXFILLog.h"
#include "Core/EXFILPlayerController.h"
#include "GAS/SurvivalViewModel.h"
#include "UI/CraftingPanelWidget.h"
#include "UI/CraftingViewModel.h"
#include "UI/EquipmentSlotWidget.h"
#include "UI/EquipmentViewModel.h"
#include "UI/EXFILUIManager.h"
#include "UI/InventoryIconOverlay.h"
#include "UI/InventorySlotWidget.h"
#include "UI/InventoryViewModel.h"
#include "UI/StatEntryWidget.h"

namespace
{
const TCHAR* LexBoolForEXFILUIFlow(const bool bValue)
{
    return bValue ? TEXT("true") : TEXT("false");
}

FString LexSlateVisibilityForEXFILUIFlow(const ESlateVisibility Visibility)
{
    if (const UEnum* Enum = StaticEnum<ESlateVisibility>())
    {
        return Enum->GetNameStringByValue(static_cast<int64>(Visibility));
    }

    return FString::FromInt(static_cast<int32>(Visibility));
}

FString GetFocusedSlateWidgetForEXFILUIFlow()
{
    if (!FSlateApplication::IsInitialized())
    {
        return TEXT("SlateNotInitialized");
    }

    const TSharedPtr<SWidget> FocusedWidget =
        FSlateApplication::Get().GetKeyboardFocusedWidget();
    return FocusedWidget.IsValid()
        ? FocusedWidget->GetTypeAsString()
        : FString(TEXT("None"));
}

void LogPanelStateForEXFILUIFlow(const TCHAR* Context, const UInventoryPanelWidget* Panel)
{
    const APlayerController* PC = Panel ? Panel->GetOwningPlayer() : nullptr;
    UE_LOG(LogEXFIL, Log,
        TEXT("[UIFlow][Panel %s] Panel=%s OwningPC=%s Pawn=%s Visibility=%s InViewport=%s KeyboardFocus=%s AnyUserFocus=%s MouseCapture=%s Cursor=%s ViewModel=%s EquipmentVM=%s CraftingVM=%s SlateFocus=%s"),
        Context ? Context : TEXT("Unknown"),
        *GetNameSafe(Panel),
        *GetNameSafe(PC),
        PC ? *GetNameSafe(PC->GetPawn()) : TEXT("None"),
        Panel ? *LexSlateVisibilityForEXFILUIFlow(Panel->GetVisibility()) : TEXT("None"),
        Panel ? LexBoolForEXFILUIFlow(Panel->IsInViewport()) : TEXT("false"),
        Panel ? LexBoolForEXFILUIFlow(Panel->HasKeyboardFocus()) : TEXT("false"),
        Panel ? LexBoolForEXFILUIFlow(Panel->HasAnyUserFocus()) : TEXT("false"),
        Panel ? LexBoolForEXFILUIFlow(Panel->HasMouseCapture()) : TEXT("false"),
        PC ? LexBoolForEXFILUIFlow(PC->bShowMouseCursor) : TEXT("false"),
        Panel ? *GetNameSafe(Panel->GetViewModel()) : TEXT("None"),
        Panel ? *GetNameSafe(Panel->GetEquipmentViewModelForDebug()) : TEXT("None"),
        Panel ? *GetNameSafe(Panel->GetCraftingViewModelForDebug()) : TEXT("None"),
        *GetFocusedSlateWidgetForEXFILUIFlow());
}
}

void UInventoryPanelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(true);
    UE_LOG(LogEXFIL, Log,
        TEXT("[UIFlow][Panel NativeOnInitialized] Panel=%s Focusable=%s GridPanel=%s IconOverlay=%s SlotWidgetClass=%s"),
        *GetNameSafe(this),
        LexBoolForEXFILUIFlow(true),
        *GetNameSafe(GridPanel),
        *GetNameSafe(IconOverlay),
        *GetNameSafe(SlotWidgetClass.Get()));

    if (Button_InventoryTab)
    {
        Button_InventoryTab->OnClicked.AddDynamic(this, &UInventoryPanelWidget::OnInventoryTabClicked);
    }
    if (Button_CraftingTab)
    {
        Button_CraftingTab->OnClicked.AddDynamic(this, &UInventoryPanelWidget::OnCraftingTabClicked);
    }
    UpdateTabStyles(0);
    ResolveInventoryScrollBox();
    LogPanelStateForEXFILUIFlow(TEXT("NativeOnInitialized:After"), this);
}

void UInventoryPanelWidget::NativeDestruct()
{
    LogPanelStateForEXFILUIFlow(TEXT("NativeDestruct:Before"), this);
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
    UE_LOG(LogEXFIL, Log,
        TEXT("[UIFlow][Panel SetViewModel:Before] Panel=%s OldVM=%s NewVM=%s ViewModelHandleValid=%s"),
        *GetNameSafe(this),
        *GetNameSafe(ViewModel),
        *GetNameSafe(InViewModel),
        LexBoolForEXFILUIFlow(ViewModelRefreshedHandle.IsValid()));

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
        LogPanelStateForEXFILUIFlow(TEXT("SetViewModel:AfterBind"), this);
        return;
    }

    ClearGrid();
    if (IconOverlay)
    {
        IconOverlay->ClearIcons();
    }
    bHasPendingOverlayRefresh = false;
    bLayoutReady = false;
    CachedCellStride = FVector2D::ZeroVector;
    CachedSquareCellSize = 0.f;
    bNeedsCellSquareFix = true;
    LogPanelStateForEXFILUIFlow(TEXT("SetViewModel:AfterClear"), this);
}

void UInventoryPanelWidget::SetEquipmentViewModel(UEquipmentViewModel* InViewModel)
{
    UE_LOG(LogEXFIL, Log,
        TEXT("[UIFlow][Panel SetEquipmentViewModel] Panel=%s OldVM=%s NewVM=%s IconOverlay=%s"),
        *GetNameSafe(this),
        *GetNameSafe(EquipmentViewModel),
        *GetNameSafe(InViewModel),
        *GetNameSafe(IconOverlay));

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
    UE_LOG(LogEXFIL, Log,
        TEXT("[UIFlow][Panel SetCraftingViewModel] Panel=%s OldVM=%s NewVM=%s CraftingPanel=%s"),
        *GetNameSafe(this),
        *GetNameSafe(CraftingViewModel),
        *GetNameSafe(InViewModel),
        *GetNameSafe(CraftingPanel));

    CraftingViewModel = InViewModel;
    if (CraftingPanel)
    {
        CraftingPanel->SetViewModel(InViewModel);
    }
}

void UInventoryPanelWidget::NotifyPanelShown()
{
    LogPanelStateForEXFILUIFlow(TEXT("NotifyPanelShown:Before"), this);

    if (!CraftingPanel)
    {
        LogPanelStateForEXFILUIFlow(TEXT("NotifyPanelShown:NoCraftingPanel"), this);
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

    LogPanelStateForEXFILUIFlow(TEXT("NotifyPanelShown:After"), this);
}

void UInventoryPanelWidget::NotifyPanelHidden()
{
    LogPanelStateForEXFILUIFlow(TEXT("NotifyPanelHidden:Before"), this);
    StopDragAutoScroll();

    if (CraftingPanel)
    {
        CraftingPanel->NotifyPanelHidden();
    }
    LogPanelStateForEXFILUIFlow(TEXT("NotifyPanelHidden:After"), this);
}

void UInventoryPanelWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    LogPanelStateForEXFILUIFlow(TEXT("NativeOnActivated"), this);
    bNeedsCellSquareFix = true;
    bLayoutReady = false;
    CachedCellStride = FVector2D::ZeroVector;
    CachedSquareCellSize = 0.f;
    NotifyPanelShown();
}

void UInventoryPanelWidget::NativeOnDeactivated()
{
    Super::NativeOnDeactivated();
    LogPanelStateForEXFILUIFlow(TEXT("NativeOnDeactivated"), this);
    StopDragAutoScroll();
    NotifyPanelHidden();
}

FReply UInventoryPanelWidget::NativeOnFocusReceived(const FGeometry& InGeometry,
    const FFocusEvent& InFocusEvent)
{
    UE_LOG(LogEXFIL, Log,
        TEXT("[UIFlow][Panel NativeOnFocusReceived] Panel=%s Cause=%d"),
        *GetNameSafe(this),
        static_cast<int32>(InFocusEvent.GetCause()));
    LogPanelStateForEXFILUIFlow(TEXT("NativeOnFocusReceived"), this);
    return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

void UInventoryPanelWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
    UE_LOG(LogEXFIL, Log,
        TEXT("[UIFlow][Panel NativeOnFocusLost] Panel=%s Cause=%d"),
        *GetNameSafe(this),
        static_cast<int32>(InFocusEvent.GetCause()));
    LogPanelStateForEXFILUIFlow(TEXT("NativeOnFocusLost"), this);
    Super::NativeOnFocusLost(InFocusEvent);
}

bool UInventoryPanelWidget::NativeOnHandleBackAction()
{
    return false;
}

FReply UInventoryPanelWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    UE_LOG(LogEXFIL, Log,
        TEXT("[UIFlow][Panel NativeOnMouseButtonDown] Panel=%s Button=%s HasCaptureBefore=%s"),
        *GetNameSafe(this),
        *InMouseEvent.GetEffectingButton().ToString(),
        LexBoolForEXFILUIFlow(HasMouseCapture()));

    if (IconOverlay)
    {
        IconOverlay->CloseContextMenuIfOpen();
    }
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        LogPanelStateForEXFILUIFlow(TEXT("NativeOnMouseButtonDown:CaptureMouse"), this);
        return FReply::Handled().CaptureMouse(TakeWidget());
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UInventoryPanelWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    UE_LOG(LogEXFIL, Log,
        TEXT("[UIFlow][Panel NativeOnMouseButtonUp] Panel=%s Button=%s HasCaptureBefore=%s"),
        *GetNameSafe(this),
        *InMouseEvent.GetEffectingButton().ToString(),
        LexBoolForEXFILUIFlow(HasMouseCapture()));

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HasMouseCapture())
    {
        LogPanelStateForEXFILUIFlow(TEXT("NativeOnMouseButtonUp:ReleaseMouse"), this);
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
    UE_LOG(LogEXFIL, Log,
        TEXT("[UIFlow][Panel NativeOnKeyDown] Panel=%s Key=%s Visibility=%s KeyboardFocus=%s AnyUserFocus=%s MouseCapture=%s"),
        *GetNameSafe(this),
        *InKeyEvent.GetKey().ToString(),
        *LexSlateVisibilityForEXFILUIFlow(GetVisibility()),
        LexBoolForEXFILUIFlow(HasKeyboardFocus()),
        LexBoolForEXFILUIFlow(HasAnyUserFocus()),
        LexBoolForEXFILUIFlow(HasMouseCapture()));

    if (InKeyEvent.GetKey() == EKeys::R && IconOverlay && IconOverlay->RotateActiveDragItem())
    {
        UE_LOG(LogEXFIL, Log,
            TEXT("[UIFlow][Panel NativeOnKeyDown] Handled R rotate. Panel=%s"),
            *GetNameSafe(this));
        return FReply::Handled();
    }

    if (InKeyEvent.GetKey() == EKeys::Tab)
    {
        if (AEXFILPlayerController* PC = Cast<AEXFILPlayerController>(GetOwningPlayer()))
        {
            if (UEXFILUIManager* UIManager = PC->GetUIManager())
            {
                UE_LOG(LogEXFIL, Log,
                    TEXT("[UIFlow][Panel NativeOnKeyDown] Handling Tab close through UIManager. Panel=%s PC=%s UIManager=%s"),
                    *GetNameSafe(this),
                    *GetNameSafe(PC),
                    *GetNameSafe(UIManager));
                UIManager->HideInventory();
                return FReply::Handled();
            }
        }

        UE_LOG(LogEXFIL, Warning,
            TEXT("[UIFlow][Panel NativeOnKeyDown] Tab pressed but UIManager route failed. Panel=%s OwningPlayer=%s"),
            *GetNameSafe(this),
            *GetNameSafe(GetOwningPlayer()));
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
    if (!GridPanel || !ViewModel)
    {
        return;
    }

    const FVector2D GridSize = GridPanel->GetCachedGeometry().GetLocalSize();
    bLayoutReady = (GridSize.X > 1.f && GridSize.Y > 1.f);

    if (!bLayoutReady)
    {
        return;
    }

    const int32 GridW = ViewModel->GetGridWidth();
    const int32 GridH = ViewModel->GetGridHeight();
    const FVector2D NewStride(GridSize.X / GridW, GridSize.Y / GridH);
    const bool bStrideChanged = !CachedCellStride.Equals(NewStride, 0.5f);

    if (bStrideChanged)
    {
        CachedCellStride = NewStride;

        FInventoryOverlayDeltaViewData PendingDelta;
        if (bHasPendingOverlayRefresh &&
            ViewModel->GetPendingOverlayDelta(PendingDelta) &&
            PendingDelta.bFullRefresh)
        {
            FlushOverlayDelta();
        }
        else
        {
            RebuildOverlayFull();
        }
        return;
    }

    FlushOverlayDelta();
}

void UInventoryPanelWidget::FlushOverlayDelta()
{
    if (!bLayoutReady)
    {
        return;
    }
    if (!bHasPendingOverlayRefresh)
    {
        return;
    }
    if (!IconOverlay || !ViewModel || !GridPanel)
    {
        return;
    }

    const int32 GridW = ViewModel->GetGridWidth();
    const int32 GridH = ViewModel->GetGridHeight();

    FInventoryOverlayDeltaViewData Delta;
    if (!ViewModel->GetPendingOverlayDelta(Delta))
    {
        bHasPendingOverlayRefresh = false;
        return;
    }

    if (IconOverlay->RefreshIcons(ViewModel, GridPanel, GridW, GridH, Delta))
    {
        ViewModel->DiscardPendingOverlayDelta();
        bHasPendingOverlayRefresh = false;
    }
}

void UInventoryPanelWidget::RebuildOverlayFull()
{
    if (!bLayoutReady)
    {
        return;
    }
    if (!IconOverlay || !ViewModel || !GridPanel)
    {
        return;
    }

    const int32 GridW = ViewModel->GetGridWidth();
    const int32 GridH = ViewModel->GetGridHeight();
    FInventoryOverlayDeltaViewData Delta;
    if (ViewModel->BuildFullOverlayDelta(Delta))
    {
        if (IconOverlay->RefreshIcons(ViewModel, GridPanel, GridW, GridH, Delta))
        {
            ViewModel->DiscardPendingOverlayDelta();
            bHasPendingOverlayRefresh = false;
        }
    }
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
        const float RawGridPixelWidth = GridPanel->GetCachedGeometry().GetLocalSize().X;
        float StableGridPixelWidth = RawGridPixelWidth;
        float ScrollViewportWidth = -1.f;
        float ReservedScrollbarWidth = 0.f;
        if (InventoryScrollBox)
        {
            ScrollViewportWidth = InventoryScrollBox->GetCachedGeometry().GetLocalSize().X;
            ReservedScrollbarWidth = InventoryScrollBox->GetScrollbarThickness().X;
            if (ScrollViewportWidth > 1.f && ReservedScrollbarWidth > 0.f)
            {
                StableGridPixelWidth = FMath::Max(1.f, ScrollViewportWidth - ReservedScrollbarWidth);
            }
        }
        const float DesiredCellSize = StableGridPixelWidth / GridWidth;

        if (DesiredCellSize > 1.f &&
            (bNeedsCellSquareFix ||
                !FMath::IsNearlyEqual(
                    CachedSquareCellSize,
                    DesiredCellSize,
                    GridSquareCellResizeTolerance)))
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

UScrollBox* UInventoryPanelWidget::ResolveInventoryScrollBox()
{
    if (InventoryScrollBox)
    {
        ConfigureInventoryScrollBox(InventoryScrollBox);
        return InventoryScrollBox;
    }

    if (GridPanel)
    {
        for (UPanelWidget* Parent = GridPanel->GetParent(); Parent; Parent = Parent->GetParent())
        {
            if (UScrollBox* ScrollBox = Cast<UScrollBox>(Parent))
            {
                InventoryScrollBox = ScrollBox;
                ConfigureInventoryScrollBox(InventoryScrollBox);
                return InventoryScrollBox;
            }
        }
    }

    UScrollBox* SingleScrollBox = nullptr;
    int32 ScrollBoxCount = 0;
    if (WidgetTree)
    {
        WidgetTree->ForEachWidget([&SingleScrollBox, &ScrollBoxCount](UWidget* Widget)
        {
            if (UScrollBox* ScrollBox = Cast<UScrollBox>(Widget))
            {
                SingleScrollBox = ScrollBox;
                ++ScrollBoxCount;
            }
        });
    }

    if (ScrollBoxCount == 1 && SingleScrollBox)
    {
        InventoryScrollBox = SingleScrollBox;
        ConfigureInventoryScrollBox(InventoryScrollBox);
        return InventoryScrollBox;
    }

    return nullptr;
}

void UInventoryPanelWidget::ConfigureInventoryScrollBox(UScrollBox* ScrollBox)
{
    if (!ScrollBox)
    {
        return;
    }

    if (bInventoryScrollBoxConfigured)
    {
        return;
    }

    const bool bWasAllowOverscroll = ScrollBox->IsAllowOverscroll();
    const bool bWasAnimateWheelScrolling = ScrollBox->IsAnimateWheelScrolling();

    if (bWasAllowOverscroll)
    {
        ScrollBox->SetAllowOverscroll(false);
    }

    if (bWasAnimateWheelScrolling)
    {
        ScrollBox->SetAnimateWheelScrolling(false);
    }

    ScrollBox->EndInertialScrolling();

    bInventoryScrollBoxConfigured = true;
}

void UInventoryPanelWidget::UpdateDragAutoScroll(
    const FVector2D& ScreenSpacePosition,
    bool bUseItemBounds,
    const FVector2D& ItemTopScreenPosition,
    const FVector2D& ItemBottomScreenPosition)
{
    UScrollBox* ScrollBox = ResolveInventoryScrollBox();
    if (!ScrollBox)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FGeometry& ScrollGeo = ScrollBox->GetCachedGeometry();
    const FVector2D CursorLocalPos = ScrollGeo.AbsoluteToLocal(ScreenSpacePosition);
    const FVector2D ScrollSize = ScrollGeo.GetLocalSize();
    const FVector2D ItemTopLocalPos = bUseItemBounds
        ? ScrollGeo.AbsoluteToLocal(ItemTopScreenPosition)
        : FVector2D::ZeroVector;
    const FVector2D ItemBottomLocalPos = bUseItemBounds
        ? ScrollGeo.AbsoluteToLocal(ItemBottomScreenPosition)
        : FVector2D::ZeroVector;
    const float TopOverflow = bUseItemBounds
        ? FMath::Max(0.f, -ItemTopLocalPos.Y)
        : 0.f;
    const float BottomOverflow = bUseItemBounds
        ? FMath::Max(0.f, ItemBottomLocalPos.Y - ScrollSize.Y)
        : 0.f;
    const float PreviousSpeed = AutoScrollSpeed;
    const int32 PreviousZone = LastAutoScrollZone;
    const bool bWasTimerActive =
        World->GetTimerManager().IsTimerActive(AutoScrollTimerHandle);
    AutoScrollLastUpdateTimeSeconds = FPlatformTime::Seconds();

    int32 NewZone = 0;
    if (TopOverflow > 0.f || BottomOverflow > 0.f)
    {
        if (TopOverflow > BottomOverflow)
        {
            AutoScrollSpeed = -ScrollRate;
            NewZone = -1;
        }
        else
        {
            AutoScrollSpeed = ScrollRate;
            NewZone = 1;
        }
    }
    else if (CursorLocalPos.Y < ScrollEdgeZone)
    {
        AutoScrollSpeed = -ScrollRate;
        NewZone = -1;
    }
    else if (CursorLocalPos.Y > ScrollSize.Y - ScrollEdgeZone)
    {
        AutoScrollSpeed = ScrollRate;
        NewZone = 1;
    }
    else
    {
        AutoScrollSpeed = 0.f;
    }

    const float CurrentScrollOffset = ScrollBox->GetScrollOffset();
    const float MaxScrollOffset = FMath::Max(0.f, ScrollBox->GetScrollOffsetOfEnd());
    if ((AutoScrollSpeed < 0.f && CurrentScrollOffset <= AutoScrollBoundaryTolerance) ||
        (AutoScrollSpeed > 0.f && CurrentScrollOffset >= MaxScrollOffset - AutoScrollBoundaryTolerance))
    {
        AutoScrollSpeed = 0.f;
        NewZone = 0;
    }

    LastAutoScrollZone = NewZone;
    const bool bShouldScrollImmediately =
        AutoScrollSpeed != 0.f &&
        (PreviousZone != NewZone ||
            !FMath::IsNearlyEqual(PreviousSpeed, AutoScrollSpeed) ||
            !bWasTimerActive);

    if (AutoScrollSpeed != 0.f && !bWasTimerActive)
    {
        World->GetTimerManager().SetTimer(
            AutoScrollTimerHandle, this,
            &UInventoryPanelWidget::TickAutoScroll,
            AutoScrollUpdateInterval, true);
    }
    else if (AutoScrollSpeed == 0.f)
    {
        World->GetTimerManager().ClearTimer(AutoScrollTimerHandle);
    }

    if (bShouldScrollImmediately)
    {
        TickAutoScroll();
    }
}

void UInventoryPanelWidget::HandleDragAutoScrollLeave(const FVector2D& ScreenSpacePosition)
{
    UScrollBox* ScrollBox = ResolveInventoryScrollBox();
    if (!ScrollBox || AutoScrollSpeed == 0.f)
    {
        StopDragAutoScroll();
        return;
    }

    const FGeometry& ScrollGeo = ScrollBox->GetCachedGeometry();
    const FVector2D LocalPos = ScrollGeo.AbsoluteToLocal(ScreenSpacePosition);
    const FVector2D ScrollSize = ScrollGeo.GetLocalSize();
    const bool bInsideHorizontalBounds = LocalPos.X >= 0.f && LocalPos.X <= ScrollSize.X;
    const bool bLeavingThroughActiveVerticalEdge =
        bInsideHorizontalBounds &&
        ((AutoScrollSpeed < 0.f && LocalPos.Y < ScrollEdgeZone) ||
            (AutoScrollSpeed > 0.f && LocalPos.Y > ScrollSize.Y - ScrollEdgeZone));

    if (bLeavingThroughActiveVerticalEdge)
    {
        return;
    }

    StopDragAutoScroll();
}

void UInventoryPanelWidget::StopDragAutoScroll()
{
    UWorld* World = GetWorld();

    AutoScrollSpeed = 0.f;
    LastAutoScrollZone = 0;
    AutoScrollLastUpdateTimeSeconds = 0.0;

    if (World)
    {
        World->GetTimerManager().ClearTimer(AutoScrollTimerHandle);
    }
}

void UInventoryPanelWidget::TickAutoScroll()
{
    UScrollBox* ScrollBox = ResolveInventoryScrollBox();
    if (!ScrollBox)
    {
        StopDragAutoScroll();
        return;
    }

    if (AutoScrollSpeed == 0.f)
    {
        StopDragAutoScroll();
        return;
    }

    const double NowSeconds = FPlatformTime::Seconds();
    const double SecondsSinceLastUpdate = AutoScrollLastUpdateTimeSeconds > 0.0
        ? NowSeconds - AutoScrollLastUpdateTimeSeconds
        : 0.0;

    if (AutoScrollLastUpdateTimeSeconds <= 0.0 ||
        SecondsSinceLastUpdate > AutoScrollStaleUpdateTimeout)
    {
        StopDragAutoScroll();
        return;
    }

    const float CurrentOffset = ScrollBox->GetScrollOffset();
    const float MaxOffset = FMath::Max(0.f, ScrollBox->GetScrollOffsetOfEnd());
    const float RequestedOffset = CurrentOffset + AutoScrollSpeed;
    const float ClampedOffset = FMath::Clamp(RequestedOffset, 0.f, MaxOffset);

    if (FMath::IsNearlyEqual(CurrentOffset, ClampedOffset, AutoScrollBoundaryTolerance) &&
        !FMath::IsNearlyEqual(RequestedOffset, ClampedOffset, AutoScrollBoundaryTolerance))
    {
        StopDragAutoScroll();
        return;
    }

    ScrollBox->SetScrollOffset(ClampedOffset);
}
