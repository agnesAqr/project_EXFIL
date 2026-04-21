// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldItem.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UTextRenderComponent;

UCLASS()
class PROJECT_EXFIL_API AWorldItem : public AActor
{
    GENERATED_BODY()

public:
    AWorldItem();
    void InitializeItem(FName InItemDataID, int32 InStackCount);
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WorldItem")
    FName GetItemDataID() const { return ItemDataID; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WorldItem")
    int32 GetStackCount() const { return StackCount; }

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:

    
    UPROPERTY(EditAnywhere, Category = "WorldItem|Config")
    float InteractionSphereRadius = 150.f;

    
    UPROPERTY(EditAnywhere, Category = "WorldItem|Config")
    float ItemNameTextHeight = 70.f;

    
    UPROPERTY(EditAnywhere, Category = "WorldItem|Config")
    float ItemNameTextSize = 35.f;
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USphereComponent> InteractionSphere;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UTextRenderComponent> ItemNameText;
    UPROPERTY(ReplicatedUsing = OnRep_ItemData)
    FName ItemDataID;

    UPROPERTY(Replicated)
    int32 StackCount = 1;
    UFUNCTION()
    void OnRep_ItemData();
    void UpdateVisual();
};
