#include "Core/CoreSupportRobot.h"

#include "Camera/PlayerCameraManager.h"
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

	FBox2D ScreenBounds;
	const FBox2D* Bounds = GetScreenBounds(ScreenBounds) ? &ScreenBounds : nullptr;
	UpdateOffscreen(DeltaSeconds, Bounds);

	// 목표까지 멀면 최대 속도로, 가까워지면 거리에 비례해 줄인 속도를 원한다.
	const FVector ToGoal = GetCurrentGoal(Bounds) - GetActorLocation();
	const float GoalDistance = ToGoal.Size();
	const FVector Desired = GoalDistance > ArriveDistance
		? ToGoal / GoalDistance * MaxSpeed * FMath::Min(1.0f, GoalDistance / SlowdownRadius)
		: FVector::ZeroVector;
	Velocity = FMath::VInterpTo(Velocity, Desired, DeltaSeconds, SteeringRate);

	const FVector Moved = MoveWithSweep(Velocity * DeltaSeconds);
	// 벽에 막힌 만큼 속도도 줄여야 다음 프레임에 벽을 계속 밀지 않는다.
	Velocity = Moved / DeltaSeconds;
	UpdateStuck(DeltaSeconds, Desired.Size() * DeltaSeconds, Moved.Size());

	const AActor* Facing = IsInCombat() ? Target.Get() : nullptr;
	const float FaceX = Facing ? Facing->GetActorLocation().X - GetActorLocation().X : Velocity.X;
	if (FMath::Abs(FaceX) > 1.0f) SetActorRotation(FRotator(0.0f, FaceX > 0.0f ? 0.0f : 180.0f, 0.0f));
}

FVector ACoreSupportRobot::GetFollowPoint() const
{
	const APlayerBase* PlayerActor = Player.Get();
	if (!PlayerActor) return GetActorLocation();

	const float FacingSign = PlayerActor->GetActorForwardVector().X >= 0.0f ? 1.0f : -1.0f;
	const UCharacterMovementComponent* Movement = PlayerActor->GetCharacterMovement();
	const float UpSign = Movement && Movement->GetGravityDirection().Z > 0.0f ? -1.0f : 1.0f;
	return PlayerActor->GetActorLocation() + FVector(-FacingSign * FollowOffset.X, 0.0f, UpSign * FollowOffset.Y);
}

FVector ACoreSupportRobot::GetCurrentGoal(const FBox2D* ScreenBounds) const
{
	FVector Goal = bHasMoveGoal && IsInCombat() ? MoveGoal : GetFollowPoint();
	if (const APlayerBase* PlayerActor = Player.Get()) Goal.Y = PlayerActor->GetActorLocation().Y;
	if (!ScreenBounds) return Goal;

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
	if (!SweepTerrain(Start, Start + Delta, Hit))
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

	// 닿기 직전에서 살짝 떨어뜨려 다음 Sweep이 겹친 채 시작하지 않게 한다.
	FVector Position = Hit.Location + Hit.Normal * 0.5f;
	FVector Remaining = FVector::VectorPlaneProject(Delta * (1.0f - Hit.Time), Hit.Normal);
	Remaining.Y = 0.0f;
	if (!Remaining.IsNearlyZero())
	{
		FHitResult SlideHit;
		if (!SweepTerrain(Position, Position + Remaining, SlideHit)) Position += Remaining;
		else if (!SlideHit.bStartPenetrating) Position = SlideHit.Location + SlideHit.Normal * 0.5f;
	}
	SetActorLocation(Position);
	return Position - Start;
}

bool ACoreSupportRobot::SweepTerrain(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SupportRobotMove), false, this);
	if (Player.IsValid()) Params.AddIgnoredActor(Player.Get());
	FCollisionResponseParams Response;
	Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	Response.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Ignore);
	return World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_WorldDynamic,
		FCollisionShape::MakeSphere(CollisionRadius), Params, Response);
}

void ACoreSupportRobot::UpdateStuck(float DeltaSeconds, float WantedDistance, float MovedDistance)
{
	// 느리게 다가가는 중은 끼임이 아니다. 어느 정도 속도를 원할 때만 본다.
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

	OffscreenElapsed += DeltaSeconds;
	if (OffscreenElapsed < OffscreenReturnTime) return;
	// 먼저 전투를 끝내 플레이어에게 날아오게 하고, 그래도 오래 화면 밖이면 순간이동한다.
	if (IsInCombat()) ClearCombat();
	else if (OffscreenElapsed >= OffscreenReturnTime + StuckTime) TeleportToPlayer();
}

void ACoreSupportRobot::TeleportToPlayer()
{
	const APlayerBase* PlayerActor = Player.Get();
	const UWorld* World = GetWorld();
	if (!PlayerActor || !World) return;

	FVector Destination = GetFollowPoint();
	Destination.Y = PlayerActor->GetActorLocation().Y;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SupportRobotTeleport), false, this);
	Params.AddIgnoredActor(PlayerActor);
	FCollisionResponseParams Response;
	Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	Response.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Ignore);
	if (World->OverlapBlockingTestByChannel(Destination, FQuat::Identity, ECC_WorldDynamic,
		FCollisionShape::MakeSphere(CollisionRadius), Params, Response))
	{
		Destination = PlayerActor->GetActorLocation();
	}

	SetActorLocation(Destination);
	Velocity = FVector::ZeroVector;
	StuckElapsed = 0.0f;
	OffscreenElapsed = 0.0f;
	// 옮긴 자리에서는 이전 경로가 맞지 않으므로 다음 계산까지 플레이어를 따라간다.
	bHasMoveGoal = false;
}

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
	// 옆에서 보는 카메라만 다룬다. 시선이 평면과 거의 나란하면 사각형을 정할 수 없다.
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

void ACoreSupportRobot::RefreshCombat(float Duration)
{
	const UWorld* World = GetWorld();
	if (!World || !FMath::IsFinite(Duration) || Duration <= 0.0f) return;
	CombatEndTime = FMath::Max(CombatEndTime, World->GetTimeSeconds() + Duration);
}

void ACoreSupportRobot::SetTarget(AActor* InTarget)
{
	Target = InTarget;
}

void ACoreSupportRobot::ClearCombat()
{
	Target.Reset();
	CombatEndTime = 0.0;
	bHasMoveGoal = false;
}

void ACoreSupportRobot::SetMoveGoal(const FVector& Goal)
{
	MoveGoal = Goal;
	bHasMoveGoal = true;
	StuckElapsed = 0.0f;
}

void ACoreSupportRobot::ClearMoveGoal()
{
	bHasMoveGoal = false;
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
