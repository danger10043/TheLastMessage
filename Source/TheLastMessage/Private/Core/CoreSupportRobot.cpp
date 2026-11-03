#include "Core/CoreSupportRobot.h"

#include "Algo/Reverse.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossTypes.h"
#include "Character/Enemy/EnemyBase.h"
#include "Character/Player/PlayerBase.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/CoreSupportRobotProjectile.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "GAS/CombatAttributeSet.h"

ACoreSupportRobot::ACoreSupportRobot()
{
	PrimaryActorTick.bCanEverTick = true;
	// 플레이어가 이번 프레임 이동을 마친 뒤 따라간다.
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	// 로봇은 다른 판정에 걸리지 않고, 지형 확인은 직접 Sweep으로 한다.
	SetActorEnableCollision(false);
	SetCanBeDamaged(false);
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	// 모습만 보여 주는 메시라 충돌·겹침을 모두 끈다(액터 충돌을 꺼도 BP에서 켜지지 않게 컴포넌트에서도 끔).
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	ProjectileClass = ACoreSupportRobotProjectile::StaticClass();
}

UAbilitySystemComponent* ACoreSupportRobot::GetAbilitySystemComponent() const
{
	return Player.IsValid() ? Player->GetAbilitySystemComponent() : nullptr;
}

FVector ACoreSupportRobot::GetVelocity() const
{
	return Velocity;
}

FVector ACoreSupportRobot::GetMuzzleLocation() const
{
	// 몬스터(AEnemyBase::GetMuzzleLocation)와 같은 방식: 소켓이 있으면 소켓, 없으면 액터 기준 오프셋.
	// 소켓 조회는 뼈 이름도 받으므로 외부 팩 메시를 고치지 않고 Minigun 뼈를 그대로 쓸 수 있다.
	if (!MuzzleSocketName.IsNone() && Mesh && Mesh->DoesSocketExist(MuzzleSocketName))
	{
		return Mesh->GetSocketLocation(MuzzleSocketName);
	}
	return GetActorTransform().TransformPosition(MuzzleOffset);
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
	FireCooldownRemaining = FMath::Max(0.0f, FireCooldownRemaining - DeltaSeconds);

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
	// 겹침에서 밀려난 양은 순간적으로 클 수 있어, 그대로 속도로 남으면 튕겨 나가므로 최대 속도로 자른다.
	Velocity = (Moved / DeltaSeconds).GetClampedToMaxSize(MaxSpeed);
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
	if (!HasPath() || !IsInCombat()) return;

	// 중간 경유지는 격자 반 칸 안에만 들어와도 넘긴다. 관성 때문에 경유지를 정확히 밟지 못하고 맴도는 것을 막는다.
	const bool bLast = PathIndex == Path.Num() - 1;
	const float Reach = bLast ? ArriveDistance : FMath::Max(ArriveDistance, GridSpacing * 0.5f);
	if (FVector::Dist(GetActorLocation(), Path[PathIndex]) > Reach) return;

	if (!bLast)
	{
		++PathIndex;
		return;
	}
	// 마지막 경유지에 도착하면 그 자리에 떠 있고, 1초를 기다리지 않고 바로 쏠 수 있는지 판단한다.
	if (!bArrivalHandled)
	{
		bArrivalHandled = true;
		bDecisionRequested = true;
	}
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
	// 옮긴 자리에서는 이전 경로가 맞지 않으므로 바로 다시 판단한다(전투가 아니면 그냥 따라간다).
	ClearMoveGoal();
	bDecisionRequested = true;
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
	bArrivalHandled = false;
	StuckElapsed = 0.0f;
}

void ACoreSupportRobot::ClearMoveGoal()
{
	Path.Reset();
	PathIndex = 0;
	bArrivalHandled = false;
}

