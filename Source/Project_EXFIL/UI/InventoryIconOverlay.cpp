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

void UInventoryIconOverlay::RefreshIcons(UInventoryViewModel* InViewModel,
                                          UUniformGridPanel* InGridPanel,
                                          int32 InGridWidth, int32 InGridHeight)
{
    if (!IconCanvas || !InViewModel || !InGridPanel)
    {
        return;
    }

    IconCanvas->ClearChildren();

    // 첫 번째 슬롯(0,0)의 geometry에서 슬롯 크기 확보
    UWidget* FirstSlot = InGridPanel->GetChildAt(0);
    if (!FirstSlot)
    {
        return;
    }

    const FVector2D SlotSize = FirstSlot->GetCachedGeometry().GetLocalSize();

    const FVector2D GridLocalSize = InGridPanel->GetCachedGeometry().GetLocalSize();
    const FVector2D CellStride(GridLocalSize.X / InGridWidth, GridLocalSize.Y / InGridHeight);
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
}
