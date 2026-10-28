#include "Character/Enemy/RattlerEnemy.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

ARattlerEnemy::ARattlerEnemy()
{
	EnemyType = EEnemyType::Rattler;

	// 기본값(임시). 블루프린트에서 조정한다.
	StatConfig.MaxHealth = 120.f;
	StatConfig.AttackDamage = 10.f;
	StatConfig.MoveSpeed = 250.f;
	StatConfig.AttackRange = 900.f;
	StatConfig.ChaseAcceptableRadius = 700.f;
}

void ARattlerEnemy::ApplyEnemyData(const UEnemyDataAsset* Data)
{
	Super::ApplyEnemyData(Data);
	ProjectileConfig.ProjectileClass = ProjectileClass;

	if (!Data)
	{
		return;
	}

	const FRattlerSettings& S = Data->Rattler;
	BurstCount = S.BurstCount;
	FireInterval = S.FireInterval;
	BurstCooldown = S.BurstCooldown;
	MuzzleOffset = S.MuzzleOffset;
	ProjectileConfig.Speed = S.ProjectileSpeed;
	ProjectileConfig.Lifetime = S.ProjectileLifetime;
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
	bAimLocked = false;
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

	// 연발의 첫 발을 쏘는 순간의 플레이어 위치로 조준 방향을 고정한다. 나머지 발도 같은 방향이라 일자로 날아간다.
	if (!bAimLocked)
	{
		LockedAimDirection = ComputeAimDirection(Muzzle);
		bAimLocked = !LockedAimDirection.IsNearlyZero();
	}

	const FRotator FlashRotation = bAimLocked ? LockedAimDirection.Rotation() : GetActorRotation();

	SpawnEnemyProjectile(ProjectileConfig, Muzzle, LockedAimDirection);
	SpawnEffectAtSocket(MuzzleFlashEffect, MuzzleSocketName, MuzzleOffset, FlashRotation);

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Muzzle);
	}

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
	bAimLocked = false;

	if (bIsAttacking)
	{
		FinishAttack();
	}
}

void ARattlerEnemy::CancelAttack()
{
	GetWorldTimerManager().ClearTimer(BurstTimerHandle);
	RemainingBurst = 0;
	bAimLocked = false;
	Super::CancelAttack();
}
