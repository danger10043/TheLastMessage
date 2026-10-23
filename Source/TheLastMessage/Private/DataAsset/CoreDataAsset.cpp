#include "DataAsset/CoreDataAsset.h"

#include "Curves/RealCurve.h"
#include "Engine/CurveTable.h"
#include "GameplayEffect.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "GAS/CombatAttributeSet.h"
#include "Misc/DataValidation.h"
#include "UObject/StrongObjectPtr.h"
#endif

#define LOCTEXT_NAMESPACE "CoreDataAsset"

namespace CoreDataAssetPrivate
{
	/** 실패 이유를 요청한 경우에만 기록하고 false를 반환한다. */
	bool Fail(FText* OutReason, const FText& Reason)
	{
		if (OutReason) *OutReason = Reason;
		return false;
	}

#if WITH_EDITOR
	/**
	 * Modifier가 Scalable Float이면 그 값을 반환한다.
	 * FGameplayEffectModifierMagnitude는 Scalable Float를 꺼내는 공개 함수가 없어 리플렉션으로 커브 정보를 읽는다.
	 */
	const FScalableFloat* GetScalableFloat(const FGameplayEffectModifierMagnitude& Magnitude)
	{
		if (Magnitude.GetMagnitudeCalculationType() != EGameplayEffectMagnitudeCalculation::ScalableFloat) return nullptr;
		const FStructProperty* Property = FindFProperty<FStructProperty>(
			FGameplayEffectModifierMagnitude::StaticStruct(), TEXT("ScalableFloatMagnitude"));
		if (!Property || Property->Struct != FScalableFloat::StaticStruct()) return nullptr;
		return Property->ContainerPtrToValuePtr<FScalableFloat>(&Magnitude);
	}

	/** 기본 스탯이나 다른 효과를 알아야 하는 값은 설명의 Modifier 값만으로 판정하지 않는다. */
	bool CanCheckAttributeLimit(const FGameplayModifierInfo& Modifier, const UCombatAttributeSet& Attributes, float Value)
	{
		if (Modifier.Attribute.GetAttributeSetClass() != UCombatAttributeSet::StaticClass()
			|| Modifier.Attribute == UCombatAttributeSet::GetHealthAttribute()) return false;

		if (Modifier.ModifierOp == EGameplayModOp::Override) return true;
		const float DefaultValue = Modifier.Attribute.GetNumericValue(&Attributes);
		if (Modifier.ModifierOp == EGameplayModOp::MultiplyAdditive)
		{
			return FMath::IsNearlyEqual(DefaultValue, 1.f)
				|| Modifier.Attribute == UCombatAttributeSet::GetMaxHealthAttribute();
		}
		// 기본 스탯을 모르는 음수 가산값은 유효한 감소 효과일 수 있다.
		return Modifier.ModifierOp == EGameplayModOp::Additive && Value >= 0.f && FMath::IsNearlyZero(DefaultValue);
	}
#endif
}

bool UCoreDataAsset::HasValidGrantSetup(FText* OutReason) const
{
	using CoreDataAssetPrivate::Fail;

	// 강화는 같은 GE의 레벨만 갱신하므로, 지속 시간이 무한이고 스택이 쌓이지 않는 GE만 허용한다.
	TSet<const UClass*> RegisteredEffects;
	for (const TSubclassOf<UGameplayEffect>& EffectClass : GrantedEffects)
	{
		if (!IsValid(EffectClass.Get()) || EffectClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return Fail(OutReason, LOCTEXT("InvalidEffect", "GrantedEffects에 비어 있거나 추상 클래스인 GE가 있습니다."));
		}
		// 한 코어의 목록에 같은 GE가 반복되면 최초 획득부터 효과가 두 번 적용된다.
		if (RegisteredEffects.Contains(EffectClass.Get()))
		{
			return Fail(OutReason, FText::Format(LOCTEXT("DuplicateEffect", "GrantedEffects에 GE {0}가 중복 등록되어 있습니다."),
				FText::FromString(GetNameSafe(EffectClass.Get()))));
		}
		RegisteredEffects.Add(EffectClass.Get());

		const UGameplayEffect* Definition = EffectClass.GetDefaultObject();
		if (!IsValid(Definition) || Definition->DurationPolicy != EGameplayEffectDurationType::Infinite)
		{
			return Fail(OutReason, FText::Format(LOCTEXT("EffectNotInfinite", "{0}의 Duration Policy가 Infinite가 아닙니다."),
				FText::FromString(GetNameSafe(EffectClass.Get()))));
		}
		if (Definition->GetStackingType() != EGameplayEffectStackingType::None)
		{
			return Fail(OutReason, FText::Format(LOCTEXT("EffectStacking", "{0}의 Stacking Type이 None이 아닙니다."),
				FText::FromString(GetNameSafe(EffectClass.Get()))));
		}
	}

	TSet<const UClass*> RegisteredAbilities;
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : GrantedAbilities)
	{
		if (!IsValid(AbilityClass.Get()) || AbilityClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return Fail(OutReason, LOCTEXT("InvalidAbility", "GrantedAbilities에 비어 있거나 추상 클래스인 GA가 있습니다."));
		}
		if (RegisteredAbilities.Contains(AbilityClass.Get()))
		{
			return Fail(OutReason, FText::Format(LOCTEXT("DuplicateAbility", "GrantedAbilities에 GA {0}가 중복 등록되어 있습니다."),
				FText::FromString(GetNameSafe(AbilityClass.Get()))));
		}
		RegisteredAbilities.Add(AbilityClass.Get());
	}

	return true;
}

