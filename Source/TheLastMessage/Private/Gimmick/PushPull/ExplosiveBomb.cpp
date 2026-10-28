// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/PushPull/ExplosiveBomb.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Gimmick/Component/PushPullComponent.h"
#include "Gimmick/DestructibleObstacle/BombDestructibleObstacle.h"
#include "Gimmick/RespawnPoint/BombRespawnPoint.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

bool AExplosiveBomb::LoadBombConfig()
{
    bHasValidBombConfig = false;

    if (!BombConfigRow.DataTable || BombConfigRow.RowName.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: BombConfigRow의 테이블과 행을 지정해주세요."), *GetName());

        return false;
    }

    const FBombConfigRow* LoadedRow = BombConfigRow.GetRow<FBombConfigRow>(TEXT("AExplosiveBomb::LoadBombConfig"));

    if (!LoadedRow)
    {
        return false;
    }

    if (!FMath::IsFinite(LoadedRow->ExplosionRadius)
        || LoadedRow->ExplosionRadius <= 0.0f
        || !FMath::IsFinite(LoadedRow->ExplosionDamage)
        || LoadedRow->ExplosionDamage < 0.0f
        || !FMath::IsFinite(LoadedRow->RespawnDelay)
        || LoadedRow->RespawnDelay < 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 폭발 반경은 양수, 피해량과 재생성 지연은 0 이상의 유효한 값이어야 합니다."), *GetName());

        return false;
    }

    // 캐릭터 피해를 사용하는 경우 피해 효과 필수
    if (LoadedRow->ExplosionDamage > 0.0f && !LoadedRow->DamageEffectClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 폭발 피해를 적용할 DamageEffectClass를 지정해주세요."), *GetName());

        return false;
    }

    if (LoadedRow->DamageEffectClass)
    {
        const UGameplayEffect* EffectDefault = LoadedRow->DamageEffectClass.GetDefaultObject();

        if (!IsValid(EffectDefault) || EffectDefault->DurationPolicy != EGameplayEffectDurationType::Instant)
        {
            UE_LOG(LogTemp, Warning, TEXT("%s: 폭발 피해 Gameplay Effect는 Instant여야 합니다."), *GetName());

            return false;
        }
    }

    BombConfig = *LoadedRow;
    bHasValidBombConfig = true;

    return true;
}

bool AExplosiveBomb::CanBeMovedBy(ACharacter* Player) const
{
    return !bHasExploded && bHasValidBombConfig && Super::CanBeMovedBy(Player);
}

void AExplosiveBomb::ReceiveAttackDamage(float DamageAmount, AActor* DamageCauser)
{
    if (!HasAuthority()
        || !bIsEnabled
        || !bHasValidBombConfig
        || bHasExploded
        || !FMath::IsFinite(DamageAmount)
        || DamageAmount <= 0.0f
        || !IsValid(DamageCauser)
        || DamageCauser == this)
    {
        return;
    }

    Explode();
}

void AExplosiveBomb::Explode()
{
    UWorld* World = GetWorld();

    if (!HasAuthority() || !World || !bIsEnabled || !bHasValidBombConfig || bHasExploded)
    {
        return;
    }

    // 피해 처리 중 다시 호출되더라도 중복 폭발 방지
    bHasExploded = true;

    const FVector ExplosionLocation = GetActorLocation();

    // 조작 중이었다면 물리 핸들과 플레이어 상태부터 정리
    if (ACharacter* Player = GetControllingPlayer())
    {
        UPushPullComponent* PushPullComponent = Player->FindComponentByClass<UPushPullComponent>();

        if (IsValid(PushPullComponent) && PushPullComponent->GetCurrentTarget() == this)
        {
            PushPullComponent->StopPushPull();
        }
    }

    EndPushPull();
    SetGimmickEnabled(false);

    // 폭발한 폭탄의 물리와 충돌 중지
    if (IsValid(PhysicsMesh))
    {
        PhysicsMesh->SetSimulatePhysics(false);
    }

    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);

    // 구형 범위 안의 충돌 대상을 수집
    FCollisionObjectQueryParams ObjectParams(FCollisionObjectQueryParams::AllObjects);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BombExplosion), false, this);

    TArray<FOverlapResult> Overlaps;

    World->OverlapMultiByObjectType(
        Overlaps,
        ExplosionLocation,
        FQuat::Identity,
        ObjectParams,
        FCollisionShape::MakeSphere(BombConfig.ExplosionRadius),
        QueryParams
    );

    // 여러 컴포넌트가 감지돼도 같은 액터에는 한 번만 적용
    TSet<AActor*> ProcessedActors;

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Target = Overlap.GetActor();

        if (!IsValid(Target) || Target == this || ProcessedActors.Contains(Target))
        {
            continue;
        }

        ProcessedActors.Add(Target);
        ApplyExplosionToActor(Target);
    }

    // 폭발 이펙트는 폭탄 액터와 독립적으로 재생
    if (IsValid(BombConfig.ExplosionEffect.Get()))
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, BombConfig.ExplosionEffect.Get(), ExplosionLocation);
    }

    // 자신을 생성한 지점에 재생성 요청
    if (ABombRespawnPoint* Point = RespawnPoint.Get())
    {
        Point->HandleBombExploded(this, BombConfig.RespawnDelay);
    }

    Destroy();
}

void AExplosiveBomb::SetRespawnPoint(ABombRespawnPoint * InRespawnPoint)
{
    RespawnPoint = InRespawnPoint;
}

void AExplosiveBomb::BeginPlay()
{
    // 부모에서 밀기·당기기 설정과 물리 시뮬레이션 초기화
    Super::BeginPlay();

    LoadBombConfig();
}

void AExplosiveBomb::ApplyExplosionToActor(AActor * Target)
{
    if (!IsValid(Target) || Target == this)
    {
        return;
    }

    // 지정된 파괴 장애물 처리
    if (ABombDestructibleObstacle* Obstacle = Cast<ABombDestructibleObstacle>(Target))
    {
        Obstacle->ReceiveBombExplosion(this);
        return;
    }

    // 캐릭터 피해는 데이터 테이블에 지정한 GAS 효과로 처리
    if (!Target->IsA<ACharacter>() || BombConfig.ExplosionDamage <= 0.0f || !BombConfig.DamageEffectClass)
    {
        return;
    }

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);

    if (!IsValid(TargetASC) || !TargetASC->AbilityActorInfo.IsValid())
    {
        return;
    }

    static const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Damage")), false);

    static const FGameplayTag CategoryTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Category.Gimmick")), false);

    static const FGameplayTag ExplosionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Type.Explosion")), false);

    if (!DamageTag.IsValid() || !CategoryTag.IsValid() || !ExplosionTag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 폭발 피해에 필요한 Gameplay Tag를 확인해주세요."), *GetName());

        return;
    }

    FGameplayEffectContextHandle EffectContext = TargetASC->MakeEffectContext();

    EffectContext.AddInstigator(this, this);
    EffectContext.AddSourceObject(this);

    FGameplayEffectSpecHandle EffectSpec = TargetASC->MakeOutgoingSpec(BombConfig.DamageEffectClass, 1.0f, EffectContext);

    if (!EffectSpec.IsValid())
    {
        return;
    }

    EffectSpec.Data->SetSetByCallerMagnitude(DamageTag, BombConfig.ExplosionDamage);

    EffectSpec.Data->AddDynamicAssetTag(CategoryTag);
    EffectSpec.Data->AddDynamicAssetTag(ExplosionTag);

    TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
}
