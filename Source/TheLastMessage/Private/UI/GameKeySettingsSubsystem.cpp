#include "UI/GameKeySettingsSubsystem.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Misc/ConfigCacheIni.h"
#include "UI/OwnedCoreViewerSubsystem.h"

namespace
{
constexpr TCHAR GameplayContextPath[] = TEXT("/Game/Main/Inputs/IMC_SideScroller.IMC_SideScroller");
constexpr TCHAR KeySettingsSection[] = TEXT("TheLastMessage.KeyBindings");
constexpr TCHAR InputSettingsSection[] = TEXT("TheLastMessage.Input");
constexpr TCHAR InputDeviceKey[] = TEXT("Device");
constexpr TCHAR CoreViewerBinding[] = TEXT("CoreViewer");
constexpr TCHAR CoreViewerGamepadBinding[] = TEXT("CoreViewerGamepad");

// EKeys는 다른 번역 단위의 정적 변수라 전역 초기화에 쓰지 않고 함수로 꺼낸다.
FKey GetDefaultCoreViewerGamepadKey() { return EKeys::Gamepad_FaceButton_Top; }

bool IsBindableKeyboardKey(const FKey& Key)
{
	return Key.IsValid() && Key.IsBindableToActions() && !Key.IsGamepadKey() && !Key.IsTouch() && !Key.IsAxis2D()
		&& !Key.IsAxis3D() && (!Key.IsAxis1D() || Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown)
		&& Key != EKeys::Escape;
}

bool IsBindableGamepadKey(const FKey& Key)
{
	if (!Key.IsValid() || !Key.IsGamepadKey() || !Key.IsBindableToActions() || Key.IsAxis1D() || Key.IsAxis2D() || Key.IsAxis3D())
		return false;
	// 스틱 방향은 이동·조준에 쓰므로 버튼으로 지정하지 않는다(스틱 누르기는 허용).
	const FString Name = Key.GetFName().ToString();
	return !Name.StartsWith(TEXT("Gamepad_LeftStick_")) && !Name.StartsWith(TEXT("Gamepad_RightStick_"));
}

bool IsAction(const FEnhancedActionKeyMapping& Mapping, const TCHAR* Name)
{
	return Mapping.Action && Mapping.Action->GetFName() == FName(Name);
}

// 일시정지와 컷신 건너뛰기는 조작 방식과 관계없이 키보드·패드 모두 받는다.
bool IsSystemMapping(const FEnhancedActionKeyMapping& Mapping)
{
	return IsAction(Mapping, TEXT("IA_PauseMenu")) || IsAction(Mapping, TEXT("IA_Skip"));
}

FString BindingId(const FEnhancedActionKeyMapping& Mapping)
{
	return Mapping.Action->GetName() + TEXT(".") + Mapping.Key.GetFName().ToString();
}

FText GetActionLabel(const FEnhancedActionKeyMapping& Mapping)
{
	if (!Mapping.Action) return FText::GetEmpty();
	const FName Action = Mapping.Action->GetFName();
	if (Action == TEXT("IA_Move"))
	{
		const bool bLeft = Mapping.Modifiers.ContainsByPredicate([](const UInputModifier* Modifier)
		{
			return Modifier && Modifier->IsA<UInputModifierNegate>();
		});
		return bLeft ? NSLOCTEXT("KeySettings", "MoveLeft", "왼쪽 이동") : NSLOCTEXT("KeySettings", "MoveRight", "오른쪽 이동");
	}
	if (Action == TEXT("IA_Jump")) return NSLOCTEXT("KeySettings", "Jump", "점프");
	if (Action == TEXT("IA_Drop")) return NSLOCTEXT("KeySettings", "Drop", "하향 점프");
	if (Action == TEXT("IA_PrimaryWeapon")) return NSLOCTEXT("KeySettings", "Primary", "주무기");
	if (Action == TEXT("IA_SecondaryWeapon")) return NSLOCTEXT("KeySettings", "Secondary", "보조 무기");
	if (Action == TEXT("IA_Guard")) return NSLOCTEXT("KeySettings", "Guard", "가드");
	if (Action == TEXT("IA_GrapplingHook")) return NSLOCTEXT("KeySettings", "Grapple", "그래플링 훅");
	if (Action == TEXT("IA_Interact")) return NSLOCTEXT("KeySettings", "Interact", "상호작용");
	if (Action == TEXT("IA_Map")) return NSLOCTEXT("KeySettings", "Map", "지도");
	if (Action == TEXT("IA_Weapon1")) return NSLOCTEXT("KeySettings", "Weapon1", "무기 슬롯 1");
	if (Action == TEXT("IA_Weapon2")) return NSLOCTEXT("KeySettings", "Weapon2", "무기 슬롯 2");
	if (Action == TEXT("IA_Weapon3")) return NSLOCTEXT("KeySettings", "Weapon3", "무기 슬롯 3");
	if (Action == TEXT("IA_Weapon4")) return NSLOCTEXT("KeySettings", "Weapon4", "무기 슬롯 4");
	return FText::GetEmpty();
}

/** 키 설정 화면의 한 줄이 되는 키보드·마우스 매핑이면 라벨, 아니면 빈 문자열. */
FText GetBindingLabel(const FEnhancedActionKeyMapping& Mapping)
{
	return IsBindableKeyboardKey(Mapping.Key) ? GetActionLabel(Mapping) : FText::GetEmpty();
}

/** 키 설정 화면에서 바꿀 수 있는 패드 매핑인지. 이동·하향 점프(스틱)는 표시만 한다. */
bool IsEditableGamepadMapping(const FEnhancedActionKeyMapping& Mapping)
{
	return Mapping.Key.IsGamepadKey() && IsBindableGamepadKey(Mapping.Key) && !GetActionLabel(Mapping).IsEmpty()
		&& !IsAction(Mapping, TEXT("IA_Move")) && !IsAction(Mapping, TEXT("IA_Drop"));
}
}

void UGameKeySettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TArray<FString> Entries;
	GConfig->GetSection(KeySettingsSection, Entries, GGameUserSettingsIni);
	for (const FString& Entry : Entries)
	{
		FString Id, Name;
		if (!Entry.Split(TEXT("="), &Id, &Name)) continue;
		const FKey Key{FName(*Name)};
		if (IsBindableKeyboardKey(Key) || IsBindableGamepadKey(Key)) SavedKeys.Add(Id, Key);
	}
	FString Device;
	if (GConfig->GetString(InputSettingsSection, InputDeviceKey, Device, GGameUserSettingsIni))
		InputDevice = Device == TEXT("Gamepad") ? EGameInputDevice::Gamepad : EGameInputDevice::KeyboardMouse;
}

const UInputMappingContext* UGameKeySettingsSubsystem::GetDefaultContext()
{
	return LoadObject<UInputMappingContext>(nullptr, GameplayContextPath);
}

UInputMappingContext* UGameKeySettingsSubsystem::GetGameplayContext(UInputMappingContext* Context)
{
	if (!Context || Context->GetPathName() != GameplayContextPath) return Context;
	if (!RuntimeContext)
	{
		RuntimeContext = DuplicateObject<UInputMappingContext>(Context, this);
		AllRuntimeMappings = RuntimeContext->GetMappings();
		ApplyKeys();
	}
	return RuntimeContext;
}

TArray<FGameKeyBinding> UGameKeySettingsSubsystem::GetBindings() const
{
	auto Bindings = GetDefaultBindings();
	for (auto& Binding : Bindings)
	{
		Binding.Key = GetEffectiveKey(Binding.MappingIndex);
		if (Binding.GamepadMappingIndex != INDEX_NONE) Binding.GamepadKey = GetEffectiveKey(Binding.GamepadMappingIndex);
	}
	return Bindings;
}

