// Copyright Project EXFIL. All Rights Reserved.

#include "UI/StatEntryWidget.h"
#include "CoreMinimal.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GAS/SurvivalViewModel.h"

void UStatEntryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (Image_TrackBg)
    {
        Image_TrackBg->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.08f));
    }
    if (Image_Icon && IconTexture)
    {
        Image_Icon->SetBrushFromTexture(IconTexture);
    }
    if (Image_TrackFill)
    {
        Image_TrackFill->SetColorAndOpacity(GetNormalFillColor());
    }
    if (TextBlock_Value)
    {
        TextBlock_Value->SetText(FText::FromString(TEXT("100/100")));
        TextBlock_Value->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.5f));
    }
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
    if (Image_TrackFill)
    {
        Image_TrackFill->SetRenderScale(FVector2D(Percent, 1.f));

        FLinearColor FillColor;
        switch (StatType)
        {
        case EExfilStatType::Health:
            FillColor = FLinearColor(0.886f, 0.294f, 0.290f, 1.f);
            break;
        case EExfilStatType::Hunger:
            FillColor = bIsLow
                ? FLinearColor(0.886f, 0.294f, 0.290f, 1.f)
                : FLinearColor(0.937f, 0.624f, 0.153f, 1.f);
            break;
        case EExfilStatType::Thirst:
            FillColor = bIsLow
                ? FLinearColor(0.886f, 0.294f, 0.290f, 1.f)
                : FLinearColor(0.216f, 0.541f, 0.867f, 1.f);
            break;
        case EExfilStatType::Stamina:
            FillColor = bIsLow
                ? FLinearColor(0.937f, 0.624f, 0.153f, 1.f)
                : FLinearColor(0.114f, 0.620f, 0.459f, 1.f);
            break;
        default:
            FillColor = GetNormalFillColor();
            break;
        }
        Image_TrackFill->SetColorAndOpacity(FillColor);
    }
    if (TextBlock_Value)
    {
        const int32 RoundedCur = FMath::RoundToInt(Current);
        const int32 RoundedMax = FMath::RoundToInt(Maximum);
        if (RoundedCur != CachedRoundedCurrent || RoundedMax != CachedRoundedMax)
        {
            CachedRoundedCurrent = RoundedCur;
            CachedRoundedMax = RoundedMax;
            TextBlock_Value->SetText(FText::FromString(
                FString::Printf(TEXT("%d/%d"), RoundedCur, RoundedMax)));
        }
    }
}

void UStatEntryWidget::BindToViewModel(USurvivalViewModel* ViewModel, FName InStatName)
{
    TrackedStatName = InStatName;
    TrackedMaxName = FName(*FString::Printf(TEXT("Max%s"), *InStatName.ToString()));

    if (BoundViewModel.IsValid())
    {
        BoundViewModel->OnStatChanged.RemoveDynamic(this, &UStatEntryWidget::OnStatUpdated);
    }

    BoundViewModel = ViewModel;
    if (!ViewModel)
    {
        return;
    }

    UpdateStat(
        ViewModel->GetStatValue(InStatName),
        ViewModel->GetMaxStatValue(InStatName));
    ViewModel->OnStatChanged.AddDynamic(this, &UStatEntryWidget::OnStatUpdated);
}

void UStatEntryWidget::OnStatUpdated(FName ChangedFieldName, float NewValue)
{
    if (ChangedFieldName == TrackedStatName)
    {
        CachedCurrent = NewValue;
    }
    else if (ChangedFieldName == TrackedMaxName)
    {
        CachedMaximum = NewValue;
    }
    else
    {
        return;
    }

    UpdateStat(CachedCurrent, CachedMaximum);
}

FLinearColor UStatEntryWidget::GetNormalFillColor() const
{
    switch (StatType)
    {
    case EExfilStatType::Health:  return FLinearColor(0.886f, 0.294f, 0.290f, 1.f);
    case EExfilStatType::Hunger:  return FLinearColor(0.937f, 0.624f, 0.153f, 1.f);
    case EExfilStatType::Thirst:  return FLinearColor(0.216f, 0.541f, 0.867f, 1.f);
    case EExfilStatType::Stamina: return FLinearColor(0.114f, 0.620f, 0.459f, 1.f);
    default:                      return FLinearColor::White;
    }
}
