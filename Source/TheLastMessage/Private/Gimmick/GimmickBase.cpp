// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/GimmickBase.h"

#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "Save/ProgressSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
// +추가----
#include "Components/AudioComponent.h"
#include "CoreGlobals.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
// +--------

// Sets default values
AGimmickBase::AGimmickBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

	SetRootComponent(SceneRoot);

	InteractionEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("InteractionEffect"));

	InteractionEffect->SetupAttachment(SceneRoot);
	InteractionEffect->SetAutoActivate(false);
	// +추가----
	GimmickLoopAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("GimmickLoopAudio"));
	GimmickLoopAudio->SetupAttachment(SceneRoot);
	GimmickLoopAudio->SetAutoActivate(false);
	GimmickLoopAudio->bAutoDestroy = false;
	// +--------
}

bool AGimmickBase::CanInteractInternal(ACharacter* Player) const
{
	return bIsEnabled && bSupportsInteraction && IsValid(Player);
}

void AGimmickBase::PerformInteraction(ACharacter* Player)
{
	if (!CanInteractInternal(Player))
	{
		return;
	}

	// 부모 기믹에는 고유 상호작용 동작이 없음.
	// 스위치 등 자식 C++ 클래스에서 실제 기능을 구현.
}

void AGimmickBase::ApplyActiveState(bool bNewActive)
{
	// 같은 상태의 반복 처리 방지
	if (bIsActive == bNewActive)
	{
		return;
	}

	bIsActive = bNewActive;
	if (GetGameInstance())
	{
		const UProgressSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>();
		if (Saves && Saves->IsRestoringGimmicks()) return;
	}

	// 스위치 그룹 등 상태를 관찰하는 대상에 알림
	OnActiveStateChanged.Broadcast(this, bNewActive);

	// 연결 장치의 BPI 호출은 Blueprint에서 수행
	NotifyLinkedDevices(bNewActive);
}

void AGimmickBase::UpdateInteractionHighlight(bool bVisible)
{
	const bool bShowHighlight = bVisible && bIsEnabled && bSupportsInteraction;

	// 실제 상호작용 대상인지 기록
	bIsHighlighted = bShowHighlight;

	// 자식 클래스에서 발광 우선순위를 결정할 수 있도록 분리
	const float TargetIntensity = CalculateGlowIntensity();

	for (UMaterialInstanceDynamic* GlowMaterial : GlowMaterials)
	{
		if (!IsValid(GlowMaterial))
		{
			continue;
		}

		GlowMaterial->SetScalarParameterValue(GlowParameterName, TargetIntensity);
	}

	if (!IsValid(InteractionEffect))
	{
		return;
	}

	if (bShowHighlight && InteractionEffect->GetAsset())
	{
		// 반복 갱신 때 효과를 처음부터 재생하지 않도록 처리
		if (!InteractionEffect->IsActive())
		{
			InteractionEffect->Activate(true);
		}
	}
	else
	{
		InteractionEffect->Deactivate();
	}
}

bool AGimmickBase::ApplyConfigOverride(FName ConfigSlot, const FDataTableRowHandle& Row)
{
	// 부모는 특정 설정 슬롯을 처리하지 않음.
	// 자식 클래스가 슬롯과 행을 검증하고 적용한 경우에만
	// true를 반환하도록 재정의.
	return false;
}

// Called when the game starts or when spawned
void AGimmickBase::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && bPersistState && GetGameInstance())
	{
		if (!PersistentId.IsValid() && HasAnyFlags(RF_WasLoaded))
			UE_LOG(LogTemp, Warning, TEXT("Gimmick has no persistent ID; open and save its map in the editor: %s"), *GetPathName());
		if (UProgressSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>())
			Saves->QueueGimmickRestore(this);
	}
	
	// 자식 Blueprint에 추가된 메시 컴포넌트까지 수집
	TArray<UMeshComponent*> MeshComponents;
	GetComponents<UMeshComponent>(MeshComponents);

	GlowMaterials.Reset();

	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (!IsValid(MeshComponent))
		{
			continue;
		}

		const int32 MaterialCount = MeshComponent->GetNumMaterials();

		for (int32 MaterialIndex = 0;
			MaterialIndex < MaterialCount;
			++MaterialIndex)
		{
			if (!MeshComponent->GetMaterial(MaterialIndex))
			{
				continue;
			}

			UMaterialInstanceDynamic* DynamicMaterial = MeshComponent->CreateDynamicMaterialInstance(MaterialIndex);

			if (IsValid(DynamicMaterial))
			{
				GlowMaterials.AddUnique(DynamicMaterial);
			}
		}
	}

	// 시작할 때 강조 표시 해제
	UpdateInteractionHighlight(false);
	// +추가----
	// 시작 상태와 체크포인트 초기화가 끝난 다음 지속음을 맞춘다.
	GimmickAudioInitTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AGimmickBase::InitializeGimmickAudio);
	// +--------
}

