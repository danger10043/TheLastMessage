#include "GAS/Abilities/GA_CoreGuard.h"
#include "AbilitySystemComponent.h"
#include "Character/Player/PlayerBase.h"
#include "Core/CoreGameplayTags.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/Effects/GE_CoreGuardRoot.h"
#include "GAS/WeaponGameplayTags.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 방벽 머티리얼은 평면의 가장자리 15%를 테두리 빛 번짐에 쓰므로 메시를 반지름보다 그만큼 크게 둔다.
	constexpr float BarrierMeshMargin = 1.15f;
	// 엔진 기본 평면의 한 변 길이(cm).
	constexpr float BarrierMeshSize = 100.0f;
}

UGA_CoreGuard::UGA_CoreGuard()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	BarrierMesh = Plane.Object;
	GuardMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Test_Jihong/Animations/Guard/AM_PlayerGuard.AM_PlayerGuard")));

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	// 공격·피격 경직·그래플 중에는 가드를 시작하지 않는다.
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_HitReacting);
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Grappling);

	// 능력이 활성화된 동안만 가드 상태 태그를 붙이고, 종료·취소 시 GAS가 함께 제거한다.
	ActivationOwnedTags.AddTag(CoreGameplayTags::State_Guarding);
}

void UGA_CoreGuard::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 활성화 전에 키를 이미 뗐다면 놓친 해제 입력 대신 바로 종료한다.
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	if (!Spec || !Spec->InputPressed)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const FGameplayEffectSpecHandle RootSpec = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, UGE_CoreGuardRoot::StaticClass());
	if (RootSpec.IsValid())
	{
		RootEffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, RootSpec);
	}

	ShowBarrier(ActorInfo->AvatarActor.Get());
	PlayGuardMontage();
	StartPerfectWindow(ActorInfo->AbilitySystemComponent.Get());

	UAbilityTask_WaitGameplayEvent* WaitBlocked = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, CoreGameplayTags::Event_GuardBlocked, nullptr, false, true);
	WaitBlocked->EventReceived.AddDynamic(this, &ThisClass::HandleGuardBlocked);
	WaitBlocked->ReadyForActivation();
}

void UGA_CoreGuard::PlayGuardMontage()
{
	UAnimMontage* Montage = GuardMontage.LoadSynchronous();
	if (!Montage)
	{
		return;
	}
	// 능력이 끝날 때 바로 끊지 않고 End 섹션으로 넘겨 풀리는 동작까지 보여 준다.
	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, TEXT("GuardPose"), Montage, 1.0f, TEXT("Start"), false);
	Task->ReadyForActivation();
	PlayingGuardMontage = Montage;
}

void UGA_CoreGuard::FinishGuardMontage(UAbilitySystemComponent* ASC, bool bWasCancelled)
{
	UAnimMontage* Montage = PlayingGuardMontage.Get();
	PlayingGuardMontage.Reset();
	// 피격·사망 등 다른 몽타주가 이미 덮어썼으면 그쪽을 그대로 둔다.
	if (!ASC || !Montage || ASC->GetCurrentMontage() != Montage)
	{
		return;
	}
	if (!bWasCancelled && Montage->IsValidSectionName(GuardEndSection))
	{
		ASC->CurrentMontageJumpToSection(GuardEndSection);
	}
	else
	{
		ASC->CurrentMontageStop(GuardMontageStopBlendTime);
	}
}

