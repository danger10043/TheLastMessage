#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Weapon/MeleeSweep.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "DataAsset/WeaponDataAsset.h"

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

 auto* Weapon = NewObject<UStaticMeshComponent>(Target);
 Weapon->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
 auto* Data = NewObject<UWeaponDataAsset>();
 Data->bCustomMeleeTrace = true;
 Data->MeleeReachExtraDistance = 0;
 Data->MeleeTraceStart = FVector::ZeroVector;
 Data->MeleeTraceEnd = FVector(100, 0, 0);
 FVector Start, End, Tip, ExtendedTip;
 TestTrue(TEXT("Resolve custom melee reach"), MeleeSweep::ResolveReach(Data, Weapon, Start, End, Tip, ExtendedTip));
 TestTrue(TEXT("Forward tip extends by 25 percent"), ExtendedTip.Equals(FVector(125, 0, 0)));
 Hits.Reset(); Target->SetActorLocation(FVector(120, 0, 0));
 MeleeSweep::Trace(World, nullptr, Start, End, 5, FTransform::Identity, FTransform::Identity, Hits);
 TestTrue(TEXT("Target in added reach is hit"), HasTarget(Hits));
 Hits.Reset(); Target->SetActorLocation(FVector(140, 0, 0));
 MeleeSweep::Trace(World, nullptr, Start, End, 5, FTransform::Identity, FTransform::Identity, Hits);
 TestFalse(TEXT("Target beyond added reach remains unhit"), HasTarget(Hits));
 Data->MeleeTraceStart = FVector(-100, 0, 0); Data->MeleeTraceEnd = FVector::ZeroVector;
 MeleeSweep::ResolveReach(Data, Weapon, Start, End, Tip, ExtendedTip);
 TestTrue(TEXT("Negative-axis weapons extend outward"), Start.Equals(FVector(-125, 0, 0)));
 Data->MeleeReachExtension = 0;
 MeleeSweep::ResolveReach(Data, Weapon, Start, End, Tip, ExtendedTip);
 TestTrue(TEXT("Zero extension preserves original reach"), Start.Equals(FVector(-100, 0, 0)));
 Data->MeleeReachExtraDistance = 100;
 Weapon->SetWorldScale3D(FVector(0.5f));
 MeleeSweep::ResolveReach(Data, Weapon, Start, End, Tip, ExtendedTip);
 TestTrue(TEXT("Fixed reach adds one meter even on a scaled weapon"),
  FMath::IsNearlyEqual(Weapon->GetComponentTransform().TransformVector(ExtendedTip - Tip).Length(), 100.f));
 Hits.Reset(); Target->SetActorLocation(FVector(-140, 0, 0));
 MeleeSweep::Trace(World, nullptr, Start, End, 5, Weapon->GetComponentTransform(), Weapon->GetComponentTransform(), Hits);
 TestTrue(TEXT("Extended attack reaches the one-meter test target"), HasTarget(Hits));
 Hits.Reset(); Target->SetActorLocation(FVector(-165, 0, 0));
 MeleeSweep::Trace(World, nullptr, Start, End, 5, Weapon->GetComponentTransform(), Weapon->GetComponentTransform(), Hits);
 TestFalse(TEXT("Target past the one-meter extension stays unhit"), HasTarget(Hits));
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 return !HasAnyErrors();
}
#endif
