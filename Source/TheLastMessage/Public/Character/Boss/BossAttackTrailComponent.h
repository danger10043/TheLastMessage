#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossAttackTrailComponent.generated.h"

class ABossMonster;
class UNiagaraComponent;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EBossTrailType : uint8
{
    MeleePhase12,
    MeleePhase3,
    JumpAttack,
    CrossLaserDrop
};

USTRUCT(BlueprintType)
struct FBossAttackTrailSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
    TObjectPtr<UNiagaraSystem> System = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
    FName SocketName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
    FVector LocationOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
    FRotator RotationOffset = FRotator::ZeroRotator;
};

UCLASS(ClassGroup = (Boss), meta = (BlueprintSpawnableComponent))
class THELASTMESSAGE_API UBossAttackTrailComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBossAttackTrailComponent();

    static void StartForBoss(ABossMonster* Boss, EBossTrailType Type);
    static void StopForBoss(ABossMonster* Boss);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Trail")
    TMap<EBossTrailType, FBossAttackTrailSettings> TrailSettings;

private:
    UFUNCTION(NetMulticast, Reliable)
    void MulticastSetTrail(bool bEnabled, EBossTrailType Type);

    void StopLocal();

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> ActiveTrail;
};