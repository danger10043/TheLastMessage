#include "Core/CoreSupportRobot.h"

#include "Camera/PlayerCameraManager.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossTypes.h"
#include "Character/Enemy/EnemyBase.h"
#include "Character/Player/PlayerBase.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

ACoreSupportRobot::ACoreSupportRobot()
{
	PrimaryActorTick.bCanEverTick = true;
	// 플레이어가 이번 프레임 이동을 마친 뒤 따라간다.
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	// 로봇은 다른 판정에 걸리지 않고, 지형 확인은 직접 Sweep으로 한다.
	SetActorEnableCollision(false);
	SetCanBeDamaged(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool ACoreSupportRobot::Initialize(APlayerBase* InPlayer)
{
	if (!IsValid(InPlayer) || InPlayer->IsDead()) return false;
	Player = InPlayer;
	InPlayer->OnDestroyed.AddUniqueDynamic(this, &ACoreSupportRobot::HandlePlayerDestroyed);
	AddTickPrerequisiteActor(InPlayer);
	return true;
}

void ACoreSupportRobot::HandlePlayerDestroyed(AActor* DestroyedActor)
{
	Destroy();
}

void ACoreSupportRobot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Player.IsValid() || DeltaSeconds <= 0.0f) return;

	// ---- 전투 판단 ----
	// 전투가 막 시작되면 바로 판단하고, 끝나면 대상·경로를 비워 다음 전투에 이전 대상이 남지 않게 한다.
	const bool bInCombat = IsInCombat();
	if (bInCombat != bWasInCombat)
	{
		bWasInCombat = bInCombat;
		if (bInCombat)
		{
			bDecisionRequested = true;
		}
		else
		{
			PlayerHitTarget.Reset();
			AttackerTarget.Reset();
			Target.Reset();
			ClearMoveGoal();
		}
	}
	if (bInCombat)
	{
		// 노리던 적이 죽거나 사라지면 1초를 기다리지 않고 다음 대상을 찾는다.
		// IsExplicitlyNull은 "처음부터 대상이 없음"과 "있었는데 사라짐"을 구분한다.
		if (!Target.IsExplicitlyNull() && !IsAliveEnemy(Target.Get())) bDecisionRequested = true;
		DecisionElapsed += DeltaSeconds;
		if (bDecisionRequested || DecisionElapsed >= DecisionInterval) RunDecision();
	}

	// ---- 이동 ----
	FBox2D ScreenBounds;
	const FBox2D* Bounds = GetScreenBounds(ScreenBounds) ? &ScreenBounds : nullptr;
	UpdateOffscreen(DeltaSeconds, Bounds);

	// 원하는 속도: 목표까지 멀면 최대 속도, 마지막 목표에 가까워지면 거리에 비례해 줄인다.
	// 중간 경유지에서는 줄이지 않아야 경유지마다 멈칫하지 않고 이어서 지나간다.
	const FVector ToGoal = GetCurrentGoal(Bounds) - GetActorLocation();
	const float GoalDistance = ToGoal.Size();
	const bool bFinalGoal = !(HasPath() && IsInCombat()) || PathIndex == Path.Num() - 1;
	const float SpeedScale = bFinalGoal ? FMath::Min(1.0f, GoalDistance / SlowdownRadius) : 1.0f;
	const FVector Desired = GoalDistance > ArriveDistance ? ToGoal / GoalDistance * MaxSpeed * SpeedScale : FVector::ZeroVector;
	// 현재 속도를 원하는 속도 쪽으로 조금씩 옮겨 출발·정지·방향 전환에 관성이 생기게 한다.
	Velocity = FMath::VInterpTo(Velocity, Desired, DeltaSeconds, SteeringRate);

	const FVector Moved = MoveWithSweep(Velocity * DeltaSeconds);
	// 벽에 막힌 만큼 속도도 줄여야 다음 프레임에 벽을 계속 밀지 않는다.
	Velocity = Moved / DeltaSeconds;
	UpdateStuck(DeltaSeconds, Desired.Size() * DeltaSeconds, Moved.Size());
	AdvancePath();

	// 횡스크롤이라 좌우만 본다. 전투 중엔 노리는 적 쪽, 아니면 이동 방향.
	const AActor* Facing = IsInCombat() ? Target.Get() : nullptr;
	const float FaceX = Facing ? Facing->GetActorLocation().X - GetActorLocation().X : Velocity.X;
	if (FMath::Abs(FaceX) > 1.0f) SetActorRotation(FRotator(0.0f, FaceX > 0.0f ? 0.0f : 180.0f, 0.0f));
}

// =====================================================================
// 이동
// =====================================================================

