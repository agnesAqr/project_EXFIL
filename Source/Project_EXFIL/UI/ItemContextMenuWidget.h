// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/Equipment/EquipmentTypes.h"
#include "ItemContextMenuWidget.generated.h"

class UButton;
class UInventoryComponent;
class UEquipmentComponent;

/**
 * UItemContextMenuWidget — 인벤토리/장비슬롯 우클릭 컨텍스트 메뉴
 *
 * 인벤토리 아이템: Use / Equip / Drop
 * 장비슬롯 아이템: Unequip / Drop
 *
 * WBP에서 Button_Use / Button_Equip / Button_Unequip / Button_Drop 바인딩 필요.
 */
UCLASS(Abstract)
class PROJECT_EXFIL_API UItemContextMenuWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    /** 인벤토리 아이템용 메뉴 열기 */
    void ShowForInventoryItem(FGuid InItemInstanceID, FName InItemDataID);

    /** 장비슬롯 아이템용 메뉴 열기 */
    void ShowForEquippedItem(EEquipmentSlot InSlot, FName InItemDataID);

    /** 메뉴 닫기 — 외부 위젯에서도 호출 가능 */
    void CloseMenu();

    /**
     * 뷰포트 스크린 좌표로 메뉴 위치 설정.
     * DPI 스케일을 자동 반영하여 CanvasPanelSlot 로컬 좌표로 변환.
     */
    void SetMenuPosition(const FVector2D& ScreenPosition);

protected:
    virtual void NativeConstruct() override;

    // === UI 요소 (BindWidget) ===
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Use;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Equip;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Unequip;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Drop;

private:
    // === 상태 ===
    FGuid CachedItemInstanceID;
    FName CachedItemDataID;
    EEquipmentSlot CachedEquipmentSlot = EEquipmentSlot::None;
    bool bIsEquippedItem = false;

    // === 버튼 핸들러 ===
    UFUNCTION()
    void OnUseClicked();

    UFUNCTION()
    void OnEquipClicked();

    UFUNCTION()
    void OnUnequipClicked();

    UFUNCTION()
    void OnDropClicked();

    /** 소유 Pawn의 InventoryComponent 획득 */
    UInventoryComponent* GetInventoryComponent() const;

    /** 소유 Pawn의 EquipmentComponent 획득 */
    UEquipmentComponent* GetEquipmentComponent() const;
};
