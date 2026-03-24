// Copyright Project EXFIL. All Rights Reserved.

#include "UI/StatEntryWidget.h"
#include "CoreMinimal.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GAS/SurvivalViewModel.h"

void UStatEntryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // TrackBg: 반투명 흰색
    if (Image_TrackBg)
    {
        Image_TrackBg->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.08f));
    }

    // 초기 Fill 색상 적용
    if (Image_TrackFill)
    {
        Image_TrackFill->SetColorAndOpacity(GetNormalFillColor());
    }

    // 초기 텍스트
    if (TextBlock_Value)
    {
        TextBlock_Value->SetText(FText::FromString(TEXT("100/100")));
        TextBlock_Value->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.5f));
    }

    // StatType별 라벨 텍스트 자동 설정
    if (TextBlock_StatLabel)
    {
        static const TMap<EExfilStatType, FText> StatLabels =
        {
            { EExfilStatType::Health,  NSLOCTEXT("Stat", "HP", "HP") },
            { EExfilStatType::Hunger,  NSLOCTEXT("Stat", "HU", "HU") },
            { EExfilStatType::Thirst,  NSLOCTEXT("Stat", "TH", "TH") },
            { EExfilStatType::Stamina, NSLOCTEXT("Stat", "ST", "ST") },
        };
        if (const FText* Label = StatLabels.Find(StatType))
        {
            TextBlock_StatLabel->SetText(*Label);
        }
    }
}

void UStatEntryWidget::UpdateStat(float Current, float Maximum)
{
    if (Maximum <= 0.f)
    {
        return;
    }

    CachedCurrent = Current;
    CachedMaximum = Maximum;

    const float Percent = FMath::Clamp(Current / Maximum, 0.f, 1.f);
    const bool bIsLow = Percent <= LowWarningThreshold;

    // TrackFill 너비 비율 업데이트
    if (Image_TrackFill)
    {
        Image_TrackFill->SetRenderScale(FVector2D(Percent, 1.f));

        FLinearColor FillColor;
        switch (StatType)
        {
        case EExfilStatType::Health:
            FillColor = FLinearColor(0.886f, 0.294f, 0.290f, 1.f); // #E24B4A
            break;
        case EExfilStatType::Hunger:
            FillColor = bIsLow
                ? FLinearColor(0.886f, 0.294f, 0.290f, 1.f)  // #E24B4A
                : FLinearColor(0.937f, 0.624f, 0.153f, 1.f); // #EF9F27
            break;
        case EExfilStatType::Thirst:
            FillColor = bIsLow
                ? FLinearColor(0.886f, 0.294f, 0.290f, 1.f)  // #E24B4A
                : FLinearColor(0.216f, 0.541f, 0.867f, 1.f); // #378ADD
            break;
        case EExfilStatType::Stamina:
            FillColor = bIsLow
                ? FLinearColor(0.937f, 0.624f, 0.153f, 1.f)  // #EF9F27
                : FLinearColor(0.114f, 0.620f, 0.459f, 1.f); // #1D9E75
            break;
        default:
            FillColor = GetNormalFillColor();
            break;
        }
        Image_TrackFill->SetColorAndOpacity(FillColor);
    }

    // 텍스트: "100/100"
    if (TextBlock_Value)
    {
        const FString ValueStr = FString::Printf(
            TEXT("%d/%d"), FMath::RoundToInt(Current), FMath::RoundToInt(Maximum));
        TextBlock_Value->SetText(FText::FromString(ValueStr));
    }
}

void UStatEntryWidget::BindToViewModel(USurvivalViewModel* ViewModel, FName InStatName)
{
    TrackedStatName = InStatName;
    if (!ViewModel)
    {
        return;
    }

    // 초기값 즉시 반영
    UpdateStat(ViewModel->GetStatValue(InStatName), ViewModel->GetMaxStatValue(InStatName));

    // 이후 변경 구독
    ViewModel->OnStatChanged.AddDynamic(this, &UStatEntryWidget::OnStatUpdated);
}

void UStatEntryWidget::OnStatUpdated(FName StatName, float NewValue)
{
    UE_LOG(LogTemp, Warning, TEXT("[STAT-5] StatEntry: %s = %.1f, tracked=%s"),
        *StatName.ToString(), NewValue, *TrackedStatName.ToString());

    if (StatName == TrackedStatName)
    {
        UpdateStat(NewValue, CachedMaximum);
    }
}

FLinearColor UStatEntryWidget::GetNormalFillColor() const
{
    switch (StatType)
    {
    case EExfilStatType::Health:  return FLinearColor(0.886f, 0.294f, 0.290f, 1.f); // #E24B4A
    case EExfilStatType::Hunger:  return FLinearColor(0.937f, 0.624f, 0.153f, 1.f); // #EF9F27
    case EExfilStatType::Thirst:  return FLinearColor(0.216f, 0.541f, 0.867f, 1.f); // #378ADD
    case EExfilStatType::Stamina: return FLinearColor(0.114f, 0.620f, 0.459f, 1.f); // #1D9E75
    default:                      return FLinearColor::White;
    }
}
