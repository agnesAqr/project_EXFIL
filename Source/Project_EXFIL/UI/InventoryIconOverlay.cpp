// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventoryIconOverlay.h"
#include "CoreMinimal.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UI/InventoryViewModel.h"
#include "UI/InventorySlotViewModel.h"

void UInventoryIconOverlay::RefreshIcons(UInventoryViewModel* InViewModel, float InCellSize)
{
    if (!IconCanvas || !InViewModel)
    {
        return;
    }

    IconCanvas->ClearChildren();

    TArray<UInventorySlotViewModel*> AllSlots = InViewModel->GetAllSlots();
    for (UInventorySlotViewModel* SlotVM : AllSlots)
    {
        if (!SlotVM || SlotVM->IsEmpty() || !SlotVM->IsRootSlot())
        {
            continue;
        }

        const TSoftObjectPtr<UTexture2D> IconPtr = SlotVM->GetIcon();
        if (IconPtr.IsNull())
        {
            continue;
        }

        UTexture2D* IconTex = IconPtr.LoadSynchronous();
        if (!IconTex)
        {
            continue;
        }

        UImage* IconImage = NewObject<UImage>(this);
        FSlateBrush Brush;
        Brush.SetResourceObject(IconTex);
        IconImage->SetBrush(Brush);

        UCanvasPanelSlot* CanvasSlot = IconCanvas->AddChildToCanvas(IconImage);
        if (CanvasSlot)
        {
            const FIntPoint GridPos = SlotVM->GetGridPosition();
            CanvasSlot->SetPosition(FVector2D(GridPos.X * InCellSize, GridPos.Y * InCellSize));
            CanvasSlot->SetSize(FVector2D(SlotVM->GetItemSizeX() * InCellSize,
                                          SlotVM->GetItemSizeY() * InCellSize));
            CanvasSlot->SetAutoSize(false);
        }
    }
}
