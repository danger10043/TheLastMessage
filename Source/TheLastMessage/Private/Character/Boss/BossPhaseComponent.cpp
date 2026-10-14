#include "Character/Boss/BossPhaseComponent.h"
#include "Character/Boss/BossMonster.h"
#include "AbilitySystemComponent.h"

EBossPhase UBossPhaseComponent::GetCurrentPhase()
{
    return CurrentPhase;
}

float UBossPhaseComponent::GetDamageHealthFloorRatio() const
{
    float Threshold = 0.f;
    switch (CurrentPhase)
    {
    case EBossPhase::Phase1:
        if (bPhase2Unlocked)
        {
            return 0.f;
        }
        Threshold = Phase2Threshold;
        break;
    case EBossPhase::Phase2:
        if (bPhase3Unlocked)
        {
            return 0.f;
        }
        Threshold = Phase3Threshold;
        break;
    case EBossPhase::Phase3:
        return 0.f;
    default:
        UE_LOG(LogTemp, Error, TEXT("BossPhaseComponent - 잘못된 페이즈입니다. 피해를 제한합니다."));
        return 1.f;
    }

    if (!FMath::IsFinite(Threshold))
    {
        UE_LOG(LogTemp, Error, TEXT("BossPhaseComponent - 임계값이 유한한 수가 아닙니다."));
        return 1.f;
    }
    if (Threshold <= 0.f)
    {
        UE_LOG(LogTemp, Error, TEXT("BossPhaseComponent - 임계값은 0보다 커야 합니다."));
        return 1.f;
    }
    if (Threshold >= 1.f)
    {
        UE_LOG(LogTemp, Error, TEXT("BossPhaseComponent - 임계값은 1보다 작아야 합니다."));
        return 1.f;
    }
    if (CurrentPhase == EBossPhase::Phase2)
    {
        if (Phase3Threshold >= Phase2Threshold)
        {
            UE_LOG(LogTemp, Error, TEXT("BossPhaseComponent - Phase3 임계값은 Phase2 임계값보다 작아야 합니다."));
            return 1.f;
        }
    }
    return Threshold;
}

void UBossPhaseComponent::CheckPhaseThreshold(float HealthPercent)
{
    if (bIsTransitionPending)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossPhaseComponent::CheckPhaseThreshold - 전환 요청이 이미 대기 중입니다."));
        return;
    }
    if (!FMath::IsFinite(HealthPercent))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossPhaseComponent::CheckPhaseThreshold - 체력 비율이 유한한 수가 아닙니다."));
        return;
    }

    const float Threshold = GetDamageHealthFloorRatio();
    if (Threshold >= 1.f)
    {
        UE_LOG(LogTemp, Error, TEXT("BossPhaseComponent::CheckPhaseThreshold - 임계값 설정 오류입니다."));
        return;
    }
    if (Threshold <= 0.f)
    {
        return;
    }
    if (HealthPercent > Threshold)
    {
        return;
    }

    switch (CurrentPhase)
    {
    case EBossPhase::Phase1:
        RequestPhaseTransition(EBossPhase::Phase2);
        break;
    case EBossPhase::Phase2:
        RequestPhaseTransition(EBossPhase::Phase3);
        break;
    default:
        break;
    }
}

void UBossPhaseComponent::RequestPhaseTransition(EBossPhase NextPhase)
{
    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!IsValid(Boss))
    {
        UE_LOG(LogTemp, Error, TEXT("BossPhaseComponent::RequestPhaseTransition - 소유자가 보스가 아닙니다."));
        return;
    }
    if (!Boss->HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossPhaseComponent::RequestPhaseTransition - 실행 권한이 없습니다."));
        return;
    }
    if (bIsTransitionPending)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossPhaseComponent::RequestPhaseTransition - 중복 요청입니다."));
        return;
    }

    EBossPuzzleType PuzzleType = EBossPuzzleType::None;
    if (CurrentPhase == EBossPhase::Phase1)
    {
        if (NextPhase == EBossPhase::Phase2)
        {
            if (!bPhase2Unlocked)
            {
                PuzzleType = EBossPuzzleType::LaserPuzzle;
            }
        }
    }
    if (CurrentPhase == EBossPhase::Phase2)
    {
        if (NextPhase == EBossPhase::Phase3)
        {
            if (!bPhase3Unlocked)
            {
                PuzzleType = EBossPuzzleType::CorePuzzle;
            }
        }
    }
    if (PuzzleType == EBossPuzzleType::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossPhaseComponent::RequestPhaseTransition - 허용되지 않는 전환입니다."));
        return;
    }

    // 이벤트에서 재진입하더라도 요청을 중복하지 않는다.
    PendingPhase = NextPhase;
    bIsTransitionPending = true;
    Boss->StartShieldPuzzle(PuzzleType);

    if (Boss->GetBossState() == EBossState::ShieldPuzzle)
    {
        return;
    }
    if (Boss->GetBossState() == EBossState::Cinematic)
    {
        return;
    }
    if (CurrentPhase == NextPhase)
    {
        return;
    }

    if (!bIsTransitionPending)
    {
        // 퍼즐 시작 이벤트 안에서 이미 실패 정리가 완료된 경우다.
        return;
    }

    PendingPhase = CurrentPhase;
    bIsTransitionPending = false;
    UE_LOG(LogTemp, Error,
        TEXT("BossPhaseComponent::RequestPhaseTransition - 퍼즐 진입이 중단되었습니다."));

    // 성공 전에는 CurrentPhase와 Unlocked 플래그를 변경하지 않는다.
}

