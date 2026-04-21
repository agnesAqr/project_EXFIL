// Copyright Project EXFIL. All Rights Reserved.

#include "World/WorldItem.h"
#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/GameInstance.h"
#include "Data/ItemDataSubsystem.h"
#include "Data/EXFILItemTypes.h"
#include "Core/EXFILLog.h"

AWorldItem::AWorldItem()
{
    bReplicates = true;
    bAlwaysRelevant = false;
    SetReplicatingMovement(true);

    PrimaryActorTick.bCanEverTick = false;
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    SetRootComponent(MeshComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        MeshComponent->SetStaticMesh(CubeMesh.Object);
    }
    MeshComponent->SetSimulatePhysics(true);
    MeshComponent->SetEnableGravity(true);
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(InteractionSphereRadius);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    ItemNameText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ItemNameText"));
    ItemNameText->SetupAttachment(RootComponent);
    ItemNameText->SetRelativeLocation(FVector(0.f, 0.f, ItemNameTextHeight));
    ItemNameText->SetHorizontalAlignment(EHTA_Center);
    ItemNameText->SetVerticalAlignment(EVRTA_TextCenter);
    ItemNameText->SetWorldSize(ItemNameTextSize);
    ItemNameText->SetTextRenderColor(FColor::Yellow);
    ItemNameText->SetVisibility(false);

}

void AWorldItem::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWorldItem, ItemDataID);
    DOREPLIFETIME(AWorldItem, StackCount);
}

void AWorldItem::InitializeItem(FName InItemDataID, int32 InStackCount)
{
    checkf(HasAuthority(), TEXT("InitializeItem must be called on server"));

    ItemDataID = InItemDataID;
    StackCount = InStackCount;

    UpdateVisual();
}

void AWorldItem::BeginPlay()
{
    Super::BeginPlay();
    if (!MeshComponent->GetStaticMesh())
    {
        UStaticMesh* FallbackMesh = LoadObject<UStaticMesh>(
            nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (FallbackMesh)
        {
            MeshComponent->SetStaticMesh(FallbackMesh);
            UE_LOG(LogEXFIL, Warning,
                TEXT("AWorldItem: ConstructorHelpers 실패 → 런타임 큐브 폴백 로드"));
        }
        else
        {
            UE_LOG(LogEXFIL, Error,
                TEXT("AWorldItem: 큐브 메시 로드 실패! 경로를 확인하세요."));
        }
    }

    UpdateVisual();
}

void AWorldItem::OnRep_ItemData()
{
    UpdateVisual();
}

void AWorldItem::UpdateVisual()
{
    if (ItemDataID.IsNone()) return;

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UItemDataSubsystem* DataSub = GI->GetSubsystem<UItemDataSubsystem>();
    if (!DataSub) return;

    const FItemData* Data = DataSub->GetItemData(ItemDataID);
    if (Data && Data->WorldMesh)
    {
        MeshComponent->SetStaticMesh(Data->WorldMesh);
    }
    if (ItemNameText)
    {
        ItemNameText->SetText(FText::FromName(ItemDataID));
    }
}
