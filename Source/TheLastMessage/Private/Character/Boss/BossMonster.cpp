#include "Character/Boss/BossMonster.h"
#include "AbilitySystemComponent.h"
#include "Character/Boss/BossAttributeSet.h"
#include "Character/Boss/BossPhaseComponent.h"
#include "Character/Boss/BossPatternComponent.h"
#include "Character/Boss/BossShieldComponent.h"
#include "Character/Boss/BossCorePuzzleComponent.h"

ABossMonster::ABossMonster()
{
    AbilitySystemComponent =
        CreateDefaultSubobject<UAbilitySystemComponent>(
            TEXT("AbilitySystemComponent"));
    AttributeSet =
        CreateDefaultSubobject<UBossAttributeSet>(TEXT("AttributeSet"));
    PhaseComponent =
        CreateDefaultSubobject<UBossPhaseComponent>(TEXT("PhaseComponent"));
    PatternComponent =
        CreateDefaultSubobject<UBossPatternComponent>(TEXT("PatternComponent"));
    ShieldComponent =
        CreateDefaultSubobject<UBossShieldComponent>(TEXT("ShieldComponent"));
    CorePuzzleComponent =
        CreateDefaultSubobject<UBossCorePuzzleComponent>(
            TEXT("CorePuzzleComponent"));
}

void ABossMonster::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystemComponent->InitAbilityActorInfo(this, this);
    // InitializeBoss의 기본 스탯 GE·능력 부여 구현은 별도 단계다.
}

UAbilitySystemComponent* ABossMonster::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent.Get();
}