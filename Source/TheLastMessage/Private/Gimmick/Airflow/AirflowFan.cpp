// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Airflow/AirflowFan.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Math/RotationMatrix.h"
#include "NiagaraComponent.h"
#include "Character/Player/PlayerBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AAirflowFan::AAirflowFan()
{
	// 이후 플레이어가 기류 안에 있을 때만 Tick 사용
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	PrimaryActorTick.TickGroup = TG_PostPhysics;

	// 환풍기는 연결된 장치의 활성 신호로 작동
	bSupportsInteraction = false;

	FanMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FanMesh"));

	FanMesh->SetupAttachment(SceneRoot);
	FanMesh->SetMobility(EComponentMobility::Movable);
	FanMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	FanMesh->SetGenerateOverlapEvents(false);

	AirflowArea = CreateDefaultSubobject<UBoxComponent>(TEXT("AirflowArea"));

	AirflowArea->SetupAttachment(SceneRoot);
	AirflowArea->SetMobility(EComponentMobility::Movable);

	// 기본 범위: 너비 200, 깊이 200, 길이 600
	AirflowArea->SetBoxExtent(FVector(100.0f, 100.0f, 300.0f));

	AirflowArea->SetRelativeLocation(FVector(0.0f, 0.0f, 300.0f));

	// 플레이어를 막지 않고 진입만 감지
	AirflowArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	AirflowArea->SetCollisionObjectType(ECC_WorldDynamic);
	AirflowArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	AirflowArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	AirflowArea->SetGenerateOverlapEvents(true);
	AirflowArea->SetCanEverAffectNavigation(false);

	AirflowDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("AirflowDirection"));

	AirflowDirection->SetupAttachment(SceneRoot);
	AirflowDirection->SetMobility(EComponentMobility::Movable);

	// 화살표의 앞쪽을 기본 바람 방향인 위쪽으로 배치
	AirflowDirection->SetRelativeRotation(FVector::UpVector.Rotation());

	AirflowEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("AirflowEffect"));

	AirflowEffect->SetupAttachment(SceneRoot);
	AirflowEffect->SetMobility(EComponentMobility::Movable);

	// 활성 상태 연결은 다음 단계에서 처리
	AirflowEffect->SetAutoActivate(false);
}

bool AAirflowFan::LoadAirflowConfig()
{
	bAirflowConfigLoaded = false;

	if (!AirflowConfigRow.DataTable || AirflowConfigRow.RowName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: AirflowConfigRow의 테이블과 행을 지정해주세요."), *GetName());

		return false;
	}

	const FAirflowConfigRow* LoadedRow = AirflowConfigRow.GetRow<FAirflowConfigRow>(TEXT("AAirflowFan::LoadAirflowConfig"));

	if (!LoadedRow)
	{
		return false;
	}

	if (LoadedRow->LocalWindDirection.ContainsNaN() || LoadedRow->LocalWindDirection.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 바람 방향은 유효한 0이 아닌 벡터여야 합니다."), *GetName());

		return false;
	}

	if (!FMath::IsFinite(LoadedRow->WindAcceleration)
		|| LoadedRow->WindAcceleration < 0.0f
		|| !FMath::IsFinite(LoadedRow->MaxRiseSpeed)
		|| LoadedRow->MaxRiseSpeed < 0.0f
		|| !FMath::IsFinite(LoadedRow->MaxFallSpeed)
		|| LoadedRow->MaxFallSpeed < 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 바람 가속도와 속도 제한은 0 이상의 유한한 값이어야 합니다."), *GetName());

		return false;
	}

	AirflowConfig = *LoadedRow;

	// 방향 벡터의 크기는 바람 세기에 영향을 주지 않도록 정규화
	AirflowConfig.LocalWindDirection = AirflowConfig.LocalWindDirection.GetSafeNormal();

	const FVector LocalDirection = AirflowConfig.LocalWindDirection;

	// 화살표는 로컬 X축이 앞쪽
	AirflowDirection->SetRelativeRotation(LocalDirection.Rotation());

	// 기류 박스와 효과는 로컬 Z축이 바람 방향
	const FRotator AirflowRotation =
		FRotationMatrix::MakeFromZ(LocalDirection).Rotator();

	AirflowArea->SetRelativeRotation(AirflowRotation);

	// 박스의 시작 부분이 환풍기 원점에 놓이도록 배치
	const float HalfLength =
		AirflowArea->GetUnscaledBoxExtent().Z;

	AirflowArea->SetRelativeLocation(LocalDirection * HalfLength);

	AirflowEffect->SetRelativeRotation(AirflowRotation);
	AirflowEffect->SetAsset(AirflowConfig.AirflowEffect.Get());

	bAirflowConfigLoaded = true;

	return true;
}