void ACoreSupportRobot::HoldPosition()
{
	// 지금 위치를 이미 도착한 마지막 경유지로 둔다. 도착 처리를 끝낸 것으로 표시해 판단이 반복 요청되지 않게 한다.
	Path = { GetActorLocation() };
	PathIndex = 0;
	bArrivalHandled = true;
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

FBox2D ACoreSupportRobot::GetSearchBounds() const
{
	// 카메라를 모르면(테스트 월드·컷신 등) 플레이어 중심의 화면 크기 사각형으로 대신한다.
	FBox2D Bounds;
	if (!GetScreenBounds(Bounds))
	{
		const FVector PlayerLocation = Player.IsValid() ? Player->GetActorLocation() : GetActorLocation();
		const FVector2D Center(PlayerLocation.X, PlayerLocation.Z);
		Bounds = FBox2D(Center - FallbackViewSize * 0.5f, Center + FallbackViewSize * 0.5f);
	}

	// 가장자리 여백을 빼고, 여백 때문에 뒤집히면 가운데 한 점으로 줄인다.
	const FVector2D Margin(ScreenMargin, ScreenMargin);
	FBox2D Inner(Bounds.Min + Margin, Bounds.Max - Margin);
	if (Inner.Min.X > Inner.Max.X || Inner.Min.Y > Inner.Max.Y) Inner = FBox2D(Bounds.GetCenter(), Bounds.GetCenter());
	return Inner;
}

// =====================================================================
// 지형 판정
// =====================================================================

bool ACoreSupportRobot::SweepTerrain(const FVector& Start, const FVector& End, float Radius, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	// WorldDynamic 채널로 구를 밀면 벽·바닥·문(모든 채널을 막는 메시)은 걸리고, 트리거·통과 발판은 지나간다.
	// 플레이어·적 캡슐(Pawn)과 적 메시(PhysicsBody)는 우리 쪽에서 무시한다.
	// 그래서 이동에서는 적을 통과하고, 시야 판정에서는 사이에 낀 적을 가림으로 보지 않는다.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SupportRobotTerrain), false, this);
	AddTerrainQueryIgnores(Params);
	FCollisionResponseParams Response;
	Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	Response.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Ignore);
	return World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_WorldDynamic,
		FCollisionShape::MakeSphere(Radius), Params, Response);
}

bool ACoreSupportRobot::IsPathBlocked(const FVector& Start, const FVector& End, float Radius) const
{
	FHitResult Hit;
	return SweepTerrain(Start, End, Radius, Hit);
}

bool ACoreSupportRobot::IsSpotFree(const FVector& Location) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	// 이동 Sweep과 같은 기준으로, 로봇 몸이 그 자리에 놓이면 지형과 겹치는지 본다.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SupportRobotSpot), false, this);
	AddTerrainQueryIgnores(Params);
	FCollisionResponseParams Response;
	Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	Response.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Ignore);
	return !World->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_WorldDynamic,
		FCollisionShape::MakeSphere(CollisionRadius), Params, Response);
}

void ACoreSupportRobot::AddTerrainQueryIgnores(FCollisionQueryParams& Params) const
{
	if (Player.IsValid()) Params.AddIgnoredActor(Player.Get());
	for (const TWeakObjectPtr<AActor>& Projectile : LiveProjectiles)
	{
		if (Projectile.IsValid()) Params.AddIgnoredActor(Projectile.Get());
	}
}

// =====================================================================
// 전투 판단
// =====================================================================

void ACoreSupportRobot::RunDecision()
{
	bDecisionRequested = false;
	DecisionElapsed = 0.0f;

	// 한 번의 판단에서 후보는 최대 둘(플레이어가 때린 적, 플레이어를 때린 적)이다.
	// 첫 후보의 쏠 자리를 못 찾으면 잠시 제외하고 다음 후보를 본다.
	for (int32 Attempt = 0; Attempt < 2; ++Attempt)
	{
		AActor* Enemy = ChooseTarget();
		Target = Enemy;
		if (!Enemy) break;

		// 지금 자리에서 쏠 수 있으면 움직이지 않고 쏜다. 9~12m 여유 구간에서도 머무르는 이유가 이것이다.
		// 도착 직후에도 이 판단이 바로 불리므로, 쿨타임이 끝나 있으면 도착하자마자 첫 발이 나간다.
		const FVector AimPoint = GetAimPoint(Enemy);
		if (CanShootFrom(GetActorLocation(), AimPoint))
		{
			HoldPosition();
			TryFire(AimPoint);
			return;
		}

		TArray<FVector> NewPath;
		if (FindAttackPath(AimPoint, NewPath))
		{
			SetMovePath(NewPath);
			return;
		}

		if (const UWorld* World = GetWorld()) ExcludedUntil.Add(Enemy, World->GetTimeSeconds() + ExcludeDuration);
	}

	// 노릴 적이 없으면 경로를 비워 플레이어 곁을 따라간다. 전투 시간은 그대로 흐른다.
	Target.Reset();
	ClearMoveGoal();
}

