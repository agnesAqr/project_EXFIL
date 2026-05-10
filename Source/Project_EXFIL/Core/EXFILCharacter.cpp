// Copyright Project EXFIL. All Rights Reserved.

#include "EXFILCharacter.h"
#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "GAS/SurvivalAttributeSet.h"
#include "Inventory/InventoryComponent.h"
#include "Crafting/CraftingComponent.h"
#include "Data/Equipment/EquipmentComponent.h"
#include "Core/EXFILPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EngineUtils.h"
#include "World/WorldItem.h"
#include "GameplayEffect.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "Core/EXFILLog.h"
#include "Components/CapsuleComponent.h"

AEXFILCharacter::AEXFILCharacter()
{
    bReplicates = true;

    InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
    CraftingComponent  = CreateDefaultSubobject<UCraftingComponent>(TEXT("CraftingComponent"));
    EquipmentComponent = CreateDefaultSubobject<UEquipmentComponent>(TEXT("EquipmentComponent"));
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    SurvivalAttributes = CreateDefaultSubobject<USurvivalAttributeSet>(TEXT("SurvivalAttributes"));
}

void AEXFILCharacter::InitAbilityActorInfoForClient()
{
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }
}

void AEXFILCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    if (!AbilitySystemComponent)
    {
        return;
    }

    AbilitySystemComponent->InitAbilityActorInfo(this, this);
    if (HasAuthority() && GA_FireClass &&
        !AbilitySystemComponent->FindAbilitySpecFromClass(GA_FireClass))
    {
        FGameplayAbilitySpec FireSpec(GA_FireClass, 1, INDEX_NONE, this);
        AbilitySystemComponent->GiveAbility(FireSpec);
    }
}

void AEXFILCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (USpringArmComponent* SpringArm = GetCameraBoom())
    {
        DefaultArmLength = SpringArm->TargetArmLength;
        DefaultSocketOffset = SpringArm->SocketOffset;
    }
    if (HasAuthority())
    {
        if (InventoryComponent)
        {
            InventoryComponent->AddItemByID_Internal(FName("Bandage"),    3);
            InventoryComponent->AddItemByID_Internal(FName("Pistol"), 2);
            InventoryComponent->AddItemByID_Internal(FName("BodyArmor"));
            InventoryComponent->AddItemByID_Internal(FName("Painkillers"), 5);
            InventoryComponent->AddItemByID_Internal(FName("Medkit"));
            ForceNetUpdate();
        }
    }
}

void AEXFILCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput =
            Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (IA_Interact)
        {
            EnhancedInput->BindAction(
                IA_Interact, ETriggerEvent::Started, this,
                &AEXFILCharacter::OnInteractPressed);
        }

        if (IA_Fire)
        {
            EnhancedInput->BindAction(
                IA_Fire, ETriggerEvent::Started, this,
                &AEXFILCharacter::OnFirePressed);
        }

        if (IA_Aim)
        {
            EnhancedInput->BindAction(
                IA_Aim, ETriggerEvent::Started, this,
                &AEXFILCharacter::OnAimToggled);
        }

    }
}

void AEXFILCharacter::OnInteractPressed()
{
    if (IsInventoryUIVisible()) return;

    AWorldItem* Target = TraceForWorldItem();
    if (!Target) return;

    Server_RequestPickupItem(Target);
}

AWorldItem* AEXFILCharacter::TraceForWorldItem() const
{
    AWorldItem* NearestItem = nullptr;
    float NearestDist = InteractionDistance;

    for (TActorIterator<AWorldItem> It(GetWorld()); It; ++It)
    {
        const float Dist = FVector::Dist(GetActorLocation(), It->GetActorLocation());
        if (Dist < NearestDist)
        {
            NearestDist = Dist;
            NearestItem = *It;
        }
    }

    return NearestItem;
}

void AEXFILCharacter::Server_RequestPickupItem_Implementation(AWorldItem* TargetItem)
{
    ExecutePickup(TargetItem);
}

void AEXFILCharacter::ExecutePickup(AWorldItem* TargetItem)
{
    if (!HasAuthority())
    {
        return;
    }
    if (!IsValid(TargetItem)) return;
    const float Distance = FVector::Dist(GetActorLocation(), TargetItem->GetActorLocation());
    if (Distance > MaxPickupDistance)
    {
        UE_LOG(LogEXFIL, Warning, TEXT("ExecutePickup: distance exceeded - %.1fcm > %.1fcm"),
            Distance, MaxPickupDistance);
        return;
    }
    UInventoryComponent* Inventory = FindComponentByClass<UInventoryComponent>();
    if (!Inventory) return;

    const bool bAdded = Inventory->AddItemByID_Internal(
        TargetItem->GetItemDataID(), TargetItem->GetStackCount());

    if (!bAdded)
    {
        Client_ShowNotification(FString::Printf(
            TEXT("Inventory full - cannot pick up '%s'"),
            *TargetItem->GetItemDataID().ToString()));
        return;
    }
    TargetItem->Destroy();
}

