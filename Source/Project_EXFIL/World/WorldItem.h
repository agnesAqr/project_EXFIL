// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldItem.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UTextRenderComponent;

/**
 * AWorldItem — 월드에 드롭된 아이템 액터
 *
 * bReplicates=true: 서버 스폰 → 모든 클라이언트에 자동 생성
 * SetReplicatingMovement(true): 물리 낙하 후 위치 동기화
 * Day 7: 픽업/드롭 리플리케이션 핵심 클래스
 */
UCLASS()
class PROJECT_EXFIL_API AWorldItem : public AActor
{
    GENERATED_BODY()

public:
    AWorldItem();

    // === 초기화 (서버 전용) ===
    void InitializeItem(FName InItemDataID, int32 InStackCount);

    // === 접근자 ===
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WorldItem")
    FName GetItemDataID() const { return ItemDataID; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WorldItem")
    int32 GetStackCount() const { return StackCount; }

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    // === 설정 ===

    /** 인터랙션 감지 스피어 반경 (cm) */
    UPROPERTY(EditAnywhere, Category = "WorldItem|Config")
    float InteractionSphereRadius = 150.f;

    /** 아이템명 텍스트 표시 높이 — 메시 기준 상대 위치 (cm) */
    UPROPERTY(EditAnywhere, Category = "WorldItem|Config")
    float ItemNameTextHeight = 70.f;

    /** 아이템명 텍스트 월드 크기 (pt) */
    UPROPERTY(EditAnywhere, Category = "WorldItem|Config")
    float ItemNameTextSize = 35.f;

    // === 컴포넌트 ===
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USphereComponent> InteractionSphere;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UTextRenderComponent> ItemNameText;

    // === Replicated 데이터 ===
    UPROPERTY(ReplicatedUsing = OnRep_ItemData)
    FName ItemDataID;

    UPROPERTY(Replicated)
    int32 StackCount = 1;

    // === OnRep ===
    UFUNCTION()
    void OnRep_ItemData();

    // === 비주얼 갱신 ===
    void UpdateVisual();
};
