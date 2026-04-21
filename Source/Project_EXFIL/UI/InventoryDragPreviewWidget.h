// Copyright Project EXFIL. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryDragPreviewWidget.generated.h"

UCLASS()
class PROJECT_EXFIL_API UInventoryDragPreviewWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    
    void BuildPreview(int32 Width, int32 Height);
};