AActor* ACoreSupportRobot::ChooseTarget()
{
	// 시간이 지났거나 사라진 적은 제외 목록에서 뺀다.
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	for (auto It = ExcludedUntil.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || It.Value() <= Now) It.RemoveCurrent();
	}

	auto IsUsable = [this](AActor* Enemy)
	{
		return IsAliveEnemy(Enemy) && !ExcludedUntil.Contains(Enemy);
	};
	if (IsUsable(PlayerHitTarget.Get())) return PlayerHitTarget.Get();
	if (IsUsable(AttackerTarget.Get())) return AttackerTarget.Get();
	return nullptr;
}

FVector ACoreSupportRobot::GetAimPoint(const AActor* Enemy) const
{
	return PredictAimPoint(GetActorLocation(), Enemy->GetActorLocation(), Enemy->GetVelocity(), ProjectileSpeed, MaxLeadTime);
}

FVector ACoreSupportRobot::PredictAimPoint(const FVector& ShooterLocation, const FVector& TargetLocation,
	const FVector& TargetVelocity, float InProjectileSpeed, float InMaxLeadTime)
{
	if (InProjectileSpeed <= 0.0f) return TargetLocation;

	// 비행시간은 조준점까지의 거리로 정해지고, 조준점은 비행시간으로 정해진다(서로 물려 있음).
	// 적 위치로 비행시간을 먼저 구하고, 그 결과 조준점으로 한 번 더 구하면 실제 만나는 점에 충분히 가까워진다.
	FVector AimPoint = TargetLocation;
	for (int32 Iteration = 0; Iteration < 2; ++Iteration)
	{
		const float FlightTime = FMath::Min(FVector::Dist(ShooterLocation, AimPoint) / InProjectileSpeed, InMaxLeadTime);
		AimPoint = TargetLocation + TargetVelocity * FlightTime;
	}
	return AimPoint;
}

bool ACoreSupportRobot::TryFire(const FVector& AimPoint)
{
	// 피해는 코어가 부여한 플레이어 속성에서 그때그때 읽는다. 코어를 강화하면 다음 발부터 바로 반영된다.
	// 0이면 코어 효과가 없는 상태라 쏘지 않는다.
	const APlayerBase* PlayerActor = Player.Get();
	const UCombatAttributeSet* Stats = PlayerActor ? PlayerActor->GetCombatAttributes() : nullptr;
	const float Damage = Stats ? Stats->GetSupportRobotDamage() : 0.0f;
	UWorld* World = GetWorld();
	if (FireCooldownRemaining > 0.0f || Damage <= 0.0f || !World || !ProjectileClass || !DamageEffectClass) return false;

	// 지연 생성으로 피해·방향을 먼저 정한 뒤 생성을 끝낸다. 생성 순간부터 정해진 속도로 날아가게 하기 위해서다.
	// 사거리보다 조금 더 날아가게 해 사거리 끝의 적도 놓치지 않게 한다.
	const FVector Start = GetMuzzleLocation();
	const FTransform SpawnTransform((AimPoint - Start).Rotation(), Start);
	ACoreSupportRobotProjectile* Projectile = World->SpawnActorDeferred<ACoreSupportRobotProjectile>(ProjectileClass,
		SpawnTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile) return false;
	Projectile->InitializeShot(this, AimPoint - Start, ProjectileSpeed, ProjectileRadius, Damage, DamageEffectClass,
		FireRange * 1.2f);
	Projectile->FinishSpawning(SpawnTransform);
	// 이미 사라진 탄은 정리하고, 새 탄을 지형 판정 제외 목록에 넣는다.
	LiveProjectiles.RemoveAll([](const TWeakObjectPtr<AActor>& Shot) { return !Shot.IsValid(); });
	LiveProjectiles.Add(Projectile);

	FireCooldownRemaining = FireInterval;
	PlayFireEffects(Start);
	return true;
}

