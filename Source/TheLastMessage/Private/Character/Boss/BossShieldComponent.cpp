#include "Character/Boss/BossShieldComponent.h"
#include "Character/Boss/BossAttributeSet.h"
#include "GameplayEffect.h"
#include "Character/Boss/BossMonster.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UBossShieldComponent::ActivateShield(EBossPuzzleType PuzzleType)
{
    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!IsValid(Boss))
    {
        UE_LOG(LogTemp, Error, TEXT("BossShieldComponent::ActivateShield - 소유자가 보스가 아닙니다."));
        return;
    }
    if (!Boss->HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossShieldComponent::ActivateShield - 실행 권한이 없습니다."));
        return;
    }
    if (bIsShieldActive)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossShieldComponent::ActivateShield - 실드가 이미 활성화되어 있습니다."));
        return;
    }

    switch (PuzzleType)
    {
    case EBossPuzzleType::LaserPuzzle:
    case EBossPuzzleType::CorePuzzle:
        break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("BossShieldComponent::ActivateShield - 잘못된 퍼즐 종류입니다."));
        return;
    }

    UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
    if (!IsValid(ASC))
    {
        UE_LOG(LogTemp, Error, TEXT("BossShieldComponent::ActivateShield - ASC가 없습니다."));
        return;
    }
    const FGameplayTag ShieldTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Boss.Immunity.Shield")), false);
    if (!ShieldTag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("BossShieldComponent::ActivateShield - 실드 면역 태그가 등록되지 않았습니다."));
        return;
    }

    if (!FMath::IsFinite(PuzzleTimeLimit))
    {
        UE_LOG(LogTemp, Error, TEXT("BossShieldComponent::ActivateShield - 제한 시간이 유한한 수가 아닙니다."));
        return;
    }
    if (PuzzleTimeLimit <= 0.f)
    {
        UE_LOG(LogTemp, Error, TEXT("BossShieldComponent::ActivateShield - PuzzleTimeLimit에 0보다 큰 시간을 설정하세요."));
        return;
    }
    if (!GetWorld())
    {
        UE_LOG(LogTemp, Error, TEXT("BossShieldComponent::ActivateShield - World가 없습니다."));
        return;
    }
    if (!CanApplyFailureRecovery())
    {
        return;
    }

    CurrentPuzzleType = PuzzleType;
    bIsShieldActive = true;
    ASC->SetLooseGameplayTagCount(ShieldTag, 1);

    // 제한 시간·레이저·코어 오브젝트는 후속 퍼즐 구현에서 연결한다.
}

bool UBossShieldComponent::IsShieldActive()
{
    return bIsShieldActive;
}

void UBossShieldComponent::StopPuzzleTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PuzzleTimerHandle);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BossShieldComponent::StopPuzzleTimer - World가 없습니다."));
    }
    PuzzleTimerHandle.Invalidate();
    RemainingPuzzleTime = 0.f;
}

void UBossShieldComponent::DeactivateShield()
{
    StopPuzzleTimer();
    bIsShieldActive = false;
    CurrentPuzzleType = EBossPuzzleType::None;

    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!IsValid(Boss))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossShieldComponent::DeactivateShield - 소유자가 보스가 아닙니다."));
        return;
    }
    UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
    if (!IsValid(ASC))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossShieldComponent::DeactivateShield - ASC가 없습니다."));
        return;
    }
    const FGameplayTag ShieldTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Boss.Immunity.Shield")), false);
    if (!ShieldTag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("BossShieldComponent::DeactivateShield - 실드 면역 태그가 없습니다."));
        return;
    }
    ASC->SetLooseGameplayTagCount(ShieldTag, 0);

    // 레이저 오브젝트를 실제로 활성화하는 구현은 아직 없으므로,
    // 장치 비활성화는 레이저 퍼즐 구현 시 함께 연결한다.
}

bool UBossShieldComponent::CanApplyFailureRecovery() const
{
    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!IsValid(Boss))
    {
        UE_LOG(LogTemp, Error, TEXT("BossShieldComponent::CanApplyFailureRecovery - 소유자가 보스가 아닙니다."));
        return false;
    }
    if (!Boss->HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossShieldComponent::CanApplyFailureRecovery - 실행 권한이 없습니다."));
        return false;
    }
    UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
    if (!IsValid(ASC))
    {
        UE_LOG(LogTemp, Error, TEXT("BossShieldComponent::CanApplyFailureRecovery - ASC가 없습니다."));
        return false;
    }
    const UBossAttributeSet* Attributes = ASC->GetSet<UBossAttributeSet>();
    if (!Attributes)
    {
        UE_LOG(LogTemp, Error, TEXT("BossShieldComponent::CanApplyFailureRecovery - BossAttributeSet이 없습니다."));
        return false;
    }
    if (!FMath::IsFinite(Attributes->GetMaxHealth()))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossShieldComponent::CanApplyFailureRecovery - 최대 체력이 유한한 수가 아닙니다."));
        return false;
    }
    if (Attributes->GetMaxHealth() <= 0.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossShieldComponent::CanApplyFailureRecovery - 최대 체력이 0 이하입니다."));
        return false;
    }
    if (!FMath::IsFinite(FailureHealPercent))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossShieldComponent::CanApplyFailureRecovery - 회복 비율이 유한한 수가 아닙니다."));
        return false;
    }
    if (FailureHealPercent <= 0.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossShieldComponent::CanApplyFailureRecovery - 회복 비율은 0보다 커야 합니다."));
        return false;
    }
    if (FailureHealPercent > 1.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossShieldComponent::CanApplyFailureRecovery - 회복 비율은 1 이하여야 합니다."));
        return false;
    }
    return true;
}

