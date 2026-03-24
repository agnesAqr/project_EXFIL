// Copyright Project EXFIL. All Rights Reserved.

#include "UI/EquipmentSlotWidget.h"
#include "CoreMinimal.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blueprint/DragDropOperation.h"
#include "UI/InventoryDragDropOp.h"
#include "UI/ItemContextMenuWidget.h"
#include "Data/Equipment/EquipmentComponent.h"
#include "Data/EXFILItemTypes.h"
#include "Data/ItemDataSubsystem.h"
#include "Engine/GameInstance.h"

void UEquipmentSlotWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // SlotLabel 자동 설정
    static const TMap<EEquipmentSlot, FText> SlotLabels =
    {
        { EEquipmentSlot::Head,    NSLOCTEXT("Equip", "Head",    "HEAD")     },
        { EEquipmentSlot::Face,    NSLOCTEXT("Equip", "Face",    "FACE")     },
        { EEquipmentSlot::Eyewear, NSLOCTEXT("Equip", "Eye",     "EYEWEAR")  },
        { EEquipmentSlot::Body,    NSLOCTEXT("Equip", "Body",    "BODY")     },
        { EEquipmentSlot::Weapon1, NSLOCTEXT("Equip", "W1",      "WEAPON 1") },
        { EEquipmentSlot::Weapon2, NSLOCTEXT("Equip", "W2",      "WEAPON 2") },
    };

    if (TextBlock_SlotLabel)
    {
        if (const FText* Label = SlotLabels.Find(SlotType))
        {
            TextBlock_SlotLabel->SetText(*Label);
        }
    }

    ApplyEmptyStyle();
}

void UEquipmentSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // OwningPlayer → Pawn → EquipmentComponent 자동 바인딩
    APlayerController* PC = GetOwningPlayer();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (Pawn)
    {
        UEquipmentComponent* EquipComp = Pawn->FindComponentByClass<UEquipmentComponent>();
        if (EquipComp && !BoundEquipComp.IsValid())
        {
            BoundEquipComp = EquipComp;
            EquipComp->OnItemEquipped.AddDynamic(this, &UEquipmentSlotWidget::OnEquipmentItemEquipped);
            EquipComp->OnItemUnequipped.AddDynamic(this, &UEquipmentSlotWidget::OnEquipmentItemUnequipped);

            // 초기 상태 반영
            RefreshFromEquipmentComponent();
        }
    }
}

void UEquipmentSlotWidget::NativeDestruct()
{
    // 델리게이트 해제
    if (BoundEquipComp.IsValid())
    {
        BoundEquipComp->OnItemEquipped.RemoveDynamic(this, &UEquipmentSlotWidget::OnEquipmentItemEquipped);
        BoundEquipComp->OnItemUnequipped.RemoveDynamic(this, &UEquipmentSlotWidget::OnEquipmentItemUnequipped);
        BoundEquipComp.Reset();
    }

    Super::NativeDestruct();
}

void UEquipmentSlotWidget::OnEquipmentItemEquipped(EEquipmentSlot InSlot, const FInventoryItemInstance& Item)
{
    if (InSlot == SlotType)
    {
        RefreshFromEquipmentComponent();
    }
}

void UEquipmentSlotWidget::OnEquipmentItemUnequipped(EEquipmentSlot InSlot, const FInventoryItemInstance& Item)
{
    if (InSlot == SlotType)
    {
        // 빈 슬롯으로 갱신
        FEquipmentSlotData EmptyData(SlotType);
        RefreshSlot(EmptyData);
    }
}

void UEquipmentSlotWidget::RefreshFromEquipmentComponent()
{
    if (!BoundEquipComp.IsValid())
    {
        return;
    }

    FInventoryItemInstance EquippedItem;
    if (BoundEquipComp->GetEquippedItem(SlotType, EquippedItem))
    {
        FEquipmentSlotData Data(SlotType);
        Data.EquippedItemID = EquippedItem.InstanceID;
        Data.ItemInstance = EquippedItem;
        RefreshSlot(Data);
    }
    else
    {
        FEquipmentSlotData EmptyData(SlotType);
        RefreshSlot(EmptyData);
    }
}