FVector ACoreSupportRobot::GetFollowPoint() const
{
	const APlayerBase* PlayerActor = Player.Get();
	if (!PlayerActor) return GetActorLocation();

	// 플레이어가 바라보는 반대쪽(등 뒤), 중력 반대쪽(머리 위)에 붙는다.
	// 천장을 걸을 때는 머리 위가 아래쪽이라 빈 공간 쪽에 붙게 된다(카메라 코드와 같은 기준).
	const float FacingSign = PlayerActor->GetActorForwardVector().X >= 0.0f ? 1.0f : -1.0f;
	const UCharacterMovementComponent* Movement = PlayerActor->GetCharacterMovement();
	const float UpSign = Movement && Movement->GetGravityDirection().Z > 0.0f ? -1.0f : 1.0f;
	return PlayerActor->GetActorLocation() + FVector(-FacingSign * FollowOffset.X, 0.0f, UpSign * FollowOffset.Y);
}

FVector ACoreSupportRobot::GetCurrentGoal(const FBox2D* ScreenBounds) const
{
	FVector Goal = HasPath() && IsInCombat() ? Path[PathIndex] : GetFollowPoint();
	if (const APlayerBase* PlayerActor = Player.Get()) Goal.Y = PlayerActor->GetActorLocation().Y;
	if (!ScreenBounds) return Goal;

	// 위치가 아니라 목표점을 화면 안으로 자른다. 위치를 강제로 옮기면 벽 속으로 밀려 들어갈 수 있다.
	// 여백을 뺀 영역이 없을 만큼 화면이 작으면 가운데로 모은다.
	const FVector2D Margin(ScreenMargin, ScreenMargin);
	const FVector2D Min = ScreenBounds->Min + Margin;
	const FVector2D Max = ScreenBounds->Max - Margin;
	const FVector2D Center = ScreenBounds->GetCenter();
	Goal.X = Min.X <= Max.X ? FMath::Clamp(Goal.X, Min.X, Max.X) : Center.X;
	Goal.Z = Min.Y <= Max.Y ? FMath::Clamp(Goal.Z, Min.Y, Max.Y) : Center.Y;
	return Goal;
}

FVector ACoreSupportRobot::MoveWithSweep(const FVector& Delta)
{
	const FVector Start = GetActorLocation();
	if (Delta.IsNearlyZero()) return FVector::ZeroVector;

	FHitResult Hit;
	if (!SweepTerrain(Start, Start + Delta, CollisionRadius, Hit))
	{
		SetActorLocation(Start + Delta);
		return Delta;
	}
	if (Hit.bStartPenetrating)
	{
		// 움직이는 지형이 로봇을 덮친 경우처럼 이미 겹쳐 있으면 밖으로 밀어낸다.
		const FVector Out = Hit.Normal * (Hit.PenetrationDepth + 0.1f);
		SetActorLocation(Start + Out);
		return Out;
	}

	// 막힌 경우: 닿기 직전에서 멈추고, 남은 이동 중 벽면과 나란한 성분만큼 한 번 미끄러진다.
	// Hit.Time은 이동 경로의 몇 %에서 닿았는지(0~1)이고, VectorPlaneProject는 벽에 수직인 성분을 뺀다.
	// 닿은 자리에서 0.5cm 띄워야 다음 Sweep이 오차로 겹친 채 시작하지 않는다.
	FVector Position = Hit.Location + Hit.Normal * 0.5f;
	FVector Remaining = FVector::VectorPlaneProject(Delta * (1.0f - Hit.Time), Hit.Normal);
	Remaining.Y = 0.0f;
	if (!Remaining.IsNearlyZero())
	{
		FHitResult SlideHit;
		if (!SweepTerrain(Position, Position + Remaining, CollisionRadius, SlideHit)) Position += Remaining;
		else if (!SlideHit.bStartPenetrating) Position = SlideHit.Location + SlideHit.Normal * 0.5f;
	}
	SetActorLocation(Position);
	return Position - Start;
}

void ACoreSupportRobot::AdvancePath()
{
	if (!HasPath() || !IsInCombat() || PathIndex == Path.Num() - 1) return;
	if (FVector::Dist(GetActorLocation(), Path[PathIndex]) <= ArriveDistance) ++PathIndex;
}

void ACoreSupportRobot::UpdateStuck(float DeltaSeconds, float WantedDistance, float MovedDistance)
{
	// "움직이려 하는데 못 움직임"이 쌓이는 시간을 잰다.
	// 목표 근처에서 천천히 감속하는 중은 끼임이 아니므로, 최대 속도의 5% 이상을 원할 때만 본다.
	// 목표와의 거리가 아니라 실제 이동량으로 보는 이유: 달리는 플레이어를 같은 거리로 따라가면 거리가 줄지 않아도 정상이다.
	const bool bWantsToMove = WantedDistance > MaxSpeed * 0.05f * DeltaSeconds;
	StuckElapsed = bWantsToMove && MovedDistance < WantedDistance * 0.2f ? StuckElapsed + DeltaSeconds : 0.0f;
	if (StuckElapsed >= StuckTime) TeleportToPlayer();
}