bool UBossShieldComponent::TryApplyFailureRecovery()
{
    if (!CanApplyFailureRecovery())
    {
        return false;
    }
    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (Boss->GetBossState() == EBossState::Dead)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossShieldComponent::TryApplyFailureRecovery - 사망 상태에서는 회복하지 않습니다."));
        return false;
    }
    UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
    const UBossAttributeSet* Attributes = ASC->GetSet<UBossAttributeSet>();
    const float HealAmount = Attributes->GetMaxHealth() * FailureHealPercent;

    // 지속 효과가 아닌, 한 번 실행하는 Instant 회복 GE를 생성한다.
    UGameplayEffect* RecoveryEffect = NewObject<UGameplayEffect>(this);
    RecoveryEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo& Modifier =
        RecoveryEffect->Modifiers.AddDefaulted_GetRef();
    Modifier.Attribute = UBossAttributeSet::GetHealthAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude =
        FGameplayEffectModifierMagnitude(FScalableFloat(HealAmount));

    FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
    Context.AddSourceObject(Boss);
    const FActiveGameplayEffectHandle Applied =
        ASC->ApplyGameplayEffectToSelf(RecoveryEffect, 1.f, Context);
    if (!Applied.WasSuccessfullyApplied())
    {
        UE_LOG(LogTemp, Error, TEXT("BossShieldComponent::TryApplyFailureRecovery - 회복 GE 적용이 거부되었습니다."));
        return false;
    }
    return true;
}

void UBossShieldComponent::ApplyFailureRecovery()
{
    TryApplyFailureRecovery();
}

void UBossShieldComponent::StartPuzzleTimer()
{
    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!IsValid(Boss))
    {
        UE_LOG(LogTemp, Error, TEXT("BossShieldComponent::StartPuzzleTimer - 소유자가 보스가 아닙니다."));
        return;
    }
    if (!Boss->HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossShieldComponent::StartPuzzleTimer - 실행 권한이 없습니다."));
        return;
    }
    if (!bIsShieldActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossShieldComponent::StartPuzzleTimer - 실드가 비활성 상태입니다."));
        return;
    }
    if (Boss->GetBossState() != EBossState::ShieldPuzzle)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossShieldComponent::StartPuzzleTimer - 실드 퍼즐 상태가 아닙니다."));
        return;
    }
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("BossShieldComponent::StartPuzzleTimer - World가 없습니다."));
        return;
    }
    if (World->GetTimerManager().TimerExists(PuzzleTimerHandle))
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossShieldComponent::StartPuzzleTimer - 기존 타이머를 유지합니다."));
        return;
    }
    if (!FMath::IsFinite(PuzzleTimeLimit))
    {
        UE_LOG(LogTemp, Error, TEXT("BossShieldComponent::StartPuzzleTimer - 제한 시간이 유한한 수가 아닙니다."));
        return;
    }
    if (PuzzleTimeLimit <= 0.f)
    {
        UE_LOG(LogTemp, Error, TEXT("BossShieldComponent::StartPuzzleTimer - 제한 시간은 0보다 커야 합니다."));
        return;
    }

    RemainingPuzzleTime = PuzzleTimeLimit;
    World->GetTimerManager().SetTimer(
        PuzzleTimerHandle, this,
        &UBossShieldComponent::HandlePuzzleTimeout,
        PuzzleTimeLimit, false);
}

void UBossShieldComponent::HandlePuzzleTimeout()
{
    // 실패 이벤트에서 다음 퍼즐이 시작되더라도 그 타이머를 지우지 않도록,
    // 기존 타이머 정리는 FailShieldPuzzle 호출 전에 끝낸다.
    StopPuzzleTimer();

    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!IsValid(Boss))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossShieldComponent::HandlePuzzleTimeout - 소유자가 보스가 아닙니다."));
        return;
    }
    if (!Boss->HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossShieldComponent::HandlePuzzleTimeout - 실행 권한이 없습니다."));
        return;
    }
    if (!bIsShieldActive)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossShieldComponent::HandlePuzzleTimeout - 이미 종료된 퍼즐입니다."));
        return;
    }
    if (Boss->GetBossState() != EBossState::ShieldPuzzle)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossShieldComponent::HandlePuzzleTimeout - 실드 퍼즐 상태가 아니므로 실패 요청을 생략합니다."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("BossShieldComponent::HandlePuzzleTimeout - 제한 시간이 만료되어 실패 처리합니다."));
    Boss->FailShieldPuzzle();
}

float UBossShieldComponent::GetRemainingPuzzleTime()
{
    if (!bIsShieldActive)
    {
        RemainingPuzzleTime = 0.f;
        return 0.f;
    }
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossShieldComponent::GetRemainingPuzzleTime - World가 없습니다."));
        RemainingPuzzleTime = 0.f;
        return 0.f;
    }

    RemainingPuzzleTime = FMath::Max(0.f,
        World->GetTimerManager().GetTimerRemaining(PuzzleTimerHandle));
    return RemainingPuzzleTime;
}

void UBossShieldComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopPuzzleTimer();
    Super::EndPlay(EndPlayReason);
}