#include "GAS/Abilities/GA_CoreGuard.h"
#include "AbilitySystemComponent.h"
#include "Character/Player/PlayerBase.h"
#include "Core/CoreGameplayTags.h"
#include "GAS/Effects/GE_CoreGuardRoot.h"
#include "GAS/WeaponGameplayTags.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
}

void UGA_CoreGuard::ShowBarrier(AActor* Avatar)
{
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
}

void UGA_CoreGuard::HideBarrier()
{
	if (BarrierComponent)
	{
		BarrierComponent->DestroyComponent();
	}
	BarrierComponent = nullptr;
	BarrierMaterialInstance = nullptr;
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

	HideBarrier();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
