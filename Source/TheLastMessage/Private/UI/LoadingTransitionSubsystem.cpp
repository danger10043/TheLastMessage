#include "UI/LoadingTransitionSubsystem.h"
#include "Character/Player/PlayerBase.h"
#include "Save/ProgressSaveSubsystem.h"
#include "Save/ProgressSaveGame.h"
#include "Map/RoomStreamingSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Camera/SideScrollingCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "MoviePlayer.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
void ULoadingTransitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 Collection.InitializeDependency<UProgressSaveSubsystem>();
 PreHandle=FCoreUObjectDelegates::PreLoadMap.AddUObject(this,&ThisClass::PreLoad);
 PostHandle=FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this,&ThisClass::OnMapLoaded);
 TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&ThisClass::Tick));
}
void ULoadingTransitionSubsystem::Deinitialize()
{
 FCoreUObjectDelegates::PreLoadMap.Remove(PreHandle);
 FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostHandle);
 FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
 if(Overlay && GetGameInstance()->GetGameViewportClient()) GetGameInstance()->GetGameViewportClient()->RemoveViewportWidgetContent(Overlay.ToSharedRef());
 Overlay.Reset(); Super::Deinitialize();
}
void ULoadingTransitionSubsystem::RemoveOverlay()
{
 if(Overlay && GetGameInstance()->GetGameViewportClient()) GetGameInstance()->GetGameViewportClient()->RemoveViewportWidgetContent(Overlay.ToSharedRef());
 Overlay.Reset();
}
void ULoadingTransitionSubsystem::ShowRespawnWait()
{
 if(bRespawnOverlay || bLoading) return;
 bRespawnOverlay=true; PreLoad(TEXT(""));
 // "불러오는 중…" 표시가 1초 뒤에야 나오는 기존 동작은 짧은 대기에서는 보이지 않으므로 처음부터 보이게 한다.
 Started=FPlatformTime::Seconds()-1.0;
}
void ULoadingTransitionSubsystem::FinishRespawnWait(APlayerBase* Player)
{
 if(!bRespawnOverlay) return;
 bRespawnOverlay=false; bArrived=true;
 // 부활한 자리에서 바닥이 확인될 때까지 플레이어를 붙잡아 두고(최대 12초) 로딩 화면을 유지한다.
 bWaitingForSpawnFloor=true; bRespawnFloorWait=true; RespawnFloorStart=FPlatformTime::Seconds();
 LockSpawnMovement(Player);
}
void ULoadingTransitionSubsystem::PreLoad(const FString& Map)
{
 if(IsRunningDedicatedServer()) return;
 // 이미 떠 있는 로딩 화면(부활 대기 중)이 있으면 겹쳐 남지 않게 먼저 걷는다.
 RemoveOverlay();
 Started=FPlatformTime::Seconds(); ReadyAt=IndicatorAt=0; bLoading=true; bArrived=bFading=false;
 bWaitingForSpawnFloor=bLoggedMissingSpawnFloor=false; bRespawnFloorWait=false;
 LockedSpawnPlayer.Reset();
 Overlay=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
 .BorderBackgroundColor_Lambda([this](){return FLinearColor(0,0,0,bFading ? 1.f-FMath::Clamp(float((FPlatformTime::Seconds()-ReadyAt)/0.5),0.f,1.f):1.f);})
 [SNew(SOverlay)+SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(40)
 [SNew(SBox).Visibility_Lambda([this](){return !bFading && FPlatformTime::Seconds()-Started>=1 ? EVisibility::HitTestInvisible:EVisibility::Hidden;})
 [SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(10,0)[SNew(SThrobber)]
 +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(NSLOCTEXT("Loading","Loading","불러오는 중…"))]]]];
 if(GetGameInstance()->GetGameViewportClient()) GetGameInstance()->GetGameViewportClient()->AddViewportWidgetContent(Overlay.ToSharedRef(),10000);
 if(!Map.IsEmpty() && IsMoviePlayerEnabled()) {FLoadingScreenAttributes Screen; Screen.bAutoCompleteWhenLoadingCompletes=true; Screen.WidgetLoadingScreen=Overlay; GetMoviePlayer()->SetupLoadingScreen(Screen);}
}
void ULoadingTransitionSubsystem::OnMapLoaded(UWorld* World)
{
 if(!bLoading || !World || World->GetGameInstance()!=GetGameInstance()) return;
 bArrived=true;
 const auto* Saves=GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>();
 const auto* Progress=Saves->GetWorkingProgress();
 bWaitingForSpawnFloor=Saves->GetActiveSlot()>0 && Progress
  && Progress->MapPackage==UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
 if(auto* PC=GetGameInstance()->GetFirstLocalPlayerController())
  LockSpawnMovement(Cast<APlayerBase>(PC->GetPawn()));
}

