#include "Character/Enemy/CinderEnemy.h"
#include "Character/Enemy/EnemyAttributeSet.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ACinderEnemy::ACinderEnemy()
{
	EnemyType = EEnemyType::Cinder;

	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = false;

	// 기본값(임시). 블루프린트·데이터 에셋에서 조정한다.
	StatConfig.MaxHealth = 120.f;
	StatConfig.AttackDamage = 10.f;           // 원거리 투사체 피해
	StatConfig.SecondaryAttackDamage = 40.f; // 자폭 피해
	StatConfig.MoveSpeed = 180.f;               // 표면을 기어가는 속도
	StatConfig.DetectRange = 1800.f;
	StatConfig.AttackRange = 1000.f;
	StatConfig.ChaseAcceptableRadius = 700.f;   // 이 거리 안에서는 기어가다 멈춘다.
	StatConfig.AttackCooldown = 2.5f;

	ProjectileConfig.Speed = 900.f;
	ProjectileConfig.Lifetime = 5.f;
}

void ACinderEnemy::ApplyEnemyData(const UEnemyDataAsset* Data)
{
	Super::ApplyEnemyData(Data);
	ProjectileConfig.ProjectileClass = ProjectileClass;

	if (!Data)
	{
		return;
	}

	const FCinderSettings& S = Data->Cinder;
	SurfaceOffset = S.SurfaceOffset;
	SurfaceSnapDistance = S.SurfaceSnapDistance;
	CornerCheckDistance = S.CornerCheckDistance;
	SpawnSnapDistance = S.SpawnSnapDistance;
	bUseWorldDynamicSurfaces = S.bUseWorldDynamicSurfaces;
	RotationInterpSpeed = S.RotationInterpSpeed;
	PositionInterpSpeed = S.PositionInterpSpeed;
	PatrolMinDistance = S.PatrolMinDistance;
	PatrolMaxDistance = S.PatrolMaxDistance;
	PatrolWaitMin = S.PatrolWaitMin;
	PatrolWaitMax = S.PatrolWaitMax;
	FireDelay = S.FireDelay;
	MuzzleOffset = S.MuzzleOffset;
	FallHealthRatio = S.FallHealthRatio;
	FallenMoveSpeed = S.FallenMoveSpeed;
	ExplosionTriggerRange = S.ExplosionTriggerRange;
	ExplosionFuseTime = S.ExplosionFuseTime;
	ExplosionRadius = S.ExplosionRadius;
	ExplosionEffectScale = S.ExplosionEffectScale;
	FuseEffectScale = S.FuseEffectScale;
	PatrolAxis = S.PatrolAxis;
	ProjectileConfig.Speed = S.ProjectileSpeed;
	ProjectileConfig.Lifetime = S.ProjectileLifetime;
}

FVector ACinderEnemy::GetVelocity() const
{
	if (Mode == ECinderMode::Crawling && CrawlSpeed > KINDA_SMALL_NUMBER)
	{
		FVector Direction = GetTangent(MoveSign);

		// 벽을 타는 중에는 이동 방향이 수직(위·아래)이라, 수평 속도(Size2D)로 이동을 판단하는 ABP는 정지로 읽는다.
		// 속도 크기는 그대로 두고 방향만 수평으로 바꿔 준다. 이동 애니메이션 선택에만 쓰이는 값이다.
		if (FMath::Abs(Direction.X) < 0.3f)
		{
			Direction = FVector::ForwardVector;
		}

		return Direction * CrawlSpeed;
	}

	return Super::GetVelocity();
}

void ACinderEnemy::BeginPlay()
{
	Super::BeginPlay();

	// 낙하할 때 되돌릴 중력 값을 기억해 둔다(BP에서 바꿨을 수 있다).
	FallGravityScale = GetCharacterMovement()->GravityScale > 0.f ? GetCharacterMovement()->GravityScale : 1.f;
	DefaultPawnResponse = GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn);

	// 자폭 피해 유형 태그가 비어 있으면 프로젝트에 등록된 Damage.Type.Explosion을 찾아 쓴다(없으면 비워 둔다).
	if (!ExplosionDamageTag.IsValid())
	{
		ExplosionDamageTag = FGameplayTag::RequestGameplayTag(TEXT("Damage.Type.Explosion"), false);
	}

	OnEnemyDamaged.AddUniqueDynamic(this, &ACinderEnemy::HandleHealthChanged);

	ResetCinderState();
}

