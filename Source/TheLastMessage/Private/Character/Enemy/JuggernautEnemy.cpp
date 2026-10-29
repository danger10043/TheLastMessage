#include "Character/Enemy/JuggernautEnemy.h"
#include "Character/Enemy/EnemyAIController.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Interface/AttackReceiverInterface.h"

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

	// 돌진 예고 띠: 월드 좌표에 놓는 평면. 몬스터가 움직이거나 회전해도 따라가지 않는다.
	TelegraphMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TelegraphMesh"));
	TelegraphMesh->SetupAttachment(GetRootComponent());
	TelegraphMesh->SetUsingAbsoluteLocation(true);
	TelegraphMesh->SetUsingAbsoluteRotation(true);
	TelegraphMesh->SetUsingAbsoluteScale(true);
	TelegraphMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TelegraphMesh->SetGenerateOverlapEvents(false);
	TelegraphMesh->SetCanEverAffectNavigation(false);
	TelegraphMesh->SetCastShadow(false);
	TelegraphMesh->bReceivesDecals = false;
	TelegraphMesh->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		TelegraphMesh->SetStaticMesh(PlaneMesh.Object);
	}
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
	TelegraphHeightPadding = S.TelegraphHeightPadding;
	TelegraphBlinkDuration = S.TelegraphBlinkDuration;
	TelegraphBlinkInterval = S.TelegraphBlinkInterval;
}

void AJuggernautEnemy::BeginPlay()
{
	Super::BeginPlay();

	DefaultMaxAcceleration = GetCharacterMovement()->MaxAcceleration;

	if (TelegraphMesh && TelegraphMaterial)
	{
		TelegraphMesh->SetMaterial(0, TelegraphMaterial);
	}
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

	// 돌진 방향은 윈드업이 시작될 때 정한다. 예고 띠가 흔들리지 않고, 플레이어는 띠를 보고 피할 수 있다.
	FaceTarget();
	ChargeDirection = ComputeChargeDirection();
	SetActorRotation(ChargeDirection.Rotation());

	PlayAttackMontage();
	SpawnEffectAtLocation(WindupEffect, GetActorLocation());

	if (ChargeSound)
	{
		ChargeAudio = UGameplayStatics::SpawnSoundAtLocation(this, ChargeSound, GetActorLocation());
	}

	ShowTelegraph();

	GetWorldTimerManager().SetTimer(WindupTimerHandle, this, &AJuggernautEnemy::BeginCharge, ChargeWindup, false);
}

void AJuggernautEnemy::BeginCharge()
{
	if (IsDead() || !IsAttacking())
	{
		return;
	}

	// 방향은 윈드업이 시작될 때 정해 둔 값(ChargeDirection)을 쓴다. 플레이어를 따라가지 않는다.
	HideTelegraph();
	StopChargeSound(); // 돌진이 시작되면 차지 소리를 끊는다.
	SetActorRotation(ChargeDirection.Rotation());

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

FVector AJuggernautEnemy::ComputeChargeDirection() const
{
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
	return Direction;
}

void AJuggernautEnemy::ShowTelegraph()
{
	UWorld* World = GetWorld();
	if (!TelegraphMesh || !TelegraphMaterial || !World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Juggernaut] %s: 돌진 예고 띠를 표시하지 못했습니다. TelegraphMesh=%s, TelegraphMaterial=%s"),
			*GetName(), TelegraphMesh ? TEXT("있음") : TEXT("없음"), TelegraphMaterial ? TEXT("있음") : TEXT("없음(BP의 Telegraph Material 미지정)"));
		return;
	}

	if (!TelegraphMesh->GetStaticMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Juggernaut] %s: 예고 띠 메시(엔진 기본 Plane)를 불러오지 못했습니다."), *GetName());
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Origin = GetActorLocation();

	// 돌진 경로 앞에 막는 벽이 있으면 벽까지만 보여 준다. 실제 돌진도 벽에서 끝난다.
	float Distance = ChargeMaxDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JuggernautTelegraph), false, this);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	// 바닥에 걸리지 않게 캡슐을 약간 줄여서 훑는다.
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(FMath::Max(Radius - 2.f, 1.f), FMath::Max(HalfHeight - 10.f, 1.f));

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, Origin, Origin + ChargeDirection * ChargeMaxDistance, FQuat::Identity, ObjectParams, Shape, QueryParams);
	for (const FHitResult& Hit : Hits)
	{
		// 시작부터 겹쳐 있던 물체(바닥에 닿은 캡슐 등)는 경로를 막는 벽이 아니다. 거리가 0이 되어 띠가 사라지는 것을 막는다.
		if (Hit.bStartPenetrating)
		{
			continue;
		}

		// 트리거처럼 몸을 막지 않는 물체는 벽으로 보지 않는다.
		const UPrimitiveComponent* HitComponent = Hit.GetComponent();
		if (HitComponent && HitComponent->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
		{
			Distance = FMath::Min(Distance, Hit.Distance);
			break;
		}
	}

	// 띠는 몸 앞쪽 가장자리에서 시작해 벽 표면(또는 최대 돌진 거리 끝)까지 이어진다.
	TelegraphStart = FVector(Origin.X + ChargeDirection.X * Radius, Origin.Y, Origin.Z);
	TelegraphLength = FMath::Max(Distance, 1.f);
	TelegraphHeight = HalfHeight * 2.f + TelegraphHeightPadding;

	UE_LOG(LogTemp, Log, TEXT("[Juggernaut] %s: 돌진 예고 길이=%.0f 높이=%.0f 방향X=%.0f 윈드업=%.2f"),
		*GetName(), TelegraphLength, TelegraphHeight, ChargeDirection.X, ChargeWindup);
	TelegraphElapsed = 0.f;
	bTelegraphActive = true;

	// 평면 메시를 카메라 쪽(Y 축)을 향해 세운다. 메시의 길이 방향이 월드 X, 높이 방향이 월드 Z가 된다.
	TelegraphMesh->SetWorldRotation(FRotator(0.f, 0.f, 90.f));
	TelegraphMesh->SetWorldLocation(TelegraphStart);
	TelegraphMesh->SetWorldScale3D(FVector(0.01f, TelegraphHeight / 100.f, 1.f));
	TelegraphMesh->SetVisibility(true);

	// 윈드업 동안 띠를 늘리기 위해 틱을 켠다.
	SetActorTickEnabled(true);
}

