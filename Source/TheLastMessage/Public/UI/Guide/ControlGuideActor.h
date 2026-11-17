#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/Guide/ControlGuideTypes.h"
#include "ControlGuideActor.generated.h"

class USceneComponent;
class UWidgetComponent;

/** 맵에 배치하는 조작법 안내 액터 */
UCLASS()
class THELASTMESSAGE_API AControlGuideActor : public AActor
{
	GENERATED_BODY()

public:
	AControlGuideActor();

protected:
	virtual void BeginPlay() override;

	/** 안내판의 위치와 회전 기준 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Control Guide")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 조작법 UI를 월드에 표시하는 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Control Guide")
	TObjectPtr<UWidgetComponent> GuideWidgetComponent;

	/** 이 안내판에서 보여줄 조작법 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Control Guide")
	EControlGuideType GuideType = EControlGuideType::Move;
};
