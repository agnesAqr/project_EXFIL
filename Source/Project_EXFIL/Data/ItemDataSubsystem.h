// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/EXFILItemTypes.h"
#include "ItemDataSubsystem.generated.h"

/**
 * UItemDataSubsystem — 아이템·레시피 DataTable 로딩 및 조회 중앙 관리자
 *
 * 접근 방법:
 *   UGameInstance* GI = GetWorld()->GetGameInstance();
 *   UItemDataSubsystem* Sub = GI->GetSubsystem<UItemDataSubsystem>();
 */
UCLASS()
class PROJECT_EXFIL_API UItemDataSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** RowName으로 FItemData 조회. 없으면 nullptr (C++ 전용 — UHT USTRUCT 포인터 반환 불가) */
    const FItemData* GetItemData(FName ItemDataID) const;

    /** DataTable에 등록된 모든 RowName 반환 */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ItemData")
    TArray<FName> GetAllItemIDs() const;

    /** 특정 타입의 RowName만 반환 */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ItemData")
    TArray<FName> GetItemIDsByType(EItemType Type) const;

    /** RowName으로 FCraftingRecipe 조회. 없으면 nullptr (C++ 전용 — UHT USTRUCT 포인터 반환 불가) */
    const FCraftingRecipe* GetCraftingRecipe(FName RecipeID) const;

    /** 크래프팅 DataTable의 모든 RowName 반환 */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ItemData")
    TArray<FName> GetAllRecipeIDs() const;

private:
    UPROPERTY()
    TObjectPtr<UDataTable> ItemDataTable;

    UPROPERTY()
    TObjectPtr<UDataTable> CraftingRecipeTable;

    void LoadDataTables();
};