bool AEXFILCharacter::IsInventoryUIVisible() const
{
    if (const AEXFILPlayerController* PC = Cast<AEXFILPlayerController>(GetController()))
    {
        return PC->IsInventoryVisible();
    }
    return false;
}

void AEXFILCharacter::OnFirePressed()
{
    if (IsInventoryUIVisible()) return;
    if (!bIsAiming) return;

    if (AbilitySystemComponent && GA_FireClass)
    {
        FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromClass(GA_FireClass);
        if (Spec)
        {
            AbilitySystemComponent->TryActivateAbility(Spec->Handle);
        }
    }
}

void AEXFILCharacter::OnAimToggled()
{
    if (IsInventoryUIVisible()) return;

    bIsAiming = !bIsAiming;
    if (USpringArmComponent* SpringArm = GetCameraBoom())
    {
        SpringArm->TargetArmLength = bIsAiming ? AimArmLength : DefaultArmLength;
        SpringArm->SocketOffset = bIsAiming ? AimSocketOffset : DefaultSocketOffset;
    }
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (APlayerCameraManager* CM = PC->PlayerCameraManager)
        {
            CM->SetFOV(bIsAiming ? AimFOV : DefaultFOV);
        }
    }
    if (UCharacterMovementComponent* CMC = GetCharacterMovement())
    {
        bUseControllerRotationYaw = bIsAiming;
        CMC->bOrientRotationToMovement = !bIsAiming;
    }
    if (AEXFILPlayerController* AimPC = Cast<AEXFILPlayerController>(GetController()))
    {
        AimPC->SetCrosshairVisible(bIsAiming);
    }
}

void AEXFILCharacter::Server_ConfirmHit_Implementation(
    AActor* HitActor, FVector_NetQuantize HitLocation, FVector_NetQuantize HitNormal)
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    FHitResult VerifyHit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * FireRange;
    GetWorld()->LineTraceSingleByChannel(VerifyHit, CameraLocation, TraceEnd, ECC_Pawn, Params);

    if (VerifyHit.GetActor() != HitActor)
    {
        UE_LOG(LogEXFIL, Warning, TEXT("Server_ConfirmHit: hit verification mismatch"));
        return;
    }

    AEXFILCharacter* TargetChar = Cast<AEXFILCharacter>(HitActor);
    if (!TargetChar || !TargetChar->GetAbilitySystemComponent()) return;

    if (DamageEffectClass && AbilitySystemComponent)
    {
        FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
        Context.AddSourceObject(this);
        Context.AddHitResult(VerifyHit);

        FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
            DamageEffectClass, 1.f, Context);

        if (SpecHandle.IsValid())
        {
            AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(
                *SpecHandle.Data.Get(), TargetChar->GetAbilitySystemComponent());
        }
    }
    TargetChar->Multicast_PlayHitReact();
}

void AEXFILCharacter::Multicast_PlayHitEffect_Implementation(
    FVector_NetQuantize HitLocation, FVector_NetQuantize HitNormal)
{

}

void AEXFILCharacter::Multicast_PlayHitReact_Implementation()
{
    if (!HitOverlayMaterial) return;

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->SetOverlayMaterial(HitOverlayMaterial);

        FTimerHandle ResetTimer;
        GetWorldTimerManager().SetTimer(ResetTimer, [WeakThis = TWeakObjectPtr<AEXFILCharacter>(this)]()
        {
            if (AEXFILCharacter* Self = WeakThis.Get())
            {
                if (USkeletalMeshComponent* M = Self->GetMesh())
                {
                    M->SetOverlayMaterial(nullptr);
                }
            }
        }, HitOverlayDuration, false);
    }
}

void AEXFILCharacter::OnDeath()
{
    if (!HasAuthority()) return;
    if (RespawnPhase != ERespawnPhase::Alive) return;

    RespawnPhase = ERespawnPhase::Dead;
    ApplyDeadState();
    ForceNetUpdate();

    GetWorldTimerManager().SetTimer(
        HideCorpseTimerHandle,
        this,
        &AEXFILCharacter::Server_HideDeadBody,
        CorpseVisibleDuration,
        false);
}

void AEXFILCharacter::Server_HideDeadBody()
{
    if (!HasAuthority()) return;
    if (RespawnPhase != ERespawnPhase::Dead) return;

    RespawnPhase = ERespawnPhase::HiddenDead;
    ApplyHiddenDeadState();
    ForceNetUpdate();

    GetWorldTimerManager().SetTimer(
        RespawnPrepareTimerHandle,
        this,
        &AEXFILCharacter::Server_PrepareRespawn,
        HiddenRespawnDelay,
        false);
}

