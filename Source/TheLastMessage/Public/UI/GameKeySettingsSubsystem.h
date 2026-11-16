#pragma once

#include "InputCoreTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameKeySettingsSubsystem.generated.h"

class UInputMappingContext;

struct FGameKeyBinding
{
	int32 MappingIndex;
	FName WidgetName;
	FText Label;
	FKey Key;
};

UCLASS()
class THELASTMESSAGE_API UGameKeySettingsSubsystem final : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UInputMappingContext* GetGameplayContext(UInputMappingContext* Context);
	TArray<FGameKeyBinding> GetBindings() const;
	static TArray<FGameKeyBinding> GetDefaultBindings();
	FKey GetCoreViewerKey() const;
	bool TrySetKey(int32 MappingIndex, FKey Key, FText& Error);
	void ResetKeys();

private:
	void ApplyKeys();
	static const UInputMappingContext* GetDefaultContext();
	TMap<FString, FKey> SavedKeys;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> RuntimeContext;
};
