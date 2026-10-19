#include "Gimmick/Laser/LaserEmitter.h"

#include "Character/Player/PlayerBase.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Gimmick/Laser/LaserReceiver.h"
#include "Gimmick/Laser/LaserReflector.h"

ALaserEmitter::ALaserEmitter()
{
	// 반사판 회전과 장애물 이동을 매 프레임 반영
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// 발신기는 다른 장치의 활성 신호로 작동
	bSupportsInteraction = false;

	LaserOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("LaserOrigin"));

	LaserOrigin->SetupAttachment(SceneRoot);
	LaserOrigin->SetMobility(EComponentMobility::Movable);

	// 기본 발사 위치를 바닥에서 100cm 위로 설정
	LaserOrigin->SetRelativeLocation(
		FVector(0.0f, 0.0f, 100.0f)
	);
}

bool ALaserEmitter::LoadLaserConfig()
{
	bHasValidLaserConfig = false;

	if (!LaserConfigRow.DataTable || LaserConfigRow.RowName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: LaserConfigRow의 테이블과 행을 지정해주세요."), *GetName());

		return false;
	}

	const FLaserConfigRow* LoadedRow = LaserConfigRow.GetRow<FLaserConfigRow>(TEXT("ALaserEmitter::LoadLaserConfig"));

	if (!LoadedRow)
	{
		return false;
	}

	if (!FMath::IsFinite(LoadedRow->MaxLaserDistance)|| LoadedRow->MaxLaserDistance <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: MaxLaserDistance는 0보다 큰 유한한 값이어야 합니다."), *GetName());

		return false;
	}

	LaserConfig = *LoadedRow;

	bHasValidLaserConfig = true;

	return true;
}

void ALaserEmitter::BeginPlay()
{
	Super::BeginPlay();

	if (!LoadLaserConfig())
	{
		return;
	}

	// 자동 발사가 설정된 발신기는 시작부터 활성화
	if (bStartActive)
	{
		ApplyActiveState(true);
	}
}

void ALaserEmitter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateLaserPath();
	UpdateReceiverContacts();
	UpdateLaserVisuals();
}

void ALaserEmitter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 발신기가 사라지면 기존 수신기와 연결 해제
	CurrentReceivers.Reset();
	LaserSegments.Reset();

	UpdateReceiverContacts();

	Super::EndPlay(EndPlayReason);
}