void ACoreSupportRobot::UpdateOffscreen(float DeltaSeconds, const FBox2D* ScreenBounds)
{
	const FVector Location = GetActorLocation();
	if (!ScreenBounds || ScreenBounds->IsInside(FVector2D(Location.X, Location.Z)))
	{
		OffscreenElapsed = 0.0f;
		return;
	}

	// 화면 밖에 오래 있으면 먼저 전투를 끝내 플레이어에게 날아오게 하고, 그래도 못 돌아오면 순간이동한다.
	OffscreenElapsed += DeltaSeconds;
	if (OffscreenElapsed < OffscreenReturnTime) return;
	if (IsInCombat()) ClearCombat();
	else if (OffscreenElapsed >= OffscreenReturnTime + StuckTime) TeleportToPlayer();
}

void ACoreSupportRobot::TeleportToPlayer()
{
	const APlayerBase* PlayerActor = Player.Get();
	if (!PlayerActor) return;

	// 따라다니는 위치가 벽 속이면 플레이어 캡슐 자리(비어 있음이 보장됨)로 옮긴다.
	FVector Destination = GetFollowPoint();
	Destination.Y = PlayerActor->GetActorLocation().Y;
	if (!IsSpotFree(Destination)) Destination = PlayerActor->GetActorLocation();

	SetActorLocation(Destination);
	Velocity = FVector::ZeroVector;
	StuckElapsed = 0.0f;
	OffscreenElapsed = 0.0f;
	// 옮긴 자리에서는 이전 경로가 맞지 않으므로 경로를 비우고 플레이어를 따라간다.
	ClearMoveGoal();
}

bool ACoreSupportRobot::HasPath() const
{
	return Path.IsValidIndex(PathIndex);
}

void ACoreSupportRobot::SetMoveGoal(const FVector& Goal)
{
	SetMovePath({ Goal });
}

void ACoreSupportRobot::SetMovePath(const TArray<FVector>& Waypoints)
{
	if (Waypoints.IsEmpty())
	{
		ClearMoveGoal();
		return;
	}
	Path = Waypoints;
	PathIndex = 0;
	StuckElapsed = 0.0f;
}

void ACoreSupportRobot::ClearMoveGoal()
{
	Path.Reset();
	PathIndex = 0;
}

// =====================================================================
// 화면
// =====================================================================

bool ACoreSupportRobot::GetScreenBounds(FBox2D& OutBounds) const
{
	const APlayerBase* PlayerActor = Player.Get();
	const APlayerController* Controller = PlayerActor ? Cast<APlayerController>(PlayerActor->GetController()) : nullptr;
	const APlayerCameraManager* Camera = Controller ? Controller->PlayerCameraManager.Get() : nullptr;
	if (!Camera) return false;

	int32 Width = 0, Height = 0;
	Controller->GetViewportSize(Width, Height);
	const float Aspect = Width > 0 && Height > 0 ? float(Width) / Height : 16.0f / 9.0f;
	return CalcScreenBounds(Camera->GetCameraLocation(), Camera->GetCameraRotation(), Camera->GetFOVAngle(),
		Aspect, PlayerActor->GetActorLocation().Y, OutBounds);
}

bool ACoreSupportRobot::CalcScreenBounds(const FVector& CameraLocation, const FRotator& CameraRotation,
	float HorizontalFOV, float AspectRatio, float PlaneY, FBox2D& OutBounds)
{
	// 원근 카메라에서 보이는 폭은 거리에 비례한다: 반폭 = 거리 × tan(가로 시야각 / 2), 반높이 = 반폭 / 화면비.
	// Depth는 카메라 시선을 따라 플레이어 평면(Y = PlaneY)까지 간 거리다.
	// 옆에서 보는 카메라만 다룬다. 시선이 평면과 거의 나란하면 평면에 닿지 않아 사각형을 정할 수 없다.
	const FVector Forward = CameraRotation.Vector();
	if (FMath::Abs(Forward.Y) < 0.5f || HorizontalFOV <= 0.0f || HorizontalFOV >= 180.0f || AspectRatio <= 0.0f) return false;
	const float Depth = (PlaneY - CameraLocation.Y) / Forward.Y;
	if (Depth <= 1.0f) return false;

	const FVector Center = CameraLocation + Forward * Depth;
	const float HalfWidth = Depth * FMath::Tan(FMath::DegreesToRadians(HorizontalFOV * 0.5f));
	const float HalfHeight = HalfWidth / AspectRatio;
	OutBounds = FBox2D(FVector2D(Center.X - HalfWidth, Center.Z - HalfHeight),
		FVector2D(Center.X + HalfWidth, Center.Z + HalfHeight));
	return true;
}

