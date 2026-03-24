// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Project_EXFILGameMode.h"
#include "EXFILGameMode.generated.h"

/**
 * AEXFILGameMode — EXFIL 프로젝트 메인 GameMode
 *
 * Day 7: 테스트용 WorldItem 스폰 (BeginPlay)
 * Day 8: 레벨 디자인으로 교체 예정
 */
UCLASS()
class PROJECT_EXFIL_API AEXFILGameMode : public AProject_EXFILGameMode
{
    GENERATED_BODY()

public:
    AEXFILGameMode();

protected:
    virtual void BeginPlay() override;

private:
    /** 테스트용 WorldItem을 월드에 스폰 (서버 전용) */
    void SpawnTestWorldItems();
};
