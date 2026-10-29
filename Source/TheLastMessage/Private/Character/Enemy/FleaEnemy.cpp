#include "Character/Enemy/FleaEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "Interface/AttackReceiverInterface.h"

AFleaEnemy::AFleaEnemy()
{
	EnemyType = EEnemyType::Flea;

	// 기본값(임시). 블루프린트에서 조정한다. 다른 몬스터보다 체력을 높게 둔다.
	StatConfig.MaxHealth = 250.f;
	StatConfig.MoveSpeed = 220.f;
	StatConfig.AttackRange = 180.f;
	StatConfig.ChaseAcceptableRadius = 120.f;
	StatConfig.AttackDamage = 15.f;
	StatConfig.AttackCooldown = 2.f;
}

void AFleaEnemy::ApplyEnemyData(const UEnemyDataAsset* Data)
{
	Super::ApplyEnemyData(Data);
	if (!Data)
	{
		return;
	}

	const FFleaSettings& S = Data->Flea;
	AreaAttackRadius = S.AreaAttackRadius;
	ChargeEffectOffset = S.ChargeEffectOffset;
	bScaleChargeEffectToAttackRadius = S.bScaleChargeEffectToAttackRadius;
	ShockwaveEffectBaseRadius = S.ShockwaveEffectBaseRadius;
	AreaAttackWindup = S.AreaAttackWindup;
}

void AFleaEnemy::StartAttack()
{
	if (IsDead() || IsAttacking() || !GetCurrentTarget())
	{
		return;
	}

	StartAreaAttack();
}

void AFleaEnemy::StartAreaAttack()
{
	bIsAttacking = true;
	FaceTarget();
	PlayAttackMontage();

	// TODO: 공격 몽타주 재생 후 애님 노티파이 시점에 ExecuteAreaAttack 호출로 교체
	GetWorldTimerManager().SetTimer(AreaAttackTimerHandle, this, &AFleaEnemy::ExecuteAreaAttack, AreaAttackWindup, false);

	// 윈드업(차지) 동안 충전 이펙트를 재생한다.
	if (ChargeEffect)
	{
		USceneComponent* AttachTo = GetRootComponent();
		FName AttachSocket = NAME_None;
		FVector AttachOffset = ChargeEffectOffset;
		EAttachLocation::Type AttachType = EAttachLocation::KeepRelativeOffset;

		if (!ChargeEffectSocketName.IsNone() && GetMesh() && GetMesh()->DoesSocketExist(ChargeEffectSocketName))
		{
			AttachTo = GetMesh();
			AttachSocket = ChargeEffectSocketName;
			AttachOffset = FVector::ZeroVector;
			AttachType = EAttachLocation::SnapToTarget;
		}

		ChargeEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(ChargeEffect, AttachTo, AttachSocket,
			AttachOffset, FRotator::ZeroRotator, AttachType, true);

		if (ChargeEffectComponent && bScaleChargeEffectToAttackRadius && ShockwaveEffectBaseRadius > 0.f)
		{
			ChargeEffectComponent->SetWorldScale3D(FVector(AreaAttackRadius / ShockwaveEffectBaseRadius));
		}
	}
}

void AFleaEnemy::ExecuteAreaAttack()
{
	StopChargeEffect();

	if (IsDead() || !IsAttacking())
	{
		return;
	}

	// 플레이어와 일반 기믹의 충돌 타입을 함께 검색
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);

	TArray<AActor*> Overlapped;

	// 폭탄도 찾을 수 있도록 클래스 필터를 일반 액터로 확장
	UKismetSystemLibrary::SphereOverlapActors(
		this,
		GetActorLocation(),
		AreaAttackRadius,
		ObjectTypes,
		AActor::StaticClass(),
		ActorsToIgnore,
		Overlapped);

	// 한 번의 공격에서 같은 대상은 한 번만 처리
	TSet<AActor*> DamagedActors;

	for (AActor* Actor : Overlapped)
	{
		if (!IsValid(Actor) || DamagedActors.Contains(Actor))
		{
			continue;
		}

		const APawn* Pawn = Cast<APawn>(Actor);

		const bool bIsPlayer =
			Pawn && Pawn->IsPlayerControlled();

		const bool bIsAttackReceiver =
			Actor->Implements<UAttackReceiverInterface>();

		// 플레이어 또는 공격을 받는 기믹만 허용
		if (!bIsPlayer && !bIsAttackReceiver)
		{
			continue;
		}

		// 공격 전달 전에 중복 명중 기록
		DamagedActors.Add(Actor);

		ApplyEnemyDamage(
			Actor,
			GetScaledDamage(GetStatConfig().AttackDamage));

		// 폭탄의 폭발로 사망하거나 공격이 중단되면 종료
		if (!IsValid(this) || IsDead() || !IsAttacking())
		{
			return;
		}
	}

	SpawnShockwaveEffect();
	FinishAttack();
}

void AFleaEnemy::SpawnShockwaveEffect()
{
	const float EffectScale = ShockwaveEffectBaseRadius > 0.f ? AreaAttackRadius / ShockwaveEffectBaseRadius : 1.f;
	const FVector FeetLocation = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	SpawnEffectAtLocation(ShockwaveEffect, FeetLocation, FRotator::ZeroRotator, FVector(EffectScale));
}

void AFleaEnemy::StopChargeEffect()
{
	if (ChargeEffectComponent)
	{
		ChargeEffectComponent->Deactivate();
		ChargeEffectComponent = nullptr;
	}
}

void AFleaEnemy::CancelAttack()
{
	GetWorldTimerManager().ClearTimer(AreaAttackTimerHandle);
	StopChargeEffect();
	Super::CancelAttack();
}
