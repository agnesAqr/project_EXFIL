// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "UI/InventorySlotViewModel.h"
#include "Inventory/EXFILInventoryTypes.h"
#include "Data/Equipment/EquipmentTypes.h"
#include "InventoryViewModel.generated.h"

class UInventoryComponent;
class UItemDataSubsystem;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryViewModelRefreshed, const TSet<int32>&);

USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FItemContextMenuViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bShowUse = false;

    UPROPERTY(BlueprintReadOnly)
    bool bShowEquip = false;

    UPROPERTY(BlueprintReadOnly)
    bool bShowUnequip = false;

    UPROPERTY(BlueprintReadOnly)
    bool bShowDrop = false;

    UPROPERTY(BlueprintReadOnly)
    FGuid TargetItemInstanceID;

    UPROPERTY(BlueprintReadOnly)
    EEquipmentSlot TargetEquipmentSlot = EEquipmentSlot::None;
};

USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FInventoryIconViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid InstanceID;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UTexture2D> Icon = nullptr;

    UPROPERTY(BlueprintReadOnly)
    int32 StackCount = 0;

    UPROPERTY(BlueprintReadOnly)
    FIntPoint RootGridPosition = FIntPoint::ZeroValue;

    UPROPERTY(BlueprintReadOnly)
    FItemSize GridSpan;

    UPROPERTY(BlueprintReadOnly)
    bool bRotated = false;
};

USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FInventoryOverlayDeltaViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TArray<FInventoryIconViewData> UpsertItems;

    UPROPERTY(BlueprintReadOnly)
    TArray<FGuid> RemovedItemInstanceIDs;

    UPROPERTY(BlueprintReadOnly)
    bool bFullRefresh = false;
};

USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FInventoryDragSourceViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid InstanceID;

    UPROPERTY(BlueprintReadOnly)
    FName ItemDataID;

    UPROPERTY(BlueprintReadOnly)
    FIntPoint OriginalRootPosition = FIntPoint::ZeroValue;

    UPROPERTY(BlueprintReadOnly)
    FItemSize BaseItemSize;

    UPROPERTY(BlueprintReadOnly)
    FItemSize InitialDragItemSize;

    UPROPERTY(BlueprintReadOnly)
    bool bOriginalRotated = false;

    UPROPERTY(BlueprintReadOnly)
    FIntPoint DragOffset = FIntPoint::ZeroValue;
};

USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FInventoryPlacementHintViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FIntPoint PreviewRootPosition = FIntPoint::ZeroValue;

    UPROPERTY(BlueprintReadOnly)
    FItemSize PreviewSize;

    UPROPERTY(BlueprintReadOnly)
    bool bPredictedPlaceable = false;
};

UCLASS()
class PROJECT_EXFIL_API UInventoryViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    
    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void Initialize(UInventoryComponent* InInventoryComponent);

    virtual void BeginDestroy() override;

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    UInventorySlotViewModel* GetSlotAt(FIntPoint Position) const;

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    const TArray<UInventorySlotViewModel*>& GetAllSlots() const { return SlotViewModels; }

    
    UFUNCTION(BlueprintPure)
    int32 GetGridWidth() const { return GridWidth; }
    
    UFUNCTION(BlueprintPure)
    int32 GetGridHeight() const { return GridHeight; }

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void RequestMoveItem(FGuid ItemInstanceID, FIntPoint NewPosition, bool bNewRotated = false);

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void RequestRemoveItem(FGuid ItemInstanceID);

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void RequestConsumeItem(FGuid ItemInstanceID);

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void RequestDropItem(FGuid ItemInstanceID);

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    FIntPoint GetItemRootPosition(FGuid ItemInstanceID) const;

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    FItemSize GetItemEffectiveSize(FGuid ItemInstanceID) const;

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|ViewModel")
    bool IsItemRotated(FGuid ItemInstanceID) const;

    bool TryGetItemContextMenuViewDataAtCell(
        FIntPoint Cell, FItemContextMenuViewData& OutViewData) const;

    bool GetPendingOverlayDelta(FInventoryOverlayDeltaViewData& OutDelta) const;

    void DiscardPendingOverlayDelta();

    bool BuildFullOverlayDelta(FInventoryOverlayDeltaViewData& OutDelta) const;

    bool TryBuildDragSourceAtCell(
        FIntPoint Cell, FIntPoint DragStartCell, FInventoryDragSourceViewData& OutViewData) const;

    FInventoryPlacementHintViewData BuildPlacementHint(
        FIntPoint HoverCell, FIntPoint DragOffset,
        FGuid IgnoredInstanceID, FItemSize PreviewSize) const;

    
    FOnInventoryViewModelRefreshed OnViewModelRefreshed;

private:
    UPROPERTY()
    TWeakObjectPtr<UInventoryComponent> InventoryComp;

    UPROPERTY()
    TWeakObjectPtr<UItemDataSubsystem> CachedItemSub;

    UPROPERTY()
    TArray<UInventorySlotViewModel*> SlotViewModels;

    UPROPERTY(BlueprintReadOnly, Getter,
              meta = (AllowPrivateAccess = true))
    int32 GridWidth = 0;

    UPROPERTY(BlueprintReadOnly, Getter,
              meta = (AllowPrivateAccess = true))
    int32 GridHeight = 0;

    FInventoryOverlayDeltaViewData PendingOverlayDelta;
    bool bHasPendingOverlayDelta = false;
    FDelegateHandle InventoryUpdatedHandle;

    void HandleInventoryUpdated(const TSet<int32>& DirtyIndices);
    void UnbindInventoryComponent();

    
    void RefreshAllSlots();

    
    void RefreshDirtySlots(const TSet<int32>& DirtyIndices);

    void BuildPendingOverlayDelta(const TSet<int32>& DirtyIndices);
    void CollectItemIDsFromSlotViewModels(
        const TSet<int32>& SlotIndices, TSet<FGuid>& OutItemIDs) const;
    bool TryBuildIconViewData(FGuid ItemInstanceID, FInventoryIconViewData& OutViewData) const;
    bool TryGetItemAtCell(FIntPoint Cell, FInventoryItemInstance& OutItem) const;
    void StorePendingOverlayDelta(const FInventoryOverlayDeltaViewData& Delta);

    int32 PositionToIndex(FIntPoint Position) const;
};
