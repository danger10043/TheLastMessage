#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Character/Player/PlayerBase.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossAttributeSet.h"
#include "Character/CombatDamageLibrary.h"
#include "Projectile/ProjectileBase.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GAS/WeaponGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossRangedDamageTest, "TheLastMessage.Weapon.BossRangedDamage",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossRangedDamageTest::RunTest(const FString& Parameters)
{
 UWorld* Factory = LoadObject<UWorld>(nullptr,TEXT("/Game/Test_Jihong/Prototype/Lv_Factory_Metroidvania.Lv_Factory_Metroidvania"));
 if (!TestNotNull(TEXT("Factory map exists"),Factory)) return false;
 ABossMonster* PlacedBoss=nullptr;
 for(AActor* Actor:Factory->PersistentLevel->Actors) if(ABossMonster* Candidate=Cast<ABossMonster>(Actor)) {PlacedBoss=Candidate;break;}
 if (!TestNotNull(TEXT("Factory map contains a boss"),PlacedBoss)) return false;
 const double BossDepth=PlacedBoss->GetActorLocation().Y;
 UWorld::InitializationValues Values;
 Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
 UClass* BossClass = LoadClass<ABossMonster>(nullptr,TEXT("/Game/Test_Inhyeok/Blueprint/BP_TestBossMonster.BP_TestBossMonster_C"));
 UClass* Effect = LoadClass<UGameplayEffect>(nullptr,TEXT("/Game/Test_Inhyeok/GAS/Effect/GE_BP_CommonIncomingDamage.GE_BP_CommonIncomingDamage_C"));
 TestNotNull(TEXT("Map boss class"),BossClass); TestNotNull(TEXT("Player damage effect"),Effect);
 if (!BossClass || !Effect) return false;
 const FTransform BossTransform(FVector(500,BossDepth,200));
 ABossMonster* Boss = World->SpawnActorDeferred<ABossMonster>(BossClass,BossTransform);
 Boss->AutoPossessAI=EAutoPossessAI::Disabled; Boss->FinishSpawning(BossTransform);
 Boss->GetCharacterMovement()->DisableMovement();
 APlayerBase* Player = World->SpawnActor<APlayerBase>(FVector(0,0,200),FRotator::ZeroRotator);
 Player->GetCharacterMovement()->DisableMovement();
 const UBossAttributeSet* Stats=Boss->GetAbilitySystemComponent()->GetSet<UBossAttributeSet>();
 const float Before=Stats->GetHealth();
 AddInfo(FString::Printf(TEXT("Boss state=%d phase=%d immune=%d health=%.1f max=%.1f floor=%.2f"),int(Boss->GetBossState()),int(Boss->GetBossPhase()),Boss->IsRangedImmune(),Before,Stats->GetMaxHealth(),Boss->GetDamageHealthFloorRatio()));
 TestTrue(TEXT("Direct ranged damage accepted"),UCombatDamageLibrary::ApplyFinalDamageToTarget(Player,Boss,Effect,10,WeaponGameplayTags::Damage_PlayerRanged));
 TestEqual(TEXT("Phase one takes direct ranged damage"),Stats->GetHealth(),Before-10);
 AProjectileBase* Shot=World->SpawnActor<AProjectileBase>(FVector(200,0,200),FRotator::ZeroRotator);
 TestTrue(TEXT("Projectile initializes"),Shot->InitializeProjectile(Player,Effect,10,1000,1000)); Shot->LaunchProjectile(FVector::ForwardVector);
 for(int32 Frame=0;Frame<50;++Frame){++GFrameCounter;World->Tick(LEVELTICK_All,0.01f);}
 TestEqual(TEXT("Projectile collision deals ranged damage to the map boss"),Stats->GetHealth(),Before-20);
 World->BeginTearingDown();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 return !HasAnyErrors();
}
#endif
