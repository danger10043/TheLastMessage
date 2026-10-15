#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Character/Enemy/DataAsset/EnemyTypes.h"
#include "EnemyDataAsset.generated.h"

/** 몬스터 종류별 설정 데이터. 에셋 하나를 여러 몬스터 인스턴스가 공유한다. */
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
};