void ACinderEnemy::ResetEnemy()
{
	Super::ResetEnemy();

	// 풀에서 재사용할 때 표면 이동 상태로 처음부터 시작한다.
	ResetCinderState();
}

void ACinderEnemy::ResetCinderState()
{
	Mode = ECinderMode::Crawling;
	bFuseStarted = false;
	bExploded = false;
	CrawlSpeed = 0.f;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, DefaultPawnResponse); // 자폭 준비에서 바꾼 충돌 복구
	WaitRemaining = 0.f;
	PatrolRemaining = FMath::FRandRange(PatrolMinDistance, FMath::Max(PatrolMaxDistance, PatrolMinDistance));

	if (GetMesh())
	{
		GetMesh()->SetVisibility(true, true); // 자폭으로 숨긴 메시를 되돌린다.
	}

	// 표면 이동은 직접 하므로 이동 모드와 중력을 끈다.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->GravityScale = 0.f;
	Movement->SetMovementMode(MOVE_None);

	SnapToSurface();
}

// ---------------------------------------------------------------------------
// 표면 찾기·이동
// ---------------------------------------------------------------------------

FVector ACinderEnemy::ProjectNormal(const FVector& Normal)
{
	const FVector Flat(Normal.X, 0.f, Normal.Z);
	return Flat.IsNearlyZero() ? FVector::UpVector : Flat.GetSafeNormal();
}

bool ACinderEnemy::TraceSurface(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	if (bUseWorldDynamicSurfaces)
	{
		ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CinderSurface), false, this);
	return World->LineTraceSingleByObjectType(OutHit, Start, End, ObjectParams, QueryParams);
}

FVector ACinderEnemy::GetTangent(float Sign) const
{
	// 법선을 평면(XZ) 안에서 90도 돌린 방향. 바닥(법선 +Z)에서 Sign이 +1이면 +X로 간다.
	return FVector(SurfaceNormal.Z, 0.f, -SurfaceNormal.X) * Sign;
}

void ACinderEnemy::SnapToSurface()
{
	const FVector Origin = GetActorLocation();
	LaneY = Origin.Y;

	// 위·아래·좌·우와 대각선 8방향으로 선을 쏴서 가장 가까운 표면을 찾는다.
	static const FVector Directions[8] = {
		FVector(1.f, 0.f, 0.f), FVector(-1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f), FVector(0.f, 0.f, -1.f),
		FVector(0.7071f, 0.f, 0.7071f), FVector(-0.7071f, 0.f, 0.7071f),
		FVector(0.7071f, 0.f, -0.7071f), FVector(-0.7071f, 0.f, -0.7071f)
	};

	float BestDistance = MAX_FLT;
	FHitResult BestHit;
	bool bFound = false;

	for (const FVector& Direction : Directions)
	{
		FHitResult Hit;
		if (TraceSurface(Origin, Origin + Direction * SpawnSnapDistance, Hit) && Hit.Distance < BestDistance)
		{
			BestDistance = Hit.Distance;
			BestHit = Hit;
			bFound = true;
		}
	}

	if (bFound)
	{
		SurfaceNormal = ProjectNormal(BestHit.ImpactNormal);
		CrawlPosition = BestHit.ImpactPoint + SurfaceNormal * SurfaceOffset;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cinder] %s: %.0f 안에서 붙을 표면을 찾지 못했습니다. 스폰 위치를 표면 가까이로 옮기세요."),
			*GetName(), SpawnSnapDistance);
		SurfaceNormal = FVector::UpVector;
		CrawlPosition = Origin;
	}

	CrawlPosition.Y = LaneY;
	MoveSign = FMath::RandBool() ? 1.f : -1.f;

	// 처음에는 보간 없이 바로 붙인다.
	const FQuat DesiredRotation = FRotationMatrix::MakeFromXZ(GetTangent(MoveSign), FVector(SurfaceNormal)).ToQuat();
	SetActorLocationAndRotation(CrawlPosition, DesiredRotation);

	// 정찰 기준점도 표면에 붙은 위치로 옮긴다.
	SetHomeLocation(CrawlPosition);
}

