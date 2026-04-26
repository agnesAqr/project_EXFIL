// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CraftingComponent.generated.h"

class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnCraftingStateChanged, bool, bIsCrafting, float, RemainingTime);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnCraftingCompleted, FName, RecipeID);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnCraftStartFailed, FName, RecipeID);

UCLASS(ClassGroup=(Crafting), meta=(BlueprintSpawnableComponent))
class PROJECT_EXFIL_API UCraftingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCraftingComponent();

    UFUNCTION(BlueprintCallable, Category = "Crafting")
    bool CanCraft(FName RecipeID) const;

    UFUNCTION(BlueprintCallable, Category = "Crafting")
    void RequestStartCraft(FName RecipeID);

    UFUNCTION(BlueprintCallable, Category = "Crafting")
    void RequestCancelCraft();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting")
    bool IsCrafting() const { return bIsCrafting; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting")
    FName GetCurrentRecipeID() const { return CurrentRecipeID; }

    UFUNCTION(BlueprintCallable, Category = "Crafting")
    TArray<FName> GetAvailableRecipes() const;

    UPROPERTY(BlueprintAssignable, Category = "Crafting|Events")
    FOnCraftingStateChanged OnCraftingStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Crafting|Events")
    FOnCraftingCompleted OnCraftingCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Crafting|Events")
    FOnCraftStartFailed OnCraftStartFailed;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

private:
    UFUNCTION(Server, Reliable)
    void Server_RequestStartCraft(FName RecipeID);

    UFUNCTION(Server, Reliable)
    void Server_RequestCancelCraft();

    UFUNCTION(Client, Reliable)
    void Client_NotifyCraftStartFailed(FName RecipeID);

    bool StartCraft_Internal(FName RecipeID);
    void CancelCraft_Internal();
    void NotifyCraftStartFailed(FName RecipeID);

    UPROPERTY(ReplicatedUsing = OnRep_CraftingState)
    bool bIsCrafting = false;

    UPROPERTY(Replicated)
    FName CurrentRecipeID;

    UFUNCTION()
    void OnRep_CraftingState();

    FTimerHandle CraftTimerHandle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Drop", meta = (AllowPrivateAccess = "true"))
    float ResultDropForwardOffset = 80.f;

    struct FConsumedIngredient
    {
        FName ItemDataID;
        int32 Count = 0;
    };

    TArray<FConsumedIngredient> ConsumedIngredients;

    void OnCraftTimerComplete();

    UInventoryComponent* GetInventoryComp() const;
    class UItemDataSubsystem* GetItemDataSubsystem() const;

    UPROPERTY()
    TObjectPtr<UInventoryComponent> CachedInventoryComp;

    UPROPERTY()
    TObjectPtr<class UItemDataSubsystem> CachedItemSub;
};
