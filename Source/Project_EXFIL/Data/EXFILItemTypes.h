// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Inventory/EXFILInventoryTypes.h"

// GameplayAbilities 모듈은 Day 4에서 추가 — 전방 선언으로 TSoftClassPtr 컴파일 허용
class UGameplayEffect;

#include "EXFILItemTypes.generated.h"

// ─────────────────────────────────────────────
// EItemType — 아이템 분류
// ─────────────────────────────────────────────
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

// ─────────────────────────────────────────────
// FItemData : FTableRowBase — 아이템 정적 정의
// ─────────────────────────────────────────────
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

    /** 아이콘 텍스처 — Soft Reference로 필요할 때만 로드 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|UI")
    TSoftObjectPtr<UTexture2D> Icon;

    /** 소비 시 적용할 GE (Day 4에서 활성화) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|GAS")
    TSoftClassPtr<UGameplayEffect> ConsumableEffect;

    /** 장착 시 적용할 GE (Day 4에서 활성화) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|GAS")
    TSoftClassPtr<UGameplayEffect> EquipmentEffect;

    /** 장비 슬롯 태그 — Equipment일 때만 유효 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Equipment")
    FName EquipmentSlotTag;

    /** FItemSize 변환 헬퍼 */
    FItemSize GetItemSize() const
    {
        return FItemSize(SizeWidth, SizeHeight);
    }
};

// ─────────────────────────────────────────────
// FCraftingIngredient — 레시피 재료 1종
// ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FCraftingIngredient
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ItemDataID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 RequiredCount = 1;
};

// ─────────────────────────────────────────────
// FCraftingRecipe : FTableRowBase — 크래프팅 레시피
// ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FCraftingRecipe : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
    FText RecipeName;

    /** TArray는 CSV 임포트 불가 — 에디터에서 직접 입력 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
    TArray<FCraftingIngredient> Ingredients;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
    FName ResultItemID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
    int32 ResultCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
    float CraftDuration = 3.0f;

    /** 빈 Name = 맨손 제작 가능 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
    FName RequiredStation;
};
