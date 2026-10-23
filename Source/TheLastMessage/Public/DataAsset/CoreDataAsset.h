#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Core/GameCoreTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "CoreDataAsset.generated.h"

class FDataValidationContext;
class UTexture2D;

/** 설명의 {Token}에 넣을 값을 어느 GE의 어떤 Modifier에서 읽을지 지정한다. */
USTRUCT(BlueprintType)
struct FCoreDescriptionValueBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Description")
	FName Token;

	/**
	 * 값을 읽을 GE. 비워 두면 GrantedEffects에서 Attribute를 수정하는 GE를 자동으로 찾는다.
	 * 여러 GE가 같은 Attribute를 수정해 어느 값인지 모호할 때만 지정한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Description")
	TSubclassOf<UGameplayEffect> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Description")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Description")
	ECoreDescriptionValueFormat Format = ECoreDescriptionValueFormat::Number;
};

/** 코어의 정의 데이터. Asset Manager에 등록되며, 실제 보유 상태와 부여 기록은 UCoreComponent에서 관리한다. */
UCLASS(BlueprintType)
class THELASTMESSAGE_API UCoreDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 보유·강화 기록을 구분하는 ID. 서로 다른 코어 에셋은 같은 값을 사용할 수 없다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category = "Core")
	FName CoreId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core")
	FText CoreName;

	/** 고정 문구 또는 {HealthBonus}처럼 DescriptionValues의 Token을 넣은 설명 서식. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core", meta = (MultiLine = "true"))
	FText CoreDescription;

	/** 숫자를 별도로 저장하지 않고 실제 GE의 단계별 수치를 설명에 연결한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core|Description", meta = (TitleProperty = "Token"))
	TArray<FCoreDescriptionValueBinding> DescriptionValues;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core")
	TObjectPtr<UTexture2D> CoreIcon = nullptr;

	/** 어떤 보상 풀에 포함할지 직접 지정한다. 흡혈처럼 수치로 강화할 효과도 스탯형으로 설정할 수 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core")
	ECoreCategory CoreCategory = ECoreCategory::Stat;

	/** 강화 횟수의 제한 방식. 효과의 수치 상한과는 별개이며, 수치 계산은 GE에서 설정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core|Upgrade")
	ECoreUpgradeLimitType UpgradeLimitType = ECoreUpgradeLimitType::Custom;

	/**
	 * Custom에서만 사용하는 최대 단계. 1이면 최초 한 번만 획득한다. Infinite에서는 무시한다.
	 * 마지막 키 이후 단계의 GE 수치는 커브의 Post-Infinity 외삽 설정을 따른다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core|Upgrade",
		meta = (ClampMin = "1", UIMin = "1", EditCondition = "UpgradeLimitType == ECoreUpgradeLimitType::Custom", EditConditionHides))
	int32 MaxLevel = 1;

	/** 선택형 보상의 전체 후보에 포함할지 정한다. 고정 보상 전용 코어는 끈다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core|Reward")
	bool bIncludeInRandomPool = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core|Grants")
	TArray<TSubclassOf<UGameplayEffect>> GrantedEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core|Grants")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

	/**
	 * GrantedEffects·GrantedAbilities가 코어 부여 규칙을 지키는지 확인한다.
	 * GE는 Infinite Duration·Stacking None이어야 하며, 클래스는 유효하고 추상이 아니어야 한다.
	 * 한 목록에 같은 클래스를 중복 등록할 수 없다. 서로 다른 코어가 같은 클래스를 쓰는 것은 허용한다.
	 * 획득 판정과 에디터 데이터 검증이 같은 기준을 쓰도록 한 곳에 둔다.
	 */
	bool HasValidGrantSetup(FText* OutReason = nullptr) const;

	/**
	 * 설명 바인딩이 가리키는 Modifier를 찾는다. Effect를 비우면 GrantedEffects에서 Attribute로 찾는다.
	 * 찾지 못하거나 후보가 여러 개라 모호하면 false와 함께 이유를 반환한다.
	 */
	bool FindDescriptionModifier(const FCoreDescriptionValueBinding& Binding,
		const FGameplayModifierInfo*& OutModifier, FText* OutReason = nullptr) const;

	/** 설명 바인딩의 Modifier 값을 지정한 단계에서 읽는다. Scalable Float 수치만 읽을 수 있다. */
	bool TryGetDescriptionValue(const FCoreDescriptionValueBinding& Binding, int32 Level,
		float& OutValue, FText* OutReason = nullptr) const;

#if WITH_EDITOR
	/** 저장·데이터 검증 시 CoreId 중복, GE 설정, 커브와 강화 단계, 설명 바인딩을 검사한다. */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
