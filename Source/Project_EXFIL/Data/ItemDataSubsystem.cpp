// Copyright Project EXFIL. All Rights Reserved.

#include "Data/ItemDataSubsystem.h"
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Project_EXFIL.h"

void UItemDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadDataTables();
}

void UItemDataSubsystem::LoadDataTables()
{
    // DT_ItemData — CSV에서 임포트한 아이템 DataTable
    ItemDataTable = LoadObject<UDataTable>(
        nullptr,
        TEXT("/Game/Data/DT_ItemData"));

    if (!ItemDataTable)
    {
        UE_LOG(LogProject_EXFIL, Warning,
               TEXT("UItemDataSubsystem: DT_ItemData 로드 실패. "
                    "Content/Data/DT_ItemData.csv를 에디터에서 임포트했는지 확인하세요."));
    }
    else
    {
        UE_LOG(LogProject_EXFIL, Log,
               TEXT("UItemDataSubsystem: DT_ItemData 로드 완료. 행 수: %d"),
               ItemDataTable->GetRowNames().Num());
    }

    // DT_CraftingRecipe — 에디터에서 직접 생성 (TArray 필드 포함으로 CSV 불가)
    CraftingRecipeTable = LoadObject<UDataTable>(
        nullptr,
        TEXT("/Game/Data/DT_CraftingRecipe"));

    if (!CraftingRecipeTable)
    {
        UE_LOG(LogProject_EXFIL, Warning,
               TEXT("UItemDataSubsystem: DT_CraftingRecipe 로드 실패. "
                    "에디터에서 DataTable을 생성했는지 확인하세요."));
    }
    else
    {
        UE_LOG(LogProject_EXFIL, Log,
               TEXT("UItemDataSubsystem: DT_CraftingRecipe 로드 완료. 행 수: %d"),
               CraftingRecipeTable->GetRowNames().Num());
    }
}

const FItemData* UItemDataSubsystem::GetItemData(FName ItemDataID) const
{
    if (!ItemDataTable)
    {
        return nullptr;
    }

    static const FString Context = TEXT("UItemDataSubsystem::GetItemData");
    return ItemDataTable->FindRow<FItemData>(ItemDataID, Context, /*bWarnIfRowMissing=*/false);
}

TArray<FName> UItemDataSubsystem::GetAllItemIDs() const
{
    if (!ItemDataTable)
    {
        return {};
    }
    return ItemDataTable->GetRowNames();
}

TArray<FName> UItemDataSubsystem::GetItemIDsByType(EItemType Type) const
{
    TArray<FName> Result;

    if (!ItemDataTable)
    {
        return Result;
    }

    for (const FName& RowName : ItemDataTable->GetRowNames())
    {
        static const FString Context = TEXT("UItemDataSubsystem::GetItemIDsByType");
        const FItemData* Row = ItemDataTable->FindRow<FItemData>(RowName, Context, false);
        if (Row && Row->ItemType == Type)
        {
            Result.Add(RowName);
        }
    }

    return Result;
}

const FCraftingRecipe* UItemDataSubsystem::GetCraftingRecipe(FName RecipeID) const
{
    if (!CraftingRecipeTable)
    {
        return nullptr;
    }

    static const FString Context = TEXT("UItemDataSubsystem::GetCraftingRecipe");
    return CraftingRecipeTable->FindRow<FCraftingRecipe>(RecipeID, Context, false);
}

TArray<FName> UItemDataSubsystem::GetAllRecipeIDs() const
{
    if (!CraftingRecipeTable)
    {
        return {};
    }
    return CraftingRecipeTable->GetRowNames();
}

// ========== 에셋 캐시 ==========

UTexture2D* UItemDataSubsystem::GetCachedTexture(const TSoftObjectPtr<UTexture2D>& SoftPtr)
{
    if (SoftPtr.IsNull()) return nullptr;

    const FSoftObjectPath& Path = SoftPtr.ToSoftObjectPath();
    if (UTexture2D** Found = TextureCache.Find(Path))
    {
        return *Found;
    }

    UTexture2D* Loaded = SoftPtr.LoadSynchronous();
    if (Loaded)
    {
        TextureCache.Add(Path, Loaded);
    }
    return Loaded;
}

TSubclassOf<UGameplayEffect> UItemDataSubsystem::GetCachedEffect(
    const TSoftClassPtr<UGameplayEffect>& SoftPtr)
{
    if (SoftPtr.IsNull()) return nullptr;

    const FSoftObjectPath& Path = SoftPtr.ToSoftObjectPath();
    if (UClass** Found = EffectClassCache.Find(Path))
    {
        return TSubclassOf<UGameplayEffect>(*Found);
    }

    TSubclassOf<UGameplayEffect> Loaded = SoftPtr.LoadSynchronous();
    if (Loaded)
    {
        EffectClassCache.Add(Path, Loaded.Get());
    }
    return Loaded;
}
