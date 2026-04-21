// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Project_EXFILGameMode.h"
#include "EXFILGameMode.generated.h"

UCLASS()
class PROJECT_EXFIL_API AEXFILGameMode : public AProject_EXFILGameMode
{
    GENERATED_BODY()

public:
    AEXFILGameMode();

protected:
    virtual void BeginPlay() override;

private:
    
    void SpawnTestWorldItems();
};
