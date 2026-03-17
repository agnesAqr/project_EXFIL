// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Project_EXFILCharacter.h"
#include "EXFILCharacter.generated.h"

class UInventoryComponent;

/**
 * AEXFILCharacter — EXFIL 프로젝트의 플레이어 캐릭터
 * 인벤토리, 장비, 크래프팅 등 게임 시스템 컴포넌트의 부착 대상
 */
UCLASS()
class PROJECT_EXFIL_API AEXFILCharacter : public AProject_EXFILCharacter
{
	GENERATED_BODY()

public:
	AEXFILCharacter();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInventoryComponent> InventoryComponent;
};
