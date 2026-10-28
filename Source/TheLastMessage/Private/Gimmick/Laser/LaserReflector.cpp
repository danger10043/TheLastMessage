// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Laser/LaserReflector.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Character/Player/PlayerBase.h"

ALaserReflector::ALaserReflector()
{
	// 반사판은 플레이어가 직접 상호작용하는 기믹
	bSupportsInteraction = true;

	ReflectorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ReflectorPivot"));

	ReflectorPivot->SetupAttachment(SceneRoot);
	ReflectorPivot->SetMobility(EComponentMobility::Movable);

	// 기본 회전 중심을 바닥에서 100cm 위에 배치
	ReflectorPivot->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));

	ReflectiveMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReflectiveMesh"));

	ReflectiveMesh->SetupAttachment(ReflectorPivot);
	ReflectiveMesh->SetMobility(EComponentMobility::Movable);

	// 플레이어 및 레이저 트레이스와 충돌할 수 있도록 설정
	ReflectiveMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	ReflectiveMesh->SetGenerateOverlapEvents(false);
}

void ALaserReflector::BeginPlay()
{
	// Blueprint의 BeginPlay에서도 초기화 함수를 사용할 수 있도록
	// 최초 회전을 먼저 기록
	if (IsValid(ReflectorPivot))
	{
		InitialPivotRotation = ReflectorPivot->GetRelativeRotation().Quaternion();

		bHasInitialPivotRotation = true;
	}

	Super::BeginPlay();
}

void ALaserReflector::PerformInteraction(ACharacter* Player)
{
	if (!CanInteractInternal(Player))
	{
		return;
	}

	RotateReflector();
}

void ALaserReflector::ReceiveAttackDamage(float DamageAmount, AActor* DamageCauser)
{
		if (!HasAuthority() || !bIsEnabled || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
		{
			return;
		}

		// 플레이어의 공격만 허용
		const APlayerBase* Player = Cast<APlayerBase>(DamageCauser);

		if (!IsValid(Player) || Player->IsDead())
		{
			return;
		}

		// 기존 상호작용과 같은 각도만큼 회전
		RotateReflector();
}

void ALaserReflector::RotateReflector()
{
	if (!bIsEnabled || !IsValid(ReflectorPivot))
	{
		return;
	}

	const FVector LocalRotationAxis = RotationAxis.GetSafeNormal();

	if (LocalRotationAxis.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: RotationAxis는 영벡터가 아니어야 합니다."), *GetName());

		return;
	}

	if (!FMath::IsFinite(RotationStepAngle) || FMath::IsNearlyZero(RotationStepAngle))
	{
		return;
	}

	const FQuat RotationDelta(LocalRotationAxis, FMath::DegreesToRadians(RotationStepAngle));

	// 메시만 직접 돌리지 않고 회전 중심을 돌림
	ReflectorPivot->AddLocalRotation(RotationDelta, false, nullptr, ETeleportType::TeleportPhysics);
}

void ALaserReflector::ResetReflector()
{
	if (!bHasInitialPivotRotation || !IsValid(ReflectorPivot))
	{
		return;
	}

	// 기능 사용 가능 여부와 관계없이 초기 자세로 복구
	ReflectorPivot->SetRelativeRotation(InitialPivotRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

FVector ALaserReflector::GetReflectedDirection(const FVector& IncomingDirection, const FVector& SurfaceNormal) const
{
	const FVector Direction = IncomingDirection.GetSafeNormal();
	const FVector Normal = SurfaceNormal.GetSafeNormal();

	if (Direction.IsNearlyZero() || Normal.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	return FMath::GetReflectionVector(Direction, Normal).GetSafeNormal();
}
FProgressActorState ALaserReflector::CaptureProgress() const
{
    FProgressActorState State = Super::CaptureProgress();
    State.ComponentTransform = ReflectorPivot->GetRelativeTransform();
    return State;
}

void ALaserReflector::RestoreProgress(const FProgressActorState& State)
{
    ReflectorPivot->SetRelativeTransform(State.ComponentTransform);
    Super::RestoreProgress(State);
}
