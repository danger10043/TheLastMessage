#include "Gimmick/Laser/LaserEmitter.h"
#include "Save/ProgressSaveSubsystem.h"
#include "Engine/GameInstance.h"

#include "Character/Player/PlayerBase.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Gimmick/Laser/LaserReceiver.h"
#include "Gimmick/Laser/LaserReflector.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"

ALaserEmitter::ALaserEmitter()
{
	// 반사판 회전과 장애물 이동을 매 프레임 반영
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// 발신기는 다른 장치의 활성 신호로 작동
	bSupportsInteraction = false;

	EmitterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EmitterMesh"));

	EmitterMesh->SetupAttachment(SceneRoot);
	EmitterMesh->SetMobility(EComponentMobility::Movable);
	EmitterMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	LaserOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("LaserOrigin"));

	// 발사 기준점이 발신기 메시를 따라가도록 연결
	LaserOrigin->SetupAttachment(EmitterMesh);
	LaserOrigin->SetMobility(EComponentMobility::Movable);

	// 정확한 발사구 위치는 블루프린트에서 조정
	LaserOrigin->SetRelativeLocation(FVector::ZeroVector);
}

void ALaserEmitter::ApplyActiveState(bool bNewActive)
{
	Super::ApplyActiveState(bNewActive && bIsEnabled);
	SetActorTickEnabled(bIsEnabled && bIsActive);

	if (!bIsActive)
	{
		LaserSegments.Reset();
		CurrentReceivers.Reset();
		UpdateReceiverContacts();
		UpdateLaserVisuals();
	}

	UpdateEmitterIndicator();
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

	if (!FMath::IsFinite(LoadedRow->MaxLaserDistance)
		|| LoadedRow->MaxLaserDistance <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: MaxLaserDistance는 0보다 큰 유한한 값이어야 합니다."), *GetName());

		return false;
	}

	if (!FMath::IsFinite(LoadedRow->PlayerHitRadiusScale)
		|| LoadedRow->PlayerHitRadiusScale <= 0.0f
		|| LoadedRow->PlayerHitRadiusScale > 1.0f
		|| !FMath::IsFinite(LoadedRow->PlayerHitHeightScale)
		|| LoadedRow->PlayerHitHeightScale <= 0.0f
		|| LoadedRow->PlayerHitHeightScale > 1.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 플레이어 판정 비율은 0보다 크고 1 이하여야 합니다."), *GetName());

		return false;
	}

	LaserConfig = *LoadedRow;
	bHasValidLaserConfig = true;

	return true;
}

