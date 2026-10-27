#include "Character/Enemy/JunkerEnemy.h"
#include "Character/Enemy/EnemyAIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

AJunkerEnemy::AJunkerEnemy()
{
	EnemyType = EEnemyType::Junker;

	// 후퇴 중에만 틱을 켠다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 기본값(임시). 블루프린트에서 조정한다.
	StatConfig.MaxHealth = 300.f;
	StatConfig.AttackDamage = 10.f;           // 기관총 1발 피해
	StatConfig.SecondaryAttackDamage = 15.f; // 주먹질 1회 피해
	StatConfig.MoveSpeed = 250.f;
	StatConfig.DetectRange = 1800.f;
	StatConfig.AttackRange = 1000.f;          // 이 거리 안에서 원거리 점사를 시작한다.
	StatConfig.ChaseAcceptableRadius = 120.f; // 돌격 때 근접 거리까지 붙는다.
}

void AJunkerEnemy::ApplyEnemyData(const UEnemyDataAsset* Data)
{
	Super::ApplyEnemyData(Data);
	MachineGunConfig.ProjectileClass = ProjectileClass;

	if (!Data)
	{
		return;
	}

	const FJunkerSettings& S = Data->Junker;
	ShotsPerBurst = S.ShotsPerBurst;
	FireInterval = S.FireInterval;
	BurstGap = S.BurstGap;
	EndDelay = S.EndDelay;
	ShootMontageBlendOut = S.ShootMontageBlendOut;
	MinBursts = S.MinBursts;
	MaxBursts = S.MaxBursts;
	RangedCooldown = S.RangedCooldown;
	MuzzleOffset = S.MuzzleOffset;
	RushDuration = S.RushDuration;
	RushStallTimeLimit = S.RushStallTimeLimit;
	RushSpeedMultiplier = S.RushSpeedMultiplier;
	MeleeRange = S.MeleeRange;
	MeleeComboCount = S.MeleeComboCount;
	MeleeInterval = S.MeleeInterval;
	MeleeHitDelay = S.MeleeHitDelay;
	MeleeReach = S.MeleeReach;
	MeleeRadius = S.MeleeRadius;
	RetreatMaxDistance = S.RetreatMaxDistance;
	RetreatLedgeCheckDistance = S.RetreatLedgeCheckDistance;
	RetreatMaxDropHeight = S.RetreatMaxDropHeight;
	RetreatSpeed = S.RetreatSpeed;
	MachineGunConfig.Speed = S.ProjectileSpeed;
	MachineGunConfig.Lifetime = S.ProjectileLifetime;
}

// ---------------------------------------------------------------------------
// 공격 시작·판정
// ---------------------------------------------------------------------------

bool AJunkerEnemy::CanAttackTarget() const
{
	// 돌격 단계에서는 근접 범위에 들어왔을 때만 공격이 시작된다. 그동안 BT는 추격 분기로 달려가게 한다.
	if (Phase == EJunkerPhase::Rush)
	{
		return GetCurrentTarget() && !IsDead() && !IsAttacking() && GetEdgeDistanceToTarget() <= MeleeRange;
	}

	return Super::CanAttackTarget();
}

void AJunkerEnemy::OnPlayerLost()
{
	// 돌격·근접·후퇴 사격 도중에 시야에서 놓쳐도 바로 정찰로 돌아가지 않는다.
	// 플레이어가 멀어졌을 때 단계가 처음으로 초기화되어 느리게 걸어가는 것을 막기 위해, 단계가 끝날 때까지 계속 추적한다.
	if (!IsDead() && (Phase != EJunkerPhase::Ranged || IsAttacking()))
	{
		bLostWhileEngaged = true;
		return;
	}

	bLostWhileEngaged = false;
	Super::OnPlayerLost();
}

void AJunkerEnemy::OnTargetDied()
{
	// 플레이어가 죽었으면 놓친 것이 아니라 대상이 사라진 것이므로 공격 단계 도중이어도 바로 정리한다.
	bLostWhileEngaged = false;
	AEnemyBase::OnPlayerLost();
}

void AJunkerEnemy::OnPlayerDetected(AActor* Player)
{
	bLostWhileEngaged = false;
	Super::OnPlayerDetected(Player);
}

float AJunkerEnemy::GetEdgeDistanceToTarget() const
{
	const AActor* Target = GetCurrentTarget();
	if (!Target)
	{
		return MAX_FLT;
	}

	// 추격(MoveTo)은 목표와 자신의 캡슐 반지름만큼 떨어진 곳에서 멈추므로, 중심 거리가 아니라 가장자리 거리로 판단한다.
	float TargetRadius = 0.f;
	if (const ACharacter* TargetCharacter = Cast<ACharacter>(Target))
	{
		TargetRadius = TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius();
	}

	const float SelfRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	return FMath::Max(GetDistanceToTarget() - SelfRadius - TargetRadius, 0.f);
}