// +추가----
bool AGimmickBase::IsGimmickAudioSuppressed() const
{
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer
		|| bRestoringGimmickAudio || IsActorBeingDestroyed())
	{
		return true;
	}
	const UGameInstance* Instance = GetGameInstance();
	const UProgressSaveSubsystem* Saves = Instance ? Instance->GetSubsystem<UProgressSaveSubsystem>() : nullptr;
	return Saves && Saves->IsRestoringGimmicks();
}

bool AGimmickBase::CanPlayGimmickEventSound() const
{
	// 같은 프레임의 시작 상태 전달에서는 단발음을 재생하지 않는다.
	return bGimmickAudioReady && GFrameCounter > GimmickAudioReadyFrame && !IsGimmickAudioSuppressed();
}

void AGimmickBase::PlayGimmickOneShot(USoundBase* Sound, USceneComponent* Source) const
{
	if (!IsValid(Sound) || !CanPlayGimmickEventSound()) return;
	const FVector Location = IsValid(Source) ? Source->GetComponentLocation() : GetActorLocation();
	// 액터에서 분리하여 폭탄이 삭제되어도 남은 소리는 끝까지 재생한다.
	UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, GimmickSoundVolume,
		GimmickSoundPitch, 0.0f, GimmickSoundAttenuation.Get());
}

void AGimmickBase::StartGimmickLoop(USoundBase* Sound, USceneComponent* Source)
{
	if (!bGimmickAudioReady || IsGimmickAudioSuppressed() || !IsValid(Sound) || !IsValid(GimmickLoopAudio))
	{
		StopGimmickLoop(true);
		return;
	}
	USceneComponent* Attachment = IsValid(Source) ? Source : GetRootComponent();
	if (!IsValid(Attachment)) return;
	// 같은 요청을 반복해도 처음부터 재생하거나 단발 음원을 임의로 반복하지 않는다.
	if (bGimmickLoopRequested && GimmickLoopAudio->Sound == Sound
		&& GimmickLoopAudio->GetAttachParent() == Attachment) return;

	GimmickLoopAudio->Stop();
	GimmickLoopAudio->AttachToComponent(Attachment, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	GimmickLoopAudio->SetSound(Sound);
	GimmickLoopAudio->AttenuationSettings = GimmickSoundAttenuation;
	GimmickLoopAudio->SetVolumeMultiplier(GimmickSoundVolume);
	GimmickLoopAudio->SetPitchMultiplier(GimmickSoundPitch);
	bGimmickLoopRequested = true;
	// 실제 반복 여부는 Sound Cue의 Wave Player 설정을 따른다.
	if (GimmickLoopFadeTime > 0.0f) GimmickLoopAudio->FadeIn(GimmickLoopFadeTime);
	else GimmickLoopAudio->Play();
}

void AGimmickBase::StopGimmickLoop(bool bImmediately)
{
	const bool bWasRequested = bGimmickLoopRequested;
	bGimmickLoopRequested = false;
	if (!IsValid(GimmickLoopAudio)) return;
	if (bImmediately || GimmickLoopFadeTime <= 0.0f) GimmickLoopAudio->Stop();
	else if (bWasRequested && GimmickLoopAudio->IsPlaying()) GimmickLoopAudio->FadeOut(GimmickLoopFadeTime, 0.0f);
}

void AGimmickBase::InitializeGimmickAudio()
{
	bGimmickAudioReady = true;
	GimmickAudioReadyFrame = GFrameCounter;
	RefreshGimmickAudioState();
}

void AGimmickBase::QueueGimmickAudioRefresh()
{
	if (GetWorld() && HasActorBegunPlay())
	{
		GetWorldTimerManager().ClearTimer(GimmickAudioRefreshTimer);
		GimmickAudioRefreshTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AGimmickBase::RefreshGimmickAudioState);
	}
}