TArray<FGameKeyBinding> UGameKeySettingsSubsystem::GetDefaultBindings()
{
	TArray<FGameKeyBinding> Bindings;
	const auto* Context = GetDefaultContext();
	if (!Context) return Bindings;
	const auto& Mappings = Context->GetMappings();
	for (int32 Index = 0; Index < Mappings.Num(); ++Index)
	{
		const auto& Mapping = Mappings[Index];
		FText Label = GetBindingLabel(Mapping);
		if (Label.IsEmpty()) continue;
		const FString Name = Mapping.Action->GetName() + TEXT("_") + Mapping.Key.GetFName().ToString();
		FGameKeyBinding Binding{Index, FName(*Name), MoveTemp(Label), Mapping.Key};
		if (IsAction(Mapping, TEXT("IA_Move")))
		{
			// 패드 이동은 왼쪽 스틱 하나(2D)라 줄마다 방향만 보여 준다.
			const bool bLeft = Mapping.Modifiers.ContainsByPredicate([](const UInputModifier* Modifier)
			{
				return Modifier && Modifier->IsA<UInputModifierNegate>();
			});
			Binding.GamepadKey = bLeft ? EKeys::Gamepad_LeftStick_Left : EKeys::Gamepad_LeftStick_Right;
		}
		else
		{
			Binding.GamepadMappingIndex = Mappings.IndexOfByPredicate([&Mapping](const FEnhancedActionKeyMapping& Other)
			{
				return Other.Action == Mapping.Action && Other.Key.IsGamepadKey();
			});
			if (Binding.GamepadMappingIndex != INDEX_NONE)
			{
				Binding.GamepadKey = Mappings[Binding.GamepadMappingIndex].Key;
				Binding.bGamepadEditable = IsEditableGamepadMapping(Mappings[Binding.GamepadMappingIndex]);
			}
		}
		Bindings.Add(MoveTemp(Binding));
	}
	FGameKeyBinding CoreViewer{CoreViewerIndex, FName(CoreViewerBinding), NSLOCTEXT("KeySettings", "CoreViewer", "코어 창"),
		GetDefault<UOwnedCoreViewerSubsystem>()->GetDefaultToggleKey()};
	CoreViewer.GamepadMappingIndex = CoreViewerGamepadIndex;
	CoreViewer.GamepadKey = GetDefaultCoreViewerGamepadKey();
	CoreViewer.bGamepadEditable = true;
	Bindings.Add(MoveTemp(CoreViewer));
	return Bindings;
}

FKey UGameKeySettingsSubsystem::GetEffectiveKey(int32 MappingIndex) const
{
	if (MappingIndex == CoreViewerIndex) return GetCoreViewerKey(EGameInputDevice::KeyboardMouse);
	if (MappingIndex == CoreViewerGamepadIndex) return GetCoreViewerKey(EGameInputDevice::Gamepad);
	const auto* Context = GetDefaultContext();
	if (!Context || !Context->GetMappings().IsValidIndex(MappingIndex)) return EKeys::Invalid;
	const auto& Mapping = Context->GetMappings()[MappingIndex];
	if (!Mapping.Action) return Mapping.Key;
	const FKey* Saved = SavedKeys.Find(BindingId(Mapping));
	return Saved ? *Saved : Mapping.Key;
}

FKey UGameKeySettingsSubsystem::GetCoreViewerKey() const
{
	return GetCoreViewerKey(InputDevice);
}

FKey UGameKeySettingsSubsystem::GetCoreViewerKey(EGameInputDevice Device) const
{
	if (Device == EGameInputDevice::Gamepad)
	{
		if (const FKey* Saved = SavedKeys.Find(CoreViewerGamepadBinding)) return *Saved;
		return GetDefaultCoreViewerGamepadKey();
	}
	if (const FKey* Saved = SavedKeys.Find(CoreViewerBinding)) return *Saved;
	return GetDefault<UOwnedCoreViewerSubsystem>()->GetDefaultToggleKey();
}