void UEquipmentSlotWidget::RefreshSlot(const FEquipmentSlotData& SlotData)
{
    CachedSlotData = SlotData;

    if (!SlotData.IsEmpty())
    {
        // 아이콘 텍스처 로드 및 표시
        if (Image_ItemIcon)
        {
            UItemDataSubsystem* Sub = nullptr;
            if (UWorld* World = GetWorld())
            {
                if (UGameInstance* GI = World->GetGameInstance())
                {
                    Sub = GI->GetSubsystem<UItemDataSubsystem>();
                }
            }
            if (Sub)
            {
                const FItemData* ItemData = Sub->GetItemData(SlotData.ItemInstance.ItemDataID);
                if (ItemData && !ItemData->Icon.IsNull())
                {
                    UTexture2D* IconTexture = ItemData->Icon.LoadSynchronous();
                    if (IconTexture)
                    {
                        Image_ItemIcon->SetBrushFromTexture(IconTexture, true);
                    }
                }
            }
            Image_ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        // 아이템 이름 표시
        if (TextBlock_ItemName)
        {
            TextBlock_ItemName->SetText(FText::FromName(SlotData.ItemInstance.ItemDataID));
            TextBlock_ItemName->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        ApplyEquippedStyle();
    }
    else
    {
        if (Image_ItemIcon)
        {
            Image_ItemIcon->SetBrushFromTexture(nullptr);
            Image_ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (TextBlock_ItemName)
        {
            TextBlock_ItemName->SetVisibility(ESlateVisibility::Collapsed);
        }
        ApplyEmptyStyle();
    }
}

void UEquipmentSlotWidget::SetDragHighlight(bool bVisible, bool bIsValid)
{
    if (bVisible)
    {
        ApplyDragHoverStyle(bIsValid);
    }
    else
    {
        if (!CachedSlotData.IsEmpty())
        {
            ApplyEquippedStyle();
        }
        else
        {
            ApplyEmptyStyle();
        }
    }
}

// ─── 스타일 헬퍼 ───────────────────────────────────────────────────────────────

void UEquipmentSlotWidget::ApplyEmptyStyle()
{
    if (!Border_Slot)
    {
        return;
    }
    // Background: (0.08, 0.08, 0.08, 1.0)
    FLinearColor BgColor(0.08f, 0.08f, 0.08f, 1.0f);
    FSlateBrush Brush = Border_Slot->GetContentColorAndOpacity() == FLinearColor::White
        ? FSlateBrush()
        : FSlateBrush();
    Brush.TintColor = FSlateColor(BgColor);
    Border_Slot->SetBrushColor(BgColor);
}

void UEquipmentSlotWidget::ApplyEquippedStyle()
{
    if (!Border_Slot)
    {
        return;
    }
    // Background: (0.05, 0.25, 0.15, 1.0)
    Border_Slot->SetBrushColor(FLinearColor(0.05f, 0.25f, 0.15f, 1.0f));
}

void UEquipmentSlotWidget::ApplyDragHoverStyle(bool bIsValid)
{
    if (!Border_Slot)
    {
        return;
    }
    if (bIsValid)
    {
        // (0.08, 0.18, 0.3, 1.0)
        Border_Slot->SetBrushColor(FLinearColor(0.08f, 0.18f, 0.3f, 1.0f));
    }
    else
    {
        // (0.25, 0.08, 0.08, 1.0)
        Border_Slot->SetBrushColor(FLinearColor(0.25f, 0.08f, 0.08f, 1.0f));
    }
}

// ─── 드래그앤드롭 ──────────────────────────────────────────────────────────────

FReply UEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                                      const FPointerEvent& InMouseEvent)
{
    // 우클릭: 메뉴가 열려있으면 무조건 먼저 닫고, 아이템이 있으면 새 메뉴 열기
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        if (ContextMenuWidget &&
            ContextMenuWidget->GetVisibility() == ESlateVisibility::Visible)
        {
            ContextMenuWidget->CloseMenu();
        }

        if (!CachedSlotData.IsEmpty())
        {
            // 지연 생성
            if (!ContextMenuWidget && ContextMenuWidgetClass)
            {
                APlayerController* PC = GetOwningPlayer();
                if (PC)
                {
                    ContextMenuWidget = CreateWidget<UItemContextMenuWidget>(PC, ContextMenuWidgetClass);
                    if (ContextMenuWidget)
                    {
                        ContextMenuWidget->AddToViewport(100);
                        ContextMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
                    }
                }
            }

            if (ContextMenuWidget)
            {
                ContextMenuWidget->ShowForEquippedItem(
                    SlotType, CachedSlotData.ItemInstance.ItemDataID);
                ContextMenuWidget->SetMenuPosition(InMouseEvent.GetScreenSpacePosition());
            }
        }
        // 빈 슬롯 우클릭 → 닫기만 하고 끝

        return FReply::Handled();
    }

    // 좌클릭 → 드래그
    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && !CachedSlotData.IsEmpty())
    {
        return FReply::Handled().DetectDrag(GetCachedWidget().ToSharedRef(), EKeys::LeftMouseButton);
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UEquipmentSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry,
                                                 const FPointerEvent& InMouseEvent,
                                                 UDragDropOperation*& OutOperation)
{
    if (CachedSlotData.IsEmpty())
    {
        return;
    }

    UInventoryDragDropOp* DragOp = NewObject<UInventoryDragDropOp>(this);
    DragOp->DraggedItemInstanceID = CachedSlotData.ItemInstance.InstanceID;
    DragOp->ItemDataID             = CachedSlotData.ItemInstance.ItemDataID;
    DragOp->ItemSize               = CachedSlotData.ItemInstance.ItemSize;
    DragOp->DragItemSize           = CachedSlotData.ItemInstance.GetEffectiveSize();
    DragOp->bWasRotated            = CachedSlotData.ItemInstance.bIsRotated;
    DragOp->bFromEquipment         = true;
    DragOp->SourceEquipmentSlot    = SlotType;
    OutOperation = DragOp;
}

bool UEquipmentSlotWidget::NativeOnDrop(const FGeometry& InGeometry,
                                         const FDragDropEvent& InDragDropEvent,
                                         UDragDropOperation* InOperation)
{
    UInventoryDragDropOp* DragOp = Cast<UInventoryDragDropOp>(InOperation);
    if (!DragOp)
    {
        return false;
    }

    // 장비슬롯에서 온 드래그는 여기서 처리하지 않음 (장비→장비 교환은 미지원)
    if (DragOp->bFromEquipment)
    {
        return false;
    }

    // 인벤토리에서 온 드래그 → 장착 시도
    // 1. DataTable에서 슬롯 타입 검증
    UItemDataSubsystem* Sub = nullptr;
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            Sub = GI->GetSubsystem<UItemDataSubsystem>();
        }
    }

    if (Sub)
    {
        const FItemData* ItemData = Sub->GetItemData(DragOp->ItemDataID);
        if (!ItemData || ItemData->ItemType != EItemType::Equipment)
        {
            UE_LOG(LogTemp, Warning, TEXT("EquipmentSlotWidget: Item '%s' is not equipment"),
                *DragOp->ItemDataID.ToString());
            return false;
        }

        // EquipmentSlotTag → 후보 슬롯 목록으로 이 슬롯 타입 허용 여부 검사
        // (예: "Weapon" → Weapon1, Weapon2 모두 허용)
        const FName& Tag = ItemData->EquipmentSlotTag;
        if (!Tag.IsNone())
        {
            // 후보 슬롯 인라인 매핑 (FindTargetSlot과 동일 기준)
            TArray<EEquipmentSlot> ValidSlots;
            if      (Tag == FName("Weapon"))  ValidSlots = { EEquipmentSlot::Weapon1, EEquipmentSlot::Weapon2 };
            else if (Tag == FName("Head"))    ValidSlots = { EEquipmentSlot::Head    };
            else if (Tag == FName("Face"))    ValidSlots = { EEquipmentSlot::Face    };
            else if (Tag == FName("Eyewear")) ValidSlots = { EEquipmentSlot::Eyewear };
            else if (Tag == FName("Body"))    ValidSlots = { EEquipmentSlot::Body    };

            if (ValidSlots.Num() > 0 && !ValidSlots.Contains(SlotType))
            {
                UE_LOG(LogTemp, Warning, TEXT("EquipmentSlotWidget: '%s'(Tag=%s)는 이 슬롯[%d]에 장착 불가"),
                    *DragOp->ItemDataID.ToString(), *Tag.ToString(), static_cast<int32>(SlotType));
                return false;
            }
        }
    }

    // 2. 소유 Actor의 EquipmentComponent로 복합 RPC 호출
    APlayerController* PC = GetOwningPlayer();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!Pawn)
    {
        return false;
    }

    UEquipmentComponent* EquipComp = Pawn->FindComponentByClass<UEquipmentComponent>();
    if (!EquipComp)
    {
        return false;
    }

    EquipComp->Server_EquipFromInventory(SlotType, DragOp->DraggedItemInstanceID);
    return true;
}
