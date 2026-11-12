#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "Gimmick/Flag/RemoteGateSettings.h"
#include "RemoteGatePresenter.generated.h"

class APlayerController;
class ACameraActor;

/**
 * 멀리 있는 스위치가 문을 열 때, 암전 -> 방 로드 -> 문 위치 카메라 -> 문 열림 -> 복귀를 진행한다.
 * 설정(Project Settings > Remote Gate Presentation)에 항목이 없으면 만들어지지 않는다.
 * 문이 열리는 연출은 플래그 보류(UWorldFlagSubsystem::DeferHandler)로 카메라가 도착한 뒤에 재생된다.
 */
UCLASS()
class THELASTMESSAGE_API URemoteGatePresenter : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	bool IsPresenting() const { return Phase != EPhase::Idle; }

private:
	enum class EPhase : uint8 { Idle, FadingOut, Loading, FadingIn, Holding, FadingBack, Returning };

	bool HandleFlagDefer(FGameplayTag Flag, bool bValue);
	void Begin(const FRemoteGateEntry& Entry, APlayerController* Controller);
	void ShowFocusCamera();
	void RestorePlayerView();
	void Finish(bool bNotify);
	void FadeTo(float From, float To) const;
	void SetInputLocked(bool bLocked);

	EPhase Phase = EPhase::Idle;
	float PhaseTime = 0.f;
	float LoadedTime = 0.f;
	FRemoteGateEntry Active;
	bool bPinned = false;
	bool bInputLocked = false;
	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<ACameraActor> FocusCamera;
};
