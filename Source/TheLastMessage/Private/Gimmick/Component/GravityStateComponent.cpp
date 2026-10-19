// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Component/GravityStateComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/Player/PlayerBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "Math/RotationMatrix.h"
#include "NativeGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StateGravityReversed, "State.Gravity.Reversed");

UGravityStateComponent::UGravityStateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// 이동 처리가 끝난 뒤 메시의 회전 연출 적용
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	ReversedGravityTag = TAG_StateGravityReversed;
	static ConstructorHelpers::FClassFinder<UGameplayEffect> GravityEffect(
		TEXT("/Game/Test_Jiho/Gimmick/GE_GravityReversed"));
	GravityEffectClass = GravityEffect.Class;
}

void UGravityStateComponent::ToggleGravityState()
{
	APlayerBase* Player = Cast<APlayerBase>(GetOwner());
	UAbilitySystemComponent* ASC = OwnerASC.Get();

	if (!IsValid(Player) || Player->IsDead() || !Player->HasAuthority() || !IsValid(ASC) || !IsValid(MovementComponent))
	{
		return;
	}

	// 반전 상태라면 이 컴포넌트가 적용한 효과 제거
	if (ASC->HasMatchingGameplayTag(ReversedGravityTag))
	{
		if (!ActiveGravityEffectHandle.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: 반전 태그는 있지만 제거할 중력 효과 핸들이 없습니다."), *GetName());

			return;
		}

		if (ASC->RemoveActiveGameplayEffect(ActiveGravityEffectHandle))
		{
			ActiveGravityEffectHandle.Invalidate();
		}

		return;
	}

	if (!ASC->AbilityActorInfo.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ASC의 ActorInfo가 아직 초기화되지 않았습니다."), *GetName());

		return;
	}

	if (!GravityEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: GravityEffectClass를 지정해주세요."), *GetName());

		return;
	}

	const UGameplayEffect* EffectDefault = GravityEffectClass->GetDefaultObject<UGameplayEffect>();

	if (!IsValid(EffectDefault) 
		|| EffectDefault->DurationPolicy != EGameplayEffectDurationType::Infinite 
		|| !EffectDefault->GetGrantedTags().HasTagExact( ReversedGravityTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 중력 효과는 Infinite이며 State.Gravity.Reversed 태그를 부여해야 합니다."), *GetName());

		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();

	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GravityEffectClass, 1.0f, Context);

	if (!Spec.IsValid())
	{
		return;
	}

	ActiveGravityEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	if (!ActiveGravityEffectHandle.IsValid() || !ASC->HasMatchingGameplayTag(ReversedGravityTag))
	{
		if (ActiveGravityEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(ActiveGravityEffectHandle);
		}

		ActiveGravityEffectHandle.Invalidate();

		UE_LOG( LogTemp, Warning, TEXT("%s: 중력 반전 효과가 정상적으로 적용되지 않았습니다."), *GetName());
	}
}


void UGravityStateComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerBase* Player = Cast<APlayerBase>(GetOwner());

	if (!IsValid(Player))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: GravityStateComponent는 PlayerBase에서 사용해야 합니다."), *GetName());

		return;
	}

	MovementComponent = Player->GetCharacterMovement();

	// 부드럽게 회전시킬 플레이어 메시 연결
	CharacterMesh = Player->GetMesh();

	OwnerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player);

	UAbilitySystemComponent* ASC = OwnerASC.Get();

	if (!IsValid(MovementComponent) || !IsValid(ASC))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 이동 컴포넌트 또는 ASC를 찾지 못했습니다."), *GetName());

		return;
	}

	if (IsValid(CharacterMesh))
	{
		// 현재 블루프린트에 설정된 메시 위치와 회전을 기록
		InitialMeshRelativeLocation = CharacterMesh->GetRelativeLocation();

		InitialMeshRelativeRotation = CharacterMesh->GetRelativeRotation().Quaternion();
	}

	AddTickPrerequisiteComponent(MovementComponent.Get());

	// 플레이 시작 시의 중력 방향을 정상 방향으로 기록
	DefaultGravityDirection = MovementComponent->GetGravityDirection().GetSafeNormal();

	if (DefaultGravityDirection.IsNearlyZero())
	{
		DefaultGravityDirection = FVector(0.0f, 0.0f, -1.0f);
	}

	GravityTagChangedHandle = ASC->RegisterGameplayTagEvent(ReversedGravityTag, EGameplayTagEventType::NewOrRemoved)
										.AddUObject(this, &UGravityStateComponent::HandleGravityTagChanged);

	ApplyGravityFromTag();

}

void UGravityStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UAbilitySystemComponent* ASC = OwnerASC.Get();

	if (IsValid(ASC))
	{
		if (GravityTagChangedHandle.IsValid())
		{
			ASC->UnregisterGameplayTagEvent(GravityTagChangedHandle, ReversedGravityTag, EGameplayTagEventType::NewOrRemoved);
		}

		AActor* OwnerActor = GetOwner();

		if (IsValid(OwnerActor) && OwnerActor->HasAuthority() && ActiveGravityEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(ActiveGravityEffectHandle);
		}
	}

	FinishMeshRotation();

	if (IsValid(MovementComponent))
	{
		RemoveTickPrerequisiteComponent(MovementComponent.Get());
	}

	GravityTagChangedHandle.Reset();
	ActiveGravityEffectHandle.Invalidate();

	OwnerASC.Reset();
	MovementComponent = nullptr;
	CharacterMesh = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UGravityStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsMeshRotating || !IsValid(CharacterMesh))
	{
		FinishMeshRotation();
		return;
	}

	MeshRotationElapsed += DeltaTime;

	const float Duration = FMath::Max(GravityRotationDuration, KINDA_SMALL_NUMBER);
	const float Alpha = FMath::Clamp(MeshRotationElapsed / Duration, 0.0f, 1.0f);

	// 회전 시작과 끝을 느리게 처리
	const float SmoothAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

	const FQuat RotationOffset = FQuat::Slerp(MeshRotationStartOffset, FQuat::Identity, SmoothAlpha).GetNormalized();

	ApplyMeshRotationOffset(RotationOffset);

	if (Alpha >= 1.0f)
	{
		FinishMeshRotation();
	}
}

void UGravityStateComponent::HandleGravityTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	ApplyGravityFromTag();
}

void UGravityStateComponent::ApplyGravityFromTag()
{
	UAbilitySystemComponent* ASC = OwnerASC.Get();

	if (!IsValid(ASC) || !IsValid(MovementComponent))
	{
		return;
	}

	const bool bReversed = ASC->HasMatchingGameplayTag(ReversedGravityTag);

	const FVector NewGravityDirection = bReversed ? -DefaultGravityDirection : DefaultGravityDirection;

	const bool bDirectionChanged = !MovementComponent->GetGravityDirection().Equals(NewGravityDirection);

	if (bDirectionChanged)
	{
		MovementComponent->SetGravityDirection(NewGravityDirection);

		// 기존 바닥에서 떨어져 새 중력 방향으로 이동하도록 처리
		if (MovementComponent->IsMovingOnGround())
		{
			MovementComponent->SetMovementMode(MOVE_Falling);
		}
	}

	AlignCharacterToGravity();
}

void UGravityStateComponent::AlignCharacterToGravity()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());

	if (!IsValid(Character) || !IsValid(MovementComponent))
	{
		return;
	}

	// 회전 도중 다시 중력이 바뀌어도 현재 보이는 자세에서 이어서 회전
	const FQuat PreviousVisualRotation = Character->GetActorQuat() * CurrentMeshRotationOffset;
	const FVector UpDirection = -MovementComponent->GetGravityDirection();

	// 현재 바라보는 방향을 새 중력의 수평면에 투영
	FVector ForwardDirection = FVector::VectorPlaneProject(Character->GetActorForwardVector(), UpDirection).GetSafeNormal();

	if (ForwardDirection.IsNearlyZero())
	{
		ForwardDirection = FVector::VectorPlaneProject(FVector::ForwardVector, UpDirection).GetSafeNormal();
	}

	if (ForwardDirection.IsNearlyZero())
	{
		ForwardDirection = FVector::VectorPlaneProject(FVector::RightVector, UpDirection).GetSafeNormal();
	}

	const FQuat NewRotation = FRotationMatrix::MakeFromXZ(ForwardDirection, UpDirection).ToQuat();

	// 충돌과 이동에 사용하는 캡슐은 즉시 정렬
	Character->SetActorRotation(NewRotation, ETeleportType::TeleportPhysics);

	if (!IsValid(CharacterMesh))
	{
		return;
	}

	// 메시가 즉시 뒤집혀 보이지 않도록 반대 회전으로 보정
	MeshRotationStartOffset = (Character->GetActorQuat().Inverse() * PreviousVisualRotation).GetNormalized();

	MeshRotationElapsed = 0.0f;

	if (GravityRotationDuration <= KINDA_SMALL_NUMBER || MeshRotationStartOffset.Equals(FQuat::Identity, KINDA_SMALL_NUMBER))
	{
		FinishMeshRotation();
		return;
	}

	ApplyMeshRotationOffset(MeshRotationStartOffset);

	bIsMeshRotating = true;
	SetComponentTickEnabled(true);

}

void UGravityStateComponent::ApplyMeshRotationOffset(const FQuat& RotationOffset)
{
	CurrentMeshRotationOffset = RotationOffset;

	if (!IsValid(CharacterMesh))
	{
		return;
	}

	// 메시의 원점이 발에 있어도 캡슐 중심을 기준으로 돌도록 위치도 회전
	const FVector MeshLocation = RotationOffset.RotateVector(InitialMeshRelativeLocation);
	const FQuat MeshRotation = (RotationOffset * InitialMeshRelativeRotation).GetNormalized();

	CharacterMesh->SetRelativeLocationAndRotation(MeshLocation, MeshRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

void UGravityStateComponent::FinishMeshRotation()
{
	bIsMeshRotating = false;
	MeshRotationElapsed = 0.0f;

	ApplyMeshRotationOffset(FQuat::Identity);

	// 회전하지 않을 때는 Tick 중지
	SetComponentTickEnabled(false);
}