void AAirflowFan::ApplyActiveState(bool bNewActive)
{
	Super::ApplyActiveState(bNewActive && bIsEnabled);

	RefreshAirflowState();
}

void AAirflowFan::BeginPlay()
{
	Super::BeginPlay();

	// 플레이어 진입·이탈 이벤트 연결
	AirflowArea->OnComponentBeginOverlap.AddUniqueDynamic(this, &AAirflowFan::OnAirflowBeginOverlap);

	AirflowArea->OnComponentEndOverlap.AddUniqueDynamic(this, &AAirflowFan::OnAirflowEndOverlap);

	if (!LoadAirflowConfig())
	{
		ApplyActiveState(false);
		return;
	}

	// 시작 설정에 따라 환풍기 활성화
	ApplyActiveState(bIsActive || bStartActive);
}

void AAirflowFan::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority() || !bIsEnabled || !bIsActive || !bAirflowConfigLoaded || !IsValid(AirflowArea))
	{
		SetActorTickEnabled(false);
		return;
	}

	// 파괴되었거나 실제 범위에서 벗어난 플레이어 정리
	AffectedPlayers.RemoveAll([this](const TWeakObjectPtr<APlayerBase>& Entry)
		{
			APlayerBase* Player = Entry.Get();

			return !IsValid(Player) || !IsValid(Player->GetCapsuleComponent()) || !AirflowArea->IsOverlappingComponent(Player->GetCapsuleComponent());
		});

	// 환풍기의 회전으로 로컬 바람 방향을 월드 방향으로 변환
	const FVector WorldWindDirection = GetActorTransform().TransformVectorNoScale(AirflowConfig.LocalWindDirection).GetSafeNormal();

	for (const TWeakObjectPtr<APlayerBase>& Entry : AffectedPlayers)
	{
		APlayerBase* Player = Entry.Get();

		if (!IsValid(Player) || Player->IsDead())
		{
			continue;
		}

		UCharacterMovementComponent* Movement = Player->GetCharacterMovement();

		if (!IsValid(Movement) || !Movement->IsActive() || Movement->MovementMode == MOVE_None)
		{
			continue;
		}

		// 현재 중력의 반대 방향을 상승 방향으로 사용
		const FVector UpDirection = -Movement->GetGravityDirection().GetSafeNormal();

		if (!UpDirection.IsNearlyZero())
		{
			const double VerticalSpeed = FVector::DotProduct(Movement->Velocity, UpDirection);

			const double LimitedSpeed = FMath::Clamp(VerticalSpeed, -static_cast<double>(AirflowConfig.MaxFallSpeed), static_cast<double>(AirflowConfig.MaxRiseSpeed));

			// 좌우 이동 속도는 유지하고 상승·낙하 성분만 제한
			Movement->Velocity += UpDirection * (LimitedSpeed - VerticalSpeed);
		}

		// 게임의 이동 평면 밖으로 향하는 힘은 제외
		const FVector WindAcceleration = Movement->ConstrainDirectionToPlane(WorldWindDirection) * AirflowConfig.WindAcceleration;

		if (Movement->Mass > KINDA_SMALL_NUMBER)
		{
			// 질량과 관계없이 설정한 가속도로 작동하도록 보정
			// 시간 간격 계산은 CharacterMovement 내부에서 처리
			Movement->AddForce(WindAcceleration * Movement->Mass);
		}
	}

	if (AffectedPlayers.IsEmpty())
	{
		SetActorTickEnabled(false);
	}
}

