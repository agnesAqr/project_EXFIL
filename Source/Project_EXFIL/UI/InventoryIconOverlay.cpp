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
#include "UI/InventoryDragPreviewWidget.h"
#include "UI/InventoryPanelWidget.h"
#include "Inventory/InventoryComponent.h"
#include "Data/Equipment/EquipmentComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UInventoryIconOverlay::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    // 마우스 이벤트(우클릭) 수신을 위해 Visible 필수
    // HitTestInvisible/SelfHitTestInvisible 이면 NativeOnMouseButtonDown이 호출되지 않음
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
                                          int32 InGridWidth, int32 InGridHeight)
{
    // 우클릭 HitTest용 캐시 갱신
    CachedViewModel  = InViewModel;
    CachedGridPanel  = InGridPanel;
    CachedGridWidth  = InGridWidth;
    CachedGridHeight = InGridHeight;

    if (!IconCanvas || !InViewModel || !InGridPanel)
    {
        return;
    }

    IconCanvas->ClearChildren();

    // 첫 번째 슬롯(0,0)의 geometry에서 슬롯 크기 확보
    UWidget* FirstSlot = InGridPanel->GetChildAt(0);
    if (!FirstSlot)
    {
        UE_LOG(LogTemp, Error, TEXT("RefreshIcons: FirstSlot null — GridPanel 자식 없음"));
        return;
    }

    const FVector2D SlotSize = FirstSlot->GetCachedGeometry().GetLocalSize();
    const FVector2D GridLocalSize = InGridPanel->GetCachedGeometry().GetLocalSize();
    const FVector2D CellStride(GridLocalSize.X / InGridWidth, GridLocalSize.Y / InGridHeight);

    UE_LOG(LogTemp, Warning, TEXT("RefreshIcons: GridLocalSize=%s CellStride=%s"),
        *GridLocalSize.ToString(), *CellStride.ToString());

    // geometry가 아직 0이면 배치 의미 없음 (RefreshIconOverlay 타이머가 재호출함)
    if (GridLocalSize.IsNearlyZero())
    {
        return;
    }
    const FVector2D Origin = FVector2D::ZeroVector;
    const FVector2D LocalStride = CellStride;


    TArray<UInventorySlotViewModel*> AllSlots = InViewModel->GetAllSlots();
    for (UInventorySlotViewModel* SlotVM : AllSlots)
    {
        if (!SlotVM || SlotVM->IsEmpty() || !SlotVM->IsRootSlot())
        {
            continue;
        }

        const FIntPoint GridPos = SlotVM->GetGridPosition();
        const FVector2D IconPos(
            Origin.X + GridPos.X * LocalStride.X,
            Origin.Y + GridPos.Y * LocalStride.Y);
        const FVector2D IconSize(
            SlotVM->GetItemSizeX() * CellStride.X,
            SlotVM->GetItemSizeY() * CellStride.Y);

        // 아이콘 이미지 — 텍스처가 있을 때만 생성
        const TSoftObjectPtr<UTexture2D> IconPtr = SlotVM->GetIcon();
        if (!IconPtr.IsNull())
        {
            UTexture2D* IconTex = IconPtr.LoadSynchronous();
            if (IconTex)
            {
                UImage* IconImage = NewObject<UImage>(this);
                IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);

                FSlateBrush Brush;
                Brush.SetResourceObject(IconTex);
                Brush.DrawAs = ESlateBrushDrawType::Image;
                IconImage->SetBrush(Brush);

                // 텍스처 원본 비율 유지하며 슬롯에 맞춤 (ScaleToFit + 중앙 정렬)
                const FVector2D TexSize(IconTex->GetSizeX(), IconTex->GetSizeY());
                const float Scale = FMath::Min(IconSize.X / TexSize.X, IconSize.Y / TexSize.Y);
                const FVector2D FinalSize = TexSize * Scale;
                const FVector2D Offset = (IconSize - FinalSize) * 0.5f;

                UCanvasPanelSlot* CanvasSlot = IconCanvas->AddChildToCanvas(IconImage);
                if (CanvasSlot)
                {
                    CanvasSlot->SetPosition(IconPos + Offset);
                    CanvasSlot->SetSize(FinalSize);
                    CanvasSlot->SetAutoSize(false);
                }
            }
        }

        // 스택 카운트 텍스트 (2 이상일 때만 표시, 텍스처 유무와 무관)
        if (SlotVM->GetStackCount() > 1)
        {
            UTextBlock* StackText = NewObject<UTextBlock>(this);
            StackText->SetVisibility(ESlateVisibility::HitTestInvisible);
            StackText->SetText(FText::AsNumber(SlotVM->GetStackCount()));

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
        }
    }

    // 자식 추가 후 명시적 Invalidate — 렌더 트리거 보장 (protected이므로 내부에서 호출)
    Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

// ========== 우클릭 컨텍스트 메뉴 ==========