void ACoreSupportRobot::PlayFireEffects(const FVector& MuzzleLocation)
{
	// 몬스터(SentryEnemy::FireProjectile)와 같은 순서: 총구 섬광 → 발사음 → 사격 애니메이션.
	// 섬광은 총구 소켓이 있으면 소켓에 붙여 로봇이 움직여도 따라가게 하고, 없으면 총구 위치에 한 번 재생한다.
	if (MuzzleFlashEffect)
	{
		if (!MuzzleSocketName.IsNone() && Mesh && Mesh->DoesSocketExist(MuzzleSocketName))
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(MuzzleFlashEffect, Mesh, MuzzleSocketName, FVector::ZeroVector,
				FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
		}
		else
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, MuzzleFlashEffect, MuzzleLocation, GetActorRotation());
		}
	}
	if (FireSound) UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLocation);

	// 몽타주는 ABP의 슬롯을 통해 대기·이동 애니메이션 위에 덮여 재생된다.
	UAnimInstance* Animation = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (FireMontage && Animation) Animation->Montage_Play(FireMontage);
}

bool ACoreSupportRobot::CanShootFrom(const FVector& From, const FVector& AimPoint) const
{
	// 탄 크기의 구로 미리 밀어 본다. 가는 선으로는 지나가도 실제 탄은 걸리는 좁은 틈을 걸러낸다.
	return FVector::Dist(From, AimPoint) <= FireRange && !IsPathBlocked(From, AimPoint, ProjectileRadius);
}

int32 ACoreSupportRobot::RateSpot(const FVector& Spot, const FVector& AimPoint) const
{
	// 거리 비교가 Sweep보다 훨씬 싸므로 먼저 거르고, 사거리 안인 자리만 시야를 확인한다.
	const float Distance = FVector::Dist(Spot, AimPoint);
	if (Distance > FireRange || IsPathBlocked(Spot, AimPoint, ProjectileRadius)) return 0;

	// 4~9m 띠 밖이면 쏠 수는 있어도 너무 가깝거나 사거리 끝이라 최후 수단(3등급)이다.
	// 띠 안이면서 조준점보다 충분히 높으면 내려다보며 쏘는 자리라 1등급이다. "위"는 중력과 관계없이 월드 +Z다.
	if (Distance < GoalMinDistance || Distance > GoalMaxDistance) return 3;
	return Spot.Z - AimPoint.Z >= AboveTargetHeight ? 1 : 2;
}

FVector ACoreSupportRobot::GetPreferredPoint(const FVector& AimPoint, const FBox2D& SearchBounds) const
{
	// 플레이어가 있는 쪽, 조준점의 위쪽 대각선. 로봇이 플레이어 곁에서 날아오므로 이동이 짧고, 화면에서도 자연스럽다.
	// 플레이어가 적 바로 위·아래(가로 차 30cm 안)면 방향이 안 정해지므로 로봇이 있는 쪽을 쓴다.
	const FVector PlayerLocation = Player.IsValid() ? Player->GetActorLocation() : GetActorLocation();
	float SideX = PlayerLocation.X - AimPoint.X;
	if (FMath::Abs(SideX) < 30.0f) SideX = GetActorLocation().X - AimPoint.X;
	const float Side = SideX >= 0.0f ? 1.0f : -1.0f;

	FVector Point(AimPoint.X + Side * PreferredOffset.X, PlayerLocation.Y, AimPoint.Z + PreferredOffset.Y);
	Point.X = FMath::Clamp(Point.X, SearchBounds.Min.X, SearchBounds.Max.X);
	Point.Z = FMath::Clamp(Point.Z, SearchBounds.Min.Y, SearchBounds.Max.Y);
	return Point;
}

