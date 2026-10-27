#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Character/Enemy/DataAsset/EnemyTypes.h"
#include "Character/Enemy/DataAsset/EnemySettings.h"
#include "EnemyDataAsset.generated.h"

/**
 * 몬스터 설정 데이터. 에셋 하나를 같은 종류의 몬스터 인스턴스들이 공유한다.
 * 능력치·공통 수치 아래에 몬스터 종류별 묶음(Rattler, Flea, ...)이 있고, 몬스터는 자기 종류의 묶음만 읽는다.
 */
UCLASS(BlueprintType)
class THELASTMESSAGE_API UEnemyDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** UI 등에 표시할 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	FText DisplayName;

	/** 기본(레벨 1) 능력치 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	FEnemyStatConfig StatConfig;

	/** 레벨당 공격력 증가율. 레벨 N의 피해 배율 = 1 + (N - 1) × 이 값 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Level", meta = (ClampMin = "0.0"))
	float DamageScalePerLevel = 0.2f;

	/** 레벨당 최대 체력 증가율. 레벨 N의 체력 배율 = 1 + (N - 1) × 이 값 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Level", meta = (ClampMin = "0.0"))
	float HealthScalePerLevel = 0.15f;

	// ---- 모든 몬스터 공통 수치 ----

	/** 켜면 체력바를 항상 표시하고, 끄면 맞은 뒤 HealthBarVisibleTime 동안만 표시한다. */
	UPROPERTY(EditAnywhere, Category = "Enemy|UI")
	bool bAlwaysShowHealthBar = false;

	/** 맞은 뒤 체력바가 유지되는 시간(초) */
	UPROPERTY(EditAnywhere, Category = "Enemy|UI", meta = (ClampMin = "0.1", EditCondition = "!bAlwaysShowHealthBar"))
	float HealthBarVisibleTime = 3.f;

	/** 피격 몽타주를 다시 재생할 수 있기까지의 최소 간격(초). 연속 피격 때 계속 끊기는 것을 막는다. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Animation", meta = (ClampMin = "0.0"))
	float HitReactionCooldown = 0.4f;

	/** 켜면 공격 중에 맞아도 피격 몽타주가 재생되고 공격이 취소된다. 끄면 공격 중에는 피격 몽타주를 재생하지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Animation")
	bool bHitReactionInterruptsAttack = false;

	/** 번쩍임이 유지되는 시간(초) */
	UPROPERTY(EditAnywhere, Category = "Enemy|Effects", meta = (ClampMin = "0.01"))
	float HitFlashDuration = 0.08f;

	/** 사망 후 시체가 남아 있는 시간(초). 이 시간이 지나면 몬스터가 풀로 돌아간다. 0 이하이면 계속 남긴다. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Death", meta = (ClampMin = "0.0"))
	float CorpseLifeSpan = 5.f;

	/**
	 * 착지한 높이가 홈과 이 값보다 많이 다르면(낭떠러지에서 떨어진 경우 등) 착지한 자리를 새 홈으로 삼는다.
	 * 다른 층에 떨어진 몬스터가 원래 홈으로 돌아가려다 막히는 것을 막는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Enemy|Patrol", meta = (ClampMin = "0.0"))
	float HomeReanchorHeight = 150.f;

	UPROPERTY(EditAnywhere, Category = "Enemy")
	float PatrolRadius = 600.f;

	// ---- 몬스터 종류별 수치 ----

	/** Rattler 전용 수치. Rattler 몬스터만 읽는다. 다른 몬스터의 에셋에서는 건드리지 않아도 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rattler")
	FRattlerSettings Rattler;

	/** Flea 전용 수치. Flea 몬스터만 읽는다. 다른 몬스터의 에셋에서는 건드리지 않아도 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flea")
	FFleaSettings Flea;

	/** Sentry 전용 수치. Sentry 몬스터만 읽는다. 다른 몬스터의 에셋에서는 건드리지 않아도 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sentry")
	FSentrySettings Sentry;

	/** Cinder 전용 수치. Cinder 몬스터만 읽는다. 다른 몬스터의 에셋에서는 건드리지 않아도 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinder")
	FCinderSettings Cinder;

	/** Juggernaut 전용 수치. Juggernaut 몬스터만 읽는다. 다른 몬스터의 에셋에서는 건드리지 않아도 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Juggernaut")
	FJuggernautSettings Juggernaut;

	/** Junker 전용 수치. Junker 몬스터만 읽는다. 다른 몬스터의 에셋에서는 건드리지 않아도 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Junker")
	FJunkerSettings Junker;
};
