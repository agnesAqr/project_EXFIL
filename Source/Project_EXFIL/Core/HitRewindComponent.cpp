// Copyright Project EXFIL. All Rights Reserved.

#include "Core/HitRewindComponent.h"
#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"

UHitRewindComponent::UHitRewindComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(false);
}

void UHitRewindComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    const AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }

    const ACharacter* Char = Cast<ACharacter>(Owner);
    const UCapsuleComponent* Capsule = Char ? Char->GetCapsuleComponent() : nullptr;
    const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
    if (!Capsule || !GameState)
    {
        return;
    }

    const float Now = GameState->GetServerWorldTimeSeconds();

    const float SampleInterval = (HitHistorySampleRate > 0.f) ? (1.f / HitHistorySampleRate) : 0.f;
    if (History.Num() > 0 && (Now - History.Last().ServerTime) < SampleInterval)
    {
        return;
    }

    FRewindFrame Frame;
    Frame.ServerTime = Now;
    Frame.Location = Capsule->GetComponentLocation();
    Frame.CapsuleRadius = Capsule->GetScaledCapsuleRadius();
    Frame.CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    History.Add(Frame);

    // 보간 여유로 window보다 1프레임 더 남긴다.
    const float Cutoff = Now - MaxHitRewindSeconds;
    int32 PruneCount = 0;
    while (PruneCount + 1 < History.Num() && History[PruneCount + 1].ServerTime < Cutoff)
    {
        ++PruneCount;
    }
    if (PruneCount > 0)
    {
        History.RemoveAt(0, PruneCount, EAllowShrinking::No);
    }
}

bool UHitRewindComponent::GetCapsuleAtTime(float ServerTime, FVector& OutLoc,
    float& OutRadius, float& OutHalfHeight) const
{
    if (History.Num() == 0)
    {
        return false;
    }

    // 요청 시각이 history 범위 밖이면 실패 → 호출자가 current fallback 결정.
    if (ServerTime < History[0].ServerTime || ServerTime > History.Last().ServerTime)
    {
        return false;
    }

    for (int32 i = 1; i < History.Num(); ++i)
    {
        const FRewindFrame& Next = History[i];
        if (Next.ServerTime < ServerTime)
        {
            continue;
        }

        const FRewindFrame& Prev = History[i - 1];
        const float Span = Next.ServerTime - Prev.ServerTime;
        const float Alpha = (Span > KINDA_SMALL_NUMBER) ? (ServerTime - Prev.ServerTime) / Span : 0.f;

        OutLoc = FMath::Lerp(Prev.Location, Next.Location, Alpha);
        OutRadius = FMath::Lerp(Prev.CapsuleRadius, Next.CapsuleRadius, Alpha);
        OutHalfHeight = FMath::Lerp(Prev.CapsuleHalfHeight, Next.CapsuleHalfHeight, Alpha);
        return true;
    }

    return false;
}