bool ACoreSupportRobot::FindAttackPath(const FVector& AimPoint, TArray<FVector>& OutPath) const
{
	// 1) 선호 지점: 띠 안에서 보이고(1·2등급), 로봇이 바로 날아갈 수 있으면 그대로 쓴다. 대부분 여기서 끝난다.
	// 2) 막혔으면 격자 BFS로 돌아가는 길을 찾는다.
	const FBox2D SearchBounds = GetSearchBounds();
	const FVector Preferred = GetPreferredPoint(AimPoint, SearchBounds);
	const int32 PreferredRating = IsSpotFree(Preferred) ? RateSpot(Preferred, AimPoint) : 0;
	if ((PreferredRating == 1 || PreferredRating == 2) && !IsPathBlocked(GetActorLocation(), Preferred, CollisionRadius))
	{
		OutPath = { Preferred };
		return true;
	}
	return SearchGridPath(AimPoint, Preferred, SearchBounds, OutPath);
}

bool ACoreSupportRobot::SearchGridPath(const FVector& AimPoint, const FVector& PreferredPoint,
	const FBox2D& SearchBounds, TArray<FVector>& OutPath) const
{
	const APlayerBase* PlayerActor = Player.Get();
	if (!PlayerActor) return false;

	// ---- 격자 ----
	// 칸 (i, j)는 월드 (i × 간격, 플레이어 Y, j × 간격)이다. 화면이 아니라 월드 좌표에 고정해
	// 카메라가 움직여도 노드가 흔들리지 않고, 같은 상황이면 매번 같은 결과가 나온다. 화면은 쓸 범위만 정한다.
	const float PlaneY = PlayerActor->GetActorLocation().Y;
	const FVector Start = GetActorLocation();
	auto ToWorld = [this, PlaneY](const FIntPoint& Cell)
	{
		return FVector(Cell.X * GridSpacing, PlaneY, Cell.Y * GridSpacing);
	};

	// 범위 밖이거나 로봇 몸이 들어가지 않는(벽 속) 칸은 못 쓴다. 같은 칸을 여러 번 묻지 않게 결과를 기억한다.
	TMap<FIntPoint, bool> FreeCells;
	auto IsCellFree = [&](const FIntPoint& Cell)
	{
		if (const bool* Cached = FreeCells.Find(Cell)) return *Cached;
		const FVector Spot = ToWorld(Cell);
		const bool bFree = SearchBounds.IsInside(FVector2D(Spot.X, Spot.Z)) && IsSpotFree(Spot);
		FreeCells.Add(Cell, bFree);
		return bFree;
	};

	// ---- BFS 시작 ----
	// 로봇 위치는 칸 중심이 아니므로, 주변 몇 칸에 바로 날아갈 수 있는지 Sweep으로 확인해 1층으로 넣는다.
	// Depth는 로봇에서 몇 번 이동해 닿는지(층)이고, 같은 층은 비슷한 거리라는 뜻이다.
	struct FVisit
	{
		FIntPoint Parent;
		int32 Depth = 0;
		bool bLinkedToStart = false;
	};
	TMap<FIntPoint, FVisit> Visits;
	TArray<FIntPoint> Queue;
	const FIntPoint StartCell(FMath::RoundToInt(Start.X / GridSpacing), FMath::RoundToInt(Start.Z / GridSpacing));
	for (int32 DX = -StartLinkCells; DX <= StartLinkCells; ++DX)
	{
		for (int32 DZ = -StartLinkCells; DZ <= StartLinkCells; ++DZ)
		{
			const FIntPoint Cell = StartCell + FIntPoint(DX, DZ);
			if (!IsCellFree(Cell) || IsPathBlocked(Start, ToWorld(Cell), CollisionRadius)) continue;
			Visits.Add(Cell, { Cell, 1, true });
			Queue.Add(Cell);
		}
	}

	// ---- BFS 확장 ----
	// 큐 앞에서 꺼낸 칸의 등급을 매기고, 이웃 8칸 중 아직 안 가 본 빈 칸으로 막힘 없이 갈 수 있으면 다음 층으로 넣는다.
	// BFS는 얕은 층부터 꺼내므로 "처음 만난 후보 = 로봇에서 가장 빨리 가는 후보"다.
	// 등급마다 가장 얕은 층의 후보를 기억하고, 같은 층이면 선호 지점에 가까운 쪽을 남긴다.
	// 1등급을 찾았으면 그 층까지만 마저 보고(동점 비교) 멈춘다. 1등급이 없으면 끝까지 보고 2·3등급 중 최선을 쓴다.
	constexpr int32 NoDepth = TNumericLimits<int32>::Max();
	int32 BestDepth[4] = { NoDepth, NoDepth, NoDepth, NoDepth };
	double BestPreferredDistSq[4] = { 0.0, 0.0, 0.0, 0.0 };
	FIntPoint BestCell[4];
	for (int32 Head = 0; Head < Queue.Num() && Head < MaxSearchNodes; ++Head)
	{
		const FIntPoint Cell = Queue[Head];
		const int32 Depth = Visits[Cell].Depth;
		if (Depth > BestDepth[1]) break;

		const FVector Spot = ToWorld(Cell);
		const int32 Rating = RateSpot(Spot, AimPoint);
		if (Rating > 0)
		{
			const double PreferredDistSq = FVector::DistSquared(Spot, PreferredPoint);
			if (Depth < BestDepth[Rating] || (Depth == BestDepth[Rating] && PreferredDistSq < BestPreferredDistSq[Rating]))
			{
				BestDepth[Rating] = Depth;
				BestPreferredDistSq[Rating] = PreferredDistSq;
				BestCell[Rating] = Cell;
			}
		}

		for (int32 DX = -1; DX <= 1; ++DX)
		{
			for (int32 DZ = -1; DZ <= 1; ++DZ)
			{
				const FIntPoint Next = Cell + FIntPoint(DX, DZ);
				if ((DX == 0 && DZ == 0) || Visits.Contains(Next) || !IsCellFree(Next)) continue;
				// 두 칸이 비어 있어도 사이에 얇은 벽이 있을 수 있어 Sweep으로 연결을 확인한다.
				if (IsPathBlocked(Spot, ToWorld(Next), CollisionRadius)) continue;
				Visits.Add(Next, { Cell, Depth + 1, false });
				Queue.Add(Next);
			}
		}
	}

	int32 Rating = 0;
	for (int32 Candidate = 1; Candidate <= 3 && Rating == 0; ++Candidate)
	{
		if (BestDepth[Candidate] != NoDepth) Rating = Candidate;
	}
	if (Rating == 0) return false;

	// ---- 경로 만들기 ----
	// 고른 칸에서 부모를 거꾸로 따라가 로봇과 바로 이어진 1층 칸까지 모은 뒤 뒤집으면 로봇 → 목표 순서가 된다.
	TArray<FVector> Points;
	for (FIntPoint Cell = BestCell[Rating];;)
	{
		Points.Add(ToWorld(Cell));
		const FVisit& Visit = Visits[Cell];
		if (Visit.bLinkedToStart) break;
		Cell = Visit.Parent;
	}
	Points.Add(Start);
	Algo::Reverse(Points);
	SmoothPath(Points);
	Points.RemoveAt(0);
	OutPath = MoveTemp(Points);
	return !OutPath.IsEmpty();
}

