#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameAudioSettingsSubsystem.generated.h"

class USoundClass;
class USoundMix;
struct FActorsInitializedParams;

enum class EGameVolume : uint8
{
	Master,
	Effects,
	Interface,
	Music,
	Ambience,
	Count
};

UCLASS()
class UGameAudioSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	float GetVolume(EGameVolume Channel) const;
	void SetVolume(EGameVolume Channel, float Volume);
	void ToggleMute(EGameVolume Channel);
	void SaveVolumes();

private:
	void OnWorldInitialized(const FActorsInitializedParams& Params);
	void ApplyVolumes(UWorld* World);

	UPROPERTY() TObjectPtr<USoundMix> VolumeMix;
	UPROPERTY() TObjectPtr<USoundClass> InterfaceClass;
	UPROPERTY() TObjectPtr<USoundClass> MusicClass;
	UPROPERTY() TObjectPtr<USoundClass> AmbienceClass;
	float Volumes[static_cast<int32>(EGameVolume::Count)] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	float LastAudibleVolumes[static_cast<int32>(EGameVolume::Count)] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	TSet<uint32> AppliedDevices;
	FDelegateHandle WorldInitializedHandle;
	bool bVolumesDirty = false;
};
