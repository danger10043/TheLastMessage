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
private:
 void PreLoad(const FString& Map);
 void OnMapLoaded(UWorld* World);
 bool Tick(float Delta);
 FDelegateHandle PreHandle, PostHandle;
 FTSTicker::FDelegateHandle TickHandle;
 TSharedPtr<class SWidget> Overlay;
 TWeakObjectPtr<class APlayerBase> DyingPlayer;
 double Started=0, ReadyAt=0, IndicatorAt=0;
 float DeathDelay=0, OriginalArm=0, OriginalZoom=0;
 bool bLoading=false, bArrived=false, bDeath=false, bFading=false;
};
