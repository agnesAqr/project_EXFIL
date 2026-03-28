// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StatEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;
class USurvivalViewModel;

/**
 * 표시할 스탯 종류
 * 이름 충돌 방지: UE Core Stats.h에 'EStatType'이 이미 정의되어 있으므로
 * 'EExfilStatType'을 사용한다.
 */
UENUM(BlueprintType)
enum class EExfilStatType : uint8
{
    Health   UMETA(DisplayName = "Health"),
    Hunger   UMETA(DisplayName = "Hunger"),
    Thirst   UMETA(DisplayName = "Thirst"),
    Stamina  UMETA(DisplayName = "Stamina")
};

/**
 * UStatEntryWidget — StatsBar 안의 스탯 하나를 표현하는 위젯
 *
 * WBP_StatEntry 레이아웃:
 *   HBox_Entry (root)
 *   ├─ Image_Icon           (BindWidget)
 *   ├─ SizeBox_Track (H:6)
 *   │  └─ Overlay_Track
 *   │     ├─ Image_TrackBg  (BindWidget)
 *   │     └─ Image_TrackFill (BindWidget)
 *   └─ TextBlock_Value      (BindWidget)
 */
UCLASS(Abstract)
class PROJECT_EXFIL_API UStatEntryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** BP 디테일 패널에서 스탯 종류 지정 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    EExfilStatType StatType = EExfilStatType::Health;

    /** 에디터에서 각 StatEntry 인스턴스별 아이콘 텍스처 지정 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    TObjectPtr<UTexture2D> IconTexture;

    /**
     * 스탯 값 갱신.
     * @param Current  현재 값
     * @param Maximum  최대 값
     */
    UFUNCTION(BlueprintCallable, Category = "Stat|UI")
    void UpdateStat(float Current, float Maximum);

    /**
     * SurvivalViewModel에 구독 등록 + 초기값 즉시 반영.
     * InventoryPanelWidget::BindStatsToViewModel에서 호출.
     * @param ViewModel  바인딩할 ViewModel
     * @param InStatName 추적할 속성 이름 (예: "Health")
     */
    void BindToViewModel(USurvivalViewModel* ViewModel, FName InStatName);

protected:
    virtual void NativeOnInitialized() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_Icon;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_TrackBg;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_TrackFill;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TextBlock_Value;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TextBlock_StatLabel;

private:
    float CachedCurrent = 100.f;
    float CachedMaximum = 100.f;

    /** 텍스트 갱신용 캐시 — 정수 반올림값이 동일하면 SetText 스킵 */
    int32 CachedRoundedCurrent = 100;
    int32 CachedRoundedMax = 100;

    /** ViewModel 구독 시 추적할 속성 이름 */
    FName TrackedStatName;

    /** OnStatChanged 델리게이트 콜백 */
    UFUNCTION()
    void OnStatUpdated(FName StatName, float NewValue);

    /** StatType별 기본 Fill 색상 */
    FLinearColor GetNormalFillColor() const;

    /** 경고 임계값 (≤20%) */
    static constexpr float LowWarningThreshold = 0.20f;
};
