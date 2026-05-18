// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/EXFILItemTypes.h"
#include "ItemDataSubsystem.generated.h"

UCLASS()
class PROJECT_EXFIL_API UItemDataSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    
    const FItemData* GetItemData(FName ItemDataID) const;

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ItemData")
    TArray<FName> GetAllItemIDs() const;

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ItemData")
    TArray<FName> GetItemIDsByType(EItemType Type) const;

    
    const FCraftingRecipe* GetCraftingRecipe(FName RecipeID) const;

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ItemData")
    TArray<FName> GetAllRecipeIDs() const;

    
    UTexture2D* GetCachedTexture(const TSoftObjectPtr<UTexture2D>& SoftPtr);

    
    TSubclassOf<UGameplayEffect> GetCachedEffect(const TSoftClassPtr<UGameplayEffect>& SoftPtr);

#if WITH_DEV_AUTOMATION_TESTS
    void SetTablesForTests(UDataTable* InItemTable, UDataTable* InRecipeTable)
    {
        ItemDataTable = InItemTable;
        CraftingRecipeTable = InRecipeTable;
    }
#endif

private:
    UPROPERTY()
    TObjectPtr<UDataTable> ItemDataTable;

    UPROPERTY()
    TObjectPtr<UDataTable> CraftingRecipeTable;

    void LoadDataTables();

    UPROPERTY()
    TMap<FSoftObjectPath, UTexture2D*> TextureCache;

    UPROPERTY()
    TMap<FSoftObjectPath, UClass*> EffectClassCache;
};
