#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "CoreDataAsset.generated.h"

class UTexture2D;

/** GE의 원래 수치를 설명에 사용할 숫자로 바꾼다. 효과 자체의 계산에는 관여하지 않는다. */
UENUM(BlueprintType)
enum class ECoreDescriptionValueFormat : uint8
{
	Number UMETA(DisplayName = "Number (그대로)"),
	Percent UMETA(DisplayName = "Percent (0.05 → 5)"),
	IncreasePercent UMETA(DisplayName = "Increase Percent (1.15 → 15)"),
	ReductionPercent UMETA(DisplayName = "Reduction Percent (0.9 → 10)")
};

/** 설명의 {Token}에 넣을 값을 어느 GE의 어떤 Modifier에서 읽을지 지정한다. */
USTRUCT(BlueprintType)
struct FCoreDescriptionValueBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Description")
	FName Token;

	/** GrantedEffects에 등록된 GE 중 하나를 지정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Description")
	TSubclassOf<UGameplayEffect> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Description")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Description")
	ECoreDescriptionValueFormat Format = ECoreDescriptionValueFormat::Number;
};

/** 보상 풀을 나누기 위한 기획 분류다. GE/GA 구현 방식이나 시각 효과 유무와는 별개다. */
UENUM(BlueprintType)
enum class ECoreCategory : uint8
{
	Stat UMETA(DisplayName = "스탯형"),
	Ability UMETA(DisplayName = "능력형")
};

/** 강화 횟수 제한을 명시적으로 선택한다. Infinite는 제한 없이, Custom은 지정한 최대 단계까지 획득한다. */
UENUM(BlueprintType)
enum class ECoreUpgradeLimitType : uint8
{
	Custom UMETA(DisplayName = "Custom (사용자 지정)"),
	Infinite UMETA(DisplayName = "Infinite (무한)")
};

/** 코어의 정의 데이터. Asset Manager에 등록되며, 실제 보유 상태와 부여 기록은 UCoreComponent에서 관리한다. */
UCLASS(BlueprintType)
class THELASTMESSAGE_API UCoreDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core")
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

	/** Custom에서만 사용하는 최대 단계. 1이면 최초 한 번만 획득한다. Infinite에서는 무시한다. */
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
};