void ALaserEmitter::UpdateLaserPath()
{
	// 이전 경로 대신 현재 배치를 기준으로 다시 계산
	LaserSegments.Reset();
	CurrentReceivers.Reset();

	if (!bIsEnabled || !bIsActive || !bHasValidLaserConfig || !IsValid(LaserOrigin))
	{
		return;
	}

	FVector TraceStart = LaserOrigin->GetComponentLocation();

	FVector TraceDirection = LaserOrigin->GetForwardVector().GetSafeNormal();

	float RemainingDistance = LaserConfig.MaxLaserDistance;

	// 반사 직후 같은 표면을 다시 맞는 현상 방지
	constexpr float SurfaceOffset = 1.0f;

	// 반복 경로가 생겨도 한 프레임의 계산량을 제한
	constexpr int32 MaxTraceIterations = 128;

	for (int32 TraceIndex = 0; TraceIndex < MaxTraceIterations && RemainingDistance > KINDA_SMALL_NUMBER; ++TraceIndex)
	{
		FHitResult Hit;

		const bool bHit = TraceLaserSegment(TraceStart, TraceDirection, RemainingDistance, Hit);

		const FVector TraceEnd = bHit ? Hit.ImpactPoint : TraceStart + TraceDirection * RemainingDistance;

		// 계산한 구간 저장
		FLaserSegment& Segment = LaserSegments.AddDefaulted_GetRef();

		Segment.StartPoint = TraceStart;
		Segment.EndPoint = TraceEnd;

		if (!bHit)
		{
			break;
		}

		// 플레이어에 닿으면 즉시 사망 처리
		if (APlayerBase* Player = Cast<APlayerBase>(Hit.GetActor()))
		{
			KillPlayerImmediately(Player);
			break;
		}

		// 수신기에 도착하면 접촉 기록 후 경로 종료
		if (ALaserReceiver* Receiver = Cast<ALaserReceiver>(Hit.GetActor()))
		{
			CurrentReceivers.AddUnique(TWeakObjectPtr<ALaserReceiver>(Receiver));

			break;
		}

		ALaserReflector* Reflector = Cast<ALaserReflector>(Hit.GetActor());

		// 일반 벽이나 장애물에서는 레이저가 멈춤
		if (!Reflector)
		{
			break;
		}

		const float TravelDistance = FVector::Distance(TraceStart, TraceEnd);

		// 표면 내부에서 시작한 비정상적인 반복 충돌 방지
		if (Hit.bStartPenetrating || TravelDistance <= KINDA_SMALL_NUMBER)
		{
			break;
		}

		RemainingDistance -= TravelDistance;

		if (RemainingDistance <= KINDA_SMALL_NUMBER)
		{
			break;
		}

		// 입사 방향과 실제 충돌 표면의 법선으로 반사 방향 계산
		const FVector ReflectedDirection = Reflector->GetReflectedDirection(TraceDirection, Hit.ImpactNormal);

		if (ReflectedDirection.IsNearlyZero())
		{
			break;
		}

		const float AdvanceDistance = FMath::Min(SurfaceOffset, RemainingDistance);

		// 충돌 표면에서 반사 방향으로 조금 이동해서 다음 구간 시작
		TraceStart = Hit.ImpactPoint + ReflectedDirection * AdvanceDistance;

		TraceDirection = ReflectedDirection;

		RemainingDistance -= AdvanceDistance;
	}
}

void ALaserEmitter::UpdateReceiverContacts()
{
	// 이전에는 받았지만 지금은 받지 못하는 수신기
	for (const TWeakObjectPtr<ALaserReceiver>& PreviousReceiver : PreviousReceivers)
	{
		if (PreviousReceiver.IsValid() && !CurrentReceivers.Contains(PreviousReceiver))
		{
			PreviousReceiver->SetLaserContact(this, false);
		}
	}

	// 현재 레이저를 받는 수신기
	for (const TWeakObjectPtr<ALaserReceiver>& CurrentReceiver : CurrentReceivers)
	{
		if (CurrentReceiver.IsValid())
		{
			CurrentReceiver->SetLaserContact(this, true);
		}
	}

	PreviousReceivers = CurrentReceivers;
}

void ALaserEmitter::UpdateLaserVisuals()
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	// 경로 검증 단계에서는 빨간 디버그 선으로 표시
	for (const FLaserSegment& Segment : LaserSegments)
	{
		DrawDebugLine(World, Segment.StartPoint, Segment.EndPoint, FColor::Red, false, 0.0f, 0, 2.0f);
	}
#endif
}

bool ALaserEmitter::TraceLaserSegment(const FVector& Start, const FVector& Direction, float Distance, FHitResult& OutHit) const
{
	OutHit = FHitResult();

	UWorld* World = GetWorld();

	if (!World || Distance <= 0.0f || Direction.IsNearlyZero())
	{
		return false;
	}

	// 복잡한 충돌을 사용해 메시 표면의 법선 확인
	// 발신기 자기 자신은 충돌 검사에서 제외
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LaserTrace), true, this);

	const FVector End = Start + Direction.GetSafeNormal() * Distance;

	return World->LineTraceSingleByChannel(OutHit, Start, End, LaserTraceChannel.GetValue(), QueryParams);
}

void ALaserEmitter::KillPlayerImmediately(ACharacter* Player)
{
	APlayerBase* PlayerBase = Cast<APlayerBase>(Player);

	if (!IsValid(PlayerBase) || PlayerBase->IsDead())
	{
		return;
	}

	// 무적이나 일반 피해 계산과 관계없이 사망 처리 요청
	PlayerBase->HandleDeath();
}