void AGimmickBase::RefreshGimmickAudioState()
{
}

void AGimmickBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bGimmickAudioReady = false;
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(GimmickAudioInitTimer);
		GetWorldTimerManager().ClearTimer(GimmickAudioRefreshTimer);
	}
	StopGimmickLoop(true);
	Super::EndPlay(EndPlayReason);
}
// +--------

// Called every frame
void AGimmickBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

float AGimmickBase::CalculateGlowIntensity() const
{
	// 기본 기믹은 기존 상호작용 강조 상태에 따라 발광
	return bIsHighlighted ? GlowIntensity : 0.0f;
}

void AGimmickBase::SetGimmickEnabled(bool bNewEnabled)
{
	bIsEnabled = bNewEnabled;
	if (!bIsEnabled)
	{
		UpdateInteractionHighlight(false);
		ApplyActiveState(false);
	}
}
void AGimmickBase::EnsurePersistentId()
{
    if (IsTemplate() || PersistentId.IsValid()) return;
    // Runtime-spawned actors have no matching placed instance on the next map load.
    if (GetWorld() && GetWorld()->IsGameWorld()) return;
#if WITH_EDITOR
    if (GetActorGuid().IsValid()) { PersistentId = GetActorGuid(); return; }
    PersistentId = FGuid::NewGuid();
#endif
}

void AGimmickBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    EnsurePersistentId();
}

void AGimmickBase::PostLoad()
{
    Super::PostLoad();
#if WITH_EDITOR
    EnsurePersistentId();
#endif
}

#if WITH_EDITOR
void AGimmickBase::PostEditImport()
{
    Super::PostEditImport();
    if (!IsTemplate()) PersistentId = FGuid::NewGuid();
}
#endif

FProgressActorState AGimmickBase::CaptureProgress() const
{
    FProgressActorState State;
    State.State = TEXT("GimmickV1");
    State.Transform = GetActorTransform();
    State.bEnabled = bIsEnabled;
    State.bActive = bIsActive;
    return State;
}

void AGimmickBase::RestoreProgress(const FProgressActorState& State)
{
    // +추가----
    TGuardValue<bool> AudioRestoreGuard(bRestoringGimmickAudio, true);
    StopGimmickLoop(true);
    // +--------
    if (bRestoreActorTransform)
    {
        SetActorTransform(State.Transform, false, nullptr, ETeleportType::TeleportPhysics);
    }
    bIsEnabled = State.bEnabled;
    bIsActive = State.bActive;
    RefreshRestoredState();
    OnCheckpointStateRestored();
    // +추가----
    QueueGimmickAudioRefresh();
    // +--------
}

void AGimmickBase::RefreshRestoredState()
{
    // State already matches, so the base event broadcaster returns without emitting events.
    ApplyActiveState(bIsActive);
    UpdateInteractionHighlight(false);
}

void AGimmickBase::RestoreLinkedActiveState(bool bActive)
{
    // +추가----
    TGuardValue<bool> AudioRestoreGuard(bRestoringGimmickAudio, true);
    StopGimmickLoop(true);
    // +--------
    bIsActive = bActive && bIsEnabled;
    RefreshRestoredState();
    OnCheckpointStateRestored();
    // +추가----
    QueueGimmickAudioRefresh();
    // +--------
}

void AGimmickBase::FinishRestoreProgress(const FProgressActorState& State)
{
    // Native device references can be synchronized without replaying interaction/BP events.
    for (AActor* Device : LinkedDevices)
        if (AGimmickBase* Gimmick = Cast<AGimmickBase>(Device)) Gimmick->RestoreLinkedActiveState(bIsActive);
}
