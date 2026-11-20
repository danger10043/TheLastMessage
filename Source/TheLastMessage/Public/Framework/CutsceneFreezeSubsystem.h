#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CutsceneFreezeSubsystem.generated.h"

/**
 * 로딩 화면이나 문 열림 연출 같은 구간에서 플레이어가 조작할 수 없는 동안, 몬스터를 멈추고 플레이어를 피해에서 보호한다.
 * - 몬스터: 시간 흐름을 거의 0으로 하고 AI와 진행 중인 공격을 멈춘다. 날아가는 투사체는 없앤다.
 * - 플레이어: 피해를 받지 않는다(APlayerBase::TakeDamage).
 *
 * 멈추는 이유(Source)별로 켜고 끈다. 하나라도 켜져 있으면 멈춘 상태이고, 모두 꺼지면 풀린다.
 * 같은 값으로 여러 번 불러도 안전하다(매 틱 불러도 된다).
 */
UCLASS()
class THELASTMESSAGE_API UCutsceneFreezeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 월드 컨텍스트 오브젝트가 속한 월드의 서브시스템. 없으면 nullptr. */
	static UCutsceneFreezeSubsystem* Get(const UObject* WorldContext);

	void SetFreezeSource(FName Source, bool bActive);

	bool IsFrozen() const { return !Sources.IsEmpty(); }

private:
	void ApplyToWorld(bool bFrozen);

	TSet<FName> Sources;
};
