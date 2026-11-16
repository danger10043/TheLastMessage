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
constexpr TCHAR CoreViewerBinding[] = TEXT("CoreViewer");

bool IsBindableKey(const FKey& Key)
{
	return Key.IsValid() && Key.IsBindableToActions() && !Key.IsGamepadKey() && !Key.IsTouch() && !Key.IsAxis2D()
		&& !Key.IsAxis3D() && (!Key.IsAxis1D() || Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown)
		&& Key != EKeys::Escape;
}

FString BindingId(const FEnhancedActionKeyMapping& Mapping)
{
	return Mapping.Action->GetName() + TEXT(".") + Mapping.Key.GetFName().ToString();
}

FText GetBindingLabel(const FEnhancedActionKeyMapping& Mapping)
{
	if (!Mapping.Action || !IsBindableKey(Mapping.Key)) return FText::GetEmpty();
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
		if (IsBindableKey(Key)) SavedKeys.Add(Id, Key);
	}
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
		ApplyKeys();
	}
	return RuntimeContext;
}

TArray<FGameKeyBinding> UGameKeySettingsSubsystem::GetBindings() const
{
	auto Bindings = GetDefaultBindings();
	const auto* Context = GetDefaultContext();
	if (!Context) return Bindings;
	for (auto& Binding : Bindings)
	{
		const FString Id = Binding.MappingIndex == INDEX_NONE ? CoreViewerBinding : BindingId(Context->GetMappings()[Binding.MappingIndex]);
		if (const auto* Saved = SavedKeys.Find(Id)) Binding.Key = *Saved;
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
		Bindings.Add({Index, FName(*Name), MoveTemp(Label), Mapping.Key});
	}
	Bindings.Add({INDEX_NONE, FName(CoreViewerBinding), NSLOCTEXT("KeySettings", "CoreViewer", "코어 창"),
		GetDefault<UOwnedCoreViewerSubsystem>()->GetDefaultToggleKey()});
	return Bindings;
}

FKey UGameKeySettingsSubsystem::GetCoreViewerKey() const
{
	if (const FKey* Saved = SavedKeys.Find(CoreViewerBinding)) return *Saved;
	return GetDefault<UOwnedCoreViewerSubsystem>()->GetDefaultToggleKey();
}

bool UGameKeySettingsSubsystem::TrySetKey(int32 MappingIndex, FKey Key, FText& Error)
{
	const auto* Context = GetDefaultContext();
	const bool bCoreViewer = MappingIndex == INDEX_NONE;
	if (!Context || (!bCoreViewer && !Context->GetMappings().IsValidIndex(MappingIndex))) return false;
	const auto& Mappings = Context->GetMappings();
	if (!bCoreViewer && GetBindingLabel(Mappings[MappingIndex]).IsEmpty()) return false;

	if (!IsBindableKey(Key))
	{
		Error = NSLOCTEXT("KeySettings", "InvalidKey", "이 키는 지정할 수 없습니다.");
		return false;
	}
	const FString Id = bCoreViewer ? CoreViewerBinding : BindingId(Mappings[MappingIndex]);
	const FKey DefaultKey = bCoreViewer ? GetDefault<UOwnedCoreViewerSubsystem>()->GetDefaultToggleKey() : Mappings[MappingIndex].Key;
	const FKey* Previous = SavedKeys.Find(Id);
	if ((Previous ? *Previous : DefaultKey) == Key) return true;
	if (!bCoreViewer && GetCoreViewerKey() == Key)
	{
		Error = FText::Format(NSLOCTEXT("KeySettings", "KeyUsed", "{0} 키는 이미 사용 중입니다."), Key.GetDisplayName());
		return false;
	}
	for (int32 Index = 0; Index < Mappings.Num(); ++Index)
	{
		if (Index == MappingIndex || !Mappings[Index].Action) continue;
		const FKey* Saved = SavedKeys.Find(BindingId(Mappings[Index]));
		if ((Saved ? *Saved : Mappings[Index].Key) == Key)
		{
			Error = FText::Format(NSLOCTEXT("KeySettings", "KeyUsed", "{0} 키는 이미 사용 중입니다."), Key.GetDisplayName());
			return false;
		}
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

void UGameKeySettingsSubsystem::ApplyKeys()
{
	if (auto* CoreViewer = GetLocalPlayer()->GetSubsystem<UOwnedCoreViewerSubsystem>())
		CoreViewer->RefreshToggleBinding();
	const auto* Context = GetDefaultContext();
	if (!RuntimeContext || !Context) return;
	const auto& Mappings = Context->GetMappings();
	for (int32 Index = 0; Index < Mappings.Num(); ++Index)
	{
		const auto& Mapping = Mappings[Index];
		const FKey* Key = GetBindingLabel(Mapping).IsEmpty() ? nullptr : SavedKeys.Find(BindingId(Mapping));
		RuntimeContext->GetMapping(Index).Key = Key ? *Key : Mapping.Key;
	}
	if (auto* Inputs = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		Inputs->RequestRebuildControlMappings();
}
