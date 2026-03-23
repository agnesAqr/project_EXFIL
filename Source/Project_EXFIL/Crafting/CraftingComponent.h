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

/**
 * UCraftingComponent — 크래프팅 로직 컴포넌트
 *
 * Day 6: Server RPC + 상태 리플리케이션
 * StartCraft() → 재료 소비 → 타이머 → OnCraftTimerComplete() → 결과물 추가
 * CancelCraft() → 타이머 클리어 → 재료 복구
 *
 * 의존: UInventoryComponent, UItemDataSubsystem
 */
UCLASS(ClassGroup=(Crafting), meta=(BlueprintSpawnableComponent))
class PROJECT_EXFIL_API UCraftingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCraftingComponent();

    /** 크래프팅 가능 여부 확인 (재료 보유 체크) */
    UFUNCTION(BlueprintCallable, Category = "Crafting")
    bool CanCraft(FName RecipeID) const;

    /** 크래프팅 시작 (재료 소비 → 타이머 시작 → 결과물 생성) */
    UFUNCTION(BlueprintCallable, Category = "Crafting")
    bool StartCraft(FName RecipeID);

    /** 크래프팅 취소 (소비한 재료 복구) */
    UFUNCTION(BlueprintCallable, Category = "Crafting")
    void CancelCraft();

    /** 현재 크래프팅 중인지 */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting")
    bool IsCrafting() const { return bIsCrafting; }

    /** 현재 크래프팅 중인 레시피 ID */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting")
    FName GetCurrentRecipeID() const { return CurrentRecipeID; }

    /** 사용 가능한 레시피 목록 (DataTable 전체 — CanCraft 여부 포함) */
    UFUNCTION(BlueprintCallable, Category = "Crafting")
    TArray<FName> GetAvailableRecipes() const;

    // ========== Server RPCs (Day 6) ==========

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_StartCraft(FName RecipeID);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_CancelCraft();

    // ========== 델리게이트 ==========

    UPROPERTY(BlueprintAssignable, Category = "Crafting|Events")
    FOnCraftingStateChanged OnCraftingStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Crafting|Events")
    FOnCraftingCompleted OnCraftingCompleted;

    // ========== Replication (Day 6) ==========
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

private:
    // ========== Replicated 상태 (Day 6) ==========

    UPROPERTY(ReplicatedUsing = OnRep_CraftingState)
    bool bIsCrafting = false;

    UPROPERTY(Replicated)
    FName CurrentRecipeID;

    UFUNCTION()
    void OnRep_CraftingState();

    // ========== 서버 전용 ==========

    FTimerHandle CraftTimerHandle;

    /** 취소 시 재료를 복구하기 위해 소비 직전 재료 목록 스냅샷 저장 */
    struct FConsumedIngredient
    {
        FName ItemDataID;
        int32 Count = 0;
    };
    TArray<FConsumedIngredient> ConsumedIngredients;

    void OnCraftTimerComplete();

    /** 소유 Actor에서 InventoryComponent 획득 헬퍼 */
    UInventoryComponent* GetInventoryComp() const;

    /** ItemDataSubsystem 획득 헬퍼 */
    class UItemDataSubsystem* GetItemDataSubsystem() const;
};
