#include "Core/CoreSupportRobot.h"

#include "Character/Player/PlayerBase.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

ACoreSupportRobot::ACoreSupportRobot()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	SetCanBeDamaged(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool ACoreSupportRobot::Initialize(APlayerBase* InPlayer)
{
	if (!IsValid(InPlayer) || InPlayer->IsDead()) return false;
	Player = InPlayer;
	InPlayer->OnDestroyed.AddUniqueDynamic(this, &ACoreSupportRobot::HandlePlayerDestroyed);
	return true;
}

void ACoreSupportRobot::HandlePlayerDestroyed(AActor* DestroyedActor)
{
	Destroy();
}

void ACoreSupportRobot::RefreshCombat(float Duration)
{
	const UWorld* World = GetWorld();
	if (!World || !FMath::IsFinite(Duration) || Duration <= 0.0f) return;
	CombatEndTime = FMath::Max(CombatEndTime, World->GetTimeSeconds() + Duration);
}

void ACoreSupportRobot::SetTarget(AActor* InTarget)
{
	Target = InTarget;
}

void ACoreSupportRobot::ClearCombat()
{
	Target.Reset();
	CombatEndTime = 0.0;
}

bool ACoreSupportRobot::IsInCombat() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < CombatEndTime;
}

AActor* ACoreSupportRobot::GetTarget() const
{
	return Target.Get();
}

APlayerBase* ACoreSupportRobot::GetPlayer() const
{
	return Player.Get();
}