void UGA_CoreGuard::HandleGuardBlocked(FGameplayEventData Payload)
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar))
	{
		return;
	}
	const bool bPartial = Payload.TargetTags.HasTagExact(CoreGameplayTags::Result_GuardPartial);
	const bool bPerfect = Payload.TargetTags.HasTagExact(CoreGameplayTags::Result_GuardPerfect);

	// 상황마다 지정한 소리만 재생하고, 비어 있으면 소리를 내지 않는다.
	USoundBase* Sound = bPerfect ? PerfectBlockSound.Get() : bPartial ? GuardBreakSound.Get() : BlockSound.Get();
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Avatar->GetActorLocation());
	}

	// 퍼펙트 가드는 방벽을 깎지 않고 튕겨 내는 연출만 한다.
	if (bPerfect)
	{
		PlayPerfectFeedback();
		return;
	}
	// 공격자가 있는 방향부터 깨지도록 방벽 평면 기준 각도로 바꾼다. 머티리얼은 V축을 뒤집어 위쪽을 +로 쓴다.
	const AActor* Attacker = Payload.Instigator.Get();
	if (BarrierMaterialInstance && Attacker)
	{
		const FVector Local = BarrierComponent->GetComponentTransform().InverseTransformVectorNoScale(
			Attacker->GetActorLocation() - Avatar->GetActorLocation());
		if (!FMath::IsNearlyZero(Local.X) || !FMath::IsNearlyZero(Local.Y))
		{
			BarrierMaterialInstance->SetScalarParameterValue(TEXT("ImpactAngle"), FMath::Atan2(-Local.Y, Local.X));
		}
	}

	if (bPartial)
	{
		BreakGuard();
		return;
	}
	if (!BarrierMaterialInstance)
	{
		return;
	}

	// 깨지는 중에 또 막으면 목표 상태에서 이어서 깎아, 연속 공격일수록 더 성겨지게 한다.
	BreakTarget = FMath::Max(MinIntegrity, FMath::Min(Integrity, BreakTarget) - BlockBreakAmount);
	BreakRemaining = BreakTime;
	Flash = 1.0f;
	EnsureBarrierTimer();
	UpdateBarrier();
}

void UGA_CoreGuard::EnsureBarrierTimer()
{
	UWorld* World = GetWorld();
	if (World && !World->GetTimerManager().IsTimerActive(BarrierTimerHandle))
	{
		LastUpdateTime = World->GetTimeSeconds();
		// 능력이 끝나면 엔진이 능력 객체에 묶인 타이머를 모두 지우므로, 가드 브레이크 연출이 이어지도록 약한 참조 람다로 건다.
		World->GetTimerManager().SetTimer(BarrierTimerHandle, FTimerDelegate::CreateLambda(
			[WeakThis = TWeakObjectPtr<UGA_CoreGuard>(this)]()
			{
				if (WeakThis.IsValid()) WeakThis->UpdateBarrier();
			}), 1.0f / 60.0f, true);
	}
}

void UGA_CoreGuard::PlayPerfectFeedback()
{
	if (!BarrierMaterialInstance)
	{
		return;
	}
	PerfectFlash = 1.0f;
	PerfectPopProgress = 1.0f;
	SpawnShockwave();
	EnsureBarrierTimer();
	UpdateBarrier();
}

void UGA_CoreGuard::SpawnShockwave()
{
	if (!ShockwaveMaterial || !BarrierMesh || !BarrierComponent)
	{
		return;
	}
	AActor* Avatar = BarrierComponent->GetOwner();
	UStaticMeshComponent* Wave = NewObject<UStaticMeshComponent>(Avatar, NAME_None, RF_Transient);
	Wave->SetStaticMesh(BarrierMesh);
	Wave->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Wave->SetCastShadow(false);
	Wave->SetupAttachment(BarrierComponent->GetAttachParent());
	Wave->SetUsingAbsoluteRotation(true);
	Wave->SetRelativeLocation(BarrierOffset);
	Wave->SetWorldRotation(BarrierComponent->GetComponentRotation());
	// 머티리얼은 평면 가장자리를 방벽 반지름 × (1 + Expand) × 1.1로 계산하므로 메시도 같은 크기로 둔다.
	const float WaveMargin = (1.0f + ShockwaveExpand) * 1.1f;
	Wave->SetWorldScale3D(FVector(BarrierRadius * 2.0f * WaveMargin / BarrierMeshSize));
	Wave->RegisterComponent();
	UMaterialInstanceDynamic* WaveMaterial = Wave->CreateDynamicMaterialInstance(0, ShockwaveMaterial);
	WaveMaterial->SetScalarParameterValue(TEXT("Expand"), ShockwaveExpand);
	WaveMaterial->SetScalarParameterValue(TEXT("Progress"), 0.0f);
	WaveMaterial->SetVectorParameterValue(TEXT("Color"), PerfectColor);
	ShockwaveComponents.Add(Wave);
	ShockwaveElapsed.Add(0.0f);

	// 충격파는 방벽과 따로 진행해, 가드를 떼거나 다시 눌러도 끝까지 퍼진 뒤 사라진다.
	UWorld* World = GetWorld();
	if (World && !World->GetTimerManager().IsTimerActive(ShockwaveTimerHandle))
	{
		LastShockwaveUpdateTime = World->GetTimeSeconds();
		World->GetTimerManager().SetTimer(ShockwaveTimerHandle, FTimerDelegate::CreateLambda(
			[WeakThis = TWeakObjectPtr<UGA_CoreGuard>(this)]()
			{
				if (WeakThis.IsValid()) WeakThis->UpdateShockwaves();
			}), 1.0f / 60.0f, true);
	}
}