void AJunkerEnemy::StartAttack()
{
	if (IsDead() || IsAttacking() || !GetCurrentTarget())
	{
		return;
	}

	bIsAttacking = true;

	// 공격 단계가 끝날 때까지 BT가 이동으로 끼어들지 않게 한다.
	PauseAI();
	FaceTarget();

	if (Phase == EJunkerPhase::Rush)
	{
		StartMeleeCombo();
	}
	else
	{
		StartRangedSequence(false);
	}
}

void AJunkerEnemy::CancelAttack()
{
	ResetPhase();
	ResumeAI();

	Super::CancelAttack();
}

void AJunkerEnemy::ResetPhase()
{
	GetWorldTimerManager().ClearTimer(SequenceTimerHandle);
	GetWorldTimerManager().ClearTimer(MeleeHitTimerHandle);
	GetWorldTimerManager().ClearTimer(RushTimerHandle);
	GetWorldTimerManager().ClearTimer(RushCheckTimerHandle);

	StopShootMontage();
	StopRetreatMovement();
	StopRushSpeed();

	Phase = EJunkerPhase::Ranged;
	bRetreating = false;
	bLostWhileEngaged = false;
	BurstsLeft = 0;
	ShotsLeft = 0;
	bAimLocked = false;
	PunchesLeft = 0;
}

// ---------------------------------------------------------------------------
// 원거리 점사 (제자리 또는 후퇴하며)
// ---------------------------------------------------------------------------

void AJunkerEnemy::StartRangedSequence(bool bInRetreat)
{
	bRetreating = bInRetreat;
	Phase = bInRetreat ? EJunkerPhase::Retreat : EJunkerPhase::Ranged;

	const int32 MinCount = FMath::Max(MinBursts, 1);
	BurstsLeft = FMath::RandRange(MinCount, FMath::Max(MaxBursts, MinCount));

	StartBurst();
}

void AJunkerEnemy::StartBurst()
{
	if (IsDead() || !GetCurrentTarget())
	{
		return;
	}

	ShotsLeft = FMath::Max(ShotsPerBurst, 1);
	bAimLocked = false;
	FaceTarget();

	ActiveShootMontage = bRetreating ? RetreatShootMontage : BurstMontage;
	if (ActiveShootMontage)
	{
		PlayAnimMontage(ActiveShootMontage);
	}

	FireShot();
}

void AJunkerEnemy::FireShot()
{
	if (IsDead() || !GetCurrentTarget())
	{
		return;
	}

	const FVector Muzzle = GetMuzzleLocation(MuzzleSocketName, MuzzleOffset);

	// 점사의 첫 발을 쏘는 순간의 플레이어 위치로 조준 방향을 고정한다. 나머지 발도 같은 방향이라 일자로 날아간다.
	if (!bAimLocked)
	{
		LockedAimDirection = ComputeAimDirection(Muzzle);
		bAimLocked = !LockedAimDirection.IsNearlyZero();
	}

	const FRotator FlashRotation = bAimLocked ? LockedAimDirection.Rotation() : GetActorRotation();

	SpawnEnemyProjectile(MachineGunConfig, Muzzle, LockedAimDirection);
	SpawnEffectAtSocket(MuzzleFlashEffect, MuzzleSocketName, MuzzleOffset, FlashRotation);

	--ShotsLeft;
	if (ShotsLeft > 0)
	{
		GetWorldTimerManager().SetTimer(SequenceTimerHandle, this, &AJunkerEnemy::FireShot, FireInterval, false);
		return;
	}

	// 점사가 끝났다. 남은 점사가 있으면 간격 뒤에 이어서 쏘고, 없으면 같은 간격 뒤에 단계를 마친다.
	--BurstsLeft;
	if (BurstsLeft > 0)
	{
		GetWorldTimerManager().SetTimer(SequenceTimerHandle, this, &AJunkerEnemy::StartBurst, BurstGap, false);
	}
	else
	{
		GetWorldTimerManager().SetTimer(SequenceTimerHandle, this, &AJunkerEnemy::FinishRangedSequence, FMath::Max(EndDelay, 0.01f), false);
	}
}

void AJunkerEnemy::StopShootMontage()
{
	if (ActiveShootMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInstance->Montage_Stop(ShootMontageBlendOut, ActiveShootMontage);
		}
		ActiveShootMontage = nullptr;
	}
}

