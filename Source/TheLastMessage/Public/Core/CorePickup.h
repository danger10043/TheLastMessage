#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/GameCoreTypes.h"
#include "CorePickup.generated.h"

class APlayerBase;
class UCoreChoiceWidget;
class UCoreComponent;
class UCoreDataAsset;
class UCoreRewardSession;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * 맵에 배치하여 플레이어에게 코어 보상을 제공하는 액터다.
 * 실제 코어의 능력과 효과 부여는 전달받은 UCoreComponent에 맡긴다.
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API ACorePickup : public AActor
{
	GENERATED_BODY()

public:
	ACorePickup();

	/** 지정된 코어의 획득을 요청한다. 성공하면 보상을 소비하고 true를 반환한다. */
	UFUNCTION(BlueprintCallable, Category = "Core|Reward")
	bool TryAcquireFixedCore(UCoreComponent* InCoreComponent);

	/** 플레이어에게 코어 후보 선택 화면을 연다. 선택에 성공하기 전에는 픽업을 소비하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "Core|Reward")
	bool TryOpenCoreChoice(APlayerBase* Player);

protected:
	/** 픽업이 제거되면 선택 화면과 진행 중인 선택 기록을 정리한다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 플레이어가 코어를 획득할 수 있는 범위를 감지한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Core|Components")
	TObjectPtr<USphereComponent> PickupCollision = nullptr;

	/** 코어 보상의 외형을 표시하는 메시 컴포넌트다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Core|Components")
	TObjectPtr<UStaticMeshComponent> CoreMesh = nullptr;

	/** 지정 코어 지급과 코어 선택 보상 중 사용할 방식을 정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Reward")
	ECoreRewardMode RewardMode = ECoreRewardMode::Fixed;

	/** 고정 보상으로 지급할 코어의 정의 데이터다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Reward",
		meta = (EditCondition = "RewardMode == ECoreRewardMode::Fixed", EditConditionHides))
	TObjectPtr<UCoreDataAsset> CoreData = nullptr;

	/** 선택형 보상에 사용할 화면 WBP. 카드 배치와 모양은 해당 WBP에서 편집한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Reward",
		meta = (EditCondition = "RewardMode == ECoreRewardMode::Choice", EditConditionHides))
	TSubclassOf<UCoreChoiceWidget> ChoiceWidgetClass;

	/** 선택형 보상을 추첨할 코어 분류다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Reward",
		meta = (EditCondition = "RewardMode == ECoreRewardMode::Choice", EditConditionHides))
	ECoreCategory ChoiceCategory = ECoreCategory::Stat;

	/** 보상 지급이 완료되었는지 나타낸다. 이미 소비한 보상의 재지급을 막는다. */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Core|Reward")
	bool bConsumed = false;

private:
	/** 선택 권한과 소유자를 확인하고, 더 이상 유효하지 않은 선택은 정리한다. */
	UCoreRewardSession* GetValidRewardSession();

	/** 대체 후보와 카드별 사용 횟수에 따라 각 리롤 버튼의 활성 상태를 갱신한다. */
	void UpdateRerollButtons();
	bool RefreshChoiceScreen();
	void HandleRewardCancelled();

	/** 선택 화면을 열지 못한 원인을 로그와 화면 디버그 메시지로 남긴다. 처리 중(Busy)에는 화면에 띄우지 않는다. */
	void ShowUnavailableNotice(ECoreRewardStartResult Result) const;

	/** 획득 범위에 들어온 플레이어에게 보상 모드에 맞는 획득 처리를 시작한다. */
	UFUNCTION()
	void OnPickupBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 화면에서 선택한 코어의 지급을 요청한다. 성공하면 화면을 닫고 픽업을 파괴한다. */
	UFUNCTION()
	void HandleCoreSelected(UCoreDataAsset* SelectedCore);

	/** 누른 카드만 재추첨하고, 새 코어와 리롤 가능 상태를 화면에 반영한다. */
	UFUNCTION()
	void HandleCoreReroll(int32 ChoiceIndex);

	/** 창이 외부에서 닫히면 진행 중인 선택 기록을 폐기한다. */
	UFUNCTION()
	void HandleChoiceClosed();

	/** 플레이어가 사망하면 선택 기록을 폐기하고 화면을 닫는다. */
	UFUNCTION()
	void HandleChoosingPlayerDied(APlayerBase* Player);

	/** 플레이어가 파괴되면 선택 기록을 폐기하고 화면을 닫는다. */
	UFUNCTION()
	void HandleChoosingPlayerDestroyed(AActor* Player);

	/** 화면과 입력 이벤트 연결만 정리한다. 획득 성공 시에는 페이드를 허용한다. */
	void CloseChoiceScreen(bool bAnimateClose = false);

	/** 획득 완료나 픽업 제거 시 화면과 선택 기록을 모두 정리한다. */
	void ClearChoiceReward(bool bAnimateClose = false);

	/** 현재 이 픽업의 후보를 표시하는 선택 화면. */
	UPROPERTY(Transient)
	TObjectPtr<UCoreChoiceWidget> ActiveChoiceWidget;

	/** 현재 보상을 선택하는 플레이어. 생존 여부와 선택 권한을 확인한다. */
	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerBase> ChoosingPlayer;

	/** 이 픽업에서 진행 중인 보상의 추첨·리롤 기록. */
	UPROPERTY(Transient)
	TObjectPtr<UCoreRewardSession> ActiveRewardSession;

	/** 현재 보상을 처리 중인지 나타낸다. 획득·재추첨 요청이 겹치는 것을 막는다. */
	bool bIsGrantingReward = false;
};
