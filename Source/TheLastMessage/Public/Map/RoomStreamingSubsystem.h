#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RoomStreamingSubsystem.generated.h"

class AActor;
class ALevelInstance;
class ULevel;
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
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/**
	 * 시작 단계: 맵이 처음 열릴 때 방은 처음부터 올리지 않고, 시작 리전(저장된 체크포인트 또는 설정의 StartRegionId)과 이어진 방만 올린다.
	 * 레벨 인스턴스는 부모 레벨이 붙을 때 자동으로 로드를 요청하므로, 필요 없는 방은 그 요청을 바로 취소한다(HandleLevelAdded).
	 * 이 구간에서는 대기 없이 바로 정리하고, 정리가 끝날 때까지(IsStartupSettled) 로딩 화면이 유지되도록
	 * LoadingTransitionSubsystem이 EndStartupPhase를 늦춘다.
	 */
	bool IsStartupPhase() const { return bStartupPhase; }
	/** 플레이어 위치의 리전 기준으로 필요한 방이 모두 올라오고 나머지가 모두 내려갔는가. 오래 걸리면(최대 25초) true로 포기한다. */
	bool IsStartupSettled() const;
	void EndStartupPhase() { bStartupPhase = false; }

	UFUNCTION(BlueprintPure, Category = "Room Streaming")
	FName GetCurrentRegionId() const { return CurrentRegionId; }

	/** 방 번호(예: 08)의 레벨 인스턴스가 로드되어 있는가. 목록에 없는 방이면 false. */
	UFUNCTION(BlueprintPure, Category = "Room Streaming")
	bool IsRoomLoaded(FName RoomId) const;

	/**
	 * 리전이 속한 방이 로드되어 있는가. 어느 방에도 속하지 않은 리전(복도 등)은 항상 true.
	 * 내려간 방에는 벽이 없어서 시야 레이가 그대로 지나가므로, 지도 안개를 밝힐 때 이 값으로 걸러낸다.
	 */
	bool IsRegionStreamedIn(FName RegionId) const;

	/** 현재 리전, 필요한 방, 로드 상태를 로그로 출력한다(콘솔: Rooms.Dump). */
	void DumpState() const;

	/**
	 * 체크포인트 부활처럼 플레이어가 아직 그 자리에 없을 때, 주어진 리전 기준으로 방을 올린다.
	 * 부활이 끝날 때까지(ReleaseRegionHold) 플레이어 위치와 상관없이 이 리전을 유지한다(최대 30초).
	 * 방이 모두 로드된 뒤 잠시 더 기다리고(RespawnSettleSeconds), 부활 위치(Location)에서 중력 방향으로 바닥 충돌이 확인되면 true.
	 * 설정에 없는 리전이면 기다릴 필요가 없어 바로 true.
	 */
	bool PrepareRegionForRespawn(FName RegionId, const FVector& Location, const FVector& Gravity);
	void ReleaseRegionHold() { HoldRegionId = NAME_None; }

	/** 원격 연출처럼 플레이어와 상관없이 방을 올려 둬야 할 때. 같은 방을 여러 번 고정해도 되고, 고정한 만큼 해제해야 한다. */
	void PinRooms(const TArray<FName>& RoomIds);
	void UnpinRooms(const TArray<FName>& RoomIds);
	/** 목록의 방이 모두 로드되었는가. 이 맵에 없는 방은 로드된 것으로 본다. */
	bool AreRoomsLoaded(const TArray<FName>& RoomIds) const;

	/**
	 * 방 레벨이 아니라 부모 레벨(Area 레벨, LV_Main)에 놓인 액터가 방이 내려갔을 때 허공에 남지 않게 한다.
	 * 액터에 태그 "RoomBound.<방 번호>"(예: RoomBound.01)를 달면, 그 방이 로드되어 있을 때만 보이고 충돌·틱이 켜진다.
	 * 방이 내려가면 숨기고 충돌·틱을 끈다. 상태(스위치 켜짐 등)는 액터에 그대로 남는다.
	 * 다른 액터에 붙어 있으면(플레이어가 들고 있는 아이템 등) 방과 상관없이 켜 둔다.
	 */
	static const TCHAR* RoomBoundTagPrefix() { return TEXT("RoomBound."); }

private:
	struct FRoomBoundActor
	{
		TWeakObjectPtr<AActor> Actor;
		FName RoomId;
		bool bEnabled = true;
		bool bOrigHidden = false;
		bool bOrigCollision = true;
		bool bOrigTick = true;
	};

	/** 레벨 안에서 RoomBound 태그가 달린 액터를 찾아 등록하고 바로 상태를 맞춘다. */
	void RegisterBoundActors(ULevel* Level);
	void ApplyBoundActors();
	TArray<FRoomBoundActor> BoundActors;

	bool AreDesiredRoomsLoaded() const;

	/** 레벨이 월드에 붙은 직후: 그 안의 방 레벨 인스턴스 중 지금 필요 없는 방의 로드 요청을 취소한다. */
	void HandleLevelAdded(ULevel* Level, UWorld* World);
	FDelegateHandle LevelAddedHandle;

	/** 시작할 때 처음 올릴 리전(체크포인트 → 설정). 없으면 NAME_None. */
	FName ResolveStartRegion() const;

	void Update();
	FName FindRegionAtPlayer() const;
	bool BuildDesiredRooms(FName RegionId, TSet<FName>& OutRooms) const;
	void BuildLevelInstanceIndex(TMap<FName, ALevelInstance*>& OutIndex) const;

	float Accumulated = 0.f;
	bool bStartupPhase = false;
	/** 시작 단계가 시작된 실시간(FPlatformTime). */
	double StartupBeginTime = 0.0;
	FName CurrentRegionId;
	TSet<FName> DesiredRooms;
	bool bHasDesired = false;
	FName HoldRegionId;
	double HoldExpireTime = 0.0;
	/** 부활 대기 중 방이 모두 로드된 것을 처음 본 시각. 아직이면 -1. */
	double LoadedSince = -1.0;
	/** 부활 대기를 시작할 때 올려야 할 방이 있었는가. 이미 다 올라와 있었다면 추가 대기 없이 바로 부활한다. */
	bool bHoldNeededLoading = false;
	/** 방 번호 -> 고정 횟수. 고정된 방은 현재 리전과 상관없이 필요한 방으로 본다. */
	TMap<FName, int32> PinnedRooms;
	/** 방 번호 -> 이 시각(월드 시간) 이후에 내려도 된다. */
	TMap<FName, double> UnloadAfter;
	/** 방 번호 -> 언로드를 요청한 시각(월드 시간). 내려가는 동안 같은 요청을 반복하지 않기 위해 쓴다. */
	TMap<FName, double> UnloadRequestedAt;
};
