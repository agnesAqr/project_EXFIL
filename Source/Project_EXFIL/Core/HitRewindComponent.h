// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HitRewindComponent.generated.h"

// 서버 전용, replicated 아님.
USTRUCT()
struct FRewindFrame
{
    GENERATED_BODY()

    float ServerTime = 0.f;
    FVector Location = FVector::ZeroVector;
    float CapsuleRadius = 0.f;
    float CapsuleHalfHeight = 0.f;
};

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class PROJECT_EXFIL_API UHitRewindComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHitRewindComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // 과거 capsule 보간 조회. history가 시각을 못 덮으면 false → 호출자가 fallback 결정.
    bool GetCapsuleAtTime(float ServerTime, FVector& OutLoc, float& OutRadius, float& OutHalfHeight) const;

    float GetMaxHitRewindSeconds() const { return MaxHitRewindSeconds; }

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Rewind")
    float MaxHitRewindSeconds = 0.2f;

    UPROPERTY(EditDefaultsOnly, Category = "Rewind")
    float HitHistorySampleRate = 30.f;

private:
    TArray<FRewindFrame> History;
};
