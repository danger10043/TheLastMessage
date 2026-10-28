#include "Character/Enemy/Drop/EnemyHealthPickup.h"
#include "AbilitySystemComponent.h"
#include "Character/Player/PlayerBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GAS/CombatAttributeSet.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

AEnemyHealthPickup::AEnemyHealthPickup()
{
	// 대기 연출과 흡수 이동을 Tick에서 처리한다.
	PrimaryActorTick.bCanEverTick = true;

	// 서버가 이동·회복을 처리하고, 위치는 클라이언트에 복제한다.
	bReplicates = true;
	SetReplicateMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 메시는 외형만 표시한다. 충돌은 쓰지 않고 거리로 흡수 범위를 판단한다.
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(Root);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetGenerateOverlapEvents(false);
	PickupMesh->SetCanEverAffectNavigation(false);
	PickupMesh->SetCastShadow(false);
}

void AEnemyHealthPickup::InitPickup(float InHealAmount)
{
	HealAmount = FMath::Max(InHealAmount, 0.f);
}

void AEnemyHealthPickup::BeginPlay()
{
	Super::BeginPlay();

	BaseMeshScale = PickupMesh->GetRelativeScale3D();

	if (HasAuthority() && LifeSeconds > 0.f)
	{
		SetLifeSpan(LifeSeconds);
	}
}

void AEnemyHealthPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateIdle(DeltaSeconds);

	if (!HasAuthority() || bCollected)
	{
		return;
	}

	if (bAbsorbing)
	{
		UpdateAbsorb(DeltaSeconds);
		return;
	}

	// 흡수 범위 확인은 매 프레임 할 필요가 없다.
	DetectTimer += DeltaSeconds;
	if (DetectTimer < 0.1f)
	{
		return;
	}
	DetectTimer = 0.f;

	if (APlayerBase* Player = FindAttractTarget())
	{
		StartAbsorb(Player);
	}
}

void AEnemyHealthPickup::UpdateIdle(float DeltaSeconds)
{
	IdleTime += DeltaSeconds;

	// 흡수 중에는 위아래로 흔들리지 않고 곧장 날아간다.
	const float Bob = bAbsorbing ? 0.f : FMath::Sin(IdleTime * BobSpeed * UE_TWO_PI) * BobHeight;
	PickupMesh->SetRelativeLocation(FVector(0.f, 0.f, Bob));
	PickupMesh->AddRelativeRotation(FRotator(0.f, SpinSpeed * DeltaSeconds, 0.f));
}

APlayerBase* AEnemyHealthPickup::FindAttractTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	APlayerBase* Best = nullptr;
	float BestDistSq = FMath::Square(AttractRadius);

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerBase* Player = Cast<APlayerBase>(It->IsValid() ? It->Get()->GetPawn() : nullptr);
		if (!Player || Player->IsDead())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Player->GetActorLocation(), GetActorLocation());
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Player;
		}
	}

	return Best;
}

void AEnemyHealthPickup::StartAbsorb(APlayerBase* Player)
{
	bAbsorbing = true;
	AbsorbElapsed = 0.f;
	AbsorbTarget = Player;
	BP_OnAbsorbStarted();
}

void AEnemyHealthPickup::UpdateAbsorb(float DeltaSeconds)
{
	APlayerBase* Player = AbsorbTarget.Get();
	if (!Player || Player->IsDead())
	{
		// 흡수하던 플레이어가 죽거나 사라졌다. 그 자리에서 다시 대기한다.
		bAbsorbing = false;
		AbsorbTarget.Reset();
		PickupMesh->SetRelativeScale3D(BaseMeshScale);
		return;
	}

	AbsorbElapsed += DeltaSeconds;

	// 시작 속도에서 최고 속도로 점점 빨라진다(쇼로록 빨려 들어가는 느낌).
	const float Alpha = FMath::Clamp(AbsorbElapsed / FMath::Max(AbsorbAccelTime, 0.01f), 0.f, 1.f);
	const float Speed = FMath::Lerp(AbsorbStartSpeed, AbsorbMaxSpeed, Alpha * Alpha);
	PickupMesh->SetRelativeScale3D(BaseMeshScale * FMath::Lerp(1.f, AbsorbEndScale, Alpha));

	// 사이드뷰라 Y(깊이)는 움직이지 않는다.
	FVector ToTarget = Player->GetActorLocation() + AbsorbTargetOffset - GetActorLocation();
	ToTarget.Y = 0.f;
	const float Distance = ToTarget.Size();
	const float Step = Speed * DeltaSeconds;

	if (Distance <= FMath::Max(ArriveDistance, Step))
	{
		Collect(Player);
		return;
	}

	SetActorLocation(GetActorLocation() + ToTarget / Distance * Step);
}

void AEnemyHealthPickup::Collect(APlayerBase* Player)
{
	if (bCollected)
	{
		return;
	}
	bCollected = true;

	// 체력 속성에 직접 더한다. 최대 체력을 넘지 않게 속성 쪽에서 제한한다.
	if (Player && HealAmount > 0.f)
	{
		if (UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent())
		{
			ASC->ApplyModToAttribute(UCombatAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, HealAmount);
		}
	}

	if (Player && GetWorld())
	{
		if (CollectSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, CollectSound, Player->GetActorLocation());
		}
		if (CollectEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), CollectEffect, Player->GetActorLocation());
		}
	}

	BP_OnCollected(Player);
	Destroy();
}
