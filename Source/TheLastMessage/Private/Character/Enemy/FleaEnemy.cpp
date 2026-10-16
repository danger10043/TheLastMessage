#include "Character/Enemy/FleaEnemy.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"

AFleaEnemy::AFleaEnemy()
{
	EnemyType = EEnemyType::Flea;

	// 기본값(임시). 블루프린트에서 조정한다. 다른 몬스터보다 체력을 높게 둔다.
	StatConfig.MaxHealth = 250.f;
	StatConfig.MoveSpeed = 220.f;
	StatConfig.AttackRange = 180.f;
	StatConfig.AttackCooldown = 2.f;
}

void AFleaEnemy::StartAttack()
{
	if (IsDead() || IsAttacking() || !GetCurrentTarget())
	{
		return;
	}

	StartAreaAttack();
}

void AFleaEnemy::StartAreaAttack()
{
	bIsAttacking = true;
	FaceTarget();
	PlayAttackMontage();

	// TODO: 공격 몽타주 재생 후 애님 노티파이 시점에 ExecuteAreaAttack 호출로 교체
	GetWorldTimerManager().SetTimer(AreaAttackTimerHandle, this, &AFleaEnemy::ExecuteAreaAttack, AreaAttackWindup, false);
}

void AFleaEnemy::ExecuteAreaAttack()
{
	if (IsDead() || !IsAttacking())
	{
		return;
	}

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);

	TArray<AActor*> Overlapped;
	UKismetSystemLibrary::SphereOverlapActors(this, GetActorLocation(), AreaAttackRadius, ObjectTypes,
		APawn::StaticClass(), ActorsToIgnore, Overlapped);

	// 한 번의 공격에서 같은 대상에게 피해가 중복 적용되지 않도록 한다.
	TSet<AActor*> DamagedActors;
	for (AActor* Actor : Overlapped)
	{
		const APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn || !Pawn->IsPlayerControlled() || DamagedActors.Contains(Actor))
		{
			continue;
		}

		DamagedActors.Add(Actor);

		ApplyEnemyDamage(Actor, GetScaledDamage(AreaAttackDamage));
	}

	SpawnShockwaveEffect();
	FinishAttack();
}

void AFleaEnemy::SpawnShockwaveEffect()
{
	const float EffectScale = ShockwaveEffectBaseRadius > 0.f ? AreaAttackRadius / ShockwaveEffectBaseRadius : 1.f;
	const FVector FeetLocation = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	SpawnEffectAtLocation(ShockwaveEffect, FeetLocation, FRotator::ZeroRotator, FVector(EffectScale));
}

void AFleaEnemy::CancelAttack()
{
	GetWorldTimerManager().ClearTimer(AreaAttackTimerHandle);
	Super::CancelAttack();
}
