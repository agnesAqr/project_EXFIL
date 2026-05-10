// Copyright Project EXFIL. All Rights Reserved.

#include "UI/EquipmentSlotWidget.h"
#include "CoreMinimal.h"
#include "Internationalization/StringTableRegistry.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/InventoryDragDropOp.h"
#include "UI/ItemContextMenuWidget.h"

void UEquipmentSlotWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    static const TMap<EEquipmentSlot, FText> SlotLabels =
    {
        { EEquipmentSlot::Head,    LOCTABLE("/Game/Localization/ST_UI", "Equip.Head")    },
        { EEquipmentSlot::Face,    LOCTABLE("/Game/Localization/ST_UI", "Equip.Face")    },
        { EEquipmentSlot::Eyewear, LOCTABLE("/Game/Localization/ST_UI", "Equip.Eyewear") },
        { EEquipmentSlot::Body,    LOCTABLE("/Game/Localization/ST_UI", "Equip.Body")    },
        { EEquipmentSlot::Weapon1, LOCTABLE("/Game/Localization/ST_UI", "Equip.Weapon1") },
        { EEquipmentSlot::Weapon2, LOCTABLE("/Game/Localization/ST_UI", "Equip.Weapon2") },
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
}

void UEquipmentSlotWidget::NativeDestruct()
{
    if (BoundViewModel.IsValid() && EquipmentSlotChangedHandle.IsValid())
    {
        BoundViewModel->OnEquipmentSlotChanged.Remove(EquipmentSlotChangedHandle);
        EquipmentSlotChangedHandle.Reset();
    }

    Super::NativeDestruct();
}

void UEquipmentSlotWidget::SetViewModel(UEquipmentViewModel* InViewModel)
{
    if (BoundViewModel.IsValid() && EquipmentSlotChangedHandle.IsValid())
    {
        BoundViewModel->OnEquipmentSlotChanged.Remove(EquipmentSlotChangedHandle);
        EquipmentSlotChangedHandle.Reset();
    }

    BoundViewModel = InViewModel;

    if (!InViewModel)
    {
        RefreshSlot(FEquipmentSlotViewData());
        return;
    }

    EquipmentSlotChangedHandle = InViewModel->OnEquipmentSlotChanged.AddUObject(
        this, &UEquipmentSlotWidget::HandleEquipmentSlotChanged);

    FEquipmentSlotViewData SlotViewData;
    if (InViewModel->GetSlotViewData(SlotType, SlotViewData))
    {
        RefreshSlot(SlotViewData);
    }
}

void UEquipmentSlotWidget::HandleEquipmentSlotChanged(
    EEquipmentSlot InSlot, const FEquipmentSlotViewData& InViewData)
{
    if (InSlot == SlotType)
    {
        RefreshSlot(InViewData);
    }
}

void UEquipmentSlotWidget::RefreshSlot(const FEquipmentSlotViewData& SlotData)
{
    CachedSlotData = SlotData;
    CachedSlotData.SlotType = SlotType;

    if (SlotData.bEquipped)
    {
        if (Image_ItemIcon)
        {
            Image_ItemIcon->SetBrushFromTexture(SlotData.Icon.Get(), true);
            Image_ItemIcon->SetVisibility(SlotData.Icon
                ? ESlateVisibility::HitTestInvisible
                : ESlateVisibility::Collapsed);
        }
        if (TextBlock_ItemName)
        {
            TextBlock_ItemName->SetText(SlotData.DisplayName);
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
        if (CachedSlotData.bEquipped)
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
    Border_Slot->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
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
    Border_Slot->SetBrushColor(bIsValid
        ? FLinearColor(0.08f, 0.18f, 0.3f, 1.0f)
        : FLinearColor(0.25f, 0.08f, 0.08f, 1.0f));
}

FReply UEquipmentSlotWidget::NativeOnMouseButtonDown(
    const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        if (ContextMenuWidget &&
            ContextMenuWidget->GetVisibility() == ESlateVisibility::Visible)
        {
            ContextMenuWidget->CloseMenu();
        }

        if (CachedSlotData.bEquipped && BoundViewModel.IsValid())
        {
            if (!ContextMenuWidget && ContextMenuWidgetClass)
            {
                APlayerController* PC = GetOwningPlayer();
                if (PC)
                {
                    ContextMenuWidget = CreateWidget<UItemContextMenuWidget>(
                        PC, ContextMenuWidgetClass);
                    if (ContextMenuWidget)
                    {
                        ContextMenuWidget->AddToViewport(100);
                        ContextMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
                    }
                }
            }

            FItemContextMenuViewData MenuViewData;
            if (ContextMenuWidget &&
                BoundViewModel->TryGetContextMenuViewDataForSlot(SlotType, MenuViewData))
            {
                ContextMenuWidget->Show(MenuViewData, nullptr, BoundViewModel.Get());
                ContextMenuWidget->SetMenuPosition(InMouseEvent.GetScreenSpacePosition());
            }
        }

        return FReply::Handled();
    }

    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) &&
        CachedSlotData.bEquipped)
    {
        return FReply::Handled().DetectDrag(
            GetCachedWidget().ToSharedRef(), EKeys::LeftMouseButton);
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UEquipmentSlotWidget::NativeOnDragDetected(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent,
    UDragDropOperation*& OutOperation)
{
    if (!BoundViewModel.IsValid())
    {
        return;
    }

    FEquipmentDragSourceViewData DragSource;
    if (!BoundViewModel->TryBuildDragSourceForSlot(SlotType, DragSource))
    {
        return;
    }

    UInventoryDragDropOp* DragOp = NewObject<UInventoryDragDropOp>(this);
    DragOp->DraggedItemInstanceID = DragSource.ItemInstanceID;
    DragOp->ItemSize = DragSource.BaseItemSize;
    DragOp->DragItemSize = DragSource.InitialDragItemSize;
    DragOp->bOriginalRotated = DragSource.bOriginalRotated;
    DragOp->bIsRotated = DragSource.bOriginalRotated;
    DragOp->bFromEquipment = true;
    DragOp->SourceEquipmentSlot = DragSource.SourceSlot;
    OutOperation = DragOp;
}

bool UEquipmentSlotWidget::NativeOnDrop(
    const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    UInventoryDragDropOp* DragOp = Cast<UInventoryDragDropOp>(InOperation);
    if (!DragOp || DragOp->bFromEquipment || !BoundViewModel.IsValid())
    {
        return false;
    }

    BoundViewModel->RequestEquip(SlotType, DragOp->DraggedItemInstanceID);
    return true;
}
