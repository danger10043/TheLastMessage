#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "FlagReceiverComponent.generated.h"

UENUM(BlueprintType)
enum class EFlagMatchMode : uint8
{
	/** 모든 플래그가 켜져야 활성. */
	All,
	/** 하나라도 켜지면 활성. */
	Any
};

/** bActive: 플래그 조건이 만족되는가. bInstant: 로드/복원 때처럼 연출 없이 바로 맞춰야 하는가. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFlagReceiverStateChanged, bool, bActive, bool, bInstant);

/**
 * 문/해치 같은 액터에 붙여서 월드 플래그에 따라 상태를 바꾸는 컴포넌트.
 * 소유 액터가 AGimmickBase(예: ElectricDoor)면 자동으로 활성/비활성 상태를 맞춘다.
 * 플래그가 라이브로 바뀌면 연출과 함께(ApplyActiveState), 로드나 체크포인트 복원 때는 연출 없이 바로 맞춘다.
 * 그 외 액터는 OnStateChanged 이벤트로 직접 처리한다.
 */
UCLASS(ClassGroup = (Gimmick), meta = (BlueprintSpawnableComponent))
class THELASTMESSAGE_API UFlagReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFlagReceiverComponent();

	/** 확인할 플래그. 예: Gate.RH0406 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flag")
	TArray<FGameplayTag> Flags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flag")
	EFlagMatchMode MatchMode = EFlagMatchMode::All;

	/** 소유 액터가 AGimmickBase일 때 활성 상태를 자동으로 맞춘다. 끄면 OnStateChanged 이벤트만 보낸다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flag")
	bool bDriveOwnerGimmick = true;

	/** 조건 결과를 뒤집는다(플래그가 꺼져 있을 때 활성). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flag")
	bool bInvert = false;

	UPROPERTY(BlueprintAssignable, Category = "Flag")
	FOnFlagReceiverStateChanged OnStateChanged;

	/** 지금 플래그 기준으로 조건이 만족되는가(Invert 적용). */
	UFUNCTION(BlueprintPure, Category = "Flag")
	bool IsConditionMet() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleFlagChanged(FGameplayTag Flag, bool bValue);
	void HandleResync();
	void ApplyInitialState();
	void Apply(bool bInstant);

	bool bHasApplied = false;
	bool bLastApplied = false;
	FDelegateHandle ResyncHandle;
	FTimerHandle InitialApplyHandle;
};