bool UGameKeySettingsSubsystem::TrySetKey(int32 MappingIndex, FKey Key, FText& Error)
{
	const auto* Context = GetDefaultContext();
	const bool bCoreKeyboard = MappingIndex == CoreViewerIndex;
	const bool bCoreGamepad = MappingIndex == CoreViewerGamepadIndex;
	const bool bCoreViewer = bCoreKeyboard || bCoreGamepad;
	if (!Context || (!bCoreViewer && !Context->GetMappings().IsValidIndex(MappingIndex))) return false;
	const auto& Mappings = Context->GetMappings();
	const bool bGamepadSlot = bCoreGamepad || (!bCoreViewer && Mappings[MappingIndex].Key.IsGamepadKey());
	if (!bCoreViewer)
	{
		const bool bEditable = bGamepadSlot ? IsEditableGamepadMapping(Mappings[MappingIndex])
			: !GetBindingLabel(Mappings[MappingIndex]).IsEmpty();
		if (!bEditable) return false;
	}

	if (bGamepadSlot ? !IsBindableGamepadKey(Key) : !IsBindableKeyboardKey(Key))
	{
		Error = bGamepadSlot ? NSLOCTEXT("KeySettings", "InvalidGamepadKey", "게임패드 버튼만 지정할 수 있습니다.")
			: NSLOCTEXT("KeySettings", "InvalidKey", "이 키는 지정할 수 없습니다.");
		return false;
	}
	const FString Id = bCoreKeyboard ? CoreViewerBinding : bCoreGamepad ? CoreViewerGamepadBinding : BindingId(Mappings[MappingIndex]);
	const FKey DefaultKey = bCoreKeyboard ? GetDefault<UOwnedCoreViewerSubsystem>()->GetDefaultToggleKey()
		: bCoreGamepad ? GetDefaultCoreViewerGamepadKey() : Mappings[MappingIndex].Key;
	if (GetEffectiveKey(MappingIndex) == Key) return true;

	const FText KeyName = bGamepadSlot ? GetGamepadKeyLabel(Key) : Key.GetDisplayName();
	const auto SetUsedError = [&Error, &KeyName]
	{
		Error = FText::Format(NSLOCTEXT("KeySettings", "KeyUsed", "{0} 키는 이미 사용 중입니다."), KeyName);
		return false;
	};
	if ((!bCoreKeyboard && GetCoreViewerKey(EGameInputDevice::KeyboardMouse) == Key)
		|| (!bCoreGamepad && GetCoreViewerKey(EGameInputDevice::Gamepad) == Key))
		return SetUsedError();
	for (int32 Index = 0; Index < Mappings.Num(); ++Index)
	{
		if (Index == MappingIndex || !Mappings[Index].Action) continue;
		// 패드 컷신 건너뛰기는 점프 버튼을 따라가므로 따로 겹침을 보지 않는다.
		if (IsAction(Mappings[Index], TEXT("IA_Skip")) && Mappings[Index].Key.IsGamepadKey()) continue;
		if (GetEffectiveKey(Index) == Key) return SetUsedError();
	}
	if (Key == DefaultKey)
	{
		SavedKeys.Remove(Id);
		GConfig->RemoveKey(KeySettingsSection, *Id, GGameUserSettingsIni);
	}
	else
	{
		SavedKeys.Add(Id, Key);
		GConfig->SetString(KeySettingsSection, *Id, *Key.GetFName().ToString(), GGameUserSettingsIni);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
	ApplyKeys();
	return true;
}

void UGameKeySettingsSubsystem::ResetKeys()
{
	SavedKeys.Reset();
	GConfig->EmptySection(KeySettingsSection, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	ApplyKeys();
}

void UGameKeySettingsSubsystem::SetInputDevice(EGameInputDevice Device)
{
	if (InputDevice == Device) return;
	InputDevice = Device;
	GConfig->SetString(InputSettingsSection, InputDeviceKey,
		Device == EGameInputDevice::Gamepad ? TEXT("Gamepad") : TEXT("KeyboardMouse"), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	ApplyKeys();
}

void UGameKeySettingsSubsystem::ApplyKeys()
{
	if (auto* CoreViewer = GetLocalPlayer()->GetSubsystem<UOwnedCoreViewerSubsystem>())
		CoreViewer->RefreshToggleBinding();
	const auto* Context = GetDefaultContext();
	if (!RuntimeContext || !Context || AllRuntimeMappings.Num() != Context->GetMappings().Num()) return;

	FKey GamepadJump;
	for (int32 Index = 0; Index < AllRuntimeMappings.Num(); ++Index)
		if (IsAction(AllRuntimeMappings[Index], TEXT("IA_Jump")) && AllRuntimeMappings[Index].Key.IsGamepadKey())
			GamepadJump = GetEffectiveKey(Index);

	// 고른 조작 방식의 매핑만 남긴다. 기본 키가 패드 키인지로 매핑의 장치를 판단한다.
	RuntimeContext->UnmapAll();
	for (int32 Index = 0; Index < AllRuntimeMappings.Num(); ++Index)
	{
		const FEnhancedActionKeyMapping& Source = AllRuntimeMappings[Index];
		if (!Source.Action) continue;
		const bool bGamepadMapping = Source.Key.IsGamepadKey();
		if (!IsSystemMapping(Source) && bGamepadMapping != IsUsingGamepad()) continue;
		FKey Key = GetEffectiveKey(Index);
		if (bGamepadMapping && IsAction(Source, TEXT("IA_Skip")) && GamepadJump.IsValid()) Key = GamepadJump;
		FEnhancedActionKeyMapping& Added = RuntimeContext->MapKey(Source.Action, Key);
		Added.Triggers = Source.Triggers;
		Added.Modifiers = Source.Modifiers;
	}
	if (auto* Inputs = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		Inputs->RequestRebuildControlMappings();
}

FText UGameKeySettingsSubsystem::GetGamepadKeyLabel(const FKey& Key)
{
	struct FLabel { FKey Key; FText Text; };
	const FLabel Labels[] = {
		{EKeys::Gamepad_FaceButton_Bottom, NSLOCTEXT("GamepadKeys", "A", "A")},
		{EKeys::Gamepad_FaceButton_Right, NSLOCTEXT("GamepadKeys", "B", "B")},
		{EKeys::Gamepad_FaceButton_Left, NSLOCTEXT("GamepadKeys", "X", "X")},
		{EKeys::Gamepad_FaceButton_Top, NSLOCTEXT("GamepadKeys", "Y", "Y")},
		{EKeys::Gamepad_LeftShoulder, NSLOCTEXT("GamepadKeys", "LB", "LB")},
		{EKeys::Gamepad_RightShoulder, NSLOCTEXT("GamepadKeys", "RB", "RB")},
		{EKeys::Gamepad_LeftTrigger, NSLOCTEXT("GamepadKeys", "LT", "LT")},
		{EKeys::Gamepad_RightTrigger, NSLOCTEXT("GamepadKeys", "RT", "RT")},
		{EKeys::Gamepad_LeftThumbstick, NSLOCTEXT("GamepadKeys", "LS", "LS 누르기")},
		{EKeys::Gamepad_RightThumbstick, NSLOCTEXT("GamepadKeys", "RS", "RS 누르기")},
		{EKeys::Gamepad_Special_Left, NSLOCTEXT("GamepadKeys", "View", "View")},
		{EKeys::Gamepad_Special_Right, NSLOCTEXT("GamepadKeys", "Menu", "Menu")},
		{EKeys::Gamepad_DPad_Up, NSLOCTEXT("GamepadKeys", "DPadUp", "십자키 ↑")},
		{EKeys::Gamepad_DPad_Down, NSLOCTEXT("GamepadKeys", "DPadDown", "십자키 ↓")},
		{EKeys::Gamepad_DPad_Left, NSLOCTEXT("GamepadKeys", "DPadLeft", "십자키 ←")},
		{EKeys::Gamepad_DPad_Right, NSLOCTEXT("GamepadKeys", "DPadRight", "십자키 →")},
		{EKeys::Gamepad_LeftStick_Up, NSLOCTEXT("GamepadKeys", "LeftStickUp", "왼쪽 스틱 ↑")},
		{EKeys::Gamepad_LeftStick_Down, NSLOCTEXT("GamepadKeys", "LeftStickDown", "왼쪽 스틱 ↓")},
		{EKeys::Gamepad_LeftStick_Left, NSLOCTEXT("GamepadKeys", "LeftStickLeft", "왼쪽 스틱 ←")},
		{EKeys::Gamepad_LeftStick_Right, NSLOCTEXT("GamepadKeys", "LeftStickRight", "왼쪽 스틱 →")},
		{EKeys::Gamepad_Left2D, NSLOCTEXT("GamepadKeys", "LeftStick", "왼쪽 스틱")},
		{EKeys::Gamepad_Right2D, NSLOCTEXT("GamepadKeys", "RightStick", "오른쪽 스틱")},
	};
	for (const FLabel& Label : Labels)
		if (Label.Key == Key) return Label.Text;
	return Key.GetDisplayName();
}
