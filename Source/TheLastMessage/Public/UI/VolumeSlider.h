#pragma once

#include "Components/Slider.h"
#include "VolumeSlider.generated.h"

UCLASS()
class UVolumeSlider : public USlider
{
	GENERATED_BODY()

public:
	FSimpleDelegate OnToggleMute;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void ToggleMute() { OnToggleMute.ExecuteIfBound(); }
};