void AJunkerEnemy::FinishRangedSequence()
{
	// 남은 사격 모션을 끊어야 서 있는 자세로 달려가지 않는다.
	StopShootMontage();

	StopRetreatMovement();
	bRetreating = false;

	ResumeAI();
	FinishAttack(); // 공격 중 해제 + 쿨다운 시작

	// 점사를 마치면 플레이어에게 돌격한다.
	StartRush();
}

// ---------------------------------------------------------------------------
// 돌격
// ---------------------------------------------------------------------------

void AJunkerEnemy::StartRush()
{
	if (IsDead())
	{
		return;
	}

	Phase = EJunkerPhase::Rush;
	GetCharacterMovement()->MaxWalkSpeed = GetStatConfig().MoveSpeed * RushSpeedMultiplier;

	GetWorldTimerManager().SetTimer(RushTimerHandle, this, &AJunkerEnemy::EndRushByTimeout, RushDuration, false);

	// 돌격 중 길이 막혀 제자리에 서 있지 않은지 주기적으로 확인한다.
	RushStallElapsed = 0.f;
	GetWorldTimerManager().SetTimer(RushCheckTimerHandle, this, &AJunkerEnemy::CheckRushStall, 0.1f, true);
}

void AJunkerEnemy::EndRush()
{
	GetWorldTimerManager().ClearTimer(RushTimerHandle);
	GetWorldTimerManager().ClearTimer(RushCheckTimerHandle);

	StopRushSpeed();
	Phase = EJunkerPhase::Ranged;

	// 돌격 중에 놓친 플레이어를 끝내 다시 찾지 못했다면 이제 놓친 것으로 처리한다.
	if (bLostWhileEngaged)
	{
		bLostWhileEngaged = false;
		OnPlayerLost();
	}
}

void AJunkerEnemy::CheckRushStall()
{
	if (Phase != EJunkerPhase::Rush || IsAttacking())
	{
		return;
	}

	// 거의 움직이지 않으면서 근접 범위에도 들어오지 못했다면(플레이어에게 닿을 길이 없는 경우) 돌격을 포기한다.
	const bool bStalled = GetVelocity().Size2D() < 20.f && !CanAttackTarget();
	RushStallElapsed = bStalled ? RushStallElapsed + 0.1f : 0.f;

	if (RushStallElapsed >= RushStallTimeLimit)
	{
		EndRush();
	}
}

void AJunkerEnemy::EndRushByTimeout()
{
	// 근접 연타 중에는 돌격 타이머가 이미 해제돼 있으므로 여기에 오는 것은 닿지 못한 경우뿐이다.
	if (Phase != EJunkerPhase::Rush || IsAttacking())
	{
		return;
	}

	EndRush();
}

void AJunkerEnemy::StopRushSpeed()
{
	GetCharacterMovement()->MaxWalkSpeed = GetStatConfig().MoveSpeed;
}

// ---------------------------------------------------------------------------
// 근접 연타
// ---------------------------------------------------------------------------

void AJunkerEnemy::StartMeleeCombo()
{
	// 돌격은 여기서 끝난다.
	GetWorldTimerManager().ClearTimer(RushTimerHandle);
	GetWorldTimerManager().ClearTimer(RushCheckTimerHandle);
	StopRushSpeed();

	Phase = EJunkerPhase::Melee;
	PunchesLeft = FMath::Max(MeleeComboCount, 1);

	StartPunch();
}

void AJunkerEnemy::StartPunch()
{
	if (IsDead() || !GetCurrentTarget())
	{
		return;
	}

	FaceTarget();

	if (MeleeMontage)
	{
		// 주먹질 순서에 맞는 섹션부터 재생한다. 섹션이 지정되지 않았거나 몽타주에 없으면 처음부터 재생한다.
		const int32 PunchIndex = FMath::Max(MeleeComboCount, 1) - PunchesLeft;
		FName Section = MeleeSectionNames.IsValidIndex(PunchIndex) ? MeleeSectionNames[PunchIndex] : NAME_None;
		if (!Section.IsNone() && MeleeMontage->GetSectionIndex(Section) == INDEX_NONE)
		{
			Section = NAME_None;
		}

		PlayAnimMontage(MeleeMontage, 1.f, Section);

		// 섹션이 다음 섹션으로 자동 연결돼 있어도 이 섹션이 끝나면 몽타주가 끝나게 한다(다음 주먹질은 따로 재생).
		if (!Section.IsNone())
		{
			if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				AnimInstance->Montage_SetNextSection(Section, NAME_None, MeleeMontage);
			}
		}
	}

	GetWorldTimerManager().SetTimer(MeleeHitTimerHandle, this, &AJunkerEnemy::DoMeleeHit, MeleeHitDelay, false);
	GetWorldTimerManager().SetTimer(SequenceTimerHandle, this, &AJunkerEnemy::NextPunch, MeleeInterval, false);
}

