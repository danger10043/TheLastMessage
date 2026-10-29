// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Plate/DamagePlate.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/Player/PlayerBase.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "NiagaraComponent.h"

ADamagePlate::ADamagePlate()
{
    // 버튼 조작 없이 접촉으로 작동하는 기믹
    bSupportsInteraction = false;

    PlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlateMesh"));

    PlateMesh->SetupAttachment(SceneRoot);
    PlateMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    PlateMesh->SetGenerateOverlapEvents(false);
    PlateMesh->SetSimulatePhysics(false);

    DamageTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageTrigger"));

    DamageTrigger->SetupAttachment(SceneRoot);
    DamageTrigger->SetBoxExtent(FVector(100.0f, 100.0f, 30.0f));
    DamageTrigger->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f));

    // 물체를 막지 않고 플레이어 등의 Pawn 진입만 감지
    DamageTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DamageTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    DamageTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    DamageTrigger->SetGenerateOverlapEvents(true);

    DamagePlateEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DamagePlateEffect"));

    DamagePlateEffect->SetupAttachment(SceneRoot);
    DamagePlateEffect->SetRelativeLocation(FVector::ZeroVector);
    DamagePlateEffect->SetRelativeRotation(FRotator::ZeroRotator);

    // 게임 시작 시 자동 재생
    DamagePlateEffect->SetAutoActivate(true);
}

bool ADamagePlate::LoadDamageConfig()
{
    bDamageConfigLoaded = false;

    if (!DamageConfigRow.DataTable || DamageConfigRow.RowName.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: DamageConfigRow의 테이블과 행을 지정해주세요."), *GetName());

        return false;
    }

    const FDamagePlateConfigRow* LoadedRow = DamageConfigRow.GetRow<FDamagePlateConfigRow>(TEXT("ADamagePlate::LoadDamageConfig"));

    if (!LoadedRow)
    {
        return false;
    }

    if (!FMath::IsFinite(LoadedRow->DamageAmount) 
        || LoadedRow->DamageAmount < 0.0f 
        || !FMath::IsFinite(LoadedRow->DamageInterval)
        || LoadedRow->DamageInterval <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 피해량은 0 이상, 피해 간격은 0보다 커야 합니다."), *GetName());

        return false;
    }

    if (!LoadedRow->DamageEffectClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 피해 Gameplay Effect를 지정해주세요."), *GetName());

        return false;
    }

    const UGameplayEffect* EffectDefault = LoadedRow->DamageEffectClass.GetDefaultObject();

    if (!IsValid(EffectDefault) || EffectDefault->DurationPolicy != EGameplayEffectDurationType::Instant)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 피해 Gameplay Effect는 Instant여야 합니다."), *GetName());

        return false;
    }

    if (!LoadedRow->ProtectionTag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 피해를 막아주는 보호 태그를 지정해주세요."), *GetName());

        return false;
    }

    DamageConfig = *LoadedRow;
    bDamageConfigLoaded = true;

    // 게임이 시작된 서버에서 피해 타이머 실행
    if (HasAuthority() && GetWorld() && (HasActorBegunPlay() || IsActorBeginningPlay()))
    {
        GetWorldTimerManager().SetTimer(DamageTimerHandle, this, &ADamagePlate::ApplyDamageToPlayers, DamageConfig.DamageInterval, true);
    }


    return true;
}

void ADamagePlate::BeginPlay()
{
    Super::BeginPlay();

    LoadDamageConfig();
}

void ADamagePlate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DamageTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

void ADamagePlate::ApplyDamageToPlayers()
{
    if (!HasAuthority() || !bDamageConfigLoaded || !bIsEnabled || !IsValid(DamageTrigger) || DamageConfig.DamageAmount <= 0.0f)
    {
        return;
    }

    // 현재 범위에 있는 플레이어만 수집
    TArray<AActor*> OverlappingActors;

    DamageTrigger->GetOverlappingActors(OverlappingActors, APlayerBase::StaticClass());

    static const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Damage")));

    for (AActor* Actor : OverlappingActors)
    {
        APlayerBase* Player = Cast<APlayerBase>(Actor);

        if (!IsValid(Player) || Player->IsDead())
        {
            continue;
        }

        UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player);

        if (!IsValid(ASC) || !ASC->AbilityActorInfo.IsValid())
        {
            continue;
        }

        // 철붕이 보호 태그가 있으면 피해를 주지 않음
        if (ASC->HasMatchingGameplayTag(DamageConfig.ProtectionTag))
        {
            continue;
        }

        FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();

        EffectContext.AddInstigator(this, this);
        EffectContext.AddSourceObject(this);

        FGameplayEffectSpecHandle EffectSpec = ASC->MakeOutgoingSpec(DamageConfig.DamageEffectClass, 1.0f, EffectContext);

        if (!EffectSpec.IsValid())
        {
            continue;
        }

        // 데이터 테이블의 피해량 전달
        EffectSpec.Data->SetSetByCallerMagnitude(DamageTag, DamageConfig.DamageAmount);

        ASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
    }
}