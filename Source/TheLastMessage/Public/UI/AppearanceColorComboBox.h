#pragma once

#include "Components/ComboBoxString.h"
#include "AppearanceColorComboBox.generated.h"

UCLASS()
class UAppearanceColorComboBox final : public UComboBoxString
{
	GENERATED_BODY()

public:
	void CopyStyleFrom(const UComboBoxString& Source);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