void UGA_CoreGuard::ClearShockwaves()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShockwaveTimerHandle);
	}
	for (UStaticMeshComponent* Wave : ShockwaveComponents)
	{
		if (Wave)
		{
			Wave->DestroyComponent();
		}
	}
	ShockwaveComponents.Reset();
	ShockwaveElapsed.Reset();
}

void UGA_CoreGuard::BreakGuard()
{
	UWorld* World = GetWorld();
	if (!World || bGuardBroken)
	{
		return;
	}
	bGuardBroken = true;

	if (BarrierMaterialInstance)
	{
		BreakTarget = 0.0f;
		BreakRemaining = GuardBreakTime;
		Flash = 1.0f;
		EnsureBarrierTimer();
	}

	// 피해 처리 도중에 받은 이벤트이므로 GE 제거와 태그 해제는 다음 틱으로 미룬다.
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsActive())
		{
			K2_EndAbility();
		}
	}));
}

void UGA_CoreGuard::StartPerfectWindow(UAbilitySystemComponent* ASC)
{
	const UCombatAttributeSet* Attributes = ASC ? ASC->GetSet<UCombatAttributeSet>() : nullptr;
	UWorld* World = GetWorld();
	const float Window = Attributes ? Attributes->GetPerfectGuardWindow() : 0.0f;
	if (!World || !FMath::IsFinite(Window) || Window <= 0.0f)
	{
		return;
	}

	ASC->AddLooseGameplayTag(CoreGameplayTags::State_GuardPerfect);
	bPerfectWindowActive = true;
	World->GetTimerManager().SetTimer(PerfectWindowTimerHandle, this, &ThisClass::EndPerfectWindow, Window, false);
}

void UGA_CoreGuard::EndPerfectWindow()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PerfectWindowTimerHandle);
	}
	if (!bPerfectWindowActive)
	{
		return;
	}
	bPerfectWindowActive = false;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(CoreGameplayTags::State_GuardPerfect);
	}
}

bool UGA_CoreGuard::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	// 막을 에너지가 없으면 가드를 시작하지 않는다. 가드 브레이크 직후에는 에너지가 0이라 따로 대기 시간을 두지 않는다.
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UWeaponSlotAttributeSet* EnergySet = ASC ? ASC->GetSet<UWeaponSlotAttributeSet>() : nullptr;
	return !EnergySet || EnergySet->GetSharedEnergy() > 0.0f;
}

