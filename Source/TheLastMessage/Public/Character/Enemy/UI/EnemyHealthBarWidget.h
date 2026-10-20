#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHealthBarWidget.generated.h"

class UProgressBar;

/**
 * 몬스터 머리 위 체력바 위젯의 C++ 부모 클래스.
 * 위젯 블루프린트(WBP_EnemyHealthBar)를 이 클래스를 부모로 만들고, 아래 이름의 ProgressBar를 배치한다.
 *  - HealthBar (필수): 현재 체력. 맞으면 바로 줄어든다.
 *  - DamageBar (선택): 체력바 뒤에 깔리는 바. 맞은 만큼 잠깐 남아 있다가 천천히 따라 내려온다.
 */
UCLASS()
class THELASTMESSAGE_API UEnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 체력 비율(0~1)을 설정한다. bInstant가 true면 지연 바도 즉시 맞춘다(스폰·리셋 때). */
	void SetHealthPercent(float Percent, bool bInstant);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> DamageBar;

	/** 맞은 뒤 지연 바가 줄어들기 시작하기까지의 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "HealthBar", meta = (ClampMin = "0.0"))
	float DamageBarDelay = 0.4f;

	/** 지연 바가 줄어드는 속도(초당 비율). 1이면 전체 길이를 1초에 줄인다. */
	UPROPERTY(EditDefaultsOnly, Category = "HealthBar", meta = (ClampMin = "0.01"))
	float DamageBarSpeed = 1.5f;

private:
	float TargetPercent = 1.f;
	float DamagePercent = 1.f;
	float DelayRemaining = 0.f;
};
