#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/LoadingTransitionSubsystem.h"
#include "Character/Player/PlayerBase.h"
#include "GAS/CombatAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRespawnTransitionReentryTest,"TheLastMessage.Checkpoint.RespawnProtectionAndReentry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRespawnTransitionReentryTest::RunTest(const FString& Parameters)
{
 UWorld::InitializationValues Values;
 Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
 auto* Player=World->SpawnActor<APlayerBase>();
 auto* PC=World->SpawnActor<APlayerController>(); PC->SetPlayer(NewObject<ULocalPlayer>(GEngine)); PC->Possess(Player);
 Player->SetPlayerState(World->SpawnActor<APlayerState>());
 auto* Stats=Player->GetCombatAttributes(); Stats->SetMaxHealth(100); Stats->SetHealth(100);
 auto* ASC=Player->GetAbilitySystemComponent();
 auto* Damage=NewObject<UGameplayEffect>(); Damage->DurationPolicy=EGameplayEffectDurationType::Instant;
 auto& Mod=Damage->Modifiers.AddDefaulted_GetRef(); Mod.Attribute=UCombatAttributeSet::GetIncomingDamageAttribute(); Mod.ModifierOp=EGameplayModOp::Additive; Mod.ModifierMagnitude=FScalableFloat(150);
 auto Apply=[&](){ASC->ApplyGameplayEffectToSelf(Damage,1,ASC->MakeEffectContext());};
 Player->HandleDeath(); TestTrue(TEXT("Initial respawn succeeds"),Player->RespawnAtCheckpoint());
 TestTrue(TEXT("Respawn grants protection"),Player->IsRespawnProtected());
 Apply(); TestEqual(TEXT("Lethal damage ignored while protected"),Stats->GetHealth(),100.f);
 TestFalse(TEXT("Protected player stays alive"),Player->IsDead());
 for(int32 I=0;I<60;++I){++GFrameCounter;World->Tick(LEVELTICK_All,0.02f);}
 TestFalse(TEXT("Protection expires after one second"),Player->IsRespawnProtected());
 Apply(); TestTrue(TEXT("Damage works after expiry"),Player->IsDead());
 TestTrue(TEXT("Second respawn succeeds"),Player->RespawnAtCheckpoint());
 auto* Direct=NewObject<UGameplayEffect>(); Direct->DurationPolicy=EGameplayEffectDurationType::Instant;
 auto& HealthMod=Direct->Modifiers.AddDefaulted_GetRef(); HealthMod.Attribute=UCombatAttributeSet::GetHealthAttribute(); HealthMod.ModifierOp=EGameplayModOp::Additive; HealthMod.ModifierMagnitude=FScalableFloat(-150);
 ASC->ApplyGameplayEffectToSelf(Direct,1,ASC->MakeEffectContext());
 TestEqual(TEXT("Direct health damage also blocked"),Stats->GetHealth(),100.f);
 // Forced death bypasses damage protection and exercises transition reentry.
 Player->HandleDeath();
 TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
 TStrongObjectPtr<ULoadingTransitionSubsystem> Transition(NewObject<ULoadingTransitionSubsystem>(Instance.Get()));
 Transition->bLoading=true;
 TestTrue(TEXT("Death during fade is accepted"),Transition->BeginDeath(Player,0.1f));
 TestTrue(TEXT("Accepted death is queued"),Transition->PendingDeathPlayer.Get()==Player);
 Transition->bLoading=false; Transition->Tick(0.01f);
 TestFalse(TEXT("Queued death revives even without an active save session"),Player->IsDead());
 TestFalse(TEXT("Queued request is consumed"),Transition->PendingDeathPlayer.IsValid());
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 return !HasAnyErrors();
}
#endif