void ULoadingTransitionSubsystem::LockSpawnMovement(APlayerBase* Player)
{
 if(!bWaitingForSpawnFloor || !IsValid(Player) || !Player->IsLocallyControlled()
  || Player->GetWorld()!=GetGameInstance()->GetWorld()) return;

 auto* Movement=Player->GetCharacterMovement();
 if(Movement->MovementMode==MOVE_None) return;
 LockedSpawnPlayer=Player;
 Movement->StopMovementImmediately();
 Movement->DisableMovement();
}
bool ULoadingTransitionSubsystem::BeginDeath(APlayerBase* Player,float Delay)
{
 auto* Saves=GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>();
 if (!IsValid(Player) || !Player->IsDead()) return false;
 if(bDeath) return DyingPlayer.Get()==Player;
 if(bLoading)
 {
  PendingDeathPlayer=Player; PendingDeathDelay=Delay;
  return true;
 }
 if(!Saves || !Player || !Player->GetController() || GetWorld()->GetNetMode()!=NM_Standalone || !Saves->GetSlotInfo(Saves->GetActiveSlot()).bHasCheckpoint) return false;
 if(auto* PC=Cast<APlayerController>(Player->GetController())) if(auto* Camera=Cast<ASideScrollingCameraManager>(PC->PlayerCameraManager)) OriginalZoom=Camera->CurrentZoom;
 DyingPlayer=Player; DeathDelay=FMath::Max(Delay,0.5f); Started=FPlatformTime::Seconds(); bDeath=true;
 if(auto* Arm=Player->FindComponentByClass<USpringArmComponent>()) OriginalArm=Arm->TargetArmLength;
 else if(auto* Camera=Player->FindComponentByClass<UCameraComponent>()) OriginalArm=Camera->GetRelativeLocation().Y;
 return true;
}
bool ULoadingTransitionSubsystem::Tick(float Delta)
{
 const double Now=FPlatformTime::Seconds();
 if(!bLoading && PendingDeathPlayer.IsValid())
 {
  APlayerBase* Pending=PendingDeathPlayer.Get(); PendingDeathPlayer.Reset();
  if(Pending->IsDead() && !BeginDeath(Pending,PendingDeathDelay))
   if (!Pending->RespawnAtCheckpoint()) { PendingDeathPlayer=Pending; }
 }
 if(bDeath)
 {
  auto* Player=DyingPlayer.Get(); if(!Player){bDeath=false;return true;}
  float Elapsed=float(Now-Started);
  if(auto* Arm=Player->FindComponentByClass<USpringArmComponent>()) Arm->TargetArmLength=FMath::Lerp(OriginalArm,OriginalArm*0.9f,FMath::Clamp(Elapsed/0.4f,0.f,1.f));
  else if(auto* Camera=Player->FindComponentByClass<UCameraComponent>()){auto Location=Camera->GetRelativeLocation();Location.Y=FMath::Lerp(OriginalArm,OriginalArm*0.9f,FMath::Clamp(Elapsed/0.4f,0.f,1.f));Camera->SetRelativeLocation(Location);}
  auto* PC=Cast<APlayerController>(Player->GetController());
  if(PC) if(auto* Camera=Cast<ASideScrollingCameraManager>(PC->PlayerCameraManager)) Camera->CurrentZoom=FMath::Lerp(OriginalZoom,OriginalZoom*0.9f,FMath::Clamp(Elapsed/0.4f,0.f,1.f));
  if(PC && PC->PlayerCameraManager) PC->PlayerCameraManager->SetManualCameraFade(FMath::Clamp((Elapsed-DeathDelay)/0.4f,0.f,1.f),FLinearColor::Black,true);
  if(Elapsed>=DeathDelay+0.4f)
  {
   auto* Saves=GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>(); if(Saves->IsSaving()) return true;
   // 방 스트리밍이 켜져 있으면 체크포인트가 있던 방이 올라오고 바닥 충돌이 확인될 때까지 로딩 화면을 띄우고 기다린다(최대 10초).
   // 안 기다리면 아직 없는 맵에 끼거나 빈 공간으로 떨어진다.
   URoomStreamingSubsystem* Rooms=GetGameInstance()->GetWorld()?GetGameInstance()->GetWorld()->GetSubsystem<URoomStreamingSubsystem>():nullptr;
   const UProgressSaveGame* RespawnProgress=Saves->GetWorkingProgress();
   if(Rooms && RespawnProgress && Elapsed<DeathDelay+0.4f+10.f
    && !Rooms->PrepareRegionForRespawn(RespawnProgress->CheckpointRegionId,RespawnProgress->RespawnTransform.GetLocation(),RespawnProgress->RespawnGravity))
   {
    // 기존 로딩 화면은 "불러오는 중…" 표시를 1초 뒤에야 보여 주는데, 방이 금방 올라오면 검은 화면만 보이므로 처음부터 보이게 한다.
    if(!bRespawnOverlay){bRespawnOverlay=true; PreLoad(TEXT("")); Started=Now-1.0;}
    return true;
   }
   bDeath=false; bLoading=true;
   const bool bRestoredInPlace=Saves->RestoreCheckpointInPlace(Player);
   if(Rooms) Rooms->ReleaseRegionHold();
   if(bRestoredInPlace)
   {
    if(PC) if(auto* Camera=Cast<ASideScrollingCameraManager>(PC->PlayerCameraManager)) Camera->CurrentZoom=OriginalZoom;
    if(auto* Arm=Player->FindComponentByClass<USpringArmComponent>()) Arm->TargetArmLength=OriginalArm;
    else if(auto* Camera=Player->FindComponentByClass<UCameraComponent>()){auto Location=Camera->GetRelativeLocation();Location.Y=OriginalArm;Camera->SetRelativeLocation(Location);}
    // Reuse the overlay/fade without scheduling another map load. 대기 중에 이미 띄운 로딩 화면은 그대로 쓴다.
    if(!bRespawnOverlay) PreLoad(TEXT(""));
    bRespawnOverlay=false; bArrived=true;
    // 방 스트리밍 중에는 부활한 자리에서 바닥이 확인될 때까지 플레이어를 붙잡아 둔다(최대 12초).
    if(Rooms){bWaitingForSpawnFloor=true; bRespawnFloorWait=true; RespawnFloorStart=Now; LockSpawnMovement(Player);}
    if(PC && PC->PlayerCameraManager) PC->PlayerCameraManager->SetManualCameraFade(0,FLinearColor::Black,false);
   }
   else if(Saves->ReloadActiveProgress()) {bRespawnOverlay=false; UGameplayStatics::OpenLevel(GetGameInstance(),FName(*Saves->GetWorkingProgress()->MapPackage));}
   else {bRespawnOverlay=false; RemoveOverlay(); bLoading=false; if(PC) if(auto* Camera=Cast<ASideScrollingCameraManager>(PC->PlayerCameraManager)) Camera->CurrentZoom=OriginalZoom; if(PC && PC->PlayerCameraManager) PC->PlayerCameraManager->SetManualCameraFade(0,FLinearColor::Black,false); if(auto* Arm=Player->FindComponentByClass<USpringArmComponent>()) Arm->TargetArmLength=OriginalArm; Player->RespawnAtCheckpoint(); UE_LOG(LogTemp,Error,TEXT("Checkpoint load failed; used in-memory respawn"));}
   return true;
  }
 }
 if(!bLoading || !bArrived) return true;
 auto* PC=GetGameInstance()->GetFirstLocalPlayerController(); if(!PC) return true;
 if(!PC->IsMoveInputIgnored()) PC->SetIgnoreMoveInput(true); if(!PC->IsLookInputIgnored()) PC->SetIgnoreLookInput(true);
 if(PC->GetPawn()) PC->GetPawn()->DisableInput(PC);
 auto* Player=Cast<APlayerBase>(PC->GetPawn());
 LockSpawnMovement(Player);
 if(Now-Started>=1 && !IndicatorAt) IndicatorAt=Now;
 if((GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>()->GetActiveSlot()>0 && !PC->GetPawn()) || GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>()->IsRestoringProgress()) return true;
 if(bWaitingForSpawnFloor && Player)
 {
  auto* Movement=Player->GetCharacterMovement();
  FFindFloorResult Floor;
  // Some PlayerStarts are several meters above the floor. This query follows character gravity.
  constexpr float SpawnFloorDistance=600.f;
  Movement->ComputeFloorDist(Player->GetActorLocation(), SpawnFloorDistance, SpawnFloorDistance,
   Floor, Player->GetCapsuleComponent()->GetScaledCapsuleRadius());
  // 부활 뒤 바닥 확인은 12초까지만 기다리고, 그래도 없으면 플레이어를 풀어 준다(영원히 갇히지 않게).
  const bool bGiveUpRespawnFloor=bRespawnFloorWait && Now-RespawnFloorStart>=12.0;
  if(!Floor.IsWalkableFloor() && !bGiveUpRespawnFloor)
  {
   if(!bLoggedMissingSpawnFloor && Now-Started>=15.0)
   {
    bLoggedMissingSpawnFloor=true;
    UE_LOG(LogTemp, Warning, TEXT("Loading is waiting for walkable floor below %s at %s"),
     *GetNameSafe(Player), *Player->GetActorLocation().ToString());
   }
   return true;
  }
  bWaitingForSpawnFloor=false; bRespawnFloorWait=false;
  if(LockedSpawnPlayer.Get()==Player) Movement->SetDefaultMovementMode();
  LockedSpawnPlayer.Reset();
 }
 if(!bFading && (!IndicatorAt || Now-IndicatorAt>=0.3)){bFading=true;ReadyAt=Now;}
 if(bFading && Now-ReadyAt>=0.5)
 {
  if(Overlay && GetGameInstance()->GetGameViewportClient()) GetGameInstance()->GetGameViewportClient()->RemoveViewportWidgetContent(Overlay.ToSharedRef());
  Overlay.Reset();bLoading=false; if(PC->GetPawn()) PC->GetPawn()->EnableInput(PC); PC->ResetIgnoreMoveInput();PC->ResetIgnoreLookInput();
 }
 return true;
}