void ALaserEmitter::BeginPlay()
{
	Super::BeginPlay();

	EmitterIndicatorMaterial = Cast<UMaterialInstanceDynamic>(EmitterMesh->GetMaterial(IndicatorMaterialIndex));

	if (!IsValid(EmitterIndicatorMaterial))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: EmitterMesh와 표시등 재질 슬롯을 확인해주세요."), *GetName());
	}

	if (!LoadLaserConfig())
	{
		ApplyActiveState(false);
		return;
	}

	// 시작 전에 받은 활성 신호와 자동 발사 설정을 함께 반영
	ApplyActiveState(bIsActive || bStartActive);
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
	// 기존 수신기와 연결 해제
	CurrentReceivers.Reset();
	LaserSegments.Reset();

	UpdateReceiverContacts();

	// 생성한 Niagara 컴포넌트 정리
	for (UNiagaraComponent* EffectComponent
		: LaserEffectComponents)
	{
		if (IsValid(EffectComponent))
		{
			EffectComponent->DestroyComponent();
		}
	}

	LaserEffectComponents.Reset();

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
			const UProgressSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>() : nullptr;
			if (!Saves || !Saves->IsRestoringGimmicks()) KillPlayerImmediately(Player);
			break;
		}

		// 수신기 입구에 맞았을 때만 접촉 등록
		if (ALaserReceiver* Receiver = Cast<ALaserReceiver>(Hit.GetActor()))
		{
			if (Receiver->CanReceiveLaserHit(Hit))
			{
				CurrentReceivers.AddUnique(TWeakObjectPtr<ALaserReceiver>(Receiver));
			}

			// 몸체에 맞은 경우에도 레이저는 여기서 멈춤
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
	UNiagaraSystem* EffectTemplate = LaserConfig.LaserEffect.Get();

	// 효과와 발사 기준점이 유효할 때 필요한 빔 개수
	const int32 RequiredCount = IsValid(EffectTemplate) && IsValid(LaserOrigin) ? LaserSegments.Num() : 0;

	// 구간이 늘어나면 컴포넌트를 보관할 공간 확보
	if (LaserEffectComponents.Num() < RequiredCount)
	{
		LaserEffectComponents.SetNum(RequiredCount);
	}

	// Niagara 에셋의 실제 파라미터 이름과 일치해야 함
	static const FName BeamStartParameter(TEXT("User.Beam Start"));

	static const FName BeamEndParameter(TEXT("User.Beam End"));

	for (int32 Index = 0; Index < LaserEffectComponents.Num(); ++Index)
	{
		UNiagaraComponent* EffectComponent = LaserEffectComponents[Index].Get();

		// 현재 경로에 필요 없는 빔은 끄고 다음 사용을 위해 보관
		if (Index >= RequiredCount)
		{
			if (IsValid(EffectComponent) && EffectComponent->IsActive())
			{
				EffectComponent->DeactivateImmediate();
			}

			continue;
		}

		const FLaserSegment& Segment = LaserSegments[Index];

		// 처음 필요한 구간에만 컴포넌트 생성
		if (!IsValid(EffectComponent))
		{
			EffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
					EffectTemplate,
					LaserOrigin,
					NAME_None,
					FVector::ZeroVector,
					FRotator::ZeroRotator,
					EAttachLocation::KeepRelativeOffset,
					false, // 종료되어도 자동 삭제하지 않음
					false, // 위치를 전달한 뒤 직접 재생
					ENCPoolMethod::None,
					false  // 생성 전 컬링 검사 생략
				);

			LaserEffectComponents[Index] = EffectComponent;

			if (!IsValid(EffectComponent))
			{
				continue;
			}
		}
		else if (EffectComponent->GetAsset() != EffectTemplate)
		{
			// 설정을 다시 읽어 효과가 바뀐 경우 반영
			EffectComponent->DeactivateImmediate();
			EffectComponent->SetAsset(EffectTemplate);
		}

		// 컴포넌트의 기준 위치를 현재 구간 시작점에 맞춤
		EffectComponent->SetWorldLocation(Segment.StartPoint);

		// C++에서 계산한 월드 좌표 전달
		EffectComponent->SetVariablePosition(BeamStartParameter, Segment.StartPoint);

		EffectComponent->SetVariablePosition(BeamEndParameter, Segment.EndPoint);

		// 새로 만들었거나 꺼져 있던 빔만 재생
		if (!EffectComponent->IsActive())
		{
			EffectComponent->Activate(true);
		}
	}
}

bool ALaserEmitter::TraceLaserSegment(const FVector& Start, const FVector& Direction, float Distance, FHitResult& OutHit) const
{
	OutHit = FHitResult();

	UWorld* World = GetWorld();

	if (!World || Distance <= 0.0f || Direction.IsNearlyZero())
	{
		return false;
	}

	const FVector TraceDirection = Direction.GetSafeNormal();
	const FVector End = Start + TraceDirection * Distance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LaserTrace), true, this);

	TArray<APlayerBase*> Players;

	for (TActorIterator<APlayerBase> It(World); It; ++It)
	{
		APlayerBase* Player = *It;

		if (!IsValid(Player))
		{
			continue;
		}

		// 기존 이동용 캡슐과 플레이어 메시를 경로 검사에서 제외
		QueryParams.AddIgnoredActor(Player);

		if (!Player->IsDead())
		{
			Players.Add(Player);
		}
	}

	// 플레이어를 제외하고 벽, 반사판, 수신기 등을 검사
	const bool bWorldHit = World->LineTraceSingleByChannel(OutHit, Start, End, LaserTraceChannel.GetValue(), QueryParams);

	// 장애물 뒤의 플레이어는 검사하지 않음
	const FVector PlayerCheckEnd = bWorldHit ? OutHit.ImpactPoint : End;

	FHitResult PlayerHit;

	if (FindPlayerHitOnSegment(Start, PlayerCheckEnd, Players, PlayerHit))
	{
		// 장애물보다 앞에서 플레이어에 닿았을 때 결과 교체
		if (!bWorldHit || PlayerHit.Distance < OutHit.Distance)
		{
			OutHit = PlayerHit;
			return true;
		}
	}

	return bWorldHit;
}