void ACoreSupportRobot::SmoothPath(TArray<FVector>& InOutPoints) const
{
	// 줄 당기기: 지금 점에서 가장 먼 점부터 거꾸로 Sweep해 처음 통하는 점으로 건너뛴다.
	// 격자 때문에 계단처럼 꺾인 경로가 직선 몇 개로 펴진다. 점이 적어서(수십 개 이하) 비용은 작다.
	if (InOutPoints.Num() <= 2) return;

	TArray<FVector> Result = { InOutPoints[0] };
	int32 From = 0;
	while (From < InOutPoints.Num() - 1)
	{
		int32 To = InOutPoints.Num() - 1;
		while (To > From + 1 && IsPathBlocked(InOutPoints[From], InOutPoints[To], CollisionRadius)) --To;
		Result.Add(InOutPoints[To]);
		From = To;
	}
	InOutPoints = MoveTemp(Result);
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
	ExcludedUntil.Reset();
	CombatEndTime = 0.0;
	ClearMoveGoal();
}

void ACoreSupportRobot::SetPlayerHitTarget(AActor* Enemy)
{
	if (!IsAliveEnemy(Enemy)) return;
	const bool bWasExcluded = ExcludedUntil.Remove(Enemy) > 0;
	if (PlayerHitTarget.Get() != Enemy || bWasExcluded)
	{
		PlayerHitTarget = Enemy;
		bDecisionRequested = true;
	}
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
