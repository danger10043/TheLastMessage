#include "UI/LoadingTransitionSubsystem.h"
#include "Character/Player/PlayerBase.h"
#include "Save/ProgressSaveSubsystem.h"
#include "Save/ProgressSaveGame.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Camera/SideScrollingCameraManager.h"
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
void ULoadingTransitionSubsystem::PreLoad(const FString& Map)
{
 if(IsRunningDedicatedServer()) return;
 Started=FPlatformTime::Seconds(); ReadyAt=IndicatorAt=0; bLoading=true; bArrived=bFading=false;
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
 if(World && World->GetGameInstance()==GetGameInstance()) bArrived=true;
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
   bDeath=false; bLoading=true;
   if(Saves->RestoreCheckpointInPlace(Player))
   {
    if(PC) if(auto* Camera=Cast<ASideScrollingCameraManager>(PC->PlayerCameraManager)) Camera->CurrentZoom=OriginalZoom;
    if(auto* Arm=Player->FindComponentByClass<USpringArmComponent>()) Arm->TargetArmLength=OriginalArm;
    else if(auto* Camera=Player->FindComponentByClass<UCameraComponent>()){auto Location=Camera->GetRelativeLocation();Location.Y=OriginalArm;Camera->SetRelativeLocation(Location);}
    // Reuse the overlay/fade without scheduling another map load.
    PreLoad(TEXT("")); bArrived=true;
    if(PC && PC->PlayerCameraManager) PC->PlayerCameraManager->SetManualCameraFade(0,FLinearColor::Black,false);
   }
   else if(Saves->ReloadActiveProgress()) UGameplayStatics::OpenLevel(GetGameInstance(),FName(*Saves->GetWorkingProgress()->MapPackage));
   else {bLoading=false; if(PC) if(auto* Camera=Cast<ASideScrollingCameraManager>(PC->PlayerCameraManager)) Camera->CurrentZoom=OriginalZoom; if(PC && PC->PlayerCameraManager) PC->PlayerCameraManager->SetManualCameraFade(0,FLinearColor::Black,false); if(auto* Arm=Player->FindComponentByClass<USpringArmComponent>()) Arm->TargetArmLength=OriginalArm; Player->RespawnAtCheckpoint(); UE_LOG(LogTemp,Error,TEXT("Checkpoint load failed; used in-memory respawn"));}
   return true;
  }
 }
 if(!bLoading || !bArrived) return true;
 auto* PC=GetGameInstance()->GetFirstLocalPlayerController(); if(!PC) return true;
 if(!PC->IsMoveInputIgnored()) PC->SetIgnoreMoveInput(true); if(!PC->IsLookInputIgnored()) PC->SetIgnoreLookInput(true);
 if(PC->GetPawn()) PC->GetPawn()->DisableInput(PC);
 if(Now-Started>=1 && !IndicatorAt) IndicatorAt=Now;
 if((GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>()->GetActiveSlot()>0 && !PC->GetPawn()) || GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>()->IsRestoringProgress()) return true;
 if(!bFading && (!IndicatorAt || Now-IndicatorAt>=0.3)){bFading=true;ReadyAt=Now;}
 if(bFading && Now-ReadyAt>=0.5)
 {
  if(Overlay && GetGameInstance()->GetGameViewportClient()) GetGameInstance()->GetGameViewportClient()->RemoveViewportWidgetContent(Overlay.ToSharedRef());
  Overlay.Reset();bLoading=false; if(PC->GetPawn()) PC->GetPawn()->EnableInput(PC); PC->ResetIgnoreMoveInput();PC->ResetIgnoreLookInput();
 }
 return true;
}
