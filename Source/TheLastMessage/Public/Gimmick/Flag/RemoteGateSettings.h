#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "RemoteGateSettings.generated.h"

/** 멀리 있는 문이 열릴 때 보여 줄 연출 하나. */
USTRUCT()
struct FRemoteGateEntry
{
	GENERATED_BODY()

	/** 이 플래그가 켜질 때 연출한다. 예: Gate.RH0406 */
	UPROPERTY(EditAnywhere, Category = "Remote Gate")
	FGameplayTag Flag;

	/** 카메라가 비출 월드 위치(문의 위치). 플레이어가 서 있는 평면(Y)을 기준으로 옆에서 비춘다. */
	UPROPERTY(EditAnywhere, Category = "Remote Gate")
	FVector FocusLocation = FVector::ZeroVector;

	/** 연출 동안 올려 둘 방 번호들(Room Streaming 목록의 RoomId). 문이 항상 로드되어 있는 복도에 있으면 비워 둔다. */
	UPROPERTY(EditAnywhere, Category = "Remote Gate")
	TArray<FName> RoomIds;

	/** 문이 열리는 모습을 보여 주는 시간(초). 문이 다 열리는 시간보다 길게 잡는다. */
	UPROPERTY(EditAnywhere, Category = "Remote Gate", meta = (ClampMin = "0"))
	float HoldSeconds = 3.f;

	/** 플레이어가 문에서 이 거리(cm)보다 가까우면 연출 없이 그냥 문이 열린다. */
	UPROPERTY(EditAnywhere, Category = "Remote Gate", meta = (ClampMin = "0"))
	float MinPlayerDistance = 2500.f;
};

/**
 * 스위치를 눌렀을 때 멀리 있는 문이 열리는 것을 카메라로 보여 주는 연출 설정. 목록이 비어 있으면 아무것도 하지 않는다.
 * 흐름: 암전 -> 방 로드 -> 문 위치로 카메라 이동 -> 밝아지며 문이 열림 -> 암전 -> 방 해제 -> 플레이어로 복귀 -> 밝아짐.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Remote Gate Presentation"))
class THELASTMESSAGE_API URemoteGateSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URemoteGateSettings() { CategoryName = TEXT("Game"); }

	/** 암전과 복귀 때 화면이 어두워지고 밝아지는 시간(초). */
	UPROPERTY(Config, EditAnywhere, Category = "Remote Gate", meta = (ClampMin = "0.05"))
	float FadeSeconds = 0.4f;

	/** 방이 다 올라온 뒤 문이 초기 상태를 잡을 때까지 더 기다리는 시간(초). */
	UPROPERTY(Config, EditAnywhere, Category = "Remote Gate", meta = (ClampMin = "0"))
	float SettleSeconds = 0.4f;

	/** 방이 올라오지 않아도 이 시간(초)이 지나면 그냥 진행한다. */
	UPROPERTY(Config, EditAnywhere, Category = "Remote Gate", meta = (ClampMin = "1"))
	float LoadTimeoutSeconds = 8.f;

	UPROPERTY(Config, EditAnywhere, Category = "Remote Gate")
	TArray<FRemoteGateEntry> RemoteGates;
};
