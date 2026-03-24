// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventoryPanelWidget.h"
#include "CoreMinimal.h"
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
            this, &UInventoryPanelWidget::RefreshIconOverlay);
        BuildGrid();
    }
}

void UInventoryPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetKeyboardFocus();
}

void UInventoryPanelWidget::NativeOnActivated()
{
    Super::NativeOnActivated();

    // 인벤토리 열 때마다 셀 크기 재계산부터 시작 (해상도/창 크기 변경 대응)
    bNeedsCellSquareFix = true;
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
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UInventoryPanelWidget::NativeOnKeyDown(const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent)
{
    // I키 또는 ESC키로 닫기
    if (InKeyEvent.GetKey() == EKeys::I || InKeyEvent.GetKey() == EKeys::Escape)
    {
        RemoveFromParent();

        APlayerController* PC = GetOwningPlayer();
        if (PC)
        {
            FInputModeGameOnly InputMode;
            PC->SetInputMode(InputMode);
            PC->bShowMouseCursor = false;
        }

        return FReply::Handled();
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

    // 초기 아이콘 오버레이 갱신
    RefreshIconOverlay();
}

bool UInventoryPanelWidget::ForwardMoveRequest(FGuid ItemInstanceID, FIntPoint NewPosition,
                                                bool bNewRotated)
{
    if (!ViewModel)
    {
        return false;
    }
    return ViewModel->RequestMoveItem(ItemInstanceID, NewPosition, bNewRotated);
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

void UInventoryPanelWidget::RefreshIconOverlay()
{
    if (!IconOverlay || !ViewModel || !GridPanel)
    {
        return;
    }

    // GridPanel geometry가 아직 0이면 (패널이 닫혀있거나 첫 프레임)
    // → 타이머로 재시도해서 유효한 geometry 확보 후 아이콘 배치
    const float CellWidth = GridPanel->GetCachedGeometry().GetLocalSize().X;
    if (CellWidth <= 1.f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            IconRefreshTimerHandle,
            [this]()
            {
                if (IsValid(this))
                {
                    RefreshIconOverlay();
                }
            },
            0.15f, false);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("RefreshIconOverlay: 실행 (CellWidth=%.1f)"), CellWidth);
    IconOverlay->RefreshIcons(ViewModel, GridPanel,
        ViewModel->GetGridWidth(), ViewModel->GetGridHeight());
}

int32 UInventoryPanelWidget::NativePaint(const FPaintArgs& Args,
    const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId,
    const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    int32 Result = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
                                       OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    // 셀 정사각형 보정만 담당 — 아이콘 배치는 타이머로 분리
    if (bNeedsCellSquareFix && GridPanel && GridPanel->GetChildrenCount() > 0)
    {
        UWidget* FirstSlot = GridPanel->GetChildAt(0);
        const float CellWidth = FirstSlot->GetCachedGeometry().GetLocalSize().X;

        if (CellWidth > 1.f)
        {
            auto* MutableThis = const_cast<UInventoryPanelWidget*>(this);
            MutableThis->bNeedsCellSquareFix = false;

            MutableThis->GridPanel->SetMinDesiredSlotHeight(CellWidth);
            MutableThis->GridPanel->SetMinDesiredSlotWidth(0.f);

            // 0.1초 후 아이콘 배치 — 레이아웃이 완전히 끝난 후
            MutableThis->GetWorld()->GetTimerManager().SetTimer(
                MutableThis->IconRefreshTimerHandle,
                [MutableThis]()
                {
                    if (IsValid(MutableThis))
                    {
                        MutableThis->RefreshIconOverlay();
                    }
                },
                0.1f, false);
        }
    }

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
    UE_LOG(LogTemp, Warning, TEXT("[STAT-3] BindStats: HP=%s, HU=%s, TH=%s, ST=%s"),
        StatEntry_HP ? TEXT("valid") : TEXT("null"),
        StatEntry_HU ? TEXT("valid") : TEXT("null"),
        StatEntry_TH ? TEXT("valid") : TEXT("null"),
        StatEntry_ST ? TEXT("valid") : TEXT("null"));

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
