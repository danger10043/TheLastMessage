#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "CoreGrantRecord.generated.h"

/**
 * 코어가 부여한 능력과 효과를 나중에 정확히 찾아 제거하기 위해 필요한 기록이다.
 * UCoreDataAsset에는 부여할 능력과 효과의 클래스가 설정되어 있다.
 * 실제 부여된 능력과 효과를 구분하기 위해, ASC가 반환한 핸들을 코어 ID와 함께 보관한다.
 * 핸들은 ASC에 등록된 능력 부여 기록 또는 현재 유지 중인 효과를 찾는 데 사용하는 식별 값이다.
 * 코어 획득 도중 일부 능력이나 효과만 부여된 뒤 실패하면, UCoreComponent가 이 핸들로 이미 부여된 능력과 효과를 제거한다.
 * 보유 코어를 초기화할 때도 이 기록으로 코어가 부여한 능력과 효과를 제거하고, 기본 능력과 다른 시스템의 효과는 유지한다.
 */

// 코어 하나가 ASC에 부여한 능력·효과의 핸들을 모아 둔 구조체
USTRUCT()
struct THELASTMESSAGE_API FCoreGrantRecord
{
	GENERATED_BODY()

	/** 이 적용 기록이 속한 코어의 고유한 정의 ID. 표시 이름과 별개로 유지한다. */
	UPROPERTY()
	FName CoreId = NAME_None;

	/** 획득한 정의를 보관해 보유 목록에서 이름·아이콘·효과를 다시 로드하지 않고 조회한다. */
	UPROPERTY(Transient)
	TObjectPtr<class UCoreDataAsset> CoreData = nullptr;

	/** 처음 획득하면 1단계이며, 재획득에 성공할 때마다 1씩 증가한다. 태그 카운트와 별도로 관리한다. */
	UPROPERTY()
	int32 Level = 1;

	/**
	 * 이 코어가 ASC에 부여한 능력의 핸들. 해당 능력의 부여 기록을 찾아 제거하는 데 사용한다.
	 * Transient는 실행 중 메모리에서는 사용하되 Unreal의 일반 저장/로드 대상에서 제외한다.
	 * 불러오기 시 능력을 다시 부여해 새 핸들을 기록하며, 능력 제거는 별도로 처리해야 한다.
	 */
	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;

	/**
	 * 이 코어가 적용한 유지 효과의 핸들. 효과 제거에 사용하며 Instant 효과는 기록하지 않는다.
	 */
	UPROPERTY(Transient)
	TArray<FActiveGameplayEffectHandle> EffectHandles;
};
