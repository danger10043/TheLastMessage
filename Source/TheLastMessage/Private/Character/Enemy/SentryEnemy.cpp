#include "Character/Enemy/SentryEnemy.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

ASentryEnemy::ASentryEnemy()
{
	EnemyType = EEnemyType::Sentry;

	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = false;

	// 기본값(임시). 블루프린트·데이터 에셋에서 조정한다.
	StatConfig.MaxHealth = 150.f;
	StatConfig.MoveSpeed = 0.f;
	StatConfig.DetectRange = 1800.f;
	StatConfig.AttackRange = 1500.f;
	StatConfig.AttackCooldown = 3.f;

	StatConfig.AttackDamage = 40.f;
	ProjectileConfig.Speed = 700.f;
	ProjectileConfig.Lifetime = 6.f;
}

void ASentryEnemy::ApplyEnemyData(const UEnemyDataAsset* Data)
{
	Super::ApplyEnemyData(Data);
	ProjectileConfig.ProjectileClass = ProjectileClass;

	if (!Data)
	{
		return;
	}

	const FSentrySettings& S = Data->Sentry;
	ChargeTime = S.ChargeTime;
	RotationSpeed = S.RotationSpeed;
	MinAimPitch = S.MinAimPitch;
	MaxAimPitch = S.MaxAimPitch;
	AimYawOffset = S.AimYawOffset;
	MuzzleOffset = S.MuzzleOffset;
	ProjectileConfig.Speed = S.ProjectileSpeed;
	ProjectileConfig.Lifetime = S.ProjectileLifetime;
}

void ASentryEnemy::BeginPlay()
{
	Super::BeginPlay();

	// 어디든 고정 설치할 수 있도록 이동과 중력을 끈다.
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	GetCharacterMovement()->GravityScale = 0.f;
}

void ASentryEnemy::ResetEnemy()
{
	Super::ResetEnemy();

	// 풀에서 재사용할 때 조준을 처음으로 되돌리고, 다시 고정 상태로 만든다.
	AimYaw = 0.f;
	AimPitch = 0.f;
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	GetCharacterMovement()->GravityScale = 0.f;
}

void ASentryEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RotateToTarget(DeltaTime);
}

void ASentryEnemy::RotateToTarget(float DeltaTime)
{
	const AActor* Target = GetCurrentTarget();
	if (!Target || IsDead())
	{
		return;
	}

	// 조준 중심: 총 상하 회전축 본이 있으면 그 위치, 없으면 액터 위치
	FVector Origin = GetActorLocation();
	if (GetMesh() && GetMesh()->DoesSocketExist(AimPivotBoneName))
	{
		Origin = GetMesh()->GetSocketLocation(AimPivotBoneName);
	}

	const FVector ToTargetWorld = Target->GetActorLocation() - Origin;
	if (ToTargetWorld.IsNearlyZero())
	{
		return;
	}

	// 몸체 기준 방향으로 바꿔 좌우·상하 각도를 구한다. 액터 자체는 회전하지 않는다.
	const FVector ToTargetLocal = GetActorTransform().InverseTransformVectorNoScale(ToTargetWorld);
	const FRotator Desired = ToTargetLocal.Rotation();

	const float DesiredYaw = FRotator::NormalizeAxis(Desired.Yaw + AimYawOffset);
	const float DesiredPitch = FMath::Clamp(Desired.Pitch, MinAimPitch, MaxAimPitch);

	const float MaxStep = RotationSpeed * DeltaTime;
	const float YawDelta = FMath::Clamp(FRotator::NormalizeAxis(DesiredYaw - AimYaw), -MaxStep, MaxStep);
	AimYaw = FRotator::NormalizeAxis(AimYaw + YawDelta);
	AimPitch = FMath::Clamp(AimPitch + FMath::Clamp(DesiredPitch - AimPitch, -MaxStep, MaxStep), MinAimPitch, MaxAimPitch);
}

void ASentryEnemy::StartAttack()
{
	if (IsDead() || IsAttacking() || !GetCurrentTarget())
	{
		return;
	}

	StartCharge();
}

void ASentryEnemy::StartCharge()
{
	bIsAttacking = true;
	bIsCharging = true;
	PlayAttackMontage();
	OnChargeStarted();

	if (ChargeSound && GetMesh())
	{
		// 총구에 붙여 재생해서 포탑이 돌아도 소리가 따라간다.
		const FName AttachSocket = GetMesh()->DoesSocketExist(MuzzleSocketName) ? MuzzleSocketName : NAME_None;
		ChargeAudio = UGameplayStatics::SpawnSoundAttached(ChargeSound, GetMesh(), AttachSocket);
	}

	// 충전 이펙트를 총구에 붙여 재생한다(총이 돌아도 따라간다).
	if (ChargeEffect)
	{
		if (GetMesh() && GetMesh()->DoesSocketExist(MuzzleSocketName))
		{
			ChargeEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(ChargeEffect, GetMesh(), MuzzleSocketName,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
		}
		else
		{
			ChargeEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(ChargeEffect, GetRootComponent(), NAME_None,
				MuzzleOffset, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
		}
	}

	GetWorldTimerManager().SetTimer(ChargeTimerHandle, this, &ASentryEnemy::FireProjectile, ChargeTime, false);
}

void ASentryEnemy::FireProjectile()
{
	// 충전 중 타겟이 사라지거나 몬스터가 죽었다면 발사하지 않고 취소한다.
	if (IsDead() || !GetCurrentTarget())
	{
		CancelAttack();
		return;
	}

	bIsCharging = false;

	// 총구 본·소켓 위치에서 발사한다. 없으면 로컬 오프셋을 쓴다.
	FVector Muzzle = GetActorTransform().TransformPosition(MuzzleOffset);
	if (GetMesh() && GetMesh()->DoesSocketExist(MuzzleSocketName))
	{
		Muzzle = GetMesh()->GetSocketLocation(MuzzleSocketName);
	}

	StopChargeEffect();

	FRotator FlashRotation = GetActorRotation();
	if (const AActor* Target = GetCurrentTarget())
	{
		FlashRotation = (Target->GetActorLocation() - Muzzle).Rotation();
	}

	SpawnEnemyProjectile(ProjectileConfig, Muzzle);
	SpawnEffectAtSocket(MuzzleFlashEffect, MuzzleSocketName, MuzzleOffset, FlashRotation);

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Muzzle);
	}
	OnFired();

	FinishAttack();
}

void ASentryEnemy::CancelCharge()
{
	CancelAttack();
}

void ASentryEnemy::StopChargeEffect()
{
	// 충전 소리도 충전 이펙트와 함께 멈춘다(발사·취소 때).
	if (ChargeAudio)
	{
		ChargeAudio->Stop();
		ChargeAudio = nullptr;
	}

	if (ChargeEffectComponent)
	{
		ChargeEffectComponent->Deactivate();
		ChargeEffectComponent = nullptr;
	}
}

void ASentryEnemy::CancelAttack()
{
	GetWorldTimerManager().ClearTimer(ChargeTimerHandle);
	StopChargeEffect();

	if (bIsCharging)
	{
		bIsCharging = false;
		OnChargeCancelled();
	}

	Super::CancelAttack();
}