void AEXFILCharacter::Server_PrepareRespawn()
{
    if (!HasAuthority()) return;
    if (RespawnPhase != ERespawnPhase::HiddenDead) return;

    FVector RespawnLocation = GetActorLocation();
    FRotator RespawnRotation = GetActorRotation();

    if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
    {
        if (AActor* PlayerStart = GM->FindPlayerStart(GetController()))
        {
            RespawnLocation = PlayerStart->GetActorLocation();
            RespawnRotation = PlayerStart->GetActorRotation();
        }
        else
        {
            UE_LOG(LogEXFIL, Warning, TEXT("Server_PrepareRespawn: FindPlayerStart failed - respawning at current location"));
        }
    }

    PendingRespawnLocation = RespawnLocation;
    PendingRespawnRotation = RespawnRotation;

    RespawnPhase = ERespawnPhase::Respawning;
    ApplyRespawningState();
    SnapToPendingRespawnTransform();

    if (AbilitySystemComponent)
    {
        USurvivalAttributeSet* AttrSet = const_cast<USurvivalAttributeSet*>(
            AbilitySystemComponent->GetSet<USurvivalAttributeSet>());
        if (AttrSet)
        {
            AttrSet->SetHealth(AttrSet->GetMaxHealth());
            AttrSet->SetHunger(AttrSet->GetMaxHunger());
            AttrSet->SetThirst(AttrSet->GetMaxThirst());
            AttrSet->SetStamina(AttrSet->GetMaxStamina());
        }
    }

    ForceNetUpdate();

    GetWorldTimerManager().SetTimer(
        RespawnRevealTimerHandle,
        this,
        &AEXFILCharacter::Server_FinishRespawn,
        RespawnRevealDelay,
        false);
}

void AEXFILCharacter::Server_FinishRespawn()
{
    if (!HasAuthority()) return;
    if (RespawnPhase != ERespawnPhase::Respawning) return;

    SnapToPendingRespawnTransform();
    RespawnPhase = ERespawnPhase::Alive;
    ApplyAliveState();
    ForceNetUpdate();
}

void AEXFILCharacter::ApplyDeadState()
{
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->SetSimulatePhysics(true);
        MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
        MeshComp->SetVisibility(true, true);
    }

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (UCharacterMovementComponent* CMC = GetCharacterMovement())
    {
        CMC->StopMovementImmediately();
        CMC->DisableMovement();
    }

    if (IsLocallyControlled())
    {
        if (bIsAiming)
        {
            OnAimToggled();
        }

        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            DisableInput(PC);
        }
    }
}

void AEXFILCharacter::ApplyHiddenDeadState()
{
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (UCharacterMovementComponent* CMC = GetCharacterMovement())
    {
        CMC->StopMovementImmediately();
        CMC->DisableMovement();
    }
}

void AEXFILCharacter::ApplyRespawningState()
{
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->SetSimulatePhysics(false);
        MeshComp->SetCollisionProfileName(TEXT("CharacterMesh"));
        MeshComp->AttachToComponent(
            GetCapsuleComponent(),
            FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        MeshComp->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
        MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
        MeshComp->SetVisibility(true, true);
        MeshComp->SetOverlayMaterial(nullptr);
    }

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (UCharacterMovementComponent* CMC = GetCharacterMovement())
    {
        CMC->StopMovementImmediately();
        CMC->DisableMovement();
    }
}

void AEXFILCharacter::ApplyAliveState()
{
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->SetSimulatePhysics(false);
        MeshComp->SetCollisionProfileName(TEXT("CharacterMesh"));
        MeshComp->AttachToComponent(
            GetCapsuleComponent(),
            FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        MeshComp->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
        MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
        MeshComp->SetVisibility(true, true);
        MeshComp->SetOverlayMaterial(nullptr);
    }

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }

    if (UCharacterMovementComponent* CMC = GetCharacterMovement())
    {
        CMC->StopMovementImmediately();
        CMC->SetMovementMode(MOVE_Walking);
    }

    SetActorEnableCollision(true);
    SetActorHiddenInGame(false);

    if (IsLocallyControlled())
    {
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            EnableInput(PC);
        }
    }
}

void AEXFILCharacter::SnapToPendingRespawnTransform()
{
    SetActorLocationAndRotation(
        PendingRespawnLocation,
        PendingRespawnRotation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);

    if (AController* MyController = GetController())
    {
        MyController->SetControlRotation(PendingRespawnRotation);
    }
}

void AEXFILCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AEXFILCharacter, PendingRespawnLocation);
    DOREPLIFETIME(AEXFILCharacter, PendingRespawnRotation);
    DOREPLIFETIME(AEXFILCharacter, RespawnPhase);
}

void AEXFILCharacter::OnRep_RespawnPhase()
{
    switch (RespawnPhase)
    {
    case ERespawnPhase::Dead:
        ApplyDeadState();
        break;

    case ERespawnPhase::HiddenDead:
        ApplyHiddenDeadState();
        break;

    case ERespawnPhase::Respawning:
        ApplyRespawningState();
        SnapToPendingRespawnTransform();
        break;

    case ERespawnPhase::Alive:
        ApplyAliveState();
        break;
    }
}

void AEXFILCharacter::Client_ShowNotification_Implementation(const FString& Message)
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, Message, true, FVector2D(2.f, 2.f));
    }
}
