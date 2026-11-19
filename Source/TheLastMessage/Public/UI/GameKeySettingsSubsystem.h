#pragma once

#include "InputCoreTypes.h"
#include "EnhancedActionKeyMapping.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameKeySettingsSubsystem.generated.h"

class UInputMappingContext;

/** 게임 조작에 쓰는 입력 장치. 고른 장치의 키만 게임 조작에 쓰인다(일시정지·컷신 건너뛰기는 둘 다 쓴다). */
UENUM(BlueprintType)
enum class EGameInputDevice : uint8
{
	KeyboardMouse,
	Gamepad
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameInputDeviceChanged, EGameInputDevice);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLastUsedInputDeviceChanged, EGameInputDevice, Device);

struct FGameKeyBinding
{
	/** 키보드·마우스 매핑 인덱스. 코어 창은 CoreViewerIndex. */
	int32 MappingIndex;
	FName WidgetName;
	FText Label;
	FKey Key;
	/** 같은 동작의 게임패드 매핑 인덱스. 코어 창은 CoreViewerGamepadIndex, 없으면 INDEX_NONE. */
	int32 GamepadMappingIndex = INDEX_NONE;
	FKey GamepadKey;
	/** 이동(왼쪽 스틱)처럼 표시만 하고 바꿀 수 없는 패드 입력이면 false. */
	bool bGamepadEditable = false;
};

UCLASS()
class THELASTMESSAGE_API UGameKeySettingsSubsystem final : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 CoreViewerIndex = INDEX_NONE;
	static constexpr int32 CoreViewerGamepadIndex = -2;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UInputMappingContext* GetGameplayContext(UInputMappingContext* Context);
	TArray<FGameKeyBinding> GetBindings() const;
	static TArray<FGameKeyBinding> GetDefaultBindings();
	/** 현재 조작 방식에서 코어 창을 여닫는 키. */
	FKey GetCoreViewerKey() const;
	FKey GetCoreViewerKey(EGameInputDevice Device) const;
	/** MappingIndex는 키보드·패드 매핑 인덱스 또는 CoreViewerIndex / CoreViewerGamepadIndex. */
	bool TrySetKey(int32 MappingIndex, FKey Key, FText& Error);
	void ResetKeys();

	UFUNCTION(BlueprintPure, Category = "Input Settings")
	EGameInputDevice GetInputDevice() const { return InputDevice; }
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	void SetInputDevice(EGameInputDevice Device);
	UFUNCTION(BlueprintPure, Category = "Input Settings")
	bool IsUsingGamepad() const { return InputDevice == EGameInputDevice::Gamepad; }

	FOnGameInputDeviceChanged OnInputDeviceChanged;

	UFUNCTION(BlueprintPure, Category = "Input Device")
	EGameInputDevice GetLastUsedInputDevice() const { return LastUsedInputDevice; }

	UPROPERTY(BlueprintAssignable, Category = "Input Device")
	FOnLastUsedInputDeviceChanged OnLastUsedInputDeviceChanged;

	virtual void Deinitialize() override;

	/** Xbox 표기 버튼 이름(A, LB, 십자키 ↑ 등). 패드 키가 아니면 엔진 표시 이름. */
	static FText GetGamepadKeyLabel(const FKey& Key);

private:
	class FLastInputProcessor;
	TSharedPtr<FLastInputProcessor> LastInputProcessor;
	EGameInputDevice LastUsedInputDevice = EGameInputDevice::KeyboardMouse;

	void ApplyKeys();
	FKey GetEffectiveKey(int32 MappingIndex) const;
	static const UInputMappingContext* GetDefaultContext();
	TMap<FString, FKey> SavedKeys;
	EGameInputDevice InputDevice = EGameInputDevice::KeyboardMouse;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> RuntimeContext;
	/** RuntimeContext를 처음 복제했을 때의 전체 매핑(트리거·모디파이어 포함). 조작 방식이 바뀌면 여기서 다시 고른다. */
	UPROPERTY(Transient) TArray<FEnhancedActionKeyMapping> AllRuntimeMappings;
};
