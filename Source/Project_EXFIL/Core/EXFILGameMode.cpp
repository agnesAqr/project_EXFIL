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
    SpawnTestWorldItems();
}

void AEXFILGameMode::SpawnTestWorldItems()
{
    FVector Origin = FVector::ZeroVector;

    AActor* PlayerStartActor = UGameplayStatics::GetActorOfClass(
        GetWorld(), APlayerStart::StaticClass());
    if (PlayerStartActor)
    {
        Origin = PlayerStartActor->GetActorLocation();
    }
    else
    {
        UE_LOG(LogEXFIL, Warning,
            TEXT("SpawnTestWorldItems: PlayerStart not found — using world origin"));
    }
    struct FTestSpawn
    {
        FName  ItemDataID;
        int32  StackCount;
        FVector Offset;
    };
    const TArray<FTestSpawn> TestItems =
    {
        { FName("Bandage"),     1, FVector( 400.f,  400.f, 250.f) },
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
        }
    }
}
