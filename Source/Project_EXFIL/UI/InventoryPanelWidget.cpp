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
#include "UI/InventorySlotWidget.h"
#include "UI/InventoryIconOverlay.h"
#include "UI/CraftingPanelWidget.h"
#include "UI/StatEntryWidget.h"
#include "GAS/SurvivalViewModel.h"
#include "Input/CommonUIInputTypes.h"
#include "Components/ScrollBox.h"

void UInventoryPanelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // I키/ESC키 입력을 받기 위해 포커스 가능하게 설정
    SetIsFocusable(true);

    // 탭 버튼 바인딩
    if (Button_InventoryTab)
    {
        Button_InventoryTab->OnClicked.AddDynamic(this, &UInventoryPanelWidget::OnInventoryTabClicked);
    }
    if (Button_CraftingTab)
    {
        Button_CraftingTab->OnClicked.AddDynamic(this, &UInventoryPanelWidget::OnCraftingTabClicked);
    }

    // 기본: 인벤토리 탭 활성
    UpdateTabStyles(0);
}

void UInventoryPanelWidget::SetViewModel(UInventoryViewModel* InViewModel)
{
    // 기존 델리게이트 해제
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

void UInventoryPanelWidget::NativeOnActivated()
{
    Super::NativeOnActivated();

    // 인벤토리 열 때마다 셀 크기 재계산 + 레이아웃 상태 리셋 (해상도/창 크기 변경 대응)
    bNeedsCellSquareFix = true;
    bLayoutReady = false;
    CachedCellStride = FVector2D::ZeroVector;
    CachedSquareCellSize = 0.f;
}

void UInventoryPanelWidget::NativeOnDeactivated()
{
    Super::NativeOnDeactivated();
}

bool UInventoryPanelWidget::NativeOnHandleBackAction()
{
    // ESC는 PIE를 종료시키므로 CommonUI BackAction 비활성화
    return false;
}

FReply UInventoryPanelWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    // 패널 아무 곳이나 클릭하면 열려있는 컨텍스트 메뉴 닫기
    if (IconOverlay)
    {
        IconOverlay->CloseContextMenuIfOpen();
    }
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // 빈 영역 드래그가 게임 입력으로 새지 않도록 패널이 직접 마우스를 소비한다.
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

            // UniformGridPanel: Column = X, Row = Y
            UUniformGridSlot* GridSlot = GridPanel->AddChildToUniformGrid(SlotWidget, Y, X);
            GridSlot->SetHorizontalAlignment(HAlign_Fill);
            GridSlot->SetVerticalAlignment(VAlign_Fill);
            SlotWidgets.Add(SlotWidget);
        }
    }

    // IconOverlay에 ParentPanel 주입 — 드롭 라우팅 및 하이라이트용
    if (IconOverlay)
    {
        IconOverlay->SetParentPanel(this);
    }

    // 초기 데이터를 pending에 등록 — NativePaint에서 레이아웃 확정 후 flush
    const int32 Total = Width * Height;
    PendingDirtyIndices.Reserve(Total);
    for (int32 i = 0; i < Total; ++i)
    {
        PendingDirtyIndices.Add(i);
    }
    bHasPendingOverlayRefresh = true;
}