FReply UInventoryIconOverlay::NativeOnMouseButtonDown(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

    // ── 우클릭: 컨텍스트 메뉴 ────────────────────────────────────────────────
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        FGuid HitInstanceID;
        FName HitItemDataID;
        const bool bHitItem = FindItemAtPosition(LocalPos, HitInstanceID, HitItemDataID);

        // 메뉴가 열려있으면 무조건 먼저 닫기
        if (ContextMenuWidget &&
            ContextMenuWidget->GetVisibility() == ESlateVisibility::Visible)
        {
            ContextMenuWidget->CloseMenu();
        }

        // 아이템 위 → 해당 아이템 메뉴 열기 (기존 메뉴가 닫힌 직후 포함)
        if (bHitItem)
        {
            UItemContextMenuWidget* Menu = GetOrCreateContextMenu();
            if (Menu)
            {
                Menu->ShowForInventoryItem(HitInstanceID, HitItemDataID);
                Menu->SetMenuPosition(InMouseEvent.GetScreenSpacePosition());
            }
        }
        // 빈 공간 → 닫기만 하고 끝 (위에서 이미 CloseMenu 호출됨)

        return FReply::Handled();
    }

    // ── 좌클릭: 드래그 감지 ──────────────────────────────────────────────────
    // InventoryIconOverlay가 Visible이므로 아래 SlotWidget은 클릭을 받지 못함.
    // Overlay가 직접 드래그를 시작해야 SlotWidget의 NativeOnDrop 체인이 동작.
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        FGuid HitInstanceID;
        FName HitItemDataID;
        const bool bHit = FindItemAtPosition(LocalPos, HitInstanceID, HitItemDataID);
        UE_LOG(LogTemp, Warning, TEXT("IconOverlay LClick: LocalPos=%s bHit=%d InstanceID=%s"),
            *LocalPos.ToString(), bHit ? 1 : 0, *HitInstanceID.ToString());

        if (bHit)
        {
            PendingDragInstanceID    = HitInstanceID;
            PendingDragClickLocalPos = LocalPos; // DragOffset 계산용 클릭 좌표 캐시
            // TakeWidget(): UWidget → SWidget 변환. GetCachedWidget()과 달리
            // 위젯이 아직 빌드되지 않았을 때도 빌드를 강제하여 유효한 레퍼런스 반환.
            return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventoryIconOverlay::NativeOnDragDetected(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    UE_LOG(LogTemp, Warning, TEXT("IconOverlay NativeOnDragDetected: PendingID=%s"),
        *PendingDragInstanceID.ToString());

    if (!PendingDragInstanceID.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("IconOverlay NativeOnDragDetected: PendingDragInstanceID 무효 — 드래그 취소"));
        return;
    }

    // InventoryComponent에서 실제 아이템 인스턴스 조회 (회전/크기 정보 포함)
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

    UInventoryDragDropOp* DragOp = NewObject<UInventoryDragDropOp>(this);
    DragOp->DraggedItemInstanceID = ItemInstance.InstanceID;
    DragOp->ItemDataID             = ItemInstance.ItemDataID;
    DragOp->OriginalPosition       = ItemInstance.RootPosition;
    DragOp->ItemSize               = ItemInstance.ItemSize;
    DragOp->DragItemSize           = ItemInstance.GetEffectiveSize();
    DragOp->bWasRotated            = ItemInstance.bIsRotated;
    DragOp->bFromEquipment         = false;
    DragOp->Pivot                  = EDragPivot::TopLeft;

    // DragOffset: 클릭한 셀 - 아이템 루트 위치 (비루트 셀 클릭 시 자연스러운 배치)
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
            // 아이템 크기 내로 클램프
            const FItemSize EffSize = ItemInstance.GetEffectiveSize();
            Offset.X = FMath::Clamp(Offset.X, 0, EffSize.Width  - 1);
            Offset.Y = FMath::Clamp(Offset.Y, 0, EffSize.Height - 1);
            DragOp->DragOffset = Offset;
        }
    }

    // 드래그 비주얼 — 아이템 크기에 맞는 그리드 프리뷰 (PC는 위에서 이미 선언됨)
    if (PC)
    {
        UInventoryDragPreviewWidget* DragVisual =
            CreateWidget<UInventoryDragPreviewWidget>(PC, UInventoryDragPreviewWidget::StaticClass());
        if (DragVisual)
        {
            DragVisual->BuildPreview(DragOp->DragItemSize.Width, DragOp->DragItemSize.Height);
        }
        DragOp->DefaultDragVisual = DragVisual;
    }

    OutOperation = DragOp;
    PendingDragInstanceID.Invalidate();

    UE_LOG(LogTemp, Warning, TEXT("IconOverlay DragOp 생성: '%s' DragOffset=(%d,%d)"),
        *ItemInstance.ItemDataID.ToString(),
        DragOp->DragOffset.X, DragOp->DragOffset.Y);
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

    // ViewModel의 루트 슬롯을 순회하여 바운드 박스 검사
    for (UInventorySlotViewModel* SlotVM : CachedViewModel->GetAllSlots())
    {
        if (!SlotVM || SlotVM->IsEmpty() || !SlotVM->IsRootSlot()) continue;

        const FIntPoint GridPos  = SlotVM->GetGridPosition();
        const FVector2D TopLeft(GridPos.X * CellStride.X, GridPos.Y * CellStride.Y);
        const FVector2D Size(SlotVM->GetItemSizeX() * CellStride.X,
                              SlotVM->GetItemSizeY() * CellStride.Y);
        const FVector2D BottomRight = TopLeft + Size;

        if (LocalPos.X >= TopLeft.X && LocalPos.X <= BottomRight.X &&
            LocalPos.Y >= TopLeft.Y && LocalPos.Y <= BottomRight.Y)
        {
            OutInstanceID = SlotVM->GetItemInstanceID();
            OutItemDataID = SlotVM->GetItemDataID();
            return true;
        }
    }

    return false;
}

