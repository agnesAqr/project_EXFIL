// Copyright Project EXFIL. All Rights Reserved.

#include "GAS/GA_Fire.h"
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Data/Equipment/EquipmentComponent.h"
#include "Core/EXFILCharacter.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"

UGA_Fire::UGA_Fire()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

bool UGA_Fire::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    const AActor* AvatarActor = ActorInfo->AvatarActor.Get();
    if (!AvatarActor)
    {
        return false;
    }

    const AEXFILCharacter* Character = Cast<AEXFILCharacter>(AvatarActor);
    if (!Character)
    {
        return false;
    }

    const UEquipmentComponent* Equipment = Character->GetEquipmentComponent();
    return Equipment && Equipment->HasWeaponEquipped();
}

void UGA_Fire::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    AActor* AvatarActor = ActorInfo->AvatarActor.Get();
    if (!AvatarActor)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    APlayerController* PC = Cast<APlayerController>(ActorInfo->PlayerController.Get());
    if (!PC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 인벤토리 UI가 열려있으면 발사 안 함
    AEXFILCharacter* OwnerChar = Cast<AEXFILCharacter>(AvatarActor);
    if (OwnerChar && OwnerChar->IsInventoryUIVisible())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    const FVector TraceStart = CameraLocation;
    const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * FireRange;

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(AvatarActor);

    const bool bHit = AvatarActor->GetWorld()->LineTraceSingleByChannel(
        HitResult, TraceStart, TraceEnd, ECC_Pawn, Params);


    if (bHit && HitResult.GetActor())
    {
        // LocalPredicted: 서버+클라이언트 양쪽 실행 → IsLocallyControlled에서만 RPC 호출하여 중복 방지
        if (OwnerChar && OwnerChar->IsLocallyControlled())
        {
            OwnerChar->Server_ConfirmHit(
                HitResult.GetActor(), HitResult.ImpactPoint, HitResult.ImpactNormal);
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
