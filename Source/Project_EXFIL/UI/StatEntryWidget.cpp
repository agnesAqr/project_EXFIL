// Copyright Project EXFIL. All Rights Reserved.

#include "UI/StatEntryWidget.h"
#include "CoreMinimal.h"
#include "Internationalization/StringTableRegistry.h"
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
        TextBlock_Value->SetText(FText::FromString(TEXT("0/0")));
        TextBlock_Value->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.5f));
    }
    if (TextBlock_StatLabel)
    {
        static const TMap<EExfilStatType, FText> StatLabels =
        {
            { EExfilStatType::Health,  LOCTABLE("/Game/Localization/ST_UI", "Stat.Health")  },
            { EExfilStatType::Hunger,  LOCTABLE("/Game/Localization/ST_UI", "Stat.Hunger")  },
            { EExfilStatType::Thirst,  LOCTABLE("/Game/Localization/ST_UI", "Stat.Thirst")  },
            { EExfilStatType::Stamina, LOCTABLE("/Game/Localization/ST_UI", "Stat.Stamina") },
        };
        if (const FText* Label = StatLabels.Find(StatType))
        {
            TextBlock_StatLabel->SetText(*Label);
        }
    }
}

void UStatEntryWidget::NativeDestruct()
{
    if (BoundViewModel.IsValid())
    {
        if (StatChangedHandle.IsValid())
        {
            BoundViewModel->OnStatChanged.Remove(StatChangedHandle);
        }
    }
    StatChangedHandle.Reset();
    BoundViewModel.Reset();

    Super::NativeDestruct();
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
        Image_TrackFill->SetColorAndOpacity(GetFillColor(bIsLow));
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

void UStatEntryWidget::BindToViewModel(USurvivalViewModel* ViewModel, EExfilStatType InStatType)
{
    StatType = InStatType;

    if (BoundViewModel.IsValid())
    {
        if (StatChangedHandle.IsValid())
        {
            BoundViewModel->OnStatChanged.Remove(StatChangedHandle);
        }
    }
    StatChangedHandle.Reset();

    BoundViewModel = ViewModel;
    if (!ViewModel)
    {
        return;
    }

    UpdateStat(
        ViewModel->GetStatValue(InStatType),
        ViewModel->GetMaxStatValue(InStatType));
    StatChangedHandle = ViewModel->OnStatChanged.AddUObject(
        this, &UStatEntryWidget::OnStatUpdated);
}

void UStatEntryWidget::OnStatUpdated(
    EExfilStatType ChangedStatType,
    float CurrentValue,
    float MaxValue)
{
    if (ChangedStatType != StatType)
    {
        return;
    }

    UpdateStat(CurrentValue, MaxValue);
}

FLinearColor UStatEntryWidget::GetFillColor(bool bIsLow) const
{
    static const FLinearColor CriticalColor(0.886f, 0.294f, 0.290f, 1.f);
    static const FLinearColor HungerColor(0.937f, 0.624f, 0.153f, 1.f);
    static const FLinearColor ThirstColor(0.216f, 0.541f, 0.867f, 1.f);
    static const FLinearColor StaminaColor(0.114f, 0.620f, 0.459f, 1.f);

    switch (StatType)
    {
    case EExfilStatType::Health:  return CriticalColor;
    case EExfilStatType::Hunger:  return bIsLow ? CriticalColor : HungerColor;
    case EExfilStatType::Thirst:  return bIsLow ? CriticalColor : ThirstColor;
    case EExfilStatType::Stamina: return bIsLow ? HungerColor : StaminaColor;
    default:                      return FLinearColor::White;
    }
}

FLinearColor UStatEntryWidget::GetNormalFillColor() const
{
    return GetFillColor(false);
}
