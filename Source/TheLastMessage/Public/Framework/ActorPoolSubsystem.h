#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ActorPoolSubsystem.generated.h"

USTRUCT()
struct FPooledActorList
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<AActor>> Actors;
};

/**
 * 범용 액터 오브젝트 풀. 클래스별로 사용하지 않는 액터를 보관했다가 재사용한다.
 *
 * - 꺼내기: AcquireActor(Class, Transform) — 풀에 있으면 재사용하고, 없으면 새로 만든다.
 * - 돌려주기: ReleaseActor(Actor) — 숨기고 비활성화한 뒤 보관한다. (Destroy 대신 사용)
 * - 풀에서 쓰는 액터는 IPoolableInterface를 구현해 상태 초기화·정리를 직접 처리한다.
 */
UCLASS()
class THELASTMESSAGE_API UActorPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 풀에서 액터를 꺼낸다. 위치·소유자를 설정하고 OnAcquiredFromPool()을 호출한다. */
	AActor* AcquireActor(UClass* ActorClass, const FTransform& Transform, AActor* Owner = nullptr, APawn* Instigator = nullptr);

	/** 액터를 풀로 돌려보낸다. 풀이 가득 차 있으면 제거한다. */
	void ReleaseActor(AActor* Actor);

	/** 자주 쓰는 클래스를 미리 만들어 둔다(첫 사용 시 끊김 방지). */
	void Prewarm(UClass* ActorClass, int32 Count);

	virtual void Deinitialize() override;

	/** 클래스 하나당 보관할 최대 개수 */
	int32 MaxPoolSizePerClass = 64;

private:
	UPROPERTY()
	TMap<TObjectPtr<UClass>, FPooledActorList> FreeLists;
};
