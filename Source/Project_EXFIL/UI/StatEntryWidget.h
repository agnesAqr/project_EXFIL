// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GAS/SurvivalViewModel.h"
#include "StatEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

UCLASS(Abstract)
class PROJECT_EXFIL_API UStatEntryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    EExfilStatType StatType = EExfilStatType::Health;

    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    TObjectPtr<UTexture2D> IconTexture;

    
    UFUNCTION(BlueprintCallable, Category = "Stat|UI")
    void UpdateStat(float Current, float Maximum);

    
    void BindToViewModel(USurvivalViewModel* ViewModel, EExfilStatType InStatType);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

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
    static constexpr float InitialStatValue = 0.f;
    static constexpr float LowWarningThreshold = 0.20f;

    float CachedCurrent = InitialStatValue;
    float CachedMaximum = InitialStatValue;

    int32 CachedRoundedCurrent = 0;
    int32 CachedRoundedMax = 0;

    TWeakObjectPtr<USurvivalViewModel> BoundViewModel;

    UFUNCTION()
    void OnStatUpdated(EExfilStatType ChangedStatType, float CurrentValue, float MaxValue);

    FLinearColor GetFillColor(bool bIsLow) const;
    FLinearColor GetNormalFillColor() const;
};
