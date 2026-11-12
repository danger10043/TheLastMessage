#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RoomStreamingSubsystem.generated.h"

class ALevelInstance;
struct FRoomStreamEntry;

DECLARE_LOG_CATEGORY_EXTERN(LogRoomStreaming, Log, All);

/**
 * 플레이어가 있는 리전을 기준으로 방 레벨 인스턴스를 올리고 내린다.
 * - 방 리전에 있으면: 그 방과, 복도로 이어진 이웃 방.
 * - 복도 리전에 있으면: 복도에 이어진 방 전부.
 * - 어느 리전에도 없거나 설정에 없는 리전이면 마지막 상태를 유지한다.
 * 설정(Project Settings > Room Streaming)이 꺼져 있으면 만들어지지 않는다.
 */
UCLASS()
class THELASTMESSAGE_API URoomStreamingSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintPure, Category = "Room Streaming")
	FName GetCurrentRegionId() const { return CurrentRegionId; }

	/** 방 번호(예: 08)의 레벨 인스턴스가 로드되어 있는가. 목록에 없는 방이면 false. */
	UFUNCTION(BlueprintPure, Category = "Room Streaming")
	bool IsRoomLoaded(FName RoomId) const;

	/** 현재 리전, 필요한 방, 로드 상태를 로그로 출력한다(콘솔: Rooms.Dump). */
	void DumpState() const;

	/**
	 * 체크포인트 부활처럼 플레이어가 아직 그 자리에 없을 때, 주어진 리전 기준으로 방을 올린다.
	 * 부활이 끝날 때까지(ReleaseRegionHold) 플레이어 위치와 상관없이 이 리전을 유지한다(최대 30초).
	 * 필요한 방이 모두 로드되었거나 설정에 없는 리전이면 true.
	 */
	bool PrepareRegionForRespawn(FName RegionId);
	void ReleaseRegionHold() { HoldRegionId = NAME_None; }

	/** 원격 연출처럼 플레이어와 상관없이 방을 올려 둬야 할 때. 같은 방을 여러 번 고정해도 되고, 고정한 만큼 해제해야 한다. */
	void PinRooms(const TArray<FName>& RoomIds);
	void UnpinRooms(const TArray<FName>& RoomIds);
	/** 목록의 방이 모두 로드되었는가. 이 맵에 없는 방은 로드된 것으로 본다. */
	bool AreRoomsLoaded(const TArray<FName>& RoomIds) const;

private:
	bool AreDesiredRoomsLoaded() const;

	void Update();
	FName FindRegionAtPlayer() const;
	bool BuildDesiredRooms(FName RegionId, TSet<FName>& OutRooms) const;
	void BuildLevelInstanceIndex(TMap<FName, ALevelInstance*>& OutIndex) const;

	float Accumulated = 0.f;
	FName CurrentRegionId;
	TSet<FName> DesiredRooms;
	bool bHasDesired = false;
	FName HoldRegionId;
	double HoldExpireTime = 0.0;
	/** 방 번호 -> 고정 횟수. 고정된 방은 현재 리전과 상관없이 필요한 방으로 본다. */
	TMap<FName, int32> PinnedRooms;
	/** 방 번호 -> 이 시각(월드 시간) 이후에 내려도 된다. */
	TMap<FName, double> UnloadAfter;
};