bool UInventoryPanelWidget::ForwardMoveRequest(FGuid ItemInstanceID, FIntPoint NewPosition)
{
    if (!ViewModel)
    {
        return false;
    }
    ViewModel->RequestMoveItem(ItemInstanceID, NewPosition);
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
            // 범위 밖 좌표는 스킵 (음수이거나 그리드 초과)
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

// ─── Deferred Overlay Refresh ────────────────────────────────────────────────

void UInventoryPanelWidget::HandleViewModelRefreshed(const TSet<int32>& DirtyIndices)
{
    (void)DirtyIndices;

    PendingDirtyIndices.Empty();
    if (ViewModel)
    {
        const int32 Total = ViewModel->GetGridWidth() * ViewModel->GetGridHeight();
        PendingDirtyIndices.Reserve(Total);
        for (int32 i = 0; i < Total; ++i)
        {
            PendingDirtyIndices.Add(i);
        }
    }

    bHasPendingOverlayRefresh = true;
    TryFlushOverlayRefresh(true);
}

void UInventoryPanelWidget::HandleLayoutMeasured(const FGeometry& AllottedGeometry)
{
    if (!GridPanel || !ViewModel) return;

    // layout 유효성 판정 — GridPanel의 실제 geometry가 유효한지
    const FVector2D GridSize = GridPanel->GetCachedGeometry().GetLocalSize();
    bLayoutReady = (GridSize.X > 1.f && GridSize.Y > 1.f);

    if (!bLayoutReady) return;

    // stride 변경 감지 (창 리사이즈, 셀 정사각형 보정 후 등)
    const int32 GridW = ViewModel->GetGridWidth();
    const int32 GridH = ViewModel->GetGridHeight();
    const FVector2D NewStride(GridSize.X / GridW, GridSize.Y / GridH);
    const bool bStrideChanged = !CachedCellStride.Equals(NewStride, 0.5f);

    if (bStrideChanged)
    {
        CachedCellStride = NewStride;
        TryFlushOverlayRefresh(true);
        return;
    }

    TryFlushOverlayRefresh(false);
}

void UInventoryPanelWidget::TryFlushOverlayRefresh(bool bForceFull)
{
    if (!bLayoutReady) return;
    if (!bForceFull && !bHasPendingOverlayRefresh) return;
    if (!IconOverlay || !ViewModel || !GridPanel) return;

    const int32 GridW = ViewModel->GetGridWidth();
    const int32 GridH = ViewModel->GetGridHeight();

    if (bForceFull)
    {
        // 전체 슬롯 갱신 (stride 변경 → 모든 아이콘 좌표 재계산)
        TSet<int32> AllIndices;
        const int32 Total = GridW * GridH;
        AllIndices.Reserve(Total);
        for (int32 i = 0; i < Total; ++i)
        {
            AllIndices.Add(i);
        }
        IconOverlay->RefreshIcons(ViewModel, GridPanel, GridW, GridH, AllIndices);
    }
    else
    {
        // dirty 슬롯만 갱신
        IconOverlay->RefreshIcons(ViewModel, GridPanel, GridW, GridH, PendingDirtyIndices);
    }

    PendingDirtyIndices.Empty();
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

    // 셀 정사각형 보정 — 레이아웃 첫 유효 시점에 1회 실행
    // SetMinDesiredSlotHeight가 레이아웃을 무효화하므로 이번 프레임은 skip
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
            // 다음 프레임에 geometry가 확정된 후 HandleLayoutMeasured에서 처리
            return Result;
        }
    }

    // geometry 확정 후 레이아웃 측정 → flush 시도
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
    if (StatEntry_HP) StatEntry_HP->BindToViewModel(InSurvivalViewModel, FName("Health"));
    if (StatEntry_HU) StatEntry_HU->BindToViewModel(InSurvivalViewModel, FName("Hunger"));
    if (StatEntry_TH) StatEntry_TH->BindToViewModel(InSurvivalViewModel, FName("Thirst"));
    if (StatEntry_ST) StatEntry_ST->BindToViewModel(InSurvivalViewModel, FName("Stamina"));
}

// ─── 탭 전환 ──────────────────────────────────────────────────────────────────

void UInventoryPanelWidget::OnInventoryTabClicked()
{
    if (WidgetSwitcher_Content)
    {
        WidgetSwitcher_Content->SetActiveWidgetIndex(0);
    }
    UpdateTabStyles(0);
}

void UInventoryPanelWidget::OnCraftingTabClicked()
{
    if (WidgetSwitcher_Content)
    {
        WidgetSwitcher_Content->SetActiveWidgetIndex(1);
    }
    UpdateTabStyles(1);

    // 크래프팅 패널 레시피 목록 갱신
    if (CraftingPanel)
    {
        CraftingPanel->RefreshRecipeList();
    }
}

void UInventoryPanelWidget::UpdateTabStyles(int32 ActiveIndex)
{
    // 활성: (1,1,1,0.8) / 비활성: (1,1,1,0.35)
    // 버튼 스타일 변경은 WBP에서 Dynamic Material 또는 SetColorAndOpacity로 처리.
    // C++에서는 TextBlock 색상으로 간단히 구분.

    // 탭 버튼에 접근해 자식 TextBlock 색상 변경
    auto ApplyTabColor = [](UButton* Btn, FLinearColor Color)
    {
        if (!Btn) return;
        // 버튼 첫 번째 자식 TextBlock 탐색
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

// ─── 드래그 자동 스크롤 ──────────────────────────────────────────────────────

void UInventoryPanelWidget::UpdateDragAutoScroll(const FVector2D& ScreenSpacePosition)
{
    if (!InventoryScrollBox) return;

    const FGeometry& ScrollGeo = InventoryScrollBox->GetCachedGeometry();
    const FVector2D LocalPos = ScrollGeo.AbsoluteToLocal(ScreenSpacePosition);
    const FVector2D ScrollSize = ScrollGeo.GetLocalSize();

    // 상단 가장자리
    if (LocalPos.Y < ScrollEdgeZone)
    {
        AutoScrollSpeed = -ScrollRate;
    }
    // 하단 가장자리
    else if (LocalPos.Y > ScrollSize.Y - ScrollEdgeZone)
    {
        AutoScrollSpeed = ScrollRate;
    }
    else
    {
        AutoScrollSpeed = 0.f;
    }

    // 타이머 시작 (이미 돌고 있으면 무시)
    if (AutoScrollSpeed != 0.f && !GetWorld()->GetTimerManager().IsTimerActive(AutoScrollTimerHandle))
    {
        GetWorld()->GetTimerManager().SetTimer(
            AutoScrollTimerHandle, this,
            &UInventoryPanelWidget::TickAutoScroll,
            0.016f, true); // ~60fps
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
