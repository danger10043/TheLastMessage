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
class UCorePickupStateSubsystem;
class UMaterialInstanceDynamic;
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
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
#endif

	/** 코어 종류의 CoreId와 별개인 배치 인스턴스 ID. 맵에 저장되며 PIE에서도 유지된다. */
	UFUNCTION(BlueprintPure, Category = "Core|Persistence")
	FGuid GetPickupId() const { return PickupId; }

	void RestoreCheckpointConsumption(bool bWasConsumed);
	void HideConsumedPickup();

	/** 평소에는 코어를 떠다니게 하고, 흡수 연출 중에는 플레이어 쪽으로 옮긴다. */
	virtual void Tick(float DeltaSeconds) override;

protected:
	/** 배치한 위치·크기를 대기 움직임의 기준으로 기록하고 발광 머티리얼을 준비한다. */
	virtual void BeginPlay() override;

	/** 픽업이 제거되면 선택 화면과 진행 중인 선택 기록을 정리한다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 플레이어가 코어를 획득할 수 있는 범위를 감지한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Core|Components")
	TObjectPtr<USphereComponent> PickupCollision = nullptr;

	/** 코어 보상의 외형을 표시하는 메시 컴포넌트다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Core|Components")
	TObjectPtr<UStaticMeshComponent> CoreMesh = nullptr;

	/** 위아래로 떠다니는 높이(cm). 배치한 위치를 중심으로 이 거리만큼 오르내린다. 0이면 움직이지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Idle", meta = (ClampMin = "0.0"))
	float BobHeight = 10.0f;

	/** 1초에 위아래로 왕복하는 횟수. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Idle", meta = (ClampMin = "0.0"))
	float BobSpeed = 0.8f;

	/** 초당 회전 각도(도). 0이면 회전하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Idle")
	float SpinSpeed = 45.0f;

	/** 크기가 커졌다 작아지는 비율. 0.05면 ±5%로 맥박치며, 0이면 크기가 일정하다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Idle", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PulseScale = 0.05f;

	/** 1초에 맥박치는 횟수. 크기와 발광이 같은 박자로 바뀐다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Idle", meta = (ClampMin = "0.0"))
	float PulseSpeed = 1.5f;

	/** 발광 세기를 조절할 머티리얼 Scalar 파라미터 이름. 비우거나 머티리얼에 없으면 발광은 바꾸지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Idle")
	FName EmissiveParameter = TEXT("EmissiveIntensity");

	/** 평소 발광 세기. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Idle", meta = (ClampMin = "0.0"))
	float IdleEmissive = 5.0f;

	/** 보상 종류별 색을 넣을 머티리얼 Vector 파라미터 이름. 비우거나 머티리얼에 없으면 색은 바꾸지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Color")
	FName ColorParameter = TEXT("CoreColor");

	/** 선택형 중 스탯 코어를 주는 픽업의 색. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Color")
	FLinearColor StatRewardColor = FLinearColor(0.2f, 0.75f, 1.0f);

	/** 선택형 중 능력 코어를 주는 픽업의 색. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Color")
	FLinearColor AbilityRewardColor = FLinearColor(0.65f, 0.3f, 1.0f);

	/** 정해진 코어 하나를 주는 고정형 픽업의 색. 더블 점프·그래플링 훅·가드처럼 핵심 기능을 여는 코어다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Color")
	FLinearColor FixedRewardColor = FLinearColor(1.0f, 0.75f, 0.15f);

	/** 맥박에 맞춰 발광이 바뀌는 비율. 0.3이면 평소 세기의 ±30%로 깜빡인다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Idle", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EmissivePulse = 0.3f;

	/** 획득 후 코어가 플레이어에게 빨려 들어가는 시간(초). 0이면 연출 없이 바로 숨긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Absorb", meta = (ClampMin = "0.0"))
	float AbsorbDuration = 0.4f;

	/** 플레이어 액터 위치에서 흡수 도착 지점까지의 오프셋. 몸 중심보다 위아래로 옮길 때 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Absorb")
	FVector AbsorbTargetOffset = FVector::ZeroVector;

	/** 흡수되는 동안 올라가는 최종 발광 세기. 플레이어에게 닿을 때 가장 밝다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Absorb", meta = (ClampMin = "0.0"))
	float AbsorbEmissive = 20.0f;

	/** 지정 코어 지급과 코어 선택 보상 중 사용할 방식을 정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Reward")
	ECoreRewardMode RewardMode = ECoreRewardMode::Fixed;

	/** 고정 보상으로 지급할 코어의 정의 데이터다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Reward",
		meta = (EditCondition = "RewardMode == ECoreRewardMode::Fixed", EditConditionHides))
	TObjectPtr<UCoreDataAsset> CoreData = nullptr;

	/**
	 * 코어 카드를 보여 줄 화면 WBP. 카드 배치와 모양은 해당 WBP에서 편집한다.
	 * 선택형은 후보 3장, 고정형은 지급할 코어 1장을 보여 주고 클릭하면 지급한다.
	 * 고정형에서 비워 두면 화면 없이 닿는 즉시 지급한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Reward")
	TSubclassOf<UCoreChoiceWidget> ChoiceWidgetClass;

	/** 선택형 보상을 추첨할 코어 분류다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Reward",
		meta = (EditCondition = "RewardMode == ECoreRewardMode::Choice", EditConditionHides))
	ECoreCategory ChoiceCategory = ECoreCategory::Stat;

	/** 보상 지급이 완료되었는지 나타낸다. 이미 소비한 보상의 재지급을 막는다. */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Core|Reward")
	bool bConsumed = false;

