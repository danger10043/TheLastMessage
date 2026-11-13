#include "UI/GameAudioSettingsSubsystem.h"

#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UI/FactoryUIAssetSettings.h"

namespace
{
constexpr TCHAR VolumeSection[] = TEXT("TheLastMessage.Audio");
constexpr const TCHAR* VolumeKeys[] = {TEXT("Master"), TEXT("Effects"), TEXT("Interface"), TEXT("Music"), TEXT("Ambience")};
static_assert(UE_ARRAY_COUNT(VolumeKeys) == static_cast<int32>(EGameVolume::Count));
}

void UGameAudioSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (IsRunningDedicatedServer()) return;

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(VolumeKeys); ++Index)
	{
		float Value = 1.0f;
		GConfig->GetFloat(VolumeSection, VolumeKeys[Index], Value, GGameUserSettingsIni);
		Volumes[Index] = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 1.0f) : 1.0f;
		const FString RestoreKey = FString(VolumeKeys[Index]) + TEXT("LastAudible");
		Value = Volumes[Index] > 0.0f ? Volumes[Index] : 1.0f;
		GConfig->GetFloat(VolumeSection, *RestoreKey, Value, GGameUserSettingsIni);
		if (FMath::IsFinite(Value) && Value > 0.0f) LastAudibleVolumes[Index] = FMath::Min(Value, 1.0f);
	}

	VolumeMix = NewObject<USoundMix>(this);
	VolumeMix->Duration = -1.0f;
	VolumeMix->FadeInTime = 0.0f;
	VolumeMix->FadeOutTime = 0.0f;
	InterfaceClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Main/Audio/Core/SC_CoreUI.SC_CoreUI"));
	MusicClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Main/Audio/Classes/SC_Music.SC_Music"));
	AmbienceClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Main/Audio/Classes/SC_Ambience.SC_Ambience"));
	if (InterfaceClass)
	{
		// Slate plays these same assets directly. Give them the existing core UI sound class.
		const auto* Assets = GetDefault<UFactoryUIAssetSettings>();
		for (const auto& Asset : {Assets->HoverSound, Assets->ClickSound, Assets->ConfirmSound, Assets->CancelSound})
		{
			if (USoundBase* Sound = Asset.LoadSynchronous()) Sound->SoundClassObject = InterfaceClass;
		}
	}

	WorldInitializedHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &ThisClass::OnWorldInitialized);
	ApplyVolumes(GetWorld());
}

void UGameAudioSettingsSubsystem::Deinitialize()
{
	SaveVolumes();
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitializedHandle);
	if (FAudioDeviceManager* Manager = GEngine ? GEngine->GetAudioDeviceManager() : nullptr)
	{
		for (uint32 DeviceId : AppliedDevices)
		{
			if (FAudioDeviceHandle Device = Manager->GetAudioDevice(DeviceId))
			{
				Device->PopSoundMixModifier(VolumeMix);
				Device->SetTransientPrimaryVolume(1.0f);
			}
		}
	}
	Super::Deinitialize();
}

float UGameAudioSettingsSubsystem::GetVolume(EGameVolume Channel) const
{
	return Volumes[static_cast<int32>(Channel)];
}

void UGameAudioSettingsSubsystem::SetVolume(EGameVolume Channel, float Volume)
{
	float& Current = Volumes[static_cast<int32>(Channel)];
	if (!FMath::IsFinite(Volume)) return;
	Volume = FMath::Clamp(Volume, 0.0f, 1.0f);
	if (Current == Volume) return;
	Current = Volume;
	if (Volume > 0.0f) LastAudibleVolumes[static_cast<int32>(Channel)] = Volume;
	bVolumesDirty = true;
	ApplyVolumes(GetWorld());
}

void UGameAudioSettingsSubsystem::ToggleMute(EGameVolume Channel)
{
	SetVolume(Channel, GetVolume(Channel) > 0.0f ? 0.0f : LastAudibleVolumes[static_cast<int32>(Channel)]);
}

void UGameAudioSettingsSubsystem::SaveVolumes()
{
	if (!bVolumesDirty) return;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(VolumeKeys); ++Index)
	{
		GConfig->SetFloat(VolumeSection, VolumeKeys[Index], Volumes[Index], GGameUserSettingsIni);
		const FString RestoreKey = FString(VolumeKeys[Index]) + TEXT("LastAudible");
		GConfig->SetFloat(VolumeSection, *RestoreKey, LastAudibleVolumes[Index], GGameUserSettingsIni);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
	bVolumesDirty = false;
}

void UGameAudioSettingsSubsystem::OnWorldInitialized(const FActorsInitializedParams& Params)
{
	if (Params.World->GetGameInstance() == GetGameInstance()) ApplyVolumes(Params.World);
}

void UGameAudioSettingsSubsystem::ApplyVolumes(UWorld* World)
{
	if (!World || !VolumeMix) return;
	FAudioDeviceHandle Device = World->GetAudioDevice();
	if (!Device) return;

	if (!AppliedDevices.Contains(Device.GetDeviceID()))
	{
		AppliedDevices.Add(Device.GetDeviceID());
		Device->PushSoundMixModifier(VolumeMix);
	}
	Device->SetTransientPrimaryVolume(GetVolume(EGameVolume::Master));
	// Most gameplay cues use the engine's default sound class. Keep the override on
	// that class only so an explicitly routed UI cue has its own independent level.
	if (USoundClass* EffectsClass = GetDefault<UAudioSettings>()->GetDefaultSoundClass())
		Device->SetSoundMixClassOverride(VolumeMix, EffectsClass, GetVolume(EGameVolume::Effects), 1.0f, 0.03f, false);
	if (InterfaceClass)
		Device->SetSoundMixClassOverride(VolumeMix, InterfaceClass, GetVolume(EGameVolume::Interface), 1.0f, 0.03f, false);
	if (MusicClass)
		Device->SetSoundMixClassOverride(VolumeMix, MusicClass, GetVolume(EGameVolume::Music), 1.0f, 0.03f, false);
	if (AmbienceClass)
		Device->SetSoundMixClassOverride(VolumeMix, AmbienceClass, GetVolume(EGameVolume::Ambience), 1.0f, 0.03f, false);
}
