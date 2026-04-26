// Copyright Project EXFIL. All Rights Reserved.

#include "UI/ItemContextMenuWidget.h"
#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "UI/EquipmentViewModel.h"

void UItemContextMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Button_Use)    Button_Use->OnClicked.AddDynamic(this, &UItemContextMenuWidget::OnUseClicked);
    if (Button_Equip)  Button_Equip->OnClicked.AddDynamic(this, &UItemContextMenuWidget::OnEquipClicked);
    if (Button_Unequip) Button_Unequip->OnClicked.AddDynamic(this, &UItemContextMenuWidget::OnUnequipClicked);
    if (Button_Drop)   Button_Drop->OnClicked.AddDynamic(this, &UItemContextMenuWidget::OnDropClicked);
}

void UItemContextMenuWidget::Show(
    const FItemContextMenuViewData& InViewData,
    UInventoryViewModel* InInventoryViewModel,
    UEquipmentViewModel* InEquipmentViewModel)
{
    CachedViewData = InViewData;
    InventoryViewModel = InInventoryViewModel;
    EquipmentViewModel = InEquipmentViewModel;

    if (Button_Use)     Button_Use->SetVisibility(InViewData.bCanUse ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (Button_Equip)   Button_Equip->SetVisibility(InViewData.bCanEquip ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (Button_Unequip) Button_Unequip->SetVisibility(InViewData.bCanUnequip ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (Button_Drop)    Button_Drop->SetVisibility(InViewData.bCanDrop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    SetVisibility(ESlateVisibility::Visible);
}

void UItemContextMenuWidget::OnUseClicked()
{
    if (InventoryViewModel.IsValid())
    {
        InventoryViewModel->RequestConsumeItem(CachedViewData.TargetItemInstanceID);
    }
    CloseMenu();
}

void UItemContextMenuWidget::OnEquipClicked()
{
    if (EquipmentViewModel.IsValid())
    {
        EquipmentViewModel->RequestEquip(
            EEquipmentSlot::None, CachedViewData.TargetItemInstanceID);
    }
    CloseMenu();
}

void UItemContextMenuWidget::OnUnequipClicked()
{
    if (EquipmentViewModel.IsValid())
    {
        EquipmentViewModel->RequestUnequip(CachedViewData.TargetEquipmentSlot);
    }
    CloseMenu();
}

void UItemContextMenuWidget::OnDropClicked()
{
    if (CachedViewData.TargetEquipmentSlot != EEquipmentSlot::None)
    {
        if (EquipmentViewModel.IsValid())
        {
            EquipmentViewModel->RequestDropEquipped(CachedViewData.TargetEquipmentSlot);
        }
    }
    else
    {
        if (InventoryViewModel.IsValid())
        {
            InventoryViewModel->RequestDropItem(CachedViewData.TargetItemInstanceID);
        }
    }
    CloseMenu();
}

void UItemContextMenuWidget::CloseMenu()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UItemContextMenuWidget::SetMenuPosition(const FVector2D& ScreenPosition)
{
    UGameViewportSubsystem* ViewportSub = UGameViewportSubsystem::Get(GetWorld());
    if (!ViewportSub) return;

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;
    float MouseX = 0.f, MouseY = 0.f;
    PC->GetMousePosition(MouseX, MouseY);

    const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
    const float InvScale = (Scale > 0.f) ? 1.f / Scale : 1.f;

    FGameViewportWidgetSlot SlotSettings = ViewportSub->GetWidgetSlot(this);
    SlotSettings.Anchors   = FAnchors(0.f, 0.f, 0.f, 0.f);
    SlotSettings.Offsets   = FMargin(MouseX * InvScale, MouseY * InvScale, 0.f, 0.f);
    SlotSettings.Alignment = FVector2D(0.f, 0.f);
    ViewportSub->SetWidgetSlot(this, SlotSettings);
}