void UGA_CoreGuard::UpdateBarrier()
{
	UWorld* World = GetWorld();
	if (!World || !BarrierMaterialInstance)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const float DeltaTime = static_cast<float>(Now - LastUpdateTime);
	LastUpdateTime = Now;

	if (BreakRemaining > 0.0f)
	{
		const float Step = FMath::Min(DeltaTime, BreakRemaining);
		Integrity += (BreakTarget - Integrity) * FMath::Min(1.0f, Step / BreakRemaining);
		BreakRemaining -= Step;
	}
	else if (bGuardBroken)
	{
		// 가드 브레이크는 다시 차오르지 않고 다 깨진 뒤 방벽을 제거한다.
		HideBarrier();
		return;
	}
	else
	{
		// 많이 깨졌을수록 빠르게, 거의 다 찼을 때는 천천히 차오른다.
		const float Range = FMath::Max(KINDA_SMALL_NUMBER, 1.0f - MinIntegrity);
		const float Speed = 0.5f + 1.5f * (1.0f - Integrity) / Range;
		Integrity = FMath::Min(1.0f, Integrity + Range / RebuildTime * DeltaTime * Speed);
		BreakTarget = Integrity;
	}
	Flash = FMath::Max(0.0f, Flash - DeltaTime * 6.0f);
	PerfectFlash = FMath::Max(0.0f, PerfectFlash - DeltaTime * 3.0f);
	PerfectPopProgress = FMath::Max(0.0f, PerfectPopProgress - DeltaTime * 6.0f);

	// 퍼펙트 강조는 밝기·선·채움·테두리를 키우고 색을 퍼펙트 색 쪽으로 물들인다.
	const float Perfect = PerfectFlash * PerfectFlashStrength;
	BarrierMaterialInstance->SetScalarParameterValue(TEXT("Integrity"), Integrity);
	BarrierMaterialInstance->SetScalarParameterValue(TEXT("Brightness"),
		BaseBrightness * (1.0f + 0.35f * Flash + 0.8f * Perfect));
	BarrierMaterialInstance->SetScalarParameterValue(TEXT("LineAlpha"), BaseLineAlpha + 0.6f * Perfect);
	BarrierMaterialInstance->SetScalarParameterValue(TEXT("FillScale"), BaseFillScale + 0.4f * Perfect);
	BarrierMaterialInstance->SetScalarParameterValue(TEXT("RimWidth"), BaseRimWidth * (1.0f + 0.65f * Perfect));
	BarrierMaterialInstance->SetVectorParameterValue(TEXT("Color"),
		FMath::Lerp(BaseColor, PerfectColor, PerfectFlash * PerfectTint));
	const float Pop = PerfectPopProgress > 0.0f ? PerfectPop * FMath::Sin(PI * (1.0f - PerfectPopProgress)) : 0.0f;
	BarrierComponent->SetWorldScale3D(FVector(BaseBarrierScale * (1.0f + Pop)));

	if (Integrity >= 1.0f && Flash <= 0.0f && BreakRemaining <= 0.0f && PerfectFlash <= 0.0f
		&& PerfectPopProgress <= 0.0f)
	{
		World->GetTimerManager().ClearTimer(BarrierTimerHandle);
	}
}

void UGA_CoreGuard::UpdateShockwaves()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const float DeltaTime = static_cast<float>(Now - LastShockwaveUpdateTime);
	LastShockwaveUpdateTime = Now;

	for (int32 Index = ShockwaveComponents.Num() - 1; Index >= 0; --Index)
	{
		ShockwaveElapsed[Index] += DeltaTime;
		const float Progress = ShockwaveElapsed[Index] / ShockwaveDuration;
		UStaticMeshComponent* Wave = ShockwaveComponents[Index];
		if (!Wave || Progress >= 1.0f)
		{
			if (Wave)
			{
				Wave->DestroyComponent();
			}
			ShockwaveComponents.RemoveAt(Index);
			ShockwaveElapsed.RemoveAt(Index);
			continue;
		}
		if (UMaterialInstanceDynamic* WaveMaterial = Cast<UMaterialInstanceDynamic>(Wave->GetMaterial(0)))
		{
			WaveMaterial->SetScalarParameterValue(TEXT("Progress"), Progress);
		}
	}

	if (ShockwaveComponents.IsEmpty())
	{
		World->GetTimerManager().ClearTimer(ShockwaveTimerHandle);
	}
}