bool UCoreDataAsset::FindDescriptionModifier(const FCoreDescriptionValueBinding& Binding,
	const FGameplayModifierInfo*& OutModifier, FText* OutReason) const
{
	using CoreDataAssetPrivate::Fail;
	OutModifier = nullptr;

	if (!Binding.Attribute.IsValid())
	{
		return Fail(OutReason, LOCTEXT("BindingNoAttribute", "Attribute가 지정되지 않았습니다."));
	}
	if (Binding.Effect && !GrantedEffects.Contains(Binding.Effect))
	{
		return Fail(OutReason, FText::Format(LOCTEXT("BindingEffectNotGranted", "지정한 GE {0}가 GrantedEffects에 없습니다."),
			FText::FromString(GetNameSafe(Binding.Effect.Get()))));
	}

	// Effect를 지정했으면 그 GE에서만, 비웠으면 지급하는 모든 GE에서 같은 Attribute의 Modifier를 찾는다.
	TSet<const UClass*> VisitedEffects;
	for (const TSubclassOf<UGameplayEffect>& EffectClass : GrantedEffects)
	{
		if (!EffectClass || (Binding.Effect && EffectClass != Binding.Effect) || VisitedEffects.Contains(EffectClass.Get()))
		{
			continue;
		}
		VisitedEffects.Add(EffectClass.Get());

		const UGameplayEffect* Definition = EffectClass.GetDefaultObject();
		if (!Definition) continue;

		for (const FGameplayModifierInfo& Modifier : Definition->Modifiers)
		{
			if (Modifier.Attribute != Binding.Attribute) continue;
			// 같은 Attribute의 Modifier가 여러 개면 어느 값을 표시할지 모호하므로 추측하지 않는다.
			if (OutModifier)
			{
				OutModifier = nullptr;
				return Fail(OutReason, FText::Format(LOCTEXT("BindingAmbiguous",
					"{0}를 수정하는 Modifier가 여러 개입니다. 바인딩의 Effect를 지정하세요."),
					FText::FromString(Binding.Attribute.GetName())));
			}
			OutModifier = &Modifier;
		}
	}

	if (!OutModifier)
	{
		return Fail(OutReason, FText::Format(LOCTEXT("BindingNotFound", "GrantedEffects에서 {0}를 수정하는 Modifier를 찾지 못했습니다."),
			FText::FromString(Binding.Attribute.GetName())));
	}
	return true;
}

