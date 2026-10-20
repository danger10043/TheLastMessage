#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerResourceBarWidget.generated.h"

class UProgressBar;
class UTextBlock;

/** Values are native; layout, font and bar brushes belong to a reusable Widget Blueprint. */
UCLASS(Blueprintable)
class THELASTMESSAGE_API UPlayerResourceBarWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Player UI")
	void SetResourceValues(float Current, float Maximum);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor FillColor = FLinearColor(0.9f, 0.0f, 0.0f, 1.0f);
	UPROPERTY(BlueprintReadOnly, Category="Player UI") float CurrentValue = 0;
	UPROPERTY(BlueprintReadOnly, Category="Player UI") float MaximumValue = 0;
protected:
	virtual void NativePreConstruct() override;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UProgressBar> ValueBar;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UTextBlock> ValueText;
};