void AJuggernautEnemy::UpdateTelegraph(float DeltaTime)
{
	if (!TelegraphMesh)
	{
		return;
	}

	TelegraphElapsed += DeltaTime;

	// 예고 시간은 윈드업 시간을 따른다. 앞부분은 띠를 늘리고, 마지막 Blink 구간은 끝까지 늘어난 채로 깜빡인다.
	const float Windup = FMath::Max(ChargeWindup, 0.01f);
	const float Blink = FMath::Clamp(TelegraphBlinkDuration, 0.f, Windup * 0.5f);
	const float GrowTime = FMath::Max(Windup - Blink, 0.01f);

	const float Alpha = FMath::Clamp(TelegraphElapsed / GrowTime, 0.f, 1.f);
	const float Eased = 1.f - FMath::Square(1.f - Alpha); // 처음에 빠르게 뻗고 끝에서 느려진다.
	const float Length = FMath::Max(TelegraphLength * Eased, 1.f);

	TelegraphMesh->SetWorldLocation(TelegraphStart + ChargeDirection * (Length * 0.5f));
	TelegraphMesh->SetWorldScale3D(FVector(Length / 100.f, TelegraphHeight / 100.f, 1.f));

	bool bVisible = true;
	const float BlinkStart = Windup - Blink;
	if (Blink > 0.f && TelegraphElapsed >= BlinkStart)
	{
		const int32 Phase = FMath::FloorToInt((TelegraphElapsed - BlinkStart) / FMath::Max(TelegraphBlinkInterval, 0.01f));
		bVisible = (Phase % 2) == 0;
	}
	TelegraphMesh->SetVisibility(bVisible);
}

void AJuggernautEnemy::StopChargeSound()
{
	if (ChargeAudio)
	{
		ChargeAudio->Stop();
		ChargeAudio = nullptr;
	}
}

void AJuggernautEnemy::HideTelegraph()
{
	bTelegraphActive = false;
	if (TelegraphMesh)
	{
		TelegraphMesh->SetVisibility(false);
	}
}

void AJuggernautEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bTelegraphActive)
	{
		UpdateTelegraph(DeltaTime);
	}

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

	const bool bHitTarget = TryHitPlayers();

	// 공격 전달 중 폭탄의 폭발로 사망하거나 돌진이 취소될 수 있음
	if (!IsValid(this) || IsActorBeingDestroyed() || IsDead() || !bCharging)
	{
		return;
	}

	if (bHitTarget && bEndChargeOnHit)
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
	if (!World || IsDead() || !bCharging)
	{
		return false;
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();

	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius() + ChargeHitMargin,
		Capsule->GetScaledCapsuleHalfHeight());

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(JuggernautCharge),
		false,
		this);

	// 플레이어와 일반 기믹의 충돌 타입을 함께 검색
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	TArray<FOverlapResult> Overlaps;

	World->OverlapMultiByObjectType(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		ObjectParams,
		Shape,
		QueryParams);

	bool bHitAny = false;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();

		if (!IsValid(Actor))
		{
			continue;
		}

		const TWeakObjectPtr<AActor> ActorKey(Actor);

		if (HitActors.Contains(ActorKey))
		{
			continue;
		}

		const APawn* Pawn = Cast<APawn>(Actor);

		const bool bIsPlayer = Pawn && Pawn->IsPlayerControlled();

		const bool bIsAttackReceiver = Actor->Implements<UAttackReceiverInterface>();

		// 플레이어 또는 공격을 받는 기믹만 허용
		if (!bIsPlayer && !bIsAttackReceiver)
		{
			continue;
		}

		// 공격 전달 중 폭탄이 파괴될 수 있으므로 위치를 먼저 기록
		const FVector HitLocation = Actor->GetActorLocation();

		// 한 번의 돌진에서 같은 대상은 한 번만 처리
		HitActors.Add(ActorKey);
		bHitAny = true;

		ApplyEnemyDamage(Actor, GetScaledDamage(GetStatConfig().AttackDamage));

		// 폭탄의 폭발로 사망하거나 돌진이 취소되면 종료
		if (!IsValid(this) || IsActorBeingDestroyed() || IsDead() || !bCharging)
		{
			return bHitAny;
		}

		// 기존 플레이어 넉백 유지
		if (bIsPlayer && IsValid(Actor) && (KnockbackHorizontal > 0.f || KnockbackUp > 0.f))
		{
			if (ACharacter* Character = Cast<ACharacter>(Actor))
			{
				Character->LaunchCharacter(
					FVector(ChargeDirection.X * KnockbackHorizontal,
						0.f,
						KnockbackUp),
					true,
					true);
			}
		}

		// 대상이 파괴되어도 미리 기록한 위치로 효과 재생
		SpawnEffectAtLocation(ImpactEffect, HitLocation);

		if (ImpactSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, HitLocation);
		}
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
		if (ImpactSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation() + ChargeDirection * Radius);
		}
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
	HideTelegraph();
	StopChargeSound();
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
