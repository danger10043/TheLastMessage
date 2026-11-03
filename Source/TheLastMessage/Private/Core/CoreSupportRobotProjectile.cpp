#include "Core/CoreSupportRobotProjectile.h"

#include "Character/CombatDamageLibrary.h"
#include "Components/SphereComponent.h"
#include "Core/CoreSupportRobot.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "GAS/WeaponGameplayTags.h"

ACoreSupportRobotProjectile::ACoreSupportRobotProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	// ---- 충돌 ----
	// 이동하며 Sweep하는 구다. 지형(WorldStatic·WorldDynamic 오브젝트)은 막혀서 Hit,
	// 캐릭터(Pawn)는 겹침(Overlap)으로 받아 적만 골라 맞힌다. 그 밖(트리거·통과 발판·적 메시 등)은 무시한다.
	// 트리거 영역은 WorldDynamic이지만 이 채널을 막지 않으므로(겹침/무시) 막는 쪽과의 약한 반응이 적용되어 통과한다.
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(10.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	SetRootComponent(Collision);

	// ---- 이동 ----
	// 중력 없이 직선으로 날아간다. 막히면 튕기지 않고 멈추며, 그때 Hit 이벤트로 사라진다.
	// 초기 속도는 월드 방향으로 준다. 기본값(로컬 방향)이면 생성 회전이 한 번 더 적용되어 각도가 두 배로 꺾인다.
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bShouldBounce = false;
	Movement->bRotationFollowsVelocity = true;
}

void ACoreSupportRobotProjectile::InitializeShot(ACoreSupportRobot* InRobot, const FVector& Direction, float Speed,
	float Radius, float InDamage, TSubclassOf<UGameplayEffect> InDamageEffect, float MaxDistance)
{
	Robot = InRobot;
	Damage = InDamage;
	DamageEffect = InDamageEffect;
	Collision->SetSphereRadius(FMath::Max(1.0f, Radius));

	// 생성이 끝나기 전에 속도를 정해 두면 이동 컴포넌트가 첫 프레임부터 이 방향·속도로 움직인다.
	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;
	Movement->Velocity = Direction.GetSafeNormal() * Speed;
	// 사거리만큼 날아가는 시간이 지나면 빗나간 탄으로 보고 지운다.
	SetLifeSpan(Speed > 0.0f ? FMath::Max(0.05f, MaxDistance / Speed) : 0.05f);

	Collision->OnComponentBeginOverlap.AddUniqueDynamic(this, &ACoreSupportRobotProjectile::HandleOverlap);
	Collision->OnComponentHit.AddUniqueDynamic(this, &ACoreSupportRobotProjectile::HandleHit);
}

void ACoreSupportRobotProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 겹친 캐릭터 중 살아 있는 적만 맞힌다. 플레이어나 죽은 적은 그대로 통과한다.
	if (!ACoreSupportRobot::IsAliveEnemy(OtherActor)) return;
	const FVector ImpactLocation = bFromSweep ? FVector(SweepResult.ImpactPoint) : GetActorLocation();
	Impact(ImpactLocation, OtherActor);
}

void ACoreSupportRobotProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	// 막는 것(지형·문·기믹 메시)에 닿으면 피해 없이 사라진다.
	Impact(Hit.ImpactPoint, OtherActor);
}

void ACoreSupportRobotProjectile::Impact(const FVector& ImpactLocation, AActor* HitActor)
{
	// 같은 프레임에 겹침과 막힘이 함께 와도 한 번만 처리한다.
	if (bImpacted) return;
	bImpacted = true;

	// 공격자를 로봇으로 넘긴다. 로봇이 플레이어 ASC를 돌려주므로 피해 효과는 플레이어 ASC로 만들어지지만,
	// 피해 알림(DamageDealt)의 공격자는 로봇이라 플레이어 전용 반응이 걸러지고, 로봇엔 피드백 컴포넌트가 없어 히트스톱도 없다.
	// 로봇이 이미 사라졌으면(플레이어 사망 등) 피해를 주지 않는다.
	ACoreSupportRobot* Source = Robot.Get();
	if (IsValid(Source) && DamageEffect && ACoreSupportRobot::IsAliveEnemy(HitActor))
	{
		UCombatDamageLibrary::ApplyFinalDamageToTarget(Source, HitActor, DamageEffect, Damage,
			WeaponGameplayTags::Damage_PlayerRanged, 0.0f);
	}

	ReceiveImpact(ImpactLocation, HitActor);
	Destroy();
}
