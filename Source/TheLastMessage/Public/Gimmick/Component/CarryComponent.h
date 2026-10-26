// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "CarryComponent.generated.h"

class AActor;
class APlayerBase;
class ACarryableGimmick;
class UAbilitySystemComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnCarriedObjectChanged,
	ACarryableGimmick*,
	NewObject);

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THELASTMESSAGE_API UCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCarryComponent();
    // 물체 집기
    UFUNCTION(BlueprintCallable, Category = "Gimmick|Carry")
    bool TryPickUp(ACarryableGimmick* Target);

    // 안전한 위치에 물체 내려놓기
    UFUNCTION(BlueprintCallable, Category = "Gimmick|Carry")
    bool DropCarriedObject();

    // 들고 있는 물체를 소켓 등의 다른 부착 지점으로 전달
    UFUNCTION(BlueprintCallable, Category = "Gimmick|Carry")
    bool TransferCarriedObject(USceneComponent* TargetParent);

    // 사망이나 컴포넌트 종료 시 운반 강제 정리
    UFUNCTION(BlueprintCallable, Category = "Gimmick|Carry")
    void ClearCarryState();

    UFUNCTION(BlueprintPure, Category = "Gimmick|Carry")
    bool HasCarriedObject() const;

    UFUNCTION(BlueprintPure, Category = "Gimmick|Carry")
    ACarryableGimmick* GetCarriedObject() const;

    // 새 물체를 들면 해당 물체, 운반이 끝나면 nullptr 전달
    UPROPERTY(BlueprintAssignable, Category = "Gimmick|Carry")
    FOnCarriedObjectChanged OnCarriedObjectChanged;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Carry")
    TWeakObjectPtr<ACarryableGimmick> CarriedObject;

    // 비워 두면 플레이어 메시를 기준으로 부착
    // 이름을 지정하면 해당 소켓을 기준으로 부착
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Carry")
    FName CarrySocketName = NAME_None;

    UPROPERTY(Transient)
    FActiveGameplayEffectHandle ActiveCarryEffectHandle;

private:
    void CacheOwner();

    void RestoreObjectState(ACarryableGimmick* Target, bool bRestorePhysics);

    void FinishCarryState();

    UFUNCTION()
    void HandleCarriedObjectEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

    UPROPERTY(Transient)
    TWeakObjectPtr<APlayerBase> OwnerPlayer;

    UPROPERTY(Transient)
    TWeakObjectPtr<UAbilitySystemComponent> OwnerASC;

    FVector OriginalWorldScale = FVector::OneVector;

    // 운반 전 점프 초기 속도
    float OriginalJumpZVelocity = 0.0f;

    // 점프 속도 변경을 적용했는지 확인
    bool bHasJumpVelocityOverride = false;

    bool bWasSimulatingPhysics = false;
    bool bWasActorCollisionEnabled = true;

    bool bIsChangingCarryState = false;
    bool bIsClearingCarryState = false;
};
