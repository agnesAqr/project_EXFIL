// Copyright Project EXFIL. All Rights Reserved.

#include "UI/InventoryDragPreviewWidget.h"
#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"

void UInventoryDragPreviewWidget::BuildPreview(int32 Width, int32 Height)
{
    if (!WidgetTree)
    {
        return;
    }

    UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
    if (!Grid)
    {
        return;
    }

    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            if (!SizeBox)
            {
                continue;
            }
            SizeBox->SetWidthOverride(64.0f);
            SizeBox->SetHeightOverride(64.0f);

            UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
            if (Border)
            {
                Border->SetBrushColor(FLinearColor(0.0f, 0.3f, 0.6f, 0.7f));
                SizeBox->AddChild(Border);
            }

            UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(SizeBox, Y, X);
            if (GridSlot)
            {
                GridSlot->SetColumn(X);
                GridSlot->SetRow(Y);
            }
        }
    }

    WidgetTree->RootWidget = Grid;
}