bool UCoreDataAsset::TryGetDescriptionValue(const FCoreDescriptionValueBinding& Binding, int32 Level,
	float& OutValue, FText* OutReason) const
{
	const FGameplayModifierInfo* Modifier = nullptr;
	if (!FindDescriptionModifier(Binding, Modifier, OutReason))
	{
		return false;
	}
	if (!Modifier->ModifierMagnitude.GetStaticMagnitudeIfPossible(Level, OutValue) || !FMath::IsFinite(OutValue))
	{
		return CoreDataAssetPrivate::Fail(OutReason, FText::Format(LOCTEXT("BindingNotStatic",
			"{0}의 수치가 Scalable Float가 아니어서 설명에 표시할 수 없습니다."),
			FText::FromString(Binding.Attribute.GetName())));
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UCoreDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	bool bHasError = Super::IsDataValid(Context) == EDataValidationResult::Invalid;
	auto AddError = [&Context, &bHasError](const FText& Message)
	{
		Context.AddError(Message);
		bHasError = true;
	};
	auto AddWarning = [&Context](const FText& Message)
	{
		Context.AddWarning(Message);
	};

	// 기본 정의
	if (CoreId.IsNone())
	{
		AddError(LOCTEXT("NoCoreId", "CoreId가 비어 있습니다."));
	}
	else if (IsAsset())
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.WaitForCompletion();
		TArray<FAssetData> CoreAssets;
		Registry.GetAssetsByClass(StaticClass()->GetClassPathName(), CoreAssets, true);
		for (const FAssetData& Asset : CoreAssets)
		{
			if (Asset.GetSoftObjectPath() == FSoftObjectPath(this)) continue;
			FName OtherId;
			// 열린 에셋은 저장 전 변경값을 우선하고, 닫힌 에셋은 검색용 태그를 사용한다.
			if (const UCoreDataAsset* Other = Cast<UCoreDataAsset>(Asset.FastGetAsset()))
			{
				OtherId = Other->CoreId;
			}
			else if (!Asset.GetTagValue(GET_MEMBER_NAME_CHECKED(UCoreDataAsset, CoreId), OtherId))
			{
				// 검색용 태그가 없는 기존 에셋도 다시 저장할 필요 없이 검사한다.
				const UCoreDataAsset* LoadedCore = Cast<UCoreDataAsset>(Asset.GetAsset());
				if (!LoadedCore)
				{
					AddWarning(FText::Format(LOCTEXT("CoreIdAssetUnreadable",
						"{0}을 읽지 못해 CoreId 중복 검사를 완료할 수 없습니다."), FText::FromString(Asset.GetObjectPathString())));
					continue;
				}
				OtherId = LoadedCore->CoreId;
			}
			if (OtherId == CoreId)
			{
				AddError(FText::Format(LOCTEXT("DuplicateCoreId", "CoreId '{0}'가 다른 코어 에셋 {1}과 중복됩니다. 서로 다른 ID를 지정하세요."),
					FText::FromName(CoreId), FText::FromString(Asset.GetObjectPathString())));
			}
		}
	}
	if (CoreName.IsEmpty())
	{
		AddWarning(LOCTEXT("NoCoreName", "CoreName이 비어 있어 선택 카드에 이름이 표시되지 않습니다."));
	}

	const bool bCustomLimit = UpgradeLimitType == ECoreUpgradeLimitType::Custom;
	if (bCustomLimit && MaxLevel < 1)
	{
		AddError(LOCTEXT("InvalidMaxLevel", "Custom의 MaxLevel은 1 이상이어야 합니다."));
	}

	FText GrantReason;
	if (!HasValidGrantSetup(&GrantReason))
	{
		AddError(GrantReason);
	}

	// 키 범위 밖의 강화 단계는 커브의 외삽 설정으로 계산한다.
	for (const TSubclassOf<UGameplayEffect>& EffectClass : GrantedEffects)
	{
		const UGameplayEffect* Definition = EffectClass ? EffectClass.GetDefaultObject() : nullptr;
		if (!Definition) continue;

		for (const FGameplayModifierInfo& Modifier : Definition->Modifiers)
		{
			const FScalableFloat* Scaling = CoreDataAssetPrivate::GetScalableFloat(Modifier.ModifierMagnitude);
			if (!Scaling || !Scaling->Curve.CurveTable)
			{
				// 커브 없이 고정된 수치(예: 과부하의 받는 피해 1.15)는 단계와 무관하다.
				continue;
			}

			const FText RowName = FText::FromName(Scaling->Curve.RowName);
			const FRealCurve* Curve = Scaling->Curve.GetCurve(TEXT("CoreDataAsset validation"), false);
			if (!Curve || Curve->GetNumKeys() == 0)
			{
				AddError(FText::Format(LOCTEXT("CurveRowMissing", "{0}의 커브 행 '{1}'을 찾을 수 없거나 키가 없습니다."),
					FText::FromString(GetNameSafe(EffectClass.Get())), RowName));
				continue;
			}

			float MinTime = 0.0f;
			float MaxTime = 0.0f;
			Curve->GetTimeRange(MinTime, MaxTime);
			if (MinTime > 1.0f + KINDA_SMALL_NUMBER)
			{
				AddWarning(FText::Format(LOCTEXT("CurveNoFirstLevel", "커브 행 '{0}'에 Lv.1 키가 없습니다."), RowName));
			}

			if (bCustomLimit && MaxLevel >= 1)
			{
				if (MaxTime + KINDA_SMALL_NUMBER < MaxLevel)
				{
					// Linear 외삽은 두 개 이상의 키로 마지막 구간의 기울기를 계산한다.
					const bool bHasLinearTail = Curve->PostInfinityExtrap == RCCE_Linear && Curve->GetNumKeys() >= 2;
					if (!bHasLinearTail)
					{
						AddWarning(FText::Format(LOCTEXT("CurveTailExtrapolation",
							"커브 행 '{0}'의 마지막 키는 Lv.{1}입니다. 이후 MaxLevel({2})까지의 값은 외삽 설정과 키 수에 따라 계산됩니다. 의도한 수치인지 확인하세요."),
							RowName, FText::AsNumber(MaxTime), FText::AsNumber(MaxLevel)));
					}
				}
				else if (MaxTime > MaxLevel + KINDA_SMALL_NUMBER)
				{
					AddWarning(FText::Format(LOCTEXT("CurveLongerThanMaxLevel",
						"커브 행 '{0}'에 MaxLevel({1}) 이후의 키가 있습니다. 이후 키도 보간에 영향을 줄 수 있으므로 의도한 설정인지 확인하세요."),
						RowName, FText::AsNumber(MaxLevel)));
				}
			}
			else if (!bCustomLimit && Curve->PostInfinityExtrap != RCCE_Constant)
			{
				AddWarning(FText::Format(LOCTEXT("InfiniteCurveExtrapolation",
					"Infinite 코어인데 커브 행 '{0}'의 Post-Infinity 외삽이 Constant가 아닙니다. 마지막 키 이후 수치가 계속 변해 의도하지 않은 값이 될 수 있습니다."),
					RowName));
			}
		}
	}

	// 설명 서식의 {토큰}과 바인딩이 서로 맞는지 확인한다.
	TArray<FString> DescriptionTokens;
	FText::GetFormatPatternParameters(FTextFormat(CoreDescription), DescriptionTokens);

	TSet<FString> BoundTokens;
	TStrongObjectPtr<UCombatAttributeSet> ValidationAttributes;
	const int32 LastCheckedLevel = bCustomLimit ? FMath::Max(1, MaxLevel) : 2;
	for (const FCoreDescriptionValueBinding& Binding : DescriptionValues)
	{
		if (Binding.Token.IsNone())
		{
			AddError(LOCTEXT("BindingNoToken", "Token이 비어 있는 설명 바인딩이 있습니다."));
			continue;
		}

		const FString Token = Binding.Token.ToString();
		const FText TokenText = FText::FromString(Token);
		// 같은 토큰에 여러 값을 연결하면 설명을 만들 때 앞의 값이 덮어써진다.
		if (BoundTokens.Contains(Token))
		{
			AddError(FText::Format(LOCTEXT("DuplicateDescriptionToken", "설명 바인딩의 Token {0}가 중복 등록되어 있습니다."), TokenText));
			continue;
		}
		BoundTokens.Add(Token);
		if (!DescriptionTokens.Contains(Token))
		{
			AddWarning(FText::Format(LOCTEXT("TokenUnused", "설명에 `{{0}`}이 없어 바인딩 {0}의 값이 표시되지 않습니다."), TokenText));
		}

		const FGameplayModifierInfo* Modifier = nullptr;
		FText Reason;
		if (!FindDescriptionModifier(Binding, Modifier, &Reason))
		{
			AddError(FText::Format(LOCTEXT("BindingInvalid", "설명 바인딩 {0}: {1}"), TokenText, Reason));
			continue;
		}
		if (!ValidationAttributes.IsValid() && Binding.Attribute.GetAttributeSetClass() == UCombatAttributeSet::StaticClass())
		{
			ValidationAttributes.Reset(NewObject<UCombatAttributeSet>());
		}

		// 표시할 모든 단계에서 값을 읽을 수 있는지, 표시 형식이 실제 연산과 맞는지 확인한다.
		bool bAnyBelowOne = false;
		bool bAnyAboveOne = false;
		bool bLimitWarningReported = false;
		for (int32 Level = 1; Level <= LastCheckedLevel; ++Level)
		{
			float Value = 0.0f;
			if (!TryGetDescriptionValue(Binding, Level, Value, &Reason))
			{
				AddError(FText::Format(LOCTEXT("BindingValueUnreadable", "설명 바인딩 {0}: {1}"), TokenText, Reason));
				break;
			}
			bAnyBelowOne |= Value < 1.0f - KINDA_SMALL_NUMBER;
			bAnyAboveOne |= Value > 1.0f + KINDA_SMALL_NUMBER;
			if (!bLimitWarningReported && ValidationAttributes.IsValid()
				&& CoreDataAssetPrivate::CanCheckAttributeLimit(*Modifier, *ValidationAttributes, Value))
			{
				// 런타임과 같은 제한을 사용하되 실제 플레이어의 스탯은 변경하지 않는다.
				float LimitedValue = Value;
				ValidationAttributes->PreAttributeChange(Binding.Attribute, LimitedValue);
				if (!FMath::IsNearlyEqual(Value, LimitedValue, UE_KINDA_SMALL_NUMBER))
				{
					AddWarning(FText::Format(LOCTEXT("DescriptionAttributeLimit",
						"설명 바인딩 {0}: Lv.{1}의 {2} 설정값 {3}은 Attribute 제한 적용 시 {4}가 됩니다. 기본 스탯과 다른 효과의 조합에 따라 설명과 실제 효과가 달라질 수 있으므로 GE·커브 또는 MaxLevel을 확인하세요."),
						TokenText, FText::AsNumber(Level), FText::FromString(Binding.Attribute.GetName()),
						FText::AsNumber(Value), FText::AsNumber(LimitedValue)));
					bLimitWarningReported = true;
				}
			}
		}

		const bool bMultiplyModifier = Modifier->ModifierOp == EGameplayModOp::MultiplyAdditive;
		const bool bPercentChangeFormat = Binding.Format == ECoreDescriptionValueFormat::IncreasePercent
			|| Binding.Format == ECoreDescriptionValueFormat::ReductionPercent;
		if (bPercentChangeFormat && !bMultiplyModifier)
		{
			AddWarning(FText::Format(LOCTEXT("FormatNeedsMultiply",
				"설명 바인딩 {0}: Increase/Reduction Percent는 Multiply (Additive) 배율용입니다. 추가값·덮어쓰기에는 Number나 Percent를 쓰세요."),
				TokenText));
		}
		else if (!bPercentChangeFormat && bMultiplyModifier)
		{
			AddWarning(FText::Format(LOCTEXT("MultiplyNeedsPercentChange",
				"설명 바인딩 {0}: 배율 Modifier는 Increase Percent(1.15 → 15)나 Reduction Percent(0.9 → 10)로 표시해야 합니다."),
				TokenText));
		}
		else if (Binding.Format == ECoreDescriptionValueFormat::IncreasePercent && bAnyBelowOne)
		{
			AddWarning(FText::Format(LOCTEXT("IncreaseBelowOne",
				"설명 바인딩 {0}: 1보다 작은 배율이 있어 음수로 표시됩니다. 감소 효과는 Reduction Percent를 쓰세요."), TokenText));
		}
		else if (Binding.Format == ECoreDescriptionValueFormat::ReductionPercent && bAnyAboveOne)
		{
			AddWarning(FText::Format(LOCTEXT("ReductionAboveOne",
				"설명 바인딩 {0}: 1보다 큰 배율이 있어 음수로 표시됩니다. 증가 효과는 Increase Percent를 쓰세요."), TokenText));
		}
	}

	// 바인딩이 없는 토큰은 화면에 {토큰} 글자가 그대로 노출된다.
	for (const FString& Token : DescriptionTokens)
	{
		if (!BoundTokens.Contains(Token))
		{
			AddError(FText::Format(LOCTEXT("TokenUnbound", "설명의 `{{0}`}에 연결된 설명 바인딩이 없습니다."), FText::FromString(Token)));
		}
	}

	return bHasError ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
