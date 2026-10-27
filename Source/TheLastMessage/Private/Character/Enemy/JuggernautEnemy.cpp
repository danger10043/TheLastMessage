#include "Character/Enemy/JuggernautEnemy.h"
#include "Character/Enemy/EnemyAIController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

AJuggernautEnemy::AJuggernautEnemy()
{
	EnemyType = EEnemyType::Juggernaut;

	// 돌진 중에만 틱을 켠다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 기본값(임시). 블루프린트에서 조정한다.
	StatConfig.MaxHealth = 400.f;
	StatConfig.AttackDamage = 30.f;           // 돌진 피해
	StatConfig.MoveSpeed = 220.f;
	StatConfig.DetectRange = 1800.f;
	StatConfig.AttackRange = 1000.f;          // 이 거리 안에서 돌진을 시작한다.
	StatConfig.ChaseAcceptableRadius = 800.f; // 돌진 거리 안까지만 접근한다.

	// 원거리 피해 50% 감소
	RangedDamageMultiplier = 0.5f;

	// 낭떠러지에서 멈추지 않고 그대로 떨어지게 한다.
	GetCharacterMovement()->bCanWalkOffLedges = true;
}

void AJuggernautEnemy::ApplyEnemyData(const UEnemyDataAsset* Data)
{
	Super::ApplyEnemyData(Data);
	if (!Data)
	{
		return;
	}

	const FJuggernautSettings& S = Data->Juggernaut;
	ChargeWindup = S.ChargeWindup;
	ChargeSpeed = S.ChargeSpeed;
	ChargeAcceleration = S.ChargeAcceleration;
	ChargeMaxDistance = S.ChargeMaxDistance;
	ChargeMaxDuration = S.ChargeMaxDuration;
	ChargeHitMargin = S.ChargeHitMargin;
	bEndChargeOnHit = S.bEndChargeOnHit;
	bPassThroughPawnsWhileCharging = S.bPassThroughPawnsWhileCharging;
	ChargeRecoveryTime = S.ChargeRecoveryTime;
	WallStunTime = S.WallStunTime;
	WallHitSpeedRatio = S.WallHitSpeedRatio;
	ChargeCooldown = S.ChargeCooldown;
	KnockbackHorizontal = S.KnockbackHorizontal;
	KnockbackUp = S.KnockbackUp;
	RangedDamageMultiplier = S.RangedDamageMultiplier;
}

void AJuggernautEnemy::BeginPlay()
{
	Super::BeginPlay();

	DefaultMaxAcceleration = GetCharacterMovement()->MaxAcceleration;
}

void AJuggernautEnemy::StartAttack()
{
	if (IsDead() || IsAttacking() || !GetCurrentTarget())
	{
		return;
	}

	bIsAttacking = true;

	// 돌진이 끝날 때까지 BT가 추격 이동으로 끼어들지 않게 한다.
	PauseAI();

	FaceTarget();
	PlayAttackMontage();
	SpawnEffectAtLocation(WindupEffect, GetActorLocation());

	GetWorldTimerManager().SetTimer(WindupTimerHandle, this, &AJuggernautEnemy::BeginCharge, ChargeWindup, false);
}

void AJuggernautEnemy::BeginCharge()
{
	if (IsDead() || !IsAttacking())
	{
		return;
	}

	// 방향은 돌진이 시작되는 이 순간에 정한다. 이후에는 플레이어를 따라가지 않는다.
	FaceTarget();

	FVector Direction = FVector::ForwardVector;
	if (const AActor* Target = GetCurrentTarget())
	{
		const float DeltaX = Target->GetActorLocation().X - GetActorLocation().X;
		if (!FMath::IsNearlyZero(DeltaX))
		{
			Direction = FVector(FMath::Sign(DeltaX), 0.f, 0.f);
		}
		else if (GetActorForwardVector().X < 0.f)
		{
			Direction = -FVector::ForwardVector;
		}
	}

	ChargeDirection = Direction;
	SetActorRotation(Direction.Rotation());

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = ChargeSpeed;
	Movement->MaxAcceleration = ChargeAcceleration;

	// 돌진하는 동안 다른 폰에 막히지 않게 폰 충돌을 겹침으로 바꾼다.
	if (bPassThroughPawnsWhileCharging && !bPawnResponseOverridden)
	{
		UCapsuleComponent* Capsule = GetCapsuleComponent();
		SavedPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		bPawnResponseOverridden = true;
	}

	ChargeStartX = GetActorLocation().X;
	ChargeElapsed = 0.f;
	HitActors.Reset();
	bCharging = true;
	SetActorTickEnabled(true);
}

void AJuggernautEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bCharging)
	{
		return;
	}

	if (IsDead())
	{
		StopCharging();
		return;
	}

	ChargeElapsed += DeltaTime;

	// bForce: AI가 이동 입력을 막아 두어도 돌진 입력은 적용한다. 공중에서도 입력을 계속 넣는다.
	AddMovementInput(ChargeDirection, 1.f, true);

	if (TryHitPlayers() && bEndChargeOnHit)
	{
		EndCharge(false);
		return;
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement->IsMovingOnGround())
	{
		// 거리는 바닥에서 달린 만큼만 센다. 낭떠러지에서 떨어지는 동안은 계속 나아간다.
		const float Traveled = FMath::Abs(GetActorLocation().X - ChargeStartX);
		if (Traveled >= ChargeMaxDistance)
		{
			EndCharge(false);
			return;
		}

		// 돌진 직후 가속 구간을 지난 뒤에도 속도가 나오지 않으면 벽에 막힌 것이다.
		if (ChargeElapsed > 0.25f && FMath::Abs(Movement->Velocity.X) < ChargeSpeed * WallHitSpeedRatio)
		{
			EndCharge(true);
			return;
		}
	}

	if (ChargeElapsed >= ChargeMaxDuration)
	{
		EndCharge(false);
	}
}

bool AJuggernautEnemy::TryHitPlayers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius() + ChargeHitMargin, Capsule->GetScaledCapsuleHalfHeight());

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JuggernautCharge), false, this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), Shape, QueryParams);

	bool bHitAny = false;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		const APawn* Pawn = Cast<APawn>(Actor);

		// 플레이어에게만, 한 번의 돌진에 한 번만 피해를 준다.
		if (!Pawn || !Pawn->IsPlayerControlled() || HitActors.Contains(Actor))
		{
			continue;
		}

		HitActors.Add(Actor);
		bHitAny = true;

		ApplyEnemyDamage(Actor, GetScaledDamage(GetStatConfig().AttackDamage));

		if (KnockbackHorizontal > 0.f || KnockbackUp > 0.f)
		{
			if (ACharacter* Character = Cast<ACharacter>(Actor))
			{
				Character->LaunchCharacter(
					FVector(ChargeDirection.X * KnockbackHorizontal, 0.f, KnockbackUp), true, true);
			}
		}

		SpawnEffectAtLocation(ImpactEffect, Actor->GetActorLocation());
	}

	return bHitAny;
}

void AJuggernautEnemy::EndCharge(bool bHitWall)
{
	if (!bCharging)
	{
		return;
	}

	StopCharging();

	// 수평 속도를 없애 그 자리에서 멈추게 한다(공중이면 수직으로 떨어진다).
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	FVector Velocity = Movement->Velocity;
	Velocity.X = 0.f;
	Velocity.Y = 0.f;
	Movement->Velocity = Velocity;

	if (bHitWall)
	{
		const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
		SpawnEffectAtLocation(ImpactEffect, GetActorLocation() + ChargeDirection * Radius);
	}

	const float Recovery = bHitWall ? WallStunTime : ChargeRecoveryTime;
	GetWorldTimerManager().SetTimer(RecoveryTimerHandle, this, &AJuggernautEnemy::FinishRecovery, FMath::Max(Recovery, 0.01f), false);
}

void AJuggernautEnemy::StopCharging()
{
	bCharging = false;
	SetActorTickEnabled(false);

	// 폰 충돌 설정을 원래대로 되돌린다.
	if (bPawnResponseOverridden)
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
		bPawnResponseOverridden = false;
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = GetStatConfig().MoveSpeed;
	Movement->MaxAcceleration = DefaultMaxAcceleration;
}

void AJuggernautEnemy::FinishRecovery()
{
	ResumeAI();
	FinishAttack();
}

void AJuggernautEnemy::CancelAttack()
{
	GetWorldTimerManager().ClearTimer(WindupTimerHandle);
	GetWorldTimerManager().ClearTimer(RecoveryTimerHandle);
	StopCharging();
	ResumeAI();

	Super::CancelAttack();
}

void AJuggernautEnemy::PauseAI()
{
	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		AIC->PauseEnemyAI();
	}
}

void AJuggernautEnemy::ResumeAI()
{
	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		AIC->ResumeEnemyAI();
	}
}
