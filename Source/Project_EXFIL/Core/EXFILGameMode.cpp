// Copyright Project EXFIL. All Rights Reserved.

#include "Core/EXFILGameMode.h"
#include "CoreMinimal.h"
#include "World/WorldItem.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Core/EXFILLog.h"

AEXFILGameMode::AEXFILGameMode()
{
}

void AEXFILGameMode::BeginPlay()
{
    Super::BeginPlay();

    // GameMode는 서버에서만 존재하므로 HasAuthority() 불필요
    SpawnTestWorldItems();
}

void AEXFILGameMode::SpawnTestWorldItems()
{
    // PlayerStart 위치를 기준점으로 사용 — 플레이어 주변에 아이템 배치
    FVector Origin = FVector::ZeroVector;

    AActor* PlayerStartActor = UGameplayStatics::GetActorOfClass(
        GetWorld(), APlayerStart::StaticClass());
    if (PlayerStartActor)
    {
        Origin = PlayerStartActor->GetActorLocation();
        UE_LOG(LogEXFIL, Log, TEXT("SpawnTestWorldItems: PlayerStart = %s"), *Origin.ToString());
    }
    else
    {
        UE_LOG(LogEXFIL, Warning,
            TEXT("SpawnTestWorldItems: PlayerStart not found — using world origin"));
    }

    // Origin으로부터의 상대 오프셋 (cm)
    struct FTestSpawn
    {
        FName  ItemDataID;
        int32  StackCount;
        FVector Offset; // PlayerStart 기준 오프셋
    };

    // 플레이어 위치 기준 정사각형 꼭짓점 4개, Z는 맵 지형 위로 충분히 띄움 (250cm)
    const TArray<FTestSpawn> TestItems =
    {
        { FName("Bandage"),     3, FVector( 400.f,  400.f, 250.f) },
        { FName("Helmet"),      1, FVector(-400.f,  400.f, 250.f) },
        { FName("Pistol"),      1, FVector(-400.f, -400.f, 250.f) },
        { FName("SniperRifle"), 1, FVector( 400.f, -400.f, 250.f) },
    };

    for (const FTestSpawn& Spawn : TestItems)
    {
        const FVector SpawnLocation = Origin + Spawn.Offset;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        AWorldItem* Item = GetWorld()->SpawnActor<AWorldItem>(
            AWorldItem::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params);

        if (Item)
        {
            Item->InitializeItem(Spawn.ItemDataID, Spawn.StackCount);
            UE_LOG(LogEXFIL, Log, TEXT("SpawnTestWorldItems: '%s' x%d @ %s"),
                *Spawn.ItemDataID.ToString(), Spawn.StackCount, *SpawnLocation.ToString());
        }
    }
}
