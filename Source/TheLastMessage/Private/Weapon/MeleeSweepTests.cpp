#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Weapon/MeleeSweep.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMeleeSweepPathTest,"TheLastMessage.Weapon.MeleeSweptPath",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMeleeSweepPathTest::RunTest(const FString& Parameters)
{
 UWorld::InitializationValues Values;Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 AActor* Target=World->SpawnActor<AActor>();
 auto* Box=NewObject<UBoxComponent>(Target);Target->SetRootComponent(Box);Target->AddInstanceComponent(Box);
 Box->SetBoxExtent(FVector(3));Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();
 auto HasTarget=[Target](const TArray<FHitResult>& Hits){for(const auto& Hit:Hits) if(Hit.GetActor()==Target) return true;return false;};
 TArray<FHitResult> Hits;
 Target->SetActorLocation(FVector(100,0,0));
 MeleeSweep::Trace(World,nullptr,FVector::ZeroVector,FVector::ZeroVector,5,FTransform(FVector::ZeroVector),FTransform(FVector(200,0,0)),Hits);
 TestTrue(TEXT("Fast swing crosses a narrow target between frames"),HasTarget(Hits));
 Hits.Reset();Target->SetActorLocation(FVector(0,100,0));
 MeleeSweep::Trace(World,nullptr,FVector(100,0,0),FVector(100,0,0),5,FTransform::Identity,FTransform(FRotator(0,180,0)),Hits);
 TestTrue(TEXT("Rotation follows the arc instead of only its chord"),HasTarget(Hits));
 Hits.Reset();Target->SetActorLocation(FVector(0,160,0));
 MeleeSweep::Trace(World,nullptr,FVector(100,0,0),FVector(100,0,0),5,FTransform::Identity,FTransform(FRotator(0,180,0)),Hits);
 TestFalse(TEXT("Targets outside weapon reach stay unhit"),HasTarget(Hits));
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 return !HasAnyErrors();
}
#endif
