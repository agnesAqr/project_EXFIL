// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "InventoryPanelWidget.generated.h"

class UInventoryViewModel;
class UEquipmentViewModel;
class UCraftingViewModel;
class UInventorySlotWidget;
class UInventoryIconOverlay;
class UCraftingPanelWidget;
class UUniformGridPanel;
class UScrollBox;
class UWidgetSwitcher;
class UButton;
class UStatEntryWidget;
class USurvivalViewModel;

UCLASS(Abstract)
class PROJECT_EXFIL_API UInventoryPanelWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void SetViewModel(UInventoryViewModel* InViewModel);

    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void SetEquipmentViewModel(UEquipmentViewModel* InViewModel);

    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void SetCraftingViewModel(UCraftingViewModel* InViewModel);

    
    bool ForwardMoveRequest(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated = false);

    
    void HighlightArea(FIntPoint RootPos, FItemSize ItemSize, bool bIsValid);

    
    void ClearAreaHighlights();

    
    UInventoryViewModel* GetViewModel() const { return ViewModel; }

    
    UCraftingPanelWidget* GetCraftingPanel() const { return CraftingPanel; }
    void NotifyPanelShown();
    void NotifyPanelHidden();

    
    void BindStatsToViewModel(USurvivalViewModel* InSurvivalViewModel);

    
    void UpdateDragAutoScroll(
        const FVector2D& ScreenSpacePosition,
        bool bUseItemBounds = false,
        const FVector2D& ItemTopScreenPosition = FVector2D::ZeroVector,
        const FVector2D& ItemBottomScreenPosition = FVector2D::ZeroVector);

    
    void StopDragAutoScroll();

    void HandleDragAutoScrollLeave(const FVector2D& ScreenSpacePosition);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual bool NativeOnHandleBackAction() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                           const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry,
                                         const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry,
                                     const FPointerEvent& InMouseEvent) override;
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                              const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                              int32 LayerId, const FWidgetStyle& InWidgetStyle,
                              bool bParentEnabled) const override;

    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UUniformGridPanel> GridPanel;

    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UInventoryIconOverlay> IconOverlay;

    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UScrollBox> InventoryScrollBox;

    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UWidgetSwitcher> WidgetSwitcher_Content;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_InventoryTab;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_CraftingTab;

    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UCraftingPanelWidget> CraftingPanel;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UStatEntryWidget> StatEntry_HP;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UStatEntryWidget> StatEntry_HU;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UStatEntryWidget> StatEntry_TH;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UStatEntryWidget> StatEntry_ST;

    
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI")
    TSubclassOf<UInventorySlotWidget> SlotWidgetClass;

private:
    UPROPERTY()
    TObjectPtr<UInventoryViewModel> ViewModel;

    UPROPERTY()
    TObjectPtr<UEquipmentViewModel> EquipmentViewModel;

    UPROPERTY()
    TObjectPtr<UCraftingViewModel> CraftingViewModel;

    UPROPERTY()
    TArray<TObjectPtr<UInventorySlotWidget>> SlotWidgets;

    
    void BuildGrid();

    
    void ClearGrid();

    
    void HandleViewModelRefreshed(const TSet<int32>& DirtyIndices);

    
    void HandleLayoutMeasured(const FGeometry& AllottedGeometry);

    
    void FlushOverlayDelta();
    void RebuildOverlayFull();

    
    bool bHasPendingOverlayRefresh = false;

    
    mutable bool bLayoutReady = false;

    
    FVector2D CachedCellStride = FVector2D::ZeroVector;
    mutable float CachedSquareCellSize = 0.f;

    
    mutable bool bNeedsCellSquareFix = true;

    
    FDelegateHandle ViewModelRefreshedHandle;

    UFUNCTION()
    void OnInventoryTabClicked();

    UFUNCTION()
    void OnCraftingTabClicked();

    
    void UpdateTabStyles(int32 ActiveIndex);

    UScrollBox* ResolveInventoryScrollBox();

    void ConfigureInventoryScrollBox(UScrollBox* ScrollBox);

    
    FTimerHandle AutoScrollTimerHandle;

    
    float AutoScrollSpeed = 0.f;

    int32 LastAutoScrollZone = 0;

    double AutoScrollLastUpdateTimeSeconds = 0.0;

    bool bInventoryScrollBoxConfigured = false;

    
    static constexpr float ScrollEdgeZone = 60.f;

    
    static constexpr float ScrollRate = 15.f;

    static constexpr float AutoScrollUpdateInterval = 0.016f;

    static constexpr float AutoScrollStaleUpdateTimeout = 0.12f;

    static constexpr float AutoScrollBoundaryTolerance = 0.1f;

    static constexpr float GridSquareCellResizeTolerance = 0.5f;

    void TickAutoScroll();
};
