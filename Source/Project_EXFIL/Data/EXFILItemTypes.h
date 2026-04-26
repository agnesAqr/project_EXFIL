// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayEffect.h"
#include "Data/Equipment/EquipmentTypes.h"
#include "Inventory/EXFILInventoryTypes.h"

#include "EXFILItemTypes.generated.h"
UENUM(BlueprintType)
enum class EItemType : uint8
{
    None        UMETA(DisplayName = "None"),
    Consumable  UMETA(DisplayName = "Consumable"),
    Equipment   UMETA(DisplayName = "Equipment"),
    Material    UMETA(DisplayName = "Material"),
    Ammo        UMETA(DisplayName = "Ammo"),
    Quest       UMETA(DisplayName = "Quest")
};
USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FItemData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    EItemType ItemType = EItemType::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Grid")
    int32 SizeWidth = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Grid")
    int32 SizeHeight = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Grid")
    float Weight = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Stack")
    int32 MaxStackCount = 1;

    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|UI")
    TSoftObjectPtr<UTexture2D> Icon;

    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|GAS")
    TSoftClassPtr<UGameplayEffect> ConsumableEffect;

    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|GAS")
    TSoftClassPtr<UGameplayEffect> EquipmentEffect;

    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Equipment")
    TArray<EEquipmentSlot> ValidEquipmentSlots;

    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
    TObjectPtr<UStaticMesh> WorldMesh = nullptr;

    
    FItemSize GetItemSize() const
    {
        return FItemSize(SizeWidth, SizeHeight);
    }
};
USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FCraftingIngredient
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ItemDataID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 RequiredCount = 1;
};
USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FCraftingRecipe : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
    FText RecipeName;

    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
    TArray<FCraftingIngredient> Ingredients;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
    FName ResultItemID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
    int32 ResultCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
    float CraftDuration = 3.0f;

    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
    FName RequiredStation;
};