// =====================================================================
// 지형 판정
// =====================================================================

bool ACoreSupportRobot::SweepTerrain(const FVector& Start, const FVector& End, float Radius, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	// WorldDynamic 채널로 구를 밀면 벽·바닥·문(모든 채널을 막는 메시)은 걸리고, 트리거·통과 발판은 지나간다.
	// 플레이어·적 캡슐(Pawn)과 적 메시(PhysicsBody)는 우리 쪽에서 무시해 이동 중 적을 통과한다.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SupportRobotTerrain), false, this);
	if (Player.IsValid()) Params.AddIgnoredActor(Player.Get());
	FCollisionResponseParams Response;
	Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	Response.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Ignore);
	return World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_WorldDynamic,
		FCollisionShape::MakeSphere(Radius), Params, Response);
}

bool ACoreSupportRobot::IsSpotFree(const FVector& Location) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	// 이동 Sweep과 같은 기준으로, 로봇 몸이 그 자리에 놓이면 지형과 겹치는지 본다.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SupportRobotSpot), false, this);
	if (Player.IsValid()) Params.AddIgnoredActor(Player.Get());
	FCollisionResponseParams Response;
	Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	Response.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Ignore);
	return !World->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_WorldDynamic,
		FCollisionShape::MakeSphere(CollisionRadius), Params, Response);
}

// =====================================================================
// 전투 판단
// =====================================================================

void ACoreSupportRobot::RunDecision()
{
	bDecisionRequested = false;
	DecisionElapsed = 0.0f;
	Target = ChooseTarget();
}

AActor* ACoreSupportRobot::ChooseTarget()
{
	if (IsAliveEnemy(PlayerHitTarget.Get())) return PlayerHitTarget.Get();
	if (IsAliveEnemy(AttackerTarget.Get())) return AttackerTarget.Get();
	return nullptr;
}

bool ACoreSupportRobot::IsAliveEnemy(const AActor* Actor)
{
	// 일반 몬스터와 보스는 서로 다른 클래스라 따로 확인한다. 보스의 상태 조회는 const가 아니어서 const_cast를 쓴다.
	if (const AEnemyBase* Enemy = Cast<AEnemyBase>(Actor)) return IsValid(Enemy) && !Enemy->IsDead();
	if (const ABossMonster* Boss = Cast<ABossMonster>(Actor))
	{
		return IsValid(Boss) && const_cast<ABossMonster*>(Boss)->GetBossState() != EBossState::Dead;
	}
	return false;
}

// =====================================================================
// 전투 상태·대상
// =====================================================================

void ACoreSupportRobot::RefreshCombat(float Duration)
{
	const UWorld* World = GetWorld();
	if (!World || !FMath::IsFinite(Duration) || Duration <= 0.0f) return;
	CombatEndTime = FMath::Max(CombatEndTime, World->GetTimeSeconds() + Duration);
}

void ACoreSupportRobot::ClearCombat()
{
	PlayerHitTarget.Reset();
	AttackerTarget.Reset();
	Target.Reset();
	CombatEndTime = 0.0;
	ClearMoveGoal();
}

void ACoreSupportRobot::SetPlayerHitTarget(AActor* Enemy)
{
	if (!IsAliveEnemy(Enemy) || PlayerHitTarget.Get() == Enemy) return;
	PlayerHitTarget = Enemy;
	bDecisionRequested = true;
}

void ACoreSupportRobot::SetAttackerTarget(AActor* Enemy)
{
	if (!IsAliveEnemy(Enemy) || AttackerTarget.Get() == Enemy) return;
	AttackerTarget = Enemy;
	// 플레이어가 때린 적이 있으면 그쪽이 우선이라 판단을 앞당길 필요가 없다.
	if (!IsAliveEnemy(PlayerHitTarget.Get())) bDecisionRequested = true;
}

void ACoreSupportRobot::ForgetTarget(const AActor* Enemy)
{
	if (!Enemy) return;
	if (PlayerHitTarget.Get() == Enemy) PlayerHitTarget.Reset();
	if (AttackerTarget.Get() == Enemy) AttackerTarget.Reset();
	if (Target.Get() == Enemy)
	{
		Target.Reset();
		bDecisionRequested = true;
	}
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