private:
	/** 외부 저장 시스템은 이 ID와 bool 소모 상태를 함께 보관한다. 런타임 bool은 맵 원본에 저장하지 않는다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Core|Persistence", meta = (AllowPrivateAccess = "true"))
	FGuid PickupId;

	void EnsurePickupId();
	void MarkConsumed();

	/** BeginPlay에서 이미 지급한 기록이 있으면 적용한다. 보상 없이 소모 상태로 바꾸고 픽업을 숨긴다. */
	void RestoreConsumedState(bool bWasConsumed);
	UCorePickupStateSubsystem* GetPickupStateSubsystem() const;
	bool bPersistenceInitialized = false;

	/** 화면 WBP가 없는 고정 보상을 닿는 즉시 지급한다. 성공하면 보상을 소비하고 true를 반환한다. */
	bool TryAcquireFixedCore(UCoreComponent* InCoreComponent);

	/**
	 * 후보를 확보한 뒤 코어가 플레이어에게 빨려 들어가는 연출을 재생하고, 도착하면 선택 화면을 연다.
	 * 선택에 성공하기 전에는 픽업을 소비하지 않으며, 취소되면 코어를 제자리로 되돌린다.
	 */
	bool TryOpenCoreChoice(APlayerBase* Player);

	/**
	 * 고정 보상을 카드 1장 화면으로 보여 준다. 받을 수 있는 코어인지 먼저 확인하고,
	 * 흡수 연출이 끝나면 화면을 연다. 지급은 카드를 클릭했을 때 한다.
	 */
	bool TryOpenFixedCoreScreen(APlayerBase* Player);

	/**
	 * 선택형·고정형 공통으로 화면을 시작할 수 있는지 확인한다(권한·저장 상태·중복 처리·열린 화면·로컬 플레이어).
	 * 시작할 수 있으면 플레이어의 코어 컴포넌트를, 아니면 nullptr을 반환한다.
	 */
	UCoreComponent* CanStartChoosing(APlayerBase* Player, ECoreRewardMode Mode) const;

	/**
	 * 화면을 열 플레이어를 기록하고 선택 도중 사망·파괴되면 정리하도록 알림을 연결한 뒤,
	 * 코어가 플레이어에게 빨려 들어가는 연출을 시작한다. 연출이 끝나면 화면을 열고, 연출 시간이 0이면 바로 연다.
	 */
	bool StartChoosing(APlayerBase* Player);

	/** 화면에서 지급에 성공한 뒤 소모를 기록하고, 화면을 닫고, 이미 빨려 들어간 코어를 숨긴다. */
	void FinishChosenReward();

	/** 화면을 연 플레이어가 아직 선택할 수 있는지(생존·같은 컨트롤러) 확인하고, 아니면 화면을 닫고 픽업을 되돌린다. */
	APlayerBase* GetValidChooser();

	/** 고정 보상 화면의 플레이어가 아직 유효한지 확인하고, 아니면 화면을 닫고 픽업을 되돌린다. */
	UCoreComponent* GetValidFixedChooser();

	/** 선택형 보상 세션이 아직 같은 플레이어의 것인지 확인하고, 더 이상 유효하지 않은 선택은 정리한다. */
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

	/** 획득 범위를 끄고 대상에게 빨려 들어가는 연출을 시작한다. */
	void StartAbsorb(AActor* Target);

	/** 흡수 연출을 마친다. 이미 지급한 보상은 픽업을 숨기고, 화면으로 지급할 보상은 코어를 숨긴 뒤 화면을 연다. */
	void FinishAbsorb();

	/** 확보한 후보로 선택 화면을 열고 입력 이벤트를 연결한다. 실패하면 선택 기록을 정리한다. */
	bool OpenChoiceScreen();

	/** 선택을 마치지 못한 코어를 배치 위치로 되돌리고 다시 획득할 수 있게 한다. */
	void RestorePickup();

	/** 배치한 위치를 중심으로 떠다니고, 회전하며, 크기와 발광이 맥박친다. */
	void UpdateIdle(float DeltaSeconds);

	/** 대상 쪽으로 가속하며 이동하고, 작아지면서 밝아진다. 도착하면 true를 반환한다. */
	bool UpdateAbsorb(float DeltaSeconds);

	/** 보상 방식(고정형)과 선택 분류(스탯·능력)에 맞는 픽업 색을 고른다. */
	FLinearColor GetRewardColor() const;

	/**
	 * 픽업만 보고도 어떤 보상인지 알 수 있게 코어 색을 바꾼다.
	 * 에디터에서 배치할 때도 보이도록 OnConstruction에서 부르고, 게임 시작 시 BeginPlay에서 다시 부른다.
	 */
	void ApplyRewardColor();

	/** 발광 파라미터가 있으면 머티리얼의 발광 세기를 바꾼다. */
	void SetEmissive(float Value);

	/** 화면에서 선택한 코어의 지급을 요청한다. 성공하면 화면을 닫고 픽업을 숨긴다. */
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

	/** 발광 세기를 바꾸기 위해 코어 메시에 적용한 머티리얼 인스턴스. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CoreMaterial;

	/** 흡수 연출의 도착 대상. 대상이 사라지면 연출을 바로 마친다. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> AbsorbTarget;

	/** 대기 움직임의 기준이 되는 배치 위치·크기와 경과 시간. */
	FVector IdleBaseLocation = FVector::ZeroVector;
	FVector IdleBaseScale = FVector::OneVector;
	float IdleTime = 0.0f;

	/** 흡수 연출을 시작한 위치·크기·발광과 경과 시간. */
	FVector AbsorbStartLocation = FVector::ZeroVector;
	FVector AbsorbStartScale = FVector::OneVector;
	float AbsorbStartEmissive = 0.0f;
	float AbsorbElapsed = 0.0f;
	bool bIsAbsorbing = false;

	/** 선택형 보상에서 흡수가 끝나면 선택 화면을 열어야 하는지, 선택을 기다리며 코어를 숨겼는지 나타낸다. */
	bool bOpenChoiceAfterAbsorb = false;
	bool bHiddenForChoice = false;

	/** 마지막으로 적용한 발광 세기. 흡수 시작 시 밝기를 이어받는다. */
	float CurrentEmissive = 0.0f;

	/** 현재 보상을 처리 중인지 나타낸다. 획득·재추첨 요청이 겹치는 것을 막는다. */
	bool bIsGrantingReward = false;
};
