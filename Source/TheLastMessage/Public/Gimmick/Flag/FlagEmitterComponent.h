#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "FlagEmitterComponent.generated.h"

class AGimmickBase;

UENUM(BlueprintType)
enum class EFlagEmitMode : uint8
{
	/** 기믹이 켜지면 플래그를 켜고, 꺼지면 끈다. */
	Follow UMETA(DisplayName = "Follow Gimmick"),
	/** 기믹이 한 번 켜지면 플래그를 켜고, 꺼져도 유지한다. */
	LatchOn UMETA(DisplayName = "Latch On")
};

/**
 * 스위치/퓨즈 소켓 같은 기믹 액터에 붙여서 월드 플래그를 켜는 컴포넌트.
 * Flag 태그만 고르면 되고, 같은 태그를 FlagReceiver를 단 문에 지정하면 서로 다른 레벨에 있어도 연결된다.
 * 기믹이 아닌 액터는 블루프린트에서 SetFlagState를 호출한다.
 */
UCLASS(ClassGroup = (Gimmick), meta = (BlueprintSpawnableComponent))
class THELASTMESSAGE_API UFlagEmitterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFlagEmitterComponent();

	/** 켤 플래그. 예: Gate.RH0406 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flag")
	FGameplayTag Flag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flag")
	EFlagEmitMode Mode = EFlagEmitMode::Follow;

	/** 기믹이 아닌 액터나 블루프린트 이벤트에서 직접 켜고 끌 때 사용한다. */
	UFUNCTION(BlueprintCallable, Category = "Flag")
	void SetFlagState(bool bValue);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleOwnerActiveStateChanged(AGimmickBase* Source, bool bActive);
};
