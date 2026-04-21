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
#include "Core/EXFILLog.h"

void UEquipmentSlotWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
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
    if (UGameInstance* GI = GetGameInstance())
    {
        CachedItemSub = GI->GetSubsystem<UItemDataSubsystem>();
    }
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
            RefreshFromEquipmentComponent();
        }
    }
}

void UEquipmentSlotWidget::NativeDestruct()
{
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
        if (Image_ItemIcon)
        {
            if (CachedItemSub)
            {
                const FItemData* ItemData = CachedItemSub->GetItemData(SlotData.ItemInstance.ItemDataID);
                if (ItemData && !ItemData->Icon.IsNull())
                {
                    UTexture2D* IconTexture = CachedItemSub->GetCachedTexture(ItemData->Icon);
                    if (IconTexture)
                    {
                        Image_ItemIcon->SetBrushFromTexture(IconTexture, true);
                    }
                }
            }
            Image_ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
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

void UEquipmentSlotWidget::ApplyEmptyStyle()
{
    if (!Border_Slot)
    {
        return;
    }
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
    Border_Slot->SetBrushColor(FLinearColor(0.18f, 0.22f, 0.15f, 0.9f));
}

void UEquipmentSlotWidget::ApplyDragHoverStyle(bool bIsValid)
{
    if (!Border_Slot)
    {
        return;
    }
    if (bIsValid)
    {
        Border_Slot->SetBrushColor(FLinearColor(0.08f, 0.18f, 0.3f, 1.0f));
    }
    else
    {
        Border_Slot->SetBrushColor(FLinearColor(0.25f, 0.08f, 0.08f, 1.0f));
    }
}

FReply UEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                                      const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        if (ContextMenuWidget &&
            ContextMenuWidget->GetVisibility() == ESlateVisibility::Visible)
        {
            ContextMenuWidget->CloseMenu();
        }

        if (!CachedSlotData.IsEmpty())
        {
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

        return FReply::Handled();
    }
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
    const bool bInitialRotated =
        CachedSlotData.ItemInstance.bIsRotated &&
        !CachedSlotData.ItemInstance.ItemSize.IsSquare();
    DragOp->DraggedItemInstanceID = CachedSlotData.ItemInstance.InstanceID;
    DragOp->ItemDataID             = CachedSlotData.ItemInstance.ItemDataID;
    DragOp->ItemSize               = CachedSlotData.ItemInstance.ItemSize;
    DragOp->DragItemSize           = bInitialRotated
        ? CachedSlotData.ItemInstance.ItemSize.GetRotated()
        : CachedSlotData.ItemInstance.ItemSize;
    DragOp->bOriginalRotated       = bInitialRotated;
    DragOp->bIsRotated             = bInitialRotated;
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
    if (DragOp->bFromEquipment)
    {
        return false;
    }
    if (CachedItemSub)
    {
        const FItemData* ItemData = CachedItemSub->GetItemData(DragOp->ItemDataID);
        if (!ItemData || ItemData->ItemType != EItemType::Equipment)
        {
            UE_LOG(LogEXFIL, Warning, TEXT("EquipmentSlotWidget: Item '%s' is not equipment"),
                *DragOp->ItemDataID.ToString());
            return false;
        }
        const FName& Tag = ItemData->EquipmentSlotTag;
        if (!Tag.IsNone())
        {
            TArray<EEquipmentSlot> ValidSlots;
            if      (Tag == FName("Weapon"))  ValidSlots = { EEquipmentSlot::Weapon1, EEquipmentSlot::Weapon2 };
            else if (Tag == FName("Head"))    ValidSlots = { EEquipmentSlot::Head    };
            else if (Tag == FName("Face"))    ValidSlots = { EEquipmentSlot::Face    };
            else if (Tag == FName("Eyewear")) ValidSlots = { EEquipmentSlot::Eyewear };
            else if (Tag == FName("Body"))    ValidSlots = { EEquipmentSlot::Body    };

            if (ValidSlots.Num() > 0 && !ValidSlots.Contains(SlotType))
            {
                UE_LOG(LogEXFIL, Warning, TEXT("EquipmentSlotWidget: '%s'(Tag=%s)는 이 슬롯[%d]에 장착 불가"),
                    *DragOp->ItemDataID.ToString(), *Tag.ToString(), static_cast<int32>(SlotType));
                return false;
            }
        }
    }
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

    EquipComp->RequestEquipFromInventory(SlotType, DragOp->DraggedItemInstanceID);
    return true;
}