bool ALaserEmitter::FindPlayerHitOnSegment(const FVector& Start, const FVector& End, const TArray<APlayerBase*>& Players, FHitResult& OutHit) const
{
	OutHit = FHitResult();

	const double SegmentLength = FVector::Distance(Start, End);

	if (SegmentLength <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector Direction = (End - Start) / SegmentLength;

	double NearestHitDistance = SegmentLength;
	bool bFoundHit = false;

	for (APlayerBase* Player : Players)
	{
		if (!IsValid(Player) || Player->IsDead())
		{
			continue;
		}

		const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();

		if (!IsValid(Capsule))
		{
			continue;
		}

		float OriginalRadius = 0.0f;
		float OriginalHalfHeight = 0.0f;

		Capsule->GetScaledCapsuleSize(OriginalRadius, OriginalHalfHeight);

		// 실제 이동용 캡슐은 변경하지 않고 판정 크기만 계산
		const double HitHalfHeight = OriginalHalfHeight * LaserConfig.PlayerHitHeightScale;

		const double HitRadius = FMath::Min(static_cast<double>(OriginalRadius * LaserConfig.PlayerHitRadiusScale), HitHalfHeight);

		if (HitRadius <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector Center = Capsule->GetComponentLocation();

		// 중력 반전 시에도 현재 캡슐 방향을 따라감
		const FVector CapsuleUp = Capsule->GetUpVector();

		// 캡슐 양 끝의 반구 중심을 연결하는 선분
		const double AxisHalfLength = FMath::Max(0.0, HitHalfHeight - HitRadius);

		const FVector AxisStart = Center - CapsuleUp * AxisHalfLength;

		const FVector AxisEnd = Center + CapsuleUp * AxisHalfLength;

		FVector ClosestOnLaser;
		FVector ClosestOnAxis;

		FMath::SegmentDistToSegmentSafe(Start, End, AxisStart, AxisEnd, ClosestOnLaser, ClosestOnAxis);

		const double RadiusSquared = HitRadius * HitRadius;

		// 레이저와 캡슐 중심선의 거리가 반지름보다 멀면 미접촉
		if (FVector::DistSquared(ClosestOnLaser, ClosestOnAxis) > RadiusSquared)
		{
			continue;
		}

		// 레이저가 작은 캡슐 안으로 처음 들어오는 위치 탐색
		double OutsideDistance = 0.0;
		double InsideDistance = FVector::Distance(Start, ClosestOnLaser);

		for (int32 Step = 0; Step < 20; ++Step)
		{
			const double MiddleDistance = (OutsideDistance + InsideDistance) * 0.5;

			const FVector MiddlePoint = Start + Direction * MiddleDistance;

			const double DistanceSquared = FMath::PointDistToSegmentSquared(MiddlePoint, AxisStart, AxisEnd);

			if (DistanceSquared <= RadiusSquared)
			{
				InsideDistance = MiddleDistance;
			}
			else
			{
				OutsideDistance = MiddleDistance;
			}
		}

		if (bFoundHit && InsideDistance >= NearestHitDistance)
		{
			continue;
		}

		NearestHitDistance = InsideDistance;

		const FVector HitLocation = Start + Direction * InsideDistance;

		// 기존 UpdateLaserPath가 플레이어 충돌로 처리하도록 전달
		OutHit = FHitResult(Player, Player->GetCapsuleComponent(), HitLocation, -Direction);

		OutHit.bBlockingHit = true;
		OutHit.Distance = static_cast<float>(InsideDistance);
		OutHit.Time = static_cast<float>(InsideDistance / SegmentLength);
		OutHit.TraceStart = Start;
		OutHit.TraceEnd = End;

		bFoundHit = true;
	}

	return bFoundHit;
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

void ALaserEmitter::UpdateEmitterIndicator()
{
	if (!IsValid(EmitterMesh))
	{
		return;
	}

	const bool bLightOn = bIsEnabled && bIsActive && bHasValidLaserConfig;

	// 켜지면 빨강, 꺼지면 검정
	const FLinearColor LightColor = bLightOn ? FLinearColor::Red : FLinearColor::Black;

	// 표시등 재질 갱신
	if (IsValid(EmitterIndicatorMaterial))
	{
		EmitterIndicatorMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), LightColor);
	}

	// 현재 발신기 에셋의 렌즈 재질은 1번 슬롯
	UMaterialInstanceDynamic* LensMaterial = Cast<UMaterialInstanceDynamic>(EmitterMesh->GetMaterial(1));

	if (IsValid(LensMaterial))
	{
		LensMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), LightColor);
	}
}

void ALaserEmitter::FinishRestoreProgress(const FProgressActorState& State)
{
    if (bIsEnabled && bIsActive)
    {
        UpdateLaserPath();
        UpdateReceiverContacts();
        UpdateLaserVisuals();
    }
    Super::FinishRestoreProgress(State);
}