void AAirflowFan::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetActorTickEnabled(false);

	AffectedPlayers.Reset();

	if (IsValid(AirflowArea))
	{
		AirflowArea->OnComponentBeginOverlap.RemoveDynamic(this, &AAirflowFan::OnAirflowBeginOverlap);

		AirflowArea->OnComponentEndOverlap.RemoveDynamic(this, &AAirflowFan::OnAirflowEndOverlap);
	}

	if (IsValid(AirflowEffect))
	{
		AirflowEffect->DeactivateImmediate();
	}

	Super::EndPlay(EndPlayReason);
}

void AAirflowFan::RefreshAirflowState()
{
	const bool bShouldRun = bIsEnabled && bIsActive && bAirflowConfigLoaded && IsValid(AirflowArea);

	// 나이아가라가 없어도 기류는 작동
	if (IsValid(AirflowEffect))
	{
		if (bShouldRun && IsValid(AirflowEffect->GetAsset()))
		{
			if (!AirflowEffect->IsActive())
			{
				AirflowEffect->Activate(true);
			}
		}
		else
		{
			AirflowEffect->DeactivateImmediate();
		}
	}

	AffectedPlayers.Reset();

	if (bShouldRun && HasAuthority())
	{
		// 켜지는 순간 이미 범위 안에 있는 플레이어도 확인
		AirflowArea->UpdateOverlaps();

		TArray<AActor*> OverlappingActors;

		AirflowArea->GetOverlappingActors(OverlappingActors, APlayerBase::StaticClass());

		for (AActor* Actor : OverlappingActors)
		{
			APlayerBase* Player = Cast<APlayerBase>(Actor);

			if (!IsValid(Player) || !IsValid(Player->GetCapsuleComponent()))
			{
				continue;
			}

			if (!AirflowArea->IsOverlappingComponent(Player->GetCapsuleComponent()))
			{
				continue;
			}

			AffectedPlayers.AddUnique(TWeakObjectPtr<APlayerBase>(Player));
		}
	}

	// 활성화된 기류 안에 플레이어가 있을 때만 Tick 사용
	SetActorTickEnabled(bShouldRun && HasAuthority() && !AffectedPlayers.IsEmpty());
}

void AAirflowFan::OnAirflowBeginOverlap(UPrimitiveComponent * OverlappedComponent, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
	if (!HasAuthority() || !bIsEnabled || !bIsActive || !bAirflowConfigLoaded)
	{
		return;
	}

	APlayerBase* Player = Cast<APlayerBase>(OtherActor);

	if (!IsValid(Player) || OtherComp != Player->GetCapsuleComponent())
	{
		return;
	}

	// 메시 등 다른 컴포넌트의 중복 진입은 제외
	AffectedPlayers.AddUnique(TWeakObjectPtr<APlayerBase>(Player));

	SetActorTickEnabled(true);
}

void AAirflowFan::OnAirflowEndOverlap(UPrimitiveComponent * OverlappedComponent, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex)
{
	APlayerBase* Player = Cast<APlayerBase>(OtherActor);

	if (!IsValid(Player)
		|| OtherComp != Player->GetCapsuleComponent())
	{
		return;
	}

	AffectedPlayers.RemoveAll([Player](const TWeakObjectPtr<APlayerBase>& Entry)
		{
			return !Entry.IsValid() || Entry.Get() == Player;
		});

	if (AffectedPlayers.IsEmpty())
	{
		SetActorTickEnabled(false);
	}
}
