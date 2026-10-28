#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CorePickupStateSubsystem.generated.h"

/** 맵 이동/재로드 동안 픽업별 소모 bool을 유지한다. 파일 저장 시점은 세이브 시스템에서 결정한다. */
UCLASS()
class THELASTMESSAGE_API UCorePickupStateSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Core|Persistence")
	bool IsPickupConsumed(FGuid PickupId) const;

	UFUNCTION(BlueprintCallable, Category = "Core|Persistence")
	void SetPickupConsumed(FGuid PickupId, bool bConsumed);

	/** 세이브 포인트에서 복사할 픽업별 상태. */
	UFUNCTION(BlueprintPure, Category = "Core|Persistence")
	TMap<FGuid, bool> GetPickupStates() const { return PickupStates; }

	/** 세이브 불러오기 시 맵 진입 전에 호출한다. 현재 실행 중 기록을 전달된 기록으로 교체한다. */
	UFUNCTION(BlueprintCallable, Category = "Core|Persistence")
	void RestorePickupStates(const TMap<FGuid, bool>& SavedStates);

	/** 새 게임을 시작할 때 기록을 비운 뒤 맵을 로드한다. */
	UFUNCTION(BlueprintCallable, Category = "Core|Persistence")
	void ResetPickupStates() { PickupStates.Reset(); }

private:
	UPROPERTY(Transient)
	TMap<FGuid, bool> PickupStates;
};
