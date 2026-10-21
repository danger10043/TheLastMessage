#include "UI/CoreDescriptionFormatter.h"

#include "DataAsset/CoreDataAsset.h"

#define LOCTEXT_NAMESPACE "CoreDescription"

DEFINE_LOG_CATEGORY_STATIC(LogCoreDescription, Log, All);

namespace
{
	int32 GetPreviewLevel(const UCoreDataAsset& Data, int32 CurrentLevel)
	{
		const int32 Next = CurrentLevel < MAX_int32 ? FMath::Max(0, CurrentLevel) + 1 : MAX_int32;
		return Data.UpgradeLimitType == ECoreUpgradeLimitType::Custom
			? FMath::Min(Next, FMath::Max(1, Data.MaxLevel)) : Next;
	}

	/** 값을 찾는 규칙은 DA가 담당하고, 여기서는 실패를 화면 대신 로그로 알린다. 같은 실패는 한 번만 기록한다. */
	bool ReadValue(const UCoreDataAsset& Data, const FCoreDescriptionValueBinding& Binding, int32 Level, float& OutValue)
	{
		FText Reason;
		if (Data.TryGetDescriptionValue(Binding, Level, OutValue, &Reason))
		{
			return true;
		}

		static TSet<FString> ReportedFailures;
		const FString Key = FString::Printf(TEXT("%s:%s"), *Data.GetPathName(), *Binding.Token.ToString());
		if (!ReportedFailures.Contains(Key))
		{
			ReportedFailures.Add(Key);
			UE_LOG(LogCoreDescription, Warning, TEXT("%s의 설명 토큰 {%s} 값을 읽지 못해 '—'로 표시합니다: %s"),
				*Data.GetName(), *Binding.Token.ToString(), *Reason.ToString());
		}
		return false;
	}

	FText FormatValue(float Value, ECoreDescriptionValueFormat Format)
	{
		double DisplayValue = Value;
		switch (Format)
		{
		case ECoreDescriptionValueFormat::Percent: DisplayValue = Value * 100.0; break;
		case ECoreDescriptionValueFormat::IncreasePercent: DisplayValue = (Value - 1.0) * 100.0; break;
		case ECoreDescriptionValueFormat::ReductionPercent: DisplayValue = (1.0 - Value) * 100.0; break;
		default: break;
		}
		FNumberFormattingOptions Options;
		Options.UseGrouping = false;
		Options.MinimumFractionalDigits = 0;
		Options.MaximumFractionalDigits = 1;
		return FText::AsNumber(FMath::Abs(DisplayValue) < 0.0001 ? 0.0 : DisplayValue, &Options);
	}
}

FText CoreDescriptionFormatter::MakeLevelText(const UCoreDataAsset& Data, int32 CurrentLevel)
{
	CurrentLevel = FMath::Max(0, CurrentLevel);
	const int32 Next = GetPreviewLevel(Data, CurrentLevel);
	FText Stage;
	if (CurrentLevel == 0) Stage = FText::Format(LOCTEXT("NewLevel", "신규 · Lv.{0}"), FText::AsNumber(Next));
	else if (Next > CurrentLevel) Stage = FText::Format(LOCTEXT("UpgradeLevel", "Lv.{0} → Lv.{1}"), FText::AsNumber(CurrentLevel), FText::AsNumber(Next));
	else Stage = FText::Format(LOCTEXT("CurrentLevel", "Lv.{0}"), FText::AsNumber(CurrentLevel));
	if (Data.UpgradeLimitType == ECoreUpgradeLimitType::Custom)
	{
		Stage = FText::Format(LOCTEXT("LimitedLevel", "{0} / {1}"), Stage, FText::AsNumber(FMath::Max(1, Data.MaxLevel)));
		if (CurrentLevel >= Data.MaxLevel) Stage = FText::Format(LOCTEXT("MaxLevel", "{0} · MAX"), Stage);
	}
	return Stage;
}

namespace
{
FText MakeDescriptionForLevels(const UCoreDataAsset& Data, int32 CurrentLevel, int32 PreviewLevel)
{
	if (Data.DescriptionValues.IsEmpty()) return Data.CoreDescription;
	FFormatNamedArguments Arguments;
	for (const FCoreDescriptionValueBinding& Binding : Data.DescriptionValues)
	{
		if (Binding.Token.IsNone()) continue;
		float NextValue = 0.0f;
		FText ValueText = LOCTEXT("UnavailableValue", "—");
		if (ReadValue(Data, Binding, PreviewLevel, NextValue))
		{
			ValueText = FormatValue(NextValue, Binding.Format);
			float CurrentValue = 0.0f;
			if (CurrentLevel > 0 && PreviewLevel > CurrentLevel)
			{
				if (!ReadValue(Data, Binding, CurrentLevel, CurrentValue)) ValueText = LOCTEXT("UnavailableValue", "—");
				else if (!FMath::IsNearlyEqual(CurrentValue, NextValue, 0.000001f))
					ValueText = FText::Format(LOCTEXT("ValueComparison", "{0} → {1}"), FormatValue(CurrentValue, Binding.Format), ValueText);
			}
		}
		Arguments.Add(Binding.Token.ToString(), ValueText);
	}
	return FText::Format(Data.CoreDescription, Arguments);
}
}

FText CoreDescriptionFormatter::MakeDescription(const UCoreDataAsset& Data, int32 CurrentLevel)
{
	CurrentLevel = FMath::Max(0, CurrentLevel);
	return MakeDescriptionForLevels(Data, CurrentLevel, GetPreviewLevel(Data, CurrentLevel));
}

FText CoreDescriptionFormatter::MakeDescriptionAtLevel(const UCoreDataAsset& Data, int32 Level)
{
	Level = FMath::Max(1, Level);
	if (Data.UpgradeLimitType == ECoreUpgradeLimitType::Custom) Level = FMath::Min(Level, FMath::Max(1, Data.MaxLevel));
	return MakeDescriptionForLevels(Data, Level, Level);
}

#undef LOCTEXT_NAMESPACE