bool UBossPhaseComponent::TryGetPendingPhase(EBossPhase& OutPhase) const
{
    if (!bIsTransitionPending)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossPhaseComponent::TryGetPendingPhase - 대기 중인 전환이 없습니다."));
        return false;
    }
    OutPhase = PendingPhase;
    return true;
}

void UBossPhaseComponent::CommitPhaseTransition()
{
    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!IsValid(Boss))
    {
        UE_LOG(LogTemp, Error, TEXT("BossPhaseComponent::CommitPhaseTransition - 소유자가 보스가 아닙니다."));
        return;
    }
    if (!Boss->HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossPhaseComponent::CommitPhaseTransition - 실행 권한이 없습니다."));
        return;
    }
    if (Boss->GetBossState() != EBossState::Cinematic)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossPhaseComponent::CommitPhaseTransition - 시네마틱 상태가 아닙니다."));
        return;
    }
    if (!bIsTransitionPending)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossPhaseComponent::CommitPhaseTransition - 대기 중인 전환이 없습니다."));
        return;
    }

    bool bValidTransition = false;
    if (CurrentPhase == EBossPhase::Phase1)
    {
        if (PendingPhase == EBossPhase::Phase2)
        {
            bValidTransition = true;
        }
    }
    if (CurrentPhase == EBossPhase::Phase2)
    {
        if (PendingPhase == EBossPhase::Phase3)
        {
            bValidTransition = true;
        }
    }
    if (!bValidTransition)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossPhaseComponent::CommitPhaseTransition - 허용되지 않는 전환입니다."));
        return;
    }
    UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
    if (!IsValid(ASC))
    {
        UE_LOG(LogTemp, Error, TEXT("BossPhaseComponent::CommitPhaseTransition - ASC가 없습니다."));
        return;
    }
    const TCHAR* RequiredTags[] = {
        TEXT("Boss.Phase.One"), TEXT("Boss.Phase.Two"),
        TEXT("Boss.Phase.Three"), TEXT("Boss.Immunity.Ranged")
    };
    for (const TCHAR* Name : RequiredTags)
    {
        if (!FGameplayTag::RequestGameplayTag(FName(Name), false).IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("BossPhaseComponent::CommitPhaseTransition - 태그가 등록되지 않았습니다: %s"), Name);
            return;
        }
    }

    CurrentPhase = PendingPhase;
    if (CurrentPhase == EBossPhase::Phase2)
    {
        bPhase2Unlocked = true;
    }
    if (CurrentPhase == EBossPhase::Phase3)
    {
        bPhase3Unlocked = true;
    }
    bIsTransitionPending = false;
    PendingPhase = CurrentPhase;
    ApplyPhaseSettings(CurrentPhase);
}

void UBossPhaseComponent::ApplyPhaseSettings(EBossPhase NewPhase)
{
    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!IsValid(Boss))
    {
        UE_LOG(LogTemp, Error, TEXT("BossPhaseComponent::ApplyPhaseSettings - 소유자가 보스가 아닙니다."));
        return;
    }
    if (!Boss->HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossPhaseComponent::ApplyPhaseSettings - 실행 권한이 없습니다."));
        return;
    }
    UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
    if (!IsValid(ASC))
    {
        UE_LOG(LogTemp, Error, TEXT("BossPhaseComponent::ApplyPhaseSettings - ASC가 없습니다."));
        return;
    }
    int32 PhaseIndex;
    switch (NewPhase)
    {
    case EBossPhase::Phase1: PhaseIndex = 0; break;
    case EBossPhase::Phase2: PhaseIndex = 1; break;
    case EBossPhase::Phase3: PhaseIndex = 2; break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("BossPhaseComponent::ApplyPhaseSettings - 잘못된 페이즈입니다."));
        return;
    }
    const TCHAR* Names[] = {
        TEXT("Boss.Phase.One"), TEXT("Boss.Phase.Two"), TEXT("Boss.Phase.Three")
    };
    FGameplayTag Tags[3];
    for (int32 Index = 0; Index < 3; ++Index)
    {
        Tags[Index] = FGameplayTag::RequestGameplayTag(FName(Names[Index]), false);
        if (!Tags[Index].IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("BossPhaseComponent::ApplyPhaseSettings - 태그가 없습니다: %s"), Names[Index]);
            return;
        }
    }
    for (int32 Index = 0; Index < 3; ++Index)
    {
        ASC->SetLooseGameplayTagCount(Tags[Index], Index == PhaseIndex ? 1 : 0);
    }
    if (Boss->GetBossState() == EBossState::Dead)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossPhaseComponent::ApplyPhaseSettings - 태그 변경 중 사망했습니다."));
        return;
    }
    Boss->SetRangedImmunity(NewPhase == EBossPhase::Phase3);

    // 장비 파괴·공격별 강화 설정은 해당 기능 구현 시 연결한다.
}

void UBossPhaseComponent::CancelPhaseTransition()
{
    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!IsValid(Boss))
    {
        UE_LOG(LogTemp, Error, TEXT("BossPhaseComponent::CancelPhaseTransition - 소유자가 보스가 아닙니다."));
        return;
    }
    if (!Boss->HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossPhaseComponent::CancelPhaseTransition - 실행 권한이 없습니다."));
        return;
    }
    bIsTransitionPending = false;
    PendingPhase = CurrentPhase;
    // CurrentPhase와 이미 완료한 Unlocked 플래그는 유지한다.
}