void AJunkerEnemy::DoMeleeHit()
{
	UWorld* World = GetWorld();
	if (!World || IsDead() || Phase != EJunkerPhase::Melee)
	{
		return;
	}

	const FVector Center = GetActorLocation() + GetActorForwardVector() * MeleeReach;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JunkerMelee), false, this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(MeleeRadius), QueryParams);

	// 한 번의 주먹질에서 같은 대상에게 피해가 중복되지 않게 한다.
	TSet<AActor*> Damaged;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		const APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn || !Pawn->IsPlayerControlled() || Damaged.Contains(Actor))
		{
			continue;
		}

		Damaged.Add(Actor);
		ApplyEnemyDamage(Actor, GetScaledDamage(GetStatConfig().SecondaryAttackDamage));
		SpawnEffectAtLocation(MeleeHitEffect, Actor->GetActorLocation());
	}
}

void AJunkerEnemy::NextPunch()
{
	--PunchesLeft;
	if (PunchesLeft > 0)
	{
		StartPunch();
	}
	else
	{
		StartRetreat();
	}
}

// ---------------------------------------------------------------------------
// 후퇴 사격
// ---------------------------------------------------------------------------

void AJunkerEnemy::StartRetreat()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();

	// 뒤로 물러나다 낭떠러지로 떨어지지 않게 한다.
	bSavedCanWalkOffLedges = Movement->bCanWalkOffLedges;
	Movement->bCanWalkOffLedges = false;
	Movement->MaxWalkSpeed = RetreatSpeed;

	bRetreatMovementActive = true;
	RetreatStartX = GetActorLocation().X;
	SetActorTickEnabled(true);

	StartRangedSequence(true);
}

void AJunkerEnemy::StopRetreatMovement()
{
	if (!bRetreatMovementActive)
	{
		return;
	}

	bRetreatMovementActive = false;
	SetActorTickEnabled(false);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bCanWalkOffLedges = bSavedCanWalkOffLedges;
	Movement->MaxWalkSpeed = GetStatConfig().MoveSpeed;
}

bool AJunkerEnemy::HasGroundBehind(const FVector& AwayDirection) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	// 캡슐 가장자리에서 조금 앞선 지점에서 아래로 쏘아 바닥을 찾는다.
	const FVector Start = GetActorLocation() + AwayDirection * (Radius + RetreatLedgeCheckDistance);
	const FVector End = Start - FVector(0.f, 0.f, HalfHeight + RetreatMaxDropHeight);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JunkerLedgeCheck), false, this);

	FHitResult Hit;
	return World->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, QueryParams);
}

void AJunkerEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bRetreatMovementActive || IsDead())
	{
		return;
	}

	const AActor* Target = GetCurrentTarget();
	if (!Target)
	{
		return;
	}

	// 플레이어를 바라본 채 반대 방향(좌우)으로 물러난다.
	FaceTarget();

	const float DeltaX = GetActorLocation().X - Target->GetActorLocation().X;
	const FVector Away(FMath::IsNearlyZero(DeltaX) ? -GetActorForwardVector().X : FMath::Sign(DeltaX), 0.f, 0.f);

	// 정해진 거리만큼 물러났으면 그 자리에서 사격만 계속한다(플레이어와 너무 멀어지지 않게).
	if (FMath::Abs(GetActorLocation().X - RetreatStartX) >= RetreatMaxDistance)
	{
		return;
	}

	// 뒤쪽에 바닥이 없으면(낭떠러지) 더 물러나지 않고 그 자리에서 사격만 계속한다.
	const FVector AwayDirection = Away.GetSafeNormal();
	if (!HasGroundBehind(AwayDirection))
	{
		return;
	}

	// bForce: AI가 이동 입력을 막아 두어도 후퇴 입력은 적용한다.
	AddMovementInput(AwayDirection, 1.f, true);
}

// ---------------------------------------------------------------------------
// AI 일시정지
// ---------------------------------------------------------------------------

void AJunkerEnemy::PauseAI()
{
	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		AIC->PauseEnemyAI();
	}
}

void AJunkerEnemy::ResumeAI()
{
	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		AIC->ResumeEnemyAI();
	}
}
