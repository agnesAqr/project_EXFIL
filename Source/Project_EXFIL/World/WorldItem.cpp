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

    PrimaryActorTick.bCanEverTick = true;

    // 메시 (루트) — QueryAndPhysics: 라인 트레이스 + 물리 동시 사용
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    SetRootComponent(MeshComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // 폴백 메시 — 먼저 메시를 세팅한 뒤 물리 활성화해야 피직스 바디가 정상 생성됨
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        MeshComponent->SetStaticMesh(CubeMesh.Object);
    }

    // 메시 세팅 후 물리 활성화
    MeshComponent->SetSimulatePhysics(true);
    MeshComponent->SetEnableGravity(true);

    // 인터랙션 스피어 — Pawn Overlap 전용
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(InteractionSphereRadius);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // 아이템 이름 텍스트 — 디버그용으로 기본 비활성화
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

    // 월드 아이템은 모든 클라이언트가 시각적으로 볼 수 있어야 함 (COND_None)
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

    // ── 진단: 메시 상태 확인 ──
    if (!MeshComponent->GetStaticMesh())
    {
        // ConstructorHelpers 실패 → 런타임 폴백 로드
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

void AWorldItem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!ItemNameText) return;

    // 로컬 플레이어 카메라를 향해 텍스트를 회전 (빌보드)
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    const FVector Dir = (CamLoc - ItemNameText->GetComponentLocation()).GetSafeNormal();
    ItemNameText->SetWorldRotation(Dir.Rotation());
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

    // WorldMesh가 있으면 적용, 없으면 생성자에서 세팅한 엔진 큐브 폴백 유지
    if (Data && Data->WorldMesh)
    {
        MeshComponent->SetStaticMesh(Data->WorldMesh);
    }
    // else: 생성자에서 /Engine/BasicShapes/Cube가 이미 세팅되어 있으므로 별도 처리 불필요

    // 아이템 이름 텍스트 갱신
    // TextRenderComponent 기본 폰트는 한글 미지원 → ItemDataID(ASCII)로 표시
    if (ItemNameText)
    {
        ItemNameText->SetText(FText::FromName(ItemDataID));
    }
}
