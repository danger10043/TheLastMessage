#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "GameCoreTypes.generated.h"

class UCoreDataAsset;

/** 보유 목록을 읽기 위한 값 복사본. 효과·능력 핸들은 외부에 노출하지 않는다. */
USTRUCT(BlueprintType)
struct FOwnedCoreEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Core")
	TObjectPtr<UCoreDataAsset> CoreData = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Core")
	int32 Level = 0;
};

namespace CoreRewardRules
{
	inline constexpr int32 MaxChoices = 3;
	inline constexpr int32 RerollsPerCard = 1;
}

/*
 * 코어 시스템 전반에서 함께 쓰는 열거형을 모아 둔 헤더다.
 * 엔진의 CoreTypes.h와 이름이 겹치지 않도록 GameCoreTypes로 둔다.
 */

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

/** GE의 원래 수치를 설명에 사용할 숫자로 바꾼다. 효과 자체의 계산에는 관여하지 않는다. */
UENUM(BlueprintType)
enum class ECoreDescriptionValueFormat : uint8
{
	Number UMETA(DisplayName = "Number (그대로)"),
	Percent UMETA(DisplayName = "Percent (0.05 → 5)"),
	IncreasePercent UMETA(DisplayName = "Increase Percent (1.15 → 15)"),
	ReductionPercent UMETA(DisplayName = "Reduction Percent (0.9 → 10)")
};

/** 코어 보상을 지정된 코어로 지급할지, 선택지로 제공할지 구분한다. */
UENUM(BlueprintType)
enum class ECoreRewardMode : uint8
{
	Fixed,		 /** 지정된 코어 하나를 획득한다. */
	Choice		/** 제시된 코어 중 하나를 선택한다. */
};

/** 코어 획득·선택 요청의 결과. 성공은 신규 획득과 강화로 나누고, 실패는 원인별로 구분한다. */
UENUM(BlueprintType)
enum class ECoreAcquireResult : uint8
{
	Acquired UMETA(DisplayName = "신규 획득"),
	Upgraded UMETA(DisplayName = "강화"),
	MaxLevel UMETA(DisplayName = "최대 단계"),
	InvalidData UMETA(DisplayName = "잘못된 데이터"),
	NoAbilitySystem UMETA(DisplayName = "ASC 없음"),
	Busy UMETA(DisplayName = "처리 중"),
	NotOffered UMETA(DisplayName = "선택지에 없음"),
	GrantFailed UMETA(DisplayName = "GE/GA 부여 실패")
};

/** 신규 획득 또는 강화에 성공했는지 확인한다. */
FORCEINLINE bool IsCoreAcquireSuccess(ECoreAcquireResult Result)
{
	return Result == ECoreAcquireResult::Acquired || Result == ECoreAcquireResult::Upgraded;
}

/** 코어 선택 보상을 시작한 결과. 선택 화면을 열 수 없을 때 원인을 구분한다. */
enum class ECoreRewardStartResult : uint8
{
	Started,		/** 후보를 추첨하고 보상을 시작했다. */
	Busy,			/** 다른 보상을 선택 중이거나 코어 지급·초기화를 처리 중이다. */
	NoCandidates,	/** 분류에 맞는 무작위 보상 코어가 없다. */
	AllAtMaxLevel,	/** 분류에 맞는 코어가 모두 최대 단계다. */
	Unavailable		/** 지급 대상이 없거나 데이터·부여 상태가 잘못되어 받을 수 있는 코어가 없다. */
};

/** 선택 화면 카드 하나에 표시할 코어와 현재 보유 단계. 보상 세션이 만들어 UI에 전달한다. */
USTRUCT(BlueprintType)
struct FCoreChoiceEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Core")
	TObjectPtr<UCoreDataAsset> CoreData = nullptr;

	/** 카드를 만든 시점의 보유 단계. 미보유는 0이다. */
	UPROPERTY(BlueprintReadOnly, Category = "Core")
	int32 CurrentLevel = 0;
};