float ACinderEnemy::CrawlStep(float DeltaTime, float Speed)
{
	const FVector OldPosition = CrawlPosition;
	const FVector Tangent = GetTangent(MoveSign);
	const float StepLength = Speed * DeltaTime;

	FVector NewPosition = OldPosition;
	FVector NewNormal = SurfaceNormal;
	FHitResult Hit;

	if (TraceSurface(OldPosition, OldPosition + Tangent * (StepLength + SurfaceOffset + CornerCheckDistance), Hit))
	{
		// 앞이 막혀 있다(오목한 모서리). 막고 있는 면으로 올라탄다.
		NewNormal = ProjectNormal(Hit.ImpactNormal);
		NewPosition = Hit.ImpactPoint + NewNormal * SurfaceOffset;
	}
	else
	{
		NewPosition = OldPosition + Tangent * StepLength;

		const float TraceLength = SurfaceOffset + SurfaceSnapDistance;
		if (TraceSurface(NewPosition, NewPosition - SurfaceNormal * TraceLength, Hit))
		{
			// 같은 면(또는 완만한 경사)을 따라간다.
			NewNormal = ProjectNormal(Hit.ImpactNormal);
			NewPosition = Hit.ImpactPoint + NewNormal * SurfaceOffset;
		}
		else
		{
			// 면이 끝났다(볼록한 모서리). 모서리 너머로 돌아 들어가 옆면을 찾는다.
			const FVector WrapStart = NewPosition - SurfaceNormal * TraceLength;
			if (TraceSurface(WrapStart, WrapStart - Tangent * (StepLength + TraceLength), Hit))
			{
				NewNormal = ProjectNormal(Hit.ImpactNormal);
				NewPosition = Hit.ImpactPoint + NewNormal * SurfaceOffset;
			}
			else
			{
				// 갈 곳이 없다(얇은 끝 등). 반대 방향으로 돌아선다.
				MoveSign = -MoveSign;
				return 0.f;
			}
		}
	}

	NewPosition.Y = LaneY;
	CrawlPosition = NewPosition;
	SurfaceNormal = NewNormal;

	return FVector::Dist(OldPosition, NewPosition);
}

void ACinderEnemy::ApplyCrawlTransform(float DeltaTime)
{
	const FQuat DesiredRotation = FRotationMatrix::MakeFromXZ(GetTangent(MoveSign), FVector(SurfaceNormal)).ToQuat();

	FQuat NewRotation = DesiredRotation;
	if (RotationInterpSpeed > 0.f)
	{
		NewRotation = FMath::QInterpTo(GetActorQuat(), DesiredRotation, DeltaTime, RotationInterpSpeed);
	}

	FVector NewLocation = CrawlPosition;
	if (PositionInterpSpeed > 0.f)
	{
		NewLocation = FMath::VInterpTo(GetActorLocation(), CrawlPosition, DeltaTime, PositionInterpSpeed);
	}
	NewLocation.Y = LaneY;

	SetActorLocationAndRotation(NewLocation, NewRotation);
}

void ACinderEnemy::FaceMoveDirectionToward(const FVector& WorldPoint)
{
	const FVector ToPoint = WorldPoint - CrawlPosition;
	const float Along = FVector::DotProduct(GetTangent(1.f), ToPoint);

	// 목표가 거의 옆(수직)에 있을 때 방향이 계속 바뀌지 않도록 여유를 둔다.
	if (FMath::Abs(Along) > 30.f)
	{
		MoveSign = Along > 0.f ? 1.f : -1.f;
	}
}

void ACinderEnemy::BeginPatrolWait()
{
	WaitRemaining = FMath::FRandRange(PatrolWaitMin, FMath::Max(PatrolWaitMax, PatrolWaitMin));
	PatrolRemaining = FMath::FRandRange(PatrolMinDistance, FMath::Max(PatrolMaxDistance, PatrolMinDistance));

	// 홈에서 너무 멀어졌으면 홈 쪽으로, 아니면 무작위 방향으로 간다.
	const FVector ToHome = GetHomeLocation() - CrawlPosition;
	if (ToHome.Size() > GetPatrolRadius())
	{
		FaceMoveDirectionToward(GetHomeLocation());
	}
	else
	{
		MoveSign = FMath::RandBool() ? 1.f : -1.f;
	}
}

void ACinderEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsDead())
	{
		return;
	}

	switch (Mode)
	{
	case ECinderMode::Crawling:
		TickCrawl(DeltaTime);
		break;
	case ECinderMode::Fallen:
		TickFallen(DeltaTime);
		break;
	default:
		break;
	}
}

void ACinderEnemy::TickCrawl(float DeltaTime)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	float Moved = 0.f;

	if (IsAttacking())
	{
		// 사격 중에는 멈춰 있고, 몸은 표면에 맞춰 둔다.
	}
	else if (const AActor* Target = GetCurrentTarget())
	{
		// 추격: 플레이어 쪽으로 표면을 따라 이동하다가 추격 허용 거리 안에서 멈춘다.
		FaceMoveDirectionToward(Target->GetActorLocation());

		if (GetDistanceToTarget() > GetStatConfig().ChaseAcceptableRadius)
		{
			Moved = CrawlStep(DeltaTime, GetStatConfig().MoveSpeed);
		}
	}
	else if (WaitRemaining > 0.f)
	{
		WaitRemaining -= DeltaTime;
	}
	else
	{
		// 정찰: 정해진 거리만큼 기어간 뒤 잠깐 멈추고 방향을 바꾼다.
		Moved = CrawlStep(DeltaTime, GetStatConfig().MoveSpeed);
		PatrolRemaining -= Moved;
		if (PatrolRemaining <= 0.f)
		{
			BeginPatrolWait();
		}
	}

	// 직접 움직이므로 이동 컴포넌트가 속도를 모른다. 속도 기반 ABP가 이동 애니메이션을 재생하도록 값을 넣어 준다.
	CrawlSpeed = DeltaTime > 0.f ? Moved / DeltaTime : 0.f;
	Movement->Velocity = CrawlSpeed > KINDA_SMALL_NUMBER ? GetVelocity() : FVector::ZeroVector;

	ApplyCrawlTransform(DeltaTime);
}

// ---------------------------------------------------------------------------
// 원거리 공격
// ---------------------------------------------------------------------------

bool ACinderEnemy::CanAttackTarget() const
{
	// 표면에 붙어 있을 때만 원거리 공격을 한다. 낙하 후에는 자폭만 한다.
	if (Mode != ECinderMode::Crawling)
	{
		return false;
	}

	return Super::CanAttackTarget();
}

void ACinderEnemy::StartAttack()
{
	if (IsDead() || IsAttacking() || !GetCurrentTarget() || Mode != ECinderMode::Crawling)
	{
		return;
	}

	bIsAttacking = true;
	PlayAttackMontage();

	GetWorldTimerManager().SetTimer(FireTimerHandle, this, &ACinderEnemy::FireProjectile, FireDelay, false);
}

void ACinderEnemy::FireProjectile()
{
	if (!IsDead() && GetCurrentTarget() && Mode == ECinderMode::Crawling)
	{
		const FVector Muzzle = GetMuzzleLocation(MuzzleSocketName, MuzzleOffset);

		FRotator FlashRotation = GetActorRotation();
		if (const AActor* Target = GetCurrentTarget())
		{
			FlashRotation = (Target->GetActorLocation() - Muzzle).Rotation();
		}

		SpawnEnemyProjectile(ProjectileConfig, Muzzle);
		SpawnEffectAtSocket(MuzzleFlashEffect, MuzzleSocketName, MuzzleOffset, FlashRotation);
	}

	if (IsAttacking())
	{
		FinishAttack();
	}
}

void ACinderEnemy::CancelAttack()
{
	GetWorldTimerManager().ClearTimer(FireTimerHandle);
	Super::CancelAttack();
}

// ---------------------------------------------------------------------------
// 낙하
// ---------------------------------------------------------------------------

void ACinderEnemy::HandleHealthChanged(float Damage)
{
	if (Mode != ECinderMode::Crawling || IsDead() || !AttributeSet)
	{
		return;
	}

	if (AttributeSet->GetHealth() > 0.f && AttributeSet->GetHealthPercent() <= FallHealthRatio)
	{
		StartFall();
	}
}

void ACinderEnemy::StartFall()
{
	Mode = ECinderMode::Falling;
	CancelAttack();
	EnterPhysicsMovement(MOVE_Falling);
}

