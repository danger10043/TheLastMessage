// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/RespawnPoint/BombRespawnPoint.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Gimmick/PushPull/ExplosiveBomb.h"

ABombRespawnPoint::ABombRespawnPoint()
{
    // 재생성 대기는 타이머로 처리
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

    SetRootComponent(SceneRoot);
}

void ABombRespawnPoint::SpawnBomb()
{
    UWorld* World = GetWorld();

    if (!HasAuthority() || !World || CurrentBomb.IsValid() || bRespawnPending)
    {
        return;
    }

    if (!BombClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: BombClass에 폭탄 Blueprint를 지정해주세요."), *GetName());

        return;
    }

    // BeginPlay 전에 재생성 지점을 연결할 수 있도록 지연 생성
    AExplosiveBomb* NewBomb =
        World->SpawnActorDeferred<AExplosiveBomb>(
            BombClass,
            SpawnTransform,
            this,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );

    if (!IsValid(NewBomb))
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 폭탄 생성에 실패했습니다."), *GetName());

        return;
    }

    // 생성 완료 전에 서로의 참조 연결
    CurrentBomb = NewBomb;
    NewBomb->SetRespawnPoint(this);

    // 생성 완료 후 폭탄의 BeginPlay 실행
    NewBomb->FinishSpawning(SpawnTransform);
}

void ABombRespawnPoint::RespawnBomb()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(RespawnTimerHandle);

    bRespawnPending = false;

    SpawnBomb();
}

void ABombRespawnPoint::HandleBombExploded(AExplosiveBomb * Bomb, float RespawnDelay)
{
    if (!HasAuthority() || !GetWorld() || !IsValid(Bomb) || CurrentBomb.Get() != Bomb || !Bomb->HasExploded() || bRespawnPending)
    {
        return;
    }

    if (!FMath::IsFinite(RespawnDelay) || RespawnDelay < 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: RespawnDelay는 유효한 0 이상의 값이어야 합니다."), *GetName());

        return;
    }

    CurrentBomb.Reset();
    bRespawnPending = true;

    if (RespawnDelay <= KINDA_SMALL_NUMBER)
    {
        // 대기 시간이 0이면 다음 프레임에 생성
        RespawnTimerHandle = GetWorldTimerManager().SetTimerForNextTick(this, &ABombRespawnPoint::RespawnBomb);
    }
    else
    {
        GetWorldTimerManager().SetTimer(
            RespawnTimerHandle, 
            this,
            &ABombRespawnPoint::RespawnBomb,
            RespawnDelay,
            false
        );
    }
}

void ABombRespawnPoint::BeginPlay()
{
    Super::BeginPlay();

    // 최초 배치된 위치와 회전을 재생성 기준으로 기록
    SpawnTransform = GetActorTransform();

    SpawnBomb();
	
}

void ABombRespawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 지점이 제거되면 남아 있는 재생성 예약 취소
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RespawnTimerHandle);
    }

    // 남아 있는 폭탄에서 이 지점의 참조 해제
    if (AExplosiveBomb* Bomb = CurrentBomb.Get())
    {
        Bomb->SetRespawnPoint(nullptr);
    }

    CurrentBomb.Reset();
    bRespawnPending = false;

    Super::EndPlay(EndPlayReason);
}

