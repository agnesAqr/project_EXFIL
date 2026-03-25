// Copyright Project EXFIL. All Rights Reserved.

#include "UI/ItemContextMenuWidget.h"
#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/GameInstance.h"
#include "Data/EXFILItemTypes.h"
#include "Data/ItemDataSubsystem.h"
#include "Inventory/InventoryComponent.h"
#include "Data/Equipment/EquipmentComponent.h"
#include "Core/EXFILLog.h"

void UItemContextMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Button_Use)    Button_Use->OnClicked.AddDynamic(this, &UItemContextMenuWidget::OnUseClicked);
    if (Button_Equip)  Button_Equip->OnClicked.AddDynamic(this, &UItemContextMenuWidget::OnEquipClicked);
    if (Button_Unequip) Button_Unequip->OnClicked.AddDynamic(this, &UItemContextMenuWidget::OnUnequipClicked);
    if (Button_Drop)   Button_Drop->OnClicked.AddDynamic(this, &UItemContextMenuWidget::OnDropClicked);
}

void UItemContextMenuWidget::ShowForInventoryItem(FGuid InItemInstanceID, FName InItemDataID)
{
    bIsEquippedItem = false;
    CachedItemInstanceID = InItemInstanceID;
    CachedItemDataID = InItemDataID;

    // 아이템 타입에 따라 버튼 가시성 결정
    const FItemData* Data = nullptr;
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UItemDataSubsystem* Sub = GI->GetSubsystem<UItemDataSubsystem>())
        {
            Data = Sub->GetItemData(InItemDataID);
        }
    }

    const bool bIsConsumable = Data && Data->ItemType == EItemType::Consumable;
    const bool bIsEquipable  = Data && Data->ItemType == EItemType::Equipment;

    if (Button_Use)     Button_Use->SetVisibility(bIsConsumable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (Button_Equip)   Button_Equip->SetVisibility(bIsEquipable  ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (Button_Unequip) Button_Unequip->SetVisibility(ESlateVisibility::Collapsed);
    if (Button_Drop)    Button_Drop->SetVisibility(ESlateVisibility::Visible); // 항상 표시

    SetVisibility(ESlateVisibility::Visible);
}

void UItemContextMenuWidget::ShowForEquippedItem(EEquipmentSlot InSlot, FName InItemDataID)
{
    bIsEquippedItem = true;
    CachedEquipmentSlot = InSlot;
    CachedItemDataID = InItemDataID;

    if (Button_Use)     Button_Use->SetVisibility(ESlateVisibility::Collapsed);
    if (Button_Equip)   Button_Equip->SetVisibility(ESlateVisibility::Collapsed);
    if (Button_Unequip) Button_Unequip->SetVisibility(ESlateVisibility::Visible);
    if (Button_Drop)    Button_Drop->SetVisibility(ESlateVisibility::Visible);

    SetVisibility(ESlateVisibility::Visible);
}

void UItemContextMenuWidget::OnUseClicked()
{
    if (UInventoryComponent* Inv = GetInventoryComponent())
    {
        Inv->Server_ConsumeItemByID(CachedItemDataID, 1);
    }
    CloseMenu();
}

void UItemContextMenuWidget::OnEquipClicked()
{
    // 슬롯 결정은 서버의 FindTargetSlot이 전담 → 클라이언트는 None 전달
    if (UEquipmentComponent* Equip = GetEquipmentComponent())
    {
        Equip->Server_EquipFromInventory(EEquipmentSlot::None, CachedItemInstanceID);
    }
    else
    {
        UE_LOG(LogEXFIL, Error, TEXT("OnEquipClicked: EquipmentComponent NULL"));
    }
    CloseMenu();
}

void UItemContextMenuWidget::OnUnequipClicked()
{
    if (UEquipmentComponent* Equip = GetEquipmentComponent())
    {
        Equip->Server_UnequipToInventory(CachedEquipmentSlot);
    }
    CloseMenu();
}

void UItemContextMenuWidget::OnDropClicked()
{
    if (bIsEquippedItem)
    {
        if (UEquipmentComponent* Equip = GetEquipmentComponent())
        {
            Equip->Server_DropEquippedItem(CachedEquipmentSlot);
        }
    }
    else
    {
        if (UInventoryComponent* Inv = GetInventoryComponent())
        {
            Inv->Server_DropItem(CachedItemInstanceID);
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
    // AddToViewport() 후 슬롯은 UGameViewportSubsystem이 관리 (UCanvasPanelSlot 아님)
    UGameViewportSubsystem* ViewportSub = UGameViewportSubsystem::Get(GetWorld());
    if (!ViewportSub) return;

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    // GetScreenSpacePosition()은 OS 창 전체 기준 Slate 절대 좌표
    // → PIE에서 에디터 툴바 높이만큼 Y 오프셋 발생
    // GetMousePosition()은 뷰포트 기준 물리 픽셀 → DPI Scale 나누면 정확한 Slate 단위
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

UInventoryComponent* UItemContextMenuWidget::GetInventoryComponent() const
{
    APlayerController* PC = GetOwningPlayer();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    return Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
}

UEquipmentComponent* UItemContextMenuWidget::GetEquipmentComponent() const
{
    APlayerController* PC = GetOwningPlayer();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    return Pawn ? Pawn->FindComponentByClass<UEquipmentComponent>() : nullptr;
}
