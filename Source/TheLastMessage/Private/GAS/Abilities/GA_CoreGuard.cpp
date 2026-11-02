#include "GAS/Abilities/GA_CoreGuard.h"
#include "AbilitySystemComponent.h"
#include "Character/Player/PlayerBase.h"
#include "Core/CoreGameplayTags.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/Effects/GE_CoreGuardRoot.h"
#include "GAS/WeaponGameplayTags.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
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
	StartPerfectWindow(ActorInfo->AbilitySystemComponent.Get());

	UAbilityTask_WaitGameplayEvent* WaitBlocked = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, CoreGameplayTags::Event_GuardBlocked, nullptr, false, true);
	WaitBlocked->EventReceived.AddDynamic(this, &ThisClass::HandleGuardBlocked);
	WaitBlocked->ReadyForActivation();
}

void UGA_CoreGuard::HandleGuardBlocked(FGameplayEventData Payload)
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar))
	{
		return;
	}
	const bool bPartial = Payload.TargetTags.HasTagExact(CoreGameplayTags::Result_GuardPartial);

	if (bPartial && GuardBreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GuardBreakSound, Avatar->GetActorLocation());
	}
	else if (BlockSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BlockSound, Avatar->GetActorLocation(), 1.0f,
			bPartial ? PartialBlockPitch : 1.0f);
	}
	if (!BarrierMaterialInstance)
	{
		if (bPartial)
		{
			BreakGuard();
		}
		return;
	}

	// 공격자가 있는 방향부터 깨지도록 방벽 평면 기준 각도로 바꾼다. 머티리얼은 V축을 뒤집어 위쪽을 +로 쓴다.
	if (const AActor* Attacker = Payload.Instigator.Get())
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

	// 깨지는 중에 또 막으면 목표 상태에서 이어서 깎아, 연속 공격일수록 더 성겨지게 한다.
	BreakTarget = FMath::Max(MinIntegrity, FMath::Min(Integrity, BreakTarget) - BlockBreakAmount);
	BreakRemaining = BreakTime;
	Flash = 1.0f;

	UWorld* World = GetWorld();
	if (World && !World->GetTimerManager().IsTimerActive(BarrierTimerHandle))
	{
		LastUpdateTime = World->GetTimeSeconds();
		World->GetTimerManager().SetTimer(BarrierTimerHandle, this, &ThisClass::UpdateBarrier, 1.0f / 60.0f, true);
	}
	UpdateBarrier();
}

void UGA_CoreGuard::BreakGuard()
{
	UWorld* World = GetWorld();
	if (!World || bGuardBroken)
	{
		return;
	}
	bGuardBroken = true;
	GuardBreakCooldownEndTime = World->GetTimeSeconds() + GuardBreakCooldown;

	if (BarrierMaterialInstance)
	{
		BreakTarget = 0.0f;
		BreakRemaining = GuardBreakTime;
		Flash = 1.0f;
		if (!World->GetTimerManager().IsTimerActive(BarrierTimerHandle))
		{
			LastUpdateTime = World->GetTimeSeconds();
			World->GetTimerManager().SetTimer(BarrierTimerHandle, this, &ThisClass::UpdateBarrier, 1.0f / 60.0f, true);
		}
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
	// 막을 에너지가 없으면 가드를 시작하지 않는다.
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UWeaponSlotAttributeSet* EnergySet = ASC ? ASC->GetSet<UWeaponSlotAttributeSet>() : nullptr;
	if (EnergySet && EnergySet->GetSharedEnergy() <= 0.0f)
	{
		return false;
	}
	const UWorld* World = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor->GetWorld() : nullptr;
	return !World || World->GetTimeSeconds() >= GuardBreakCooldownEndTime;
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

	BarrierMaterialInstance->SetScalarParameterValue(TEXT("Integrity"), Integrity);
	BarrierMaterialInstance->SetScalarParameterValue(TEXT("Brightness"), BaseBrightness * (1.0f + 0.35f * Flash));

	if (Integrity >= 1.0f && Flash <= 0.0f && BreakRemaining <= 0.0f)
	{
		World->GetTimerManager().ClearTimer(BarrierTimerHandle);
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

	// 새 가드는 온전한 방벽으로 시작한다.
	Integrity = 1.0f;
	BreakTarget = 1.0f;
	BreakRemaining = 0.0f;
	Flash = 0.0f;
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

	// 가드 브레이크면 깨지는 연출이 끝난 뒤 UpdateBarrier가 방벽을 제거한다.
	if (!bGuardBroken)
	{
		HideBarrier();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
