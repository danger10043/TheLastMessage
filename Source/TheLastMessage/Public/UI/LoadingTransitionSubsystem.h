#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "LoadingTransitionSubsystem.generated.h"
UCLASS()
class THELASTMESSAGE_API ULoadingTransitionSubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 bool BeginDeath(class APlayerBase* Player, float Delay);
 void LockSpawnMovement(class APlayerBase* Player);
 // 체크포인트 방이 올라올 때까지 부활을 미루는 동안 로딩 화면을 띄운다. 부활이 끝나면 FinishRespawnWait로 바닥 확인 후 걷는다.
 void ShowRespawnWait();
 void FinishRespawnWait(class APlayerBase* Player);
private:
 void PreLoad(const FString& Map);
 void OnMapLoaded(UWorld* World);
 bool Tick(float Delta);
 FDelegateHandle PreHandle, PostHandle;
 FTSTicker::FDelegateHandle TickHandle;
 TSharedPtr<class SWidget> Overlay;
 TWeakObjectPtr<class APlayerBase> DyingPlayer;
 TWeakObjectPtr<class APlayerBase> PendingDeathPlayer;
 TWeakObjectPtr<class APlayerBase> LockedSpawnPlayer;
 float PendingDeathDelay=0;
 friend class FRespawnTransitionReentryTest;
 double Started=0, ReadyAt=0, IndicatorAt=0;
 float DeathDelay=0, OriginalArm=0, OriginalZoom=0;
 bool bLoading=false, bArrived=false, bDeath=false, bFading=false;
 bool bWaitingForSpawnFloor=false, bLoggedMissingSpawnFloor=false;
 // 방 스트리밍: 체크포인트 방이 올라오는 동안 로딩 화면을 띄운 상태인지, 부활 뒤 바닥 확인을 기다리는지.
 bool bRespawnOverlay=false, bRespawnFloorWait=false;
 double RespawnFloorStart=0;
 void RemoveOverlay();
};