void ACinderEnemy::EnterPhysicsMovement(EMovementMode NewMode)
{
	// 캡슐을 세운 뒤 물리 이동(낙하)으로 전환한다. 플레이어 쪽을 바라보게 한다.
	float FacingX = GetActorForwardVector().X;
	if (const AActor* Target = GetCurrentTarget())
	{
		FacingX = Target->GetActorLocation().X - GetActorLocation().X;
	}
	SetActorRotation(FRotator(0.f, FacingX >= 0.f ? 0.f : 180.f, 0.f));

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->GravityScale = FallGravityScale;
	Movement->Velocity = FVector::ZeroVector;
	Movement->SetMovementMode(NewMode);
}

void ACinderEnemy::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (Mode != ECinderMode::Falling || IsDead())
	{
		return;
	}

	// 착지했다. 빠르게 달려가 자폭한다.
	Mode = ECinderMode::Fallen;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Walking);
	Movement->MaxWalkSpeed = FallenMoveSpeed;
}

float ACinderEnemy::GetEdgeDistanceToTarget() const
{
	const AActor* Target = GetCurrentTarget();
	if (!Target)
	{
		return MAX_FLT;
	}

	float TargetRadius = 0.f;
	if (const ACharacter* TargetCharacter = Cast<ACharacter>(Target))
	{
		TargetRadius = TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius();
	}

	const float SelfRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	return FMath::Max(GetDistanceToTarget() - SelfRadius - TargetRadius, 0.f);
}

void ACinderEnemy::TickFallen(float DeltaTime)
{
	const AActor* Target = GetCurrentTarget();
	if (!Target || bFuseStarted)
	{
		return;
	}

	FaceTarget();

	// 길 찾기 없이 플레이어 쪽(좌우)으로 곧장 달려간다.
	const float DeltaX = Target->GetActorLocation().X - GetActorLocation().X;
	AddMovementInput(FVector(FMath::Sign(DeltaX), 0.f, 0.f), 1.f);

	if (GetEdgeDistanceToTarget() <= ExplosionTriggerRange)
	{
		StartFuse();
	}
}

// ---------------------------------------------------------------------------
// 자폭
// ---------------------------------------------------------------------------

void ACinderEnemy::StartFuse()
{
	if (bFuseStarted)
	{
		return;
	}

	bFuseStarted = true;

	// 폭발 전에 제자리에 고정한다. 이동을 막고, 플레이어와 겹쳐도 서로 밀지 않게 폰 충돌을 겹침으로 바꾼다.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->DisableMovement();
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	if (FuseMontage)
	{
		PlayAnimMontage(FuseMontage);
	}
	SpawnEffectAtLocation(FuseEffect, GetActorLocation(), FRotator::ZeroRotator, FVector(FuseEffectScale));

	GetWorldTimerManager().SetTimer(FuseTimerHandle, this, &ACinderEnemy::Explode, FMath::Max(ExplosionFuseTime, 0.01f), false);
}

void ACinderEnemy::Explode()
{
	if (IsDead())
	{
		return;
	}

	// 자폭: 체력을 0으로 만들고 사망 처리한다. 폭발은 사망 처리에서 일어난다(처치당했을 때와 같은 경로).
	if (AttributeSet)
	{
		AttributeSet->SetHealth(0.f);
	}
	HandleDeath();
}

void ACinderEnemy::Detonate()
{
	UWorld* World = GetWorld();
	if (!World || bExploded)
	{
		return;
	}

	bExploded = true;

	// 범위 안의 플레이어에게 피해를 준다. 한 대상에게 한 번만 적용한다.
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CinderExplosion), false, this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(ExplosionRadius), QueryParams);

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
		ApplyEnemyDamage(Actor, GetScaledDamage(GetStatConfig().SecondaryAttackDamage), ExplosionDamageTag);
	}

	SpawnEffectAtLocation(ExplosionEffect, GetActorLocation(), FRotator::ZeroRotator, FVector(ExplosionEffectScale));

	// 몸을 숨긴다.
	if (GetMesh())
	{
		GetMesh()->SetVisibility(false, true);
	}
}

void ACinderEnemy::HandleDeath()
{
	if (IsDead())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(FuseTimerHandle);

	// 어떤 이유로 죽든(처치당함·자폭) 그 자리에서 바로 폭발한다. 공중에서 죽었다면 폭발한 뒤 그대로 떨어진다.
	Detonate();

	Super::HandleDeath();
}