// ========== ParentPanel 주입 ==========

void UInventoryIconOverlay::SetParentPanel(UInventoryPanelWidget* InPanel)
{
    ParentPanel = InPanel;
}

// ========== 드롭 / 하이라이트 ==========

/** 로컬 좌표 → 그리드 셀 인덱스 변환 헬퍼 */
static FIntPoint LocalPosToGridCell(const FVector2D& LocalPos,
    const FVector2D& GridLocalSize, int32 GridW, int32 GridH)
{
    const FVector2D CellStride(GridLocalSize.X / GridW, GridLocalSize.Y / GridH);
    return FIntPoint(FMath::FloorToInt(LocalPos.X / CellStride.X),
                     FMath::FloorToInt(LocalPos.Y / CellStride.Y));
}

bool UInventoryIconOverlay::NativeOnDrop(const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UInventoryDragDropOp* DragOp = Cast<UInventoryDragDropOp>(InOperation);
    if (!DragOp) return false;

    if (ParentPanel.IsValid())
    {
        ParentPanel->ClearAreaHighlights();
    }

    // ── 장비슬롯에서 온 드래그 → 해제 + 인벤토리 복귀 ──
    if (DragOp->bFromEquipment)
    {
        APlayerController* PC = GetOwningPlayer();
        APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        UEquipmentComponent* EquipComp = Pawn
            ? Pawn->FindComponentByClass<UEquipmentComponent>() : nullptr;
        if (!EquipComp) return false;
        EquipComp->Server_UnequipToInventory(DragOp->SourceEquipmentSlot);
        return true;
    }

    // ── 인벤토리 내부 이동 ──
    if (!ParentPanel.IsValid() || !CachedGridPanel.IsValid()
        || CachedGridWidth <= 0 || CachedGridHeight <= 0)
    {
        return false;
    }

    const FVector2D GridLocalSize = CachedGridPanel->GetCachedGeometry().GetLocalSize();
    if (GridLocalSize.IsNearlyZero()) return false;

    const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition());
    const FIntPoint DroppedCell = LocalPosToGridCell(LocalPos, GridLocalSize,
                                                       CachedGridWidth, CachedGridHeight);
    const FIntPoint NewRootPos = DroppedCell - DragOp->DragOffset;

    if (NewRootPos == DragOp->OriginalPosition)
    {
        return false; // 같은 위치 드롭 무시
    }

    UE_LOG(LogTemp, Warning, TEXT("IconOverlay NativeOnDrop: NewRoot=(%d,%d)"),
        NewRootPos.X, NewRootPos.Y);

    return ParentPanel->ForwardMoveRequest(
        DragOp->DraggedItemInstanceID, NewRootPos, DragOp->bWasRotated);
}

bool UInventoryIconOverlay::NativeOnDragOver(const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UInventoryDragDropOp* DragOp = Cast<UInventoryDragDropOp>(InOperation);
    if (!DragOp || !ParentPanel.IsValid()
        || !CachedViewModel || !CachedGridPanel.IsValid()
        || CachedGridWidth <= 0 || CachedGridHeight <= 0)
    {
        return false;
    }

    const FVector2D GridLocalSize = CachedGridPanel->GetCachedGeometry().GetLocalSize();
    if (GridLocalSize.IsNearlyZero()) return false;

    const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition());
    const FIntPoint DroppedCell = LocalPosToGridCell(LocalPos, GridLocalSize,
                                                       CachedGridWidth, CachedGridHeight);
    const FIntPoint RootPos = DroppedCell - DragOp->DragOffset;

    const int32 GridW = CachedViewModel->GetGridWidth();
    const int32 GridH = CachedViewModel->GetGridHeight();

    // 경계 초과 시 하이라이트 없이 리턴
    if (RootPos.X < 0 || RootPos.Y < 0 ||
        RootPos.X + DragOp->DragItemSize.Width  > GridW ||
        RootPos.Y + DragOp->DragItemSize.Height > GridH)
    {
        ParentPanel->ClearAreaHighlights();
        return true;
    }

    // 이전 하이라이트 초기화 후 새로 적용
    ParentPanel->ClearAreaHighlights();

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

    ParentPanel->HighlightArea(RootPos, DragOp->DragItemSize, bCanPlace);
    return true;
}

void UInventoryIconOverlay::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    if (ParentPanel.IsValid())
    {
        ParentPanel->ClearAreaHighlights();
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
            ContextMenuWidget->AddToViewport(100); // 높은 ZOrder로 항상 최상단
            ContextMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    return ContextMenuWidget;
}
