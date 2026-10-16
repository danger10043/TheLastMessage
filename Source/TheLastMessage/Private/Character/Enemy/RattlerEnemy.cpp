#include "Character/Enemy/RattlerEnemy.h"
#include "TimerManager.h"

ARattlerEnemy::ARattlerEnemy()
{
	EnemyType = EEnemyType::Rattler;

	// 기본값(임시). 블루프린트에서 조정한다.
	StatConfig.MaxHealth = 120.f;
	StatConfig.MoveSpeed = 250.f;
	StatConfig.AttackRange = 900.f;
}

void ARattlerEnemy::StartAttack()
{
	if (IsDead() || IsAttacking() || !GetCurrentTarget())
	{
		return;
	}

	StartBurst();
}

void ARattlerEnemy::StartBurst()
{
	bIsAttacking = true;
	RemainingBurst = BurstCount;
	FaceTarget();
	PlayAttackMontage();

	FireOnce();

	// 첫 발에서 연발이 끝나지 않았다면 간격마다 이어서 발사한다.
	if (bIsAttacking && RemainingBurst > 0)
	{
		GetWorldTimerManager().SetTimer(BurstTimerHandle, this, &ARattlerEnemy::FireOnce, FireInterval, true);
	}
}

void ARattlerEnemy::FireOnce()
{
	if (IsDead() || !GetCurrentTarget() || RemainingBurst <= 0)
	{
		EndBurst();
		return;
	}

	const FVector Muzzle = GetMuzzleLocation(MuzzleSocketName, MuzzleOffset);

	FRotator FlashRotation = GetActorRotation();
	if (const AActor* Target = GetCurrentTarget())
	{
		FlashRotation = (Target->GetActorLocation() - Muzzle).Rotation();
	}

	SpawnEnemyProjectile(ProjectileConfig, Muzzle);
	SpawnEffectAtSocket(MuzzleFlashEffect, MuzzleSocketName, MuzzleOffset, FlashRotation);

	--RemainingBurst;
	if (RemainingBurst <= 0)
	{
		EndBurst();
	}
}

void ARattlerEnemy::EndBurst()
{
	GetWorldTimerManager().ClearTimer(BurstTimerHandle);
	RemainingBurst = 0;

	if (bIsAttacking)
	{
		FinishAttack();
	}
}

void ARattlerEnemy::CancelAttack()
{
	GetWorldTimerManager().ClearTimer(BurstTimerHandle);
	RemainingBurst = 0;
	Super::CancelAttack();
}