void UGA_CoreGuard::ShowBarrier(AActor* Avatar)
{
	// 이전 가드 브레이크 연출이 남아 있으면 정리하고 새 방벽으로 시작한다.
	HideBarrier();

	USceneComponent* Root = IsValid(Avatar) ? Avatar->GetRootComponent() : nullptr;
	if (!Root || !BarrierMesh || !BarrierMaterial)
	{
		return;
	}

	BarrierComponent = NewObject<UStaticMeshComponent>(Avatar, NAME_None, RF_Transient);
	BarrierComponent->SetStaticMesh(BarrierMesh);
	BarrierComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrierComponent->SetCastShadow(false);
	BarrierComponent->SetupAttachment(Root);
	// 위치만 캐릭터를 따라가고, 회전은 고정해 사이드뷰 카메라(+Y 쪽에서 -Y 방향)를 항상 바라보게 한다.
	BarrierComponent->SetUsingAbsoluteRotation(true);
	BarrierComponent->SetRelativeLocation(BarrierOffset);
	BarrierComponent->SetWorldRotation(FRotator(0.0f, 0.0f, 90.0f));
	BarrierComponent->SetWorldScale3D(FVector(BarrierRadius * 2.0f * BarrierMeshMargin / BarrierMeshSize));
	BarrierComponent->RegisterComponent();
	BarrierMaterialInstance = BarrierComponent->CreateDynamicMaterialInstance(0, BarrierMaterial);
	BarrierMaterialInstance->GetScalarParameterValue(TEXT("Brightness"), BaseBrightness);
	BarrierMaterialInstance->GetScalarParameterValue(TEXT("LineAlpha"), BaseLineAlpha);
	BarrierMaterialInstance->GetScalarParameterValue(TEXT("FillScale"), BaseFillScale);
	BarrierMaterialInstance->GetScalarParameterValue(TEXT("RimWidth"), BaseRimWidth);
	BarrierMaterialInstance->GetVectorParameterValue(TEXT("Color"), BaseColor);
	BaseBarrierScale = BarrierComponent->GetComponentScale().X;

	// 새 가드는 온전한 방벽으로 시작한다.
	Integrity = 1.0f;
	BreakTarget = 1.0f;
	BreakRemaining = 0.0f;
	Flash = 0.0f;
	PerfectFlash = 0.0f;
	PerfectPopProgress = 0.0f;
	BarrierMaterialInstance->SetScalarParameterValue(TEXT("Integrity"), Integrity);
}

void UGA_CoreGuard::HideBarrier()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BarrierTimerHandle);
	}
	if (BarrierComponent)
	{
		BarrierComponent->DestroyComponent();
	}
	BarrierComponent = nullptr;
	BarrierMaterialInstance = nullptr;
	bGuardBroken = false;
}

void UGA_CoreGuard::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	// 코어 초기화·복원으로 능력이 제거되면 남은 방벽·충격파와 판정도 함께 정리한다.
	EndPerfectWindow();
	HideBarrier();
	ClearShockwaves();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UGA_CoreGuard::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	if (IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UGA_CoreGuard::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	EndPerfectWindow();

	// 종료·취소 경로와 관계없이 이동·충전 정지를 해제한다.
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC && RootEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(RootEffectHandle);

		// 무기 사용 후와 같이 가드를 푼 뒤에도 대기 시간이 지나야 에너지가 충전된다.
		if (APlayerBase* Player = Cast<APlayerBase>(ActorInfo->AvatarActor.Get()))
		{
			Player->NotifyWeaponEnergySpent();
		}
	}
	RootEffectHandle.Invalidate();

	FinishGuardMontage(ASC, bWasCancelled);

	// 가드 브레이크로 끝나면 깨지는 연출이 끝난 뒤 UpdateBarrier가 방벽을 제거한다.
	// 사망 등으로 취소되면 연출 없이 바로 지워 부활 후에 남지 않게 한다.
	if (!bGuardBroken || bWasCancelled)
	{
		HideBarrier();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
