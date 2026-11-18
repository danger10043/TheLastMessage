#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/Guide/ControlGuideTypes.h"
#include "ControlGuideActor.generated.h"

class USceneComponent;
class UTexture;
class UWidgetComponent;
class UMediaPlayer;
class UMediaSource;
class UMediaTexture;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UControlGuideWidget;

/** 맵에 배치하는 조작법 안내 액터 */
UCLASS()
class THELASTMESSAGE_API AControlGuideActor : public AActor
{
	GENERATED_BODY()

public:
	AControlGuideActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 안내판의 위치와 회전 기준 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Control Guide")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 조작법 UI를 월드에 표시하는 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Control Guide")
	TObjectPtr<UWidgetComponent> GuideWidgetComponent;

	/** 이 안내판에서 보여줄 조작법 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Control Guide")
	EControlGuideType GuideType = EControlGuideType::Move;

	/** 안내 종류별 그림 또는 미디어 텍스처. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Control Guide")
	TMap<EControlGuideType, TObjectPtr<UTexture>> GuideTextures;

	/** 미디어 텍스처를 사용하는 안내 종류별 영상 소스. 재생할 때만 불러온다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Control Guide|Video")
	TMap<EControlGuideType, TSoftObjectPtr<UMediaSource>> GuideMediaSources;

	/** 예전 무기 장착 안내 설정과의 호환용 소스. 종류별 소스가 있으면 그 값을 우선한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Control Guide|Video")
	TSoftObjectPtr<UMediaSource> WeaponEquipMediaSource;

	/** GuideVideoTexture 파라미터와 알파를 사용하는 UI 머티리얼. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Control Guide|Video")
	TObjectPtr<UMaterialInterface> GuideVideoMaterial;

private:
	void PlayGuideVideo(UControlGuideWidget* GuideWidget, UMediaTexture* TextureTemplate);

	UFUNCTION()
	void HandleMediaOpened(FString OpenedUrl);

	UFUNCTION()
	void HandleMediaOpenFailed(FString FailedUrl);

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> GuideMediaPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> GuideMediaTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GuideMediaMaterial;
};
