#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Guide/ControlGuideTypes.h"
#include "ControlGuideWidget.generated.h"

class UImage;
class UTexture2D;
class UMaterialInterface;

/** 조작법 안내 UI의 부모 클래스 */
UCLASS()
class THELASTMESSAGE_API UControlGuideWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 액터가 전달한 안내 종류를 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "Control Guide")
	void SetGuideType(EControlGuideType InGuideType);

	/** 안내 그림을 저장하고, 생성된 이미지 위젯에 반영한다. */
	UFUNCTION(BlueprintCallable, Category = "Control Guide")
	void SetGuideTexture(UTexture2D* InTexture);

	/** 투명 영상을 표시할 UI 머티리얼을 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "Control Guide")
	void SetGuideMaterial(UMaterialInterface* InMaterial);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	/** 현재 위젯에서 표시할 안내 종류 */
	UPROPERTY(BlueprintReadOnly, Category = "Control Guide")
	EControlGuideType GuideType = EControlGuideType::Move;

	/** 기존 위젯 블루프린트와의 호환용 이벤트. C++ 안내판에서는 구현이 필요 없다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Control Guide")
	void OnGuideTypeChanged(EControlGuideType NewGuideType);

private:
	void BuildGuideLayout();
	void RefreshGuideImage();

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> GuideTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> GuideMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UImage> GuideImage;
};
