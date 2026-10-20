#include "Character/Player/PlayerBase.h"
#include "Character/Player/GrapplingHookComponent.h"
#include "Character/Player/CombatFeedbackComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gimmick/Component/InteractionComponent.h"
#include "Gimmick/Component/GravityStateComponent.h"
#include "Core/CoreComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Weapon/MeleeWeaponBase.h"
#include "Weapon/RangedWeaponBase.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"
#include "EnhancedInputComponent.h"
#include "GAS/Abilities/GA_AttackPrimary.h"
#include "GAS/Abilities/GA_AttackSecondary.h"
#include "InputCoreTypes.h"
#include "GAS/WeaponGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Engine/DamageEvents.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

namespace PlayerReaction
{
FName ResolveSection(const UAnimMontage* Montage, FName Requested)
{
	if (Requested.IsNone()) Requested = TEXT("Backward");
	return Montage && Montage->GetSectionIndex(Requested) != INDEX_NONE ? Requested : NAME_None;
}

void SelectSection(UAnimInstance* AnimInstance, UAnimMontage* Montage, FName Requested)
{
	const FName Section = ResolveSection(Montage, Requested);
	if (Section.IsNone()) return;
	AnimInstance->Montage_JumpToSection(Section, Montage);
	// 다음 섹션으로 연결돼 있어도 이번에는 선택한 방향의 모션만 재생한다.
	AnimInstance->Montage_SetNextSection(Section, NAME_None, Montage);
}
}

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PlayerDoubleJump, "Unlock.DoubleJump");

#if ENABLE_DRAW_DEBUG
static TAutoConsoleVariable<int32> CVarDebugAim(
	TEXT("tlm.DebugAim"), 0, TEXT("Draw mouse aim direction and target: 0=off, 1=on."));
#endif

APlayerBase::APlayerBase()
{
	GravityStateComponent = CreateDefaultSubobject<UGravityStateComponent>(TEXT("GravityStateComponent"));
	CombatFeedbackComponent = CreateDefaultSubobject<UCombatFeedbackComponent>(TEXT("CombatFeedbackComponent"));
	GrapplingHookComponent = CreateDefaultSubobject<UGrapplingHookComponent>(TEXT("GrapplingHookComponent"));
	static ConstructorHelpers::FObjectFinder<UInputAction> GrappleAction(TEXT("/Game/Main/Inputs/Actions/IA_GrapplingHook.IA_GrapplingHook"));
	GrapplingHookAction = GrappleAction.Object;
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	CombatAttributes = CreateDefaultSubobject<UCombatAttributeSet>(TEXT("CombatAttributes"));
	WeaponSlotAttributes = CreateDefaultSubobject<UWeaponSlotAttributeSet>(TEXT("WeaponSlotAttributes"));

	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));

	CoreComponent = CreateDefaultSubobject<UCoreComponent>(TEXT("CoreComponent"));

	PrimaryWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrimaryWeaponMesh"));
	PrimaryWeaponMesh->SetupAttachment(GetMesh());
	PrimaryWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrimaryWeaponMesh->SetGenerateOverlapEvents(false);
	PrimaryWeaponMesh->SetSimulatePhysics(false);
	PrimaryWeaponMesh->SetHiddenInGame(true);
	SecondaryWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SecondaryWeaponMesh"));
	SecondaryWeaponMesh->SetupAttachment(GetMesh());
	SecondaryWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SecondaryWeaponMesh->SetGenerateOverlapEvents(false);
	SecondaryWeaponMesh->SetSimulatePhysics(false);
	SecondaryWeaponMesh->SetHiddenInGame(true);

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);

	JumpMaxCount = 1;

	FAbilityInputBinding PrimaryInput;
	PrimaryInput.InputTag = WeaponGameplayTags::Input_Primary;
	AbilityInputBindings.Add(PrimaryInput);
	FAbilityInputBinding SecondaryInput;
	SecondaryInput.InputTag = WeaponGameplayTags::Input_Secondary;
	AbilityInputBindings.Add(SecondaryInput);
}

UAbilitySystemComponent* APlayerBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void APlayerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APlayerBase, VisibleWeaponSlot);
	DOREPLIFETIME_CONDITION(APlayerBase, WeaponChargeState, COND_OwnerOnly);
	DOREPLIFETIME(APlayerBase, InitialPrimaryWeaponData);
	DOREPLIFETIME(APlayerBase, InitialSecondaryWeaponData);
	DOREPLIFETIME(APlayerBase, DeathState);
	DOREPLIFETIME_CONDITION(APlayerBase, AimDirection, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(APlayerBase, AimTargetLocation, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(APlayerBase, bHasAimTarget, COND_SkipOwner);
}

FVector APlayerBase::GetAimPlaneNormal() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const FVector Normal = Movement->bConstrainToPlane ? Movement->GetPlaneConstraintNormal().GetSafeNormal() : FVector::YAxisVector;
	return Normal.IsNearlyZero() ? FVector::YAxisVector : Normal;
}

FVector APlayerBase::GetAimTargetLocation() const
{
	return bHasAimTarget ? AimTargetLocation : GetActorLocation() + AimDirection * 1000.0f;
}

bool APlayerBase::SetAimTarget(FVector WorldPoint)
{
	if (WorldPoint.ContainsNaN() || IsDead()) return false;
	const FVector Delta = FVector::VectorPlaneProject(WorldPoint - GetActorLocation(), GetAimPlaneNormal());
	// 캐릭터 중심 부근에서 방향이 뒤집히거나 비정상 좌표가 전파되지 않도록 한다.
	if (Delta.ContainsNaN() || Delta.SizeSquared() < 1.0f || Delta.SizeSquared() > 1.e12f) return false;
	AimTargetLocation = GetActorLocation() + Delta;
	AimDirection = Delta.GetSafeNormal();
	bHasAimTarget = true;
	return true;
}

bool APlayerBase::UpdateAimFromWorldRay(FVector RayOrigin, FVector RayDirection)
{
	if ((!HasAuthority() && !IsLocallyControlled()) || RayOrigin.ContainsNaN() || RayDirection.ContainsNaN()) return false;
	RayDirection = RayDirection.GetSafeNormal();
	const FVector Normal = GetAimPlaneNormal();
	const double Denominator = FVector::DotProduct(RayDirection, Normal);
	if (FMath::Abs(Denominator) < UE_SMALL_NUMBER) return false;
	const double Distance = FVector::DotProduct(GetActorLocation() - RayOrigin, Normal) / Denominator;
	if (Distance < 0.0) return false;
	return SetAimTarget(RayOrigin + RayDirection * Distance);
}

void APlayerBase::UpdateMouseAim()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController() || IsDead()) return;
	FVector RayOrigin, RayDirection;
	if (PC->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		UpdateAimFromWorldRay(RayOrigin, RayDirection);
	}
}

void APlayerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateMouseAim();
	if (HasAuthority() && !IsLocallyControlled() && bHasAimTarget)
	{
		// 클라이언트의 다음 갱신 전에도 이동한 캐릭터 위치를 기준으로 방향을 계산한다.
		SetAimTarget(AimTargetLocation);
	}
	if (IsLocallyControlled() && !HasAuthority() && bHasAimTarget && !IsDead())
	{
		AimSendElapsed += DeltaSeconds;
		if (AimSendElapsed >= 0.05f)
		{
			ServerUpdateAim(AimTargetLocation);
			AimSendElapsed = 0.0f;
		}
	}
#if ENABLE_DRAW_DEBUG
	if (IsLocallyControlled() && bHasAimTarget && CVarDebugAim.GetValueOnGameThread() != 0)
	{
		DrawDebugLine(GetWorld(), GetActorLocation(), GetAimTargetLocation(), FColor::Cyan);
		DrawDebugSphere(GetWorld(), GetAimTargetLocation(), 8.0f, 12, FColor::Yellow);
	}
#endif
}

void APlayerBase::ServerUpdateAim_Implementation(FVector_NetQuantize AimPoint)
{
	SetAimTarget(AimPoint);
}

void APlayerBase::RequestGrapplingHook()
{
	if (IsGrappling())
	{
		GrapplingHookComponent->CancelGrapple();
		return;
	}
	if (IsDead() || IsHitReacting()) return;
	UpdateMouseAim();
	if (bHasAimTarget) GrapplingHookComponent->RequestGrapple(GetAimTargetLocation());
}

void APlayerBase::ReleaseGrapplingHook()
{
	GrapplingHookComponent->CancelGrapple();
}

void APlayerBase::CaptureGrappleJumpState()
{
	SavedGrappleJumpCount = JumpCurrentCount;
	SavedGrappleJumpCountPreJump = JumpCurrentCountPreJump;
	bSavedGrappleDoubleJump = bHasDoubleJumped;
	bGrappleJumpStateCaptured = true;
	bGrappleCancelJumpAvailable = false;
}

void APlayerBase::RestoreGrappleJumpState()
{
	if (!bGrappleJumpStateCaptured) return;
	JumpCurrentCount = SavedGrappleJumpCount;
	JumpCurrentCountPreJump = SavedGrappleJumpCountPreJump;
	bHasDoubleJumped = bSavedGrappleDoubleJump;
	bGrappleJumpStateCaptured = false;
}

void APlayerBase::GrantGrappleCancelJump()
{
	if (IsDead() || !GetCharacterMovement()->IsFalling()) return;
	StopJumping();
	bGrappleCancelJumpAvailable = true;
	JumpCurrentCount = FMath::Max(0, JumpMaxCount - 1);
	JumpCurrentCountPreJump = JumpCurrentCount;
	bHasDoubleJumped = false;
}

bool APlayerBase::CanJumpWhileFalling() const
{
	return bGrappleCancelJumpAvailable || Super::CanJumpWhileFalling();
}

void APlayerBase::CheckJumpInput(float DeltaTime)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const bool bHadGrappleJump = bGrappleCancelJumpAvailable;
	const FVector IncomingVelocity = Movement->Velocity;
	Super::CheckJumpInput(DeltaTime);
	if (bHadGrappleJump && !bGrappleCancelJumpAvailable && Movement->IsFalling())
	{
		// Normal jumping replaces vertical speed. A tether release adds lift to its momentum.
		Movement->Velocity = Movement->ConstrainDirectionToPlane(IncomingVelocity + FVector::UpVector * Movement->JumpZVelocity);
	}
}

void APlayerBase::OnJumped_Implementation()
{
	if (bGrappleCancelJumpAvailable)
	{
		bGrappleCancelJumpAvailable = false;
		JumpCurrentCount = JumpMaxCount;
		bHasDoubleJumped = true;
	}
	Super::OnJumped_Implementation();
}

void APlayerBase::Landed(const FHitResult& Hit)
{
	bGrappleCancelJumpAvailable = false;
	Super::Landed(Hit);
}

bool APlayerBase::IsGrappling() const
{
	return GrapplingHookComponent && GrapplingHookComponent->IsPulling();
}

void APlayerBase::BeginPlay()
{
	Super::BeginPlay();
	InitializePlayer();
	if (HasAuthority() && !bHasCheckpoint) SetCheckpoint(GetActorTransform());
}

void APlayerBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializePlayer();
}

void APlayerBase::OnRep_Controller()
{
	Super::OnRep_Controller();
	InitializePlayer();
}

void APlayerBase::InitializePlayer()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	// Explicitly register before applying initial stats; repeated calls do not duplicate the set.
	AbilitySystemComponent->AddAttributeSetSubobject(CombatAttributes.Get());
	AbilitySystemComponent->AddAttributeSetSubobject(WeaponSlotAttributes.Get());

	if (!MoveSpeedChangedHandle.IsValid())
	{
		BaseWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
		MoveSpeedChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UCombatAttributeSet::GetMoveSpeedMultiplierAttribute()).AddUObject(this, &APlayerBase::HandleMoveSpeedChanged);
	}
	UpdateMoveSpeed();

	if (!DoubleJumpTagChangedHandle.IsValid())
	{
		DoubleJumpTagChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(TAG_PlayerDoubleJump)
			.AddUObject(this, &APlayerBase::HandleDoubleJumpTagChanged);
	}
	UpdateJumpLimit();
	if (!HasAuthority() || bStartupGranted || IsDead())
	{
		RefreshPrimaryWeaponVisual();
		CoreComponent->Initialize(AbilitySystemComponent);
		return;
	}
	bStartupGranted = true;

	if (InitialCombatStatsEffect)
	{
		FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(InitialCombatStatsEffect, 1.0f, Context);
		if (Spec.IsValid()) AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
	if (!PrimaryWeapon && IsValid(InitialPrimaryWeaponData)) EquipWeapon(EWeaponSlot::Primary, InitialPrimaryWeaponData);
	if (!SecondaryWeapon && IsValid(InitialSecondaryWeaponData)) EquipWeapon(EWeaponSlot::Secondary, InitialSecondaryWeaponData);
	LastEnergyRechargeTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(EnergyRechargeTimerHandle, this, &APlayerBase::RechargeWeaponEnergy, 0.05f, true);
	RefreshPrimaryWeaponVisual();
	CoreComponent->Initialize(AbilitySystemComponent);
	TArray<TSubclassOf<UGameplayAbility>> AbilitiesToGrant = StartupAbilities;
	const bool bHasPrimaryAttack = AbilitiesToGrant.ContainsByPredicate([](const TSubclassOf<UGameplayAbility>& AbilityClass)
	{
		return AbilityClass && AbilityClass->IsChildOf(UGA_AttackPrimary::StaticClass());
	});
	if (!bHasPrimaryAttack) AbilitiesToGrant.Add(UGA_AttackPrimary::StaticClass());
	const bool bHasSecondaryAttack = AbilitiesToGrant.ContainsByPredicate([](const TSubclassOf<UGameplayAbility>& AbilityClass)
	{
		return AbilityClass && AbilityClass->IsChildOf(UGA_AttackSecondary::StaticClass());
	});
	if (!bHasSecondaryAttack) AbilitiesToGrant.Add(UGA_AttackSecondary::StaticClass());
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilitiesToGrant)
	{
		if (AbilityClass && !AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
		{
			FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
			if (AbilityClass->IsChildOf(UGA_AttackPrimary::StaticClass()))
			{
				Spec.GetDynamicSpecSourceTags().AddTag(WeaponGameplayTags::Input_Primary);
			}
			if (AbilityClass->IsChildOf(UGA_AttackSecondary::StaticClass()))
			{
				Spec.GetDynamicSpecSourceTags().AddTag(WeaponGameplayTags::Input_Secondary);
			}
			AbilitySystemComponent->GiveAbility(Spec);
		}
	}
}

void APlayerBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput) return;
	if (GrapplingHookAction)
	{
		EnhancedInput->BindAction(GrapplingHookAction, ETriggerEvent::Started, this, &APlayerBase::RequestGrapplingHook);
	}
	else
	{
		FInputKeyBinding GrappleBinding(FInputChord(EKeys::E), IE_Pressed);
		GrappleBinding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &APlayerBase::RequestGrapplingHook);
		EnhancedInput->KeyBindings.Add(MoveTemp(GrappleBinding));
	}
	TSet<const UInputAction*> BoundActions;
	for (const FAbilityInputBinding& Binding : AbilityInputBindings)
	{
		if (!Binding.InputAction || !Binding.InputTag.IsValid() || BoundActions.Contains(Binding.InputAction.Get())) continue;
		BoundActions.Add(Binding.InputAction.Get());
		EnhancedInput->BindAction(Binding.InputAction, ETriggerEvent::Started, this, &APlayerBase::AbilityInputPressed, Binding.InputTag);
		EnhancedInput->BindAction(Binding.InputAction, ETriggerEvent::Completed, this, &APlayerBase::AbilityInputReleased, Binding.InputTag);
		EnhancedInput->BindAction(Binding.InputAction, ETriggerEvent::Canceled, this, &APlayerBase::AbilityInputCanceled, Binding.InputTag);
	}
	// 설정된 액션이 없는 슬롯은 기본 좌/우 클릭을 사용한다.
	auto BindDefaultMouse = [this, EnhancedInput](FGameplayTag Tag, FKey Key)
	{
		if (AbilityInputBindings.ContainsByPredicate([Tag](const FAbilityInputBinding& Binding)
			{ return Binding.InputAction && Binding.InputTag == Tag; })) return;
		FInputKeyBinding PressBinding(FInputChord(Key), IE_Pressed);
		PressBinding.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(this, [this, Tag]() { AbilityInputPressed(Tag); });
		EnhancedInput->KeyBindings.Add(MoveTemp(PressBinding));
		FInputKeyBinding ReleaseBinding(FInputChord(Key), IE_Released);
		ReleaseBinding.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(this, [this, Tag]() { AbilityInputReleased(Tag); });
		EnhancedInput->KeyBindings.Add(MoveTemp(ReleaseBinding));
	};
	BindDefaultMouse(WeaponGameplayTags::Input_Primary, EKeys::LeftMouseButton);
	BindDefaultMouse(WeaponGameplayTags::Input_Secondary, EKeys::RightMouseButton);
}

void APlayerBase::AbilityInputPressed(FGameplayTag InputTag)
{
	UpdateMouseAim();
	if (!HasAuthority())
	{
		ServerAbilityInput(InputTag, 1, GetAimTargetLocation());
		return;
	}
	if (IsDead() || IsHitReacting()) return;
	TArray<FGameplayAbilitySpecHandle> Handles;
	{
		FScopedAbilityListLock Lock(*AbilitySystemComponent);
		for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
		{
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) Handles.Add(Spec.Handle);
		}
	}
	const EWeaponSlot RequestedSlot = InputTag == WeaponGameplayTags::Input_Primary ? EWeaponSlot::Primary
		: InputTag == WeaponGameplayTags::Input_Secondary ? EWeaponSlot::Secondary : EWeaponSlot::None;
	if (IsChargingWeapon() && RequestedSlot != EWeaponSlot::None && RequestedSlot != GetChargingWeaponSlot()
		&& IsValid(GetWeaponInSlot(RequestedSlot)) && !Handles.IsEmpty())
	{
		// 반대 슬롯 입력은 발사 전 차지만 취소한다. 비용과 쿨다운은 실제 공격 실행 때 적용된다.
		const FGameplayTag ChargingInput = GetChargingWeaponSlot() == EWeaponSlot::Primary
			? WeaponGameplayTags::Input_Primary : WeaponGameplayTags::Input_Secondary;
		TArray<FGameplayAbilitySpecHandle> ChargingHandles;
		{
			FScopedAbilityListLock Lock(*AbilitySystemComponent);
			for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
			{
				if (Spec.IsActive() && Spec.Ability && Spec.Ability->IsA<UGA_WeaponAttackBase>()
					&& Spec.GetDynamicSpecSourceTags().HasTagExact(ChargingInput)) ChargingHandles.Add(Spec.Handle);
			}
		}
		for (const FGameplayAbilitySpecHandle Handle : ChargingHandles)
		{
			AbilitySystemComponent->CancelAbilityHandle(Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle Handle : Handles)
	{
		FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
		if (!Spec) continue;
		const bool bAlreadyActive = Spec->IsActive();
		UGameplayAbility* Instance = Spec->GetPrimaryInstance();
		const FPredictionKey PredictionKey = Instance ? Instance->GetCurrentActivationInfo().GetActivationPredictionKey() : FPredictionKey();
		AbilitySystemComponent->AbilitySpecInputPressed(*Spec);
		if (bAlreadyActive)
		{
			AbilitySystemComponent->InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Handle, PredictionKey);
		}
		if (!bAlreadyActive)
		{
			AbilitySystemComponent->TryActivateAbility(Handle);
		}
	}
}

void APlayerBase::AbilityInputReleased(FGameplayTag InputTag)
{
	UpdateMouseAim();
	if (!HasAuthority())
	{
		ServerAbilityInput(InputTag, 0, GetAimTargetLocation());
		return;
	}
	TArray<FGameplayAbilitySpecHandle> Handles;
	{
		FScopedAbilityListLock Lock(*AbilitySystemComponent);
		for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
		{
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) Handles.Add(Spec.Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle Handle : Handles)
	{
		if (FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle))
		{
			const bool bWasActive = Spec->IsActive();
			UGameplayAbility* Instance = Spec->GetPrimaryInstance();
			const FPredictionKey PredictionKey = Instance ? Instance->GetCurrentActivationInfo().GetActivationPredictionKey() : FPredictionKey();
			AbilitySystemComponent->AbilitySpecInputReleased(*Spec);
			if (bWasActive)
			{
				AbilitySystemComponent->InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Handle, PredictionKey);
			}
		}
	}
}

void APlayerBase::RefreshPrimaryWeaponVisual()
{
	// 클라이언트 외형은 시작 장비 데이터로도 구성할 수 있다. 무기 UObject는 서버 소유다.
	const UWeaponDataAsset* PrimaryData = PrimaryWeapon ? PrimaryWeapon->WeaponData.Get() : InitialPrimaryWeaponData.Get();
	const UWeaponDataAsset* SecondaryData = SecondaryWeapon ? SecondaryWeapon->WeaponData.Get() : InitialSecondaryWeaponData.Get();
	RefreshWeaponVisual(PrimaryData, PrimaryWeaponMesh, VisibleWeaponSlot == EWeaponSlot::Primary);
	RefreshWeaponVisual(SecondaryData, SecondaryWeaponMesh, VisibleWeaponSlot == EWeaponSlot::Secondary);
}

void APlayerBase::ShowWeaponInSlot(EWeaponSlot Slot)
{
	if (!HasAuthority() || !IsValid(GetWeaponInSlot(Slot))) return;
	VisibleWeaponSlot = Slot;
	RefreshPrimaryWeaponVisual();
	ForceNetUpdate();
}

void APlayerBase::OnRep_VisibleWeaponSlot()
{
	RefreshPrimaryWeaponVisual();
	OnWeaponDataChanged.Broadcast();
}

UWeaponBase* APlayerBase::GetWeaponInSlot(EWeaponSlot Slot) const
{
	if (Slot == EWeaponSlot::Primary) return PrimaryWeapon;
	if (Slot == EWeaponSlot::Secondary) return SecondaryWeapon;
	return nullptr;
}

UWeaponDataAsset* APlayerBase::GetWeaponDataInSlot(EWeaponSlot Slot) const
{
	if (const UWeaponBase* Weapon = GetWeaponInSlot(Slot)) return Weapon->WeaponData;
	if (Slot == EWeaponSlot::Primary) return InitialPrimaryWeaponData;
	if (Slot == EWeaponSlot::Secondary) return InitialSecondaryWeaponData;
	return nullptr;
}

double APlayerBase::GetChargeClockTime() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
}

bool APlayerBase::IsChargingWeapon() const
{
	return !IsDead() && WeaponChargeState.Slot != EWeaponSlot::None && WeaponChargeState.Duration > 0.0f;
}

float APlayerBase::GetWeaponChargeRatio() const
{
	return IsChargingWeapon() ? FMath::Clamp(float((GetChargeClockTime() - WeaponChargeState.ServerStartTime)
		/ WeaponChargeState.Duration), 0.0f, 1.0f) : 0.0f;
}

void APlayerBase::BeginWeaponCharge(EWeaponSlot Slot, float Duration)
{
	if (!HasAuthority() || IsDead() || Slot == EWeaponSlot::None || !FMath::IsFinite(Duration) || Duration <= 0.0f) return;
	WeaponChargeState.Slot = Slot;
	WeaponChargeState.Duration = Duration;
	WeaponChargeState.ServerStartTime = GetChargeClockTime();
	ForceNetUpdate();
}

void APlayerBase::ClearWeaponCharge(EWeaponSlot Slot)
{
	if (!HasAuthority() || WeaponChargeState.Slot != Slot) return;
	WeaponChargeState = FPlayerWeaponChargeState();
	ForceNetUpdate();
}

bool APlayerBase::EquipWeapon(EWeaponSlot Slot, UWeaponDataAsset* Data)
{
	if (!HasAuthority() || IsDead() || !IsValid(Data) || Slot == EWeaponSlot::None) return false;
	// Equipment changes during a charge/swing must not replace its captured weapon.
	if (AbilitySystemComponent->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking)) return false;
	const bool bRanged = Data->WeaponType == EWeaponType::LightRanged || Data->WeaponType == EWeaponType::HeavyRanged;
	UWeaponBase* Weapon = bRanged ? static_cast<UWeaponBase*>(NewObject<URangedWeaponBase>(this))
		: static_cast<UWeaponBase*>(NewObject<UMeleeWeaponBase>(this));
	if (!Weapon->Initialize(Data)) return false;
	if (Slot == EWeaponSlot::Primary) { PrimaryWeapon = Weapon; InitialPrimaryWeaponData = Data; }
	else if (Slot == EWeaponSlot::Secondary) { SecondaryWeapon = Weapon; InitialSecondaryWeaponData = Data; }
	else return false;
	// Set base maximum so existing GAS modifiers still participate in the effective maximum.
	AbilitySystemComponent->SetNumericAttributeBase(UWeaponSlotAttributeSet::MaxEnergyAttribute(Slot),
		bRanged ? FMath::Max(0.0f, Data->MaxEnergy) : 0.0f);
	AbilitySystemComponent->SetNumericAttributeBase(UWeaponSlotAttributeSet::EnergyAttribute(Slot),
		AbilitySystemComponent->GetNumericAttribute(UWeaponSlotAttributeSet::MaxEnergyAttribute(Slot)));
	RefreshPrimaryWeaponVisual();
	OnWeaponDataChanged.Broadcast();
	return true;
}

void APlayerBase::RechargeWeaponEnergy()
{
	const double Now = GetWorld()->GetTimeSeconds();
	const double Previous = LastEnergyRechargeTime;
	LastEnergyRechargeTime = Now;
	if (!HasAuthority() || IsDead()) return;
	const float RechargeMultiplier = CombatAttributes->GetEnergyRechargeRateMultiplier();
	if (!FMath::IsFinite(RechargeMultiplier) || RechargeMultiplier <= 0.0f) return;
	for (EWeaponSlot Slot : {EWeaponSlot::Primary, EWeaponSlot::Secondary})
	{
		const URangedWeaponBase* Weapon = Cast<URangedWeaponBase>(GetWeaponInSlot(Slot));
		const UWeaponDataAsset* Data = Weapon ? Weapon->WeaponData.Get() : nullptr;
		if (!Data || !FMath::IsFinite(Data->EnergyRechargeRate) || Data->EnergyRechargeRate <= 0.0f) continue;
		const double Start = FMath::Max(Previous, Weapon->LastSuccessfulFireTime + FMath::Max(0.0f, Data->EnergyRechargeDelay));
		// 무기 기본 충전량에 코어 배율을 적용해 이번 시간 간격의 회복량을 계산한다.
		const float Amount = float(FMath::Max(0.0, Now - Start)) * Data->EnergyRechargeRate * RechargeMultiplier;
		if (!FMath::IsFinite(Amount) || Amount <= 0.0f) continue;
		const FGameplayAttribute Energy = UWeaponSlotAttributeSet::EnergyAttribute(Slot);
		const float Current = AbilitySystemComponent->GetNumericAttribute(Energy);
		const float Maximum = AbilitySystemComponent->GetNumericAttribute(UWeaponSlotAttributeSet::MaxEnergyAttribute(Slot));
		if (Current < Maximum) AbilitySystemComponent->ApplyModToAttribute(Energy, EGameplayModOp::Additive, FMath::Min(Amount, Maximum - Current));
	}
}

void APlayerBase::AbilityInputCanceled(FGameplayTag InputTag)
{
	if (!HasAuthority())
	{
		ServerAbilityInput(InputTag, 2, GetAimTargetLocation());
		return;
	}
	TArray<FGameplayAbilitySpecHandle> Handles;
	{
		FScopedAbilityListLock Lock(*AbilitySystemComponent);
		for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
		{
			if (Spec.IsActive() && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) Handles.Add(Spec.Handle);
		}
	}
	for (const auto Handle : Handles) AbilitySystemComponent->CancelAbilityHandle(Handle);
}

void APlayerBase::ServerAbilityInput_Implementation(FGameplayTag InputTag, uint8 InputState, FVector_NetQuantize AimPoint)
{
	SetAimTarget(AimPoint);
	if (!InputTag.IsValid()) return;
	if (InputState == 1) AbilityInputPressed(InputTag);
	else if (InputState == 0) AbilityInputReleased(InputTag);
	else if (InputState == 2) AbilityInputCanceled(InputTag);
}

void APlayerBase::RefreshWeaponVisual(const UWeaponDataAsset* Data, UStaticMeshComponent* WeaponMesh, bool bShow)
{
	if (!IsValid(Data) || !IsValid(Data->WeaponMesh))
	{
		WeaponMesh->SetStaticMesh(nullptr);
		WeaponMesh->SetHiddenInGame(true);
		return;
	}

	FName SocketName = Data->AttachSocketName;
	if (SocketName.IsNone() || !GetMesh()->DoesSocketExist(SocketName))
	{
		SocketName = FallbackWeaponSocketName;
	}
	if (SocketName.IsNone() || !GetMesh()->DoesSocketExist(SocketName))
	{
		WeaponMesh->SetStaticMesh(nullptr);
		WeaponMesh->SetHiddenInGame(true);
		UE_LOG(LogTemp, Warning, TEXT("%s: weapon %s socket '%s' and fallback '%s' were not found on the character mesh."),
			*GetName(), *GetNameSafe(Data), *Data->AttachSocketName.ToString(), *FallbackWeaponSocketName.ToString());
		return;
	}

	WeaponMesh->SetStaticMesh(Data->WeaponMesh);
	WeaponMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	WeaponMesh->SetRelativeTransform(Data->AttachOffset);
	WeaponMesh->SetVisibility(true);
	WeaponMesh->SetHiddenInGame(!bShow);
}

bool APlayerBase::IsDead() const
{
	return DeathState.bDead || AbilitySystemComponent->HasMatchingGameplayTag(WeaponGameplayTags::State_Dead);
}

float APlayerBase::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || IsDead() || !CanBeDamaged() || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f) return 0.0f;
	const float AcceptedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (IsDead() || !FMath::IsFinite(AcceptedDamage) || AcceptedDamage <= 0.0f) return 0.0f;
	// 엔진 기본 피해도 GAS와 같은 IncomingDamage 경로로 전달한다.
	UGameplayEffect* DamageEffect = NewObject<UGameplayEffect>();
	DamageEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UCombatAttributeSet::GetIncomingDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(AcceptedDamage));
	DamageEffect->Modifiers.Add(Modifier);
	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	Context.AddInstigator(EventInstigator ? EventInstigator->GetPawn() : DamageCauser, DamageCauser);
	FHitResult HitInfo;
	FVector ImpulseDirection;
	DamageEvent.GetBestHitInfo(this, DamageCauser, HitInfo, ImpulseDirection);
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointDamage = static_cast<const FPointDamageEvent&>(DamageEvent);
		HitInfo.TraceStart = HitInfo.ImpactPoint - PointDamage.ShotDirection;
		HitInfo.TraceEnd = HitInfo.ImpactPoint;
	}
	Context.AddHitResult(HitInfo, true);
	const float PreviousHealth = CombatAttributes->GetHealth();
	FGameplayEffectSpec Spec(DamageEffect, Context, 1.0f);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(Spec);
	return FMath::Max(0.0f, PreviousHealth - CombatAttributes->GetHealth());
}

bool APlayerBase::IsHitReacting() const
{
	return AbilitySystemComponent->HasMatchingGameplayTag(WeaponGameplayTags::State_HitReacting);
}

FName APlayerBase::GetDamageReactionSection(const FGameplayEffectContextHandle& Context) const
{
	FVector DirectionToSource = FVector::ZeroVector;
	const FHitResult* Hit = Context.GetHitResult();
	if (Hit) DirectionToSource = Hit->TraceStart - Hit->TraceEnd;
	if (DirectionToSource.IsNearlyZero())
	{
		AActor* Source = Context.GetOriginalInstigator();
		if (!IsValid(Source) || Source == this) Source = Context.GetEffectCauser();
		if (IsValid(Source) && Source != this) DirectionToSource = Source->GetActorLocation() - GetActorLocation();
	}
	if (DirectionToSource.IsNearlyZero() && Hit) DirectionToSource = Hit->ImpactNormal;
	DirectionToSource = FVector::VectorPlaneProject(DirectionToSource, FVector::YAxisVector).GetSafeNormal();
	const float FacingDot = FVector::DotProduct(GetActorForwardVector(), DirectionToSource);
	// 출처가 없거나 정확히 옆/위인 피해는 기본적으로 앞에서 맞은 모션을 사용한다.
	return FacingDot < -KINDA_SMALL_NUMBER ? FName(TEXT("Forward")) : FName(TEXT("Backward"));
}

void APlayerBase::HandleHitReaction(FName ReactionSection)
{
	if (!HasAuthority() || IsDead()) return;
	bGrappleCancelJumpAvailable = false;
	// Actual damage interrupts grappling even when no reaction montage is assigned.
	GrapplingHookComponent->ClearAnchor();
	if (IsHitReacting() || !HitReactMontage) return;
	const FName Section = PlayerReaction::ResolveSection(HitReactMontage, ReactionSection);
	const int32 SectionIndex = HitReactMontage->GetSectionIndex(Section);
	const float Length = SectionIndex != INDEX_NONE ? HitReactMontage->GetSectionLength(SectionIndex) : HitReactMontage->GetPlayLength();
	const float Duration = Length /
		FMath::Max(0.01f, HitReactPlayRate * FMath::Abs(HitReactMontage->RateScale));
	if (!FMath::IsFinite(Duration) || Duration <= 0.0f) return;
	AbilitySystemComponent->AddLooseGameplayTag(WeaponGameplayTags::State_HitReacting, 1, EGameplayTagReplicationState::TagAndCountToAll);
	// 피격 중 재활성화를 먼저 막은 다음 공격과 차지만 취소한다.
	TArray<FGameplayAbilitySpecHandle> Attacks;
	{
		FScopedAbilityListLock Lock(*AbilitySystemComponent);
		for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
		{
			if (Spec.IsActive() && Spec.Ability && Spec.Ability->IsA<UGA_WeaponAttackBase>()) Attacks.Add(Spec.Handle);
		}
	}
	for (const auto Handle : Attacks) AbilitySystemComponent->CancelAbilityHandle(Handle);
	if (IsDead()) return;
	StopMovementForAttack();
	StopJumping();
	GetWorldTimerManager().SetTimer(HitReactionTimer, this, &APlayerBase::FinishHitReaction, Duration, false);
	MulticastHitReaction(Section);
}

void APlayerBase::MulticastHitReaction_Implementation(FName ReactionSection)
{
	if (IsDead() || bDeathPresentationStarted || !HitReactMontage) return;
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance) return;
	if (AnimInstance->Montage_Play(HitReactMontage, FMath::Max(0.01f, HitReactPlayRate)) > 0.0f)
	{
		PlayerReaction::SelectSection(AnimInstance, HitReactMontage, ReactionSection);
		if (HasAuthority())
		{
			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &APlayerBase::HandleHitMontageEnded);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, HitReactMontage);
		}
	}
}

void APlayerBase::HandleHitMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == HitReactMontage) FinishHitReaction();
}

void APlayerBase::FinishHitReaction()
{
	GetWorldTimerManager().ClearTimer(HitReactionTimer);
	if (HasAuthority() && IsHitReacting())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(WeaponGameplayTags::State_HitReacting, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void APlayerBase::MulticastDeath_Implementation(FName ReactionSection)
{
	PlayDeathPresentation(ReactionSection);
}

void APlayerBase::OnRep_DeathHandled()
{
	if (DeathState.bDead) PlayDeathPresentation(DeathState.Section);
	else if (bDeathPresentationStarted) ResetRespawnPresentation();
}

void APlayerBase::PlayDeathPresentation(FName ReactionSection)
{
	if (bDeathPresentationStarted) return;
	bDeathPresentationStarted = true;
	StopJumping();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance) return;
	AnimInstance->StopAllMontages(0.1f);
	if (DeathMontage && AnimInstance->Montage_Play(DeathMontage, FMath::Max(0.01f, DeathPlayRate)) > 0.0f)
	{
		PlayerReaction::SelectSection(AnimInstance, DeathMontage, ReactionSection);
		if (FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(DeathMontage))
		{
			// 공유 에셋은 변경하지 않고 이번 재생만 마지막 사망 자세를 유지한다.
			Instance->bEnableAutoBlendOut = false;
		}
	}
}

void APlayerBase::HandleDeath(FName ReactionSection)
{
	if (!HasAuthority() || DeathState.bDead) return;
	DeathState.bDead = true;
	DeathState.Section = PlayerReaction::ResolveSection(DeathMontage, ReactionSection);
	AbilitySystemComponent->AddLooseGameplayTag(WeaponGameplayTags::State_Dead, 1, EGameplayTagReplicationState::TagAndCountToAll);
	CleanupPlayerActions();
	MulticastDeath(DeathState.Section);
	ForceNetUpdate();
	OnPlayerDied.Broadcast(this);
	if (bAutoRespawn && IsValid(this) && DeathState.bDead)
	{
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &APlayerBase::TryAutoRespawn, FMath::Max(0.1f, RespawnDelay), false);
	}
}

bool APlayerBase::SetCheckpoint(const FTransform& Transform)
{
	if (!HasAuthority() || IsDead() || Transform.ContainsNaN()) return false;
	CheckpointTransform = FTransform(Transform.GetRotation(), Transform.GetLocation(), GetActorScale3D());
	CheckpointGravity = GetCharacterMovement()->GetGravityDirection();
	bCheckpointGravityReversed = AbilitySystemComponent->HasMatchingGameplayTag(
		FGameplayTag::RequestGameplayTag(TEXT("State.Gravity.Reversed")));
	bHasCheckpoint = true;
	return true;
}

void APlayerBase::TryAutoRespawn()
{
	// A temporarily obstructed checkpoint is retried instead of reviving inside geometry.
	if (IsDead() && !RespawnAtCheckpoint())
	{
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &APlayerBase::TryAutoRespawn, 0.5f, false);
	}
}

bool APlayerBase::RespawnAtCheckpoint()
{
	if (!HasAuthority() || !DeathState.bDead || !bHasCheckpoint || CombatAttributes->GetMaxHealth() <= 0.0f) return false;
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CheckpointRespawn), false, this);
	if (GetWorld()->OverlapBlockingTestByProfile(CheckpointTransform.GetLocation(), CheckpointTransform.GetRotation(),
		Capsule->GetCollisionProfileName(), FCollisionShape::MakeCapsule(
			Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query)) return false;
	if (!TeleportTo(CheckpointTransform.GetLocation(), CheckpointTransform.Rotator())) return false;
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	// Keep equipment, core grants and permanent effects on the existing ASC.
	const bool bGravityReversed = AbilitySystemComponent->HasMatchingGameplayTag(
		FGameplayTag::RequestGameplayTag(TEXT("State.Gravity.Reversed")));
	// ToggleGravityState requires a living player; movement stays disabled until reset.
	DeathState.bDead = false;
	AbilitySystemComponent->RemoveLooseGameplayTag(WeaponGameplayTags::State_Dead, 1, EGameplayTagReplicationState::TagAndCountToAll);
	if (bGravityReversed != bCheckpointGravityReversed)
	{
		if (UGravityStateComponent* Gravity = FindComponentByClass<UGravityStateComponent>()) Gravity->ToggleGravityState();
	}
	GetCharacterMovement()->SetGravityDirection(CheckpointGravity);
	SetActorRotation(CheckpointTransform.GetRotation(), ETeleportType::TeleportPhysics);
	CombatAttributes->SetIncomingDamage(0.0f);
	CombatAttributes->SetHealth(CombatAttributes->GetMaxHealth());
	DeathState.Section = NAME_None;
	LastEnergyRechargeTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(EnergyRechargeTimerHandle, this, &APlayerBase::RechargeWeaponEnergy, 0.05f, true);
	MulticastRespawn();
	ForceNetUpdate();
	OnPlayerRespawned.Broadcast(this);
	return true;
}

void APlayerBase::MulticastRespawn_Implementation()
{
	ResetRespawnPresentation();
}

void APlayerBase::ResetRespawnPresentation()
{
	bDeathPresentationStarted = false;
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) Anim->StopAllMontages(0.0f);
	ActionValueY = DropValue = 0.0f;
	bHasDoubleJumped = bMovingHorizontally = false;
	bGrappleJumpStateCaptured = bGrappleCancelJumpAvailable = false;
	StopJumping();
	ResetJumpState();
	ConsumeMovementInputVector();
	for (const auto& Platform : IgnoredSoftPlatforms)
	{
		if (Platform.IsValid()) GetCapsuleComponent()->IgnoreComponentWhenMoving(Platform.Get(), false);
	}
	IgnoredSoftPlatforms.Empty();
	DropThroughPlatform.Reset();
	SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->ClearAccumulatedForces();
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	RefreshSoftPlatformCollision();
	bHasAimTarget = false;
}

void APlayerBase::FellOutOfWorld(const UDamageType& DamageType)
{
	// The engine normally destroys the pawn here, which would lose its checkpoint and core grants.
	if (HasAuthority()) HandleDeath();
}

void APlayerBase::CleanupPlayerActions()
{
	WeaponChargeState = FPlayerWeaponChargeState();
	bGrappleCancelJumpAvailable = false;
	GrapplingHookComponent->ClearAnchor();
	FinishHitReaction();
	AbilitySystemComponent->CancelAllAbilities();
	GetWorldTimerManager().ClearTimer(EnergyRechargeTimerHandle);
	ActionValueY = 0;
	DropValue = 0;
	StopJumping();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
}

void APlayerBase::HandleDoubleJumpTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	UpdateJumpLimit();
}

void APlayerBase::UpdateJumpLimit()
{
	JumpMaxCount = AbilitySystemComponent->HasMatchingGameplayTag(TAG_PlayerDoubleJump) ? 2 : 1;
}

void APlayerBase::HandleMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	UpdateMoveSpeed();
}

void APlayerBase::UpdateMoveSpeed()
{
	const float Multiplier = CombatAttributes->GetMoveSpeedMultiplier();
	if (!FMath::IsFinite(Multiplier)) return;
	// 항상 기본 속도에서 계산하여 배율 변경이나 재초기화 시 속도가 중복 증가하지 않게 한다.
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * FMath::Max(0.0f, Multiplier);
}

void APlayerBase::DoMove(float Forward)
{
	if (IsDead() || IsHitReacting() || IsGrappling() || AbilitySystemComponent->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking))
	{
		ActionValueY = 0.0f;
		return;
	}
	Super::DoMove(Forward);
}

void APlayerBase::DoDrop(float Value)
{
	if (IsDead() || IsHitReacting() || IsGrappling() || AbilitySystemComponent->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking))
	{
		DropValue = 0.0f;
		return;
	}
	Super::DoDrop(Value);
}

void APlayerBase::DoJumpStart()
{
	if (IsDead() || IsHitReacting() || AbilitySystemComponent->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking)) return;
	if (IsGrappling()) GrapplingHookComponent->CancelGrapple();
	if (bGrappleCancelJumpAvailable && !IsGrappling())
	{
		// Keep the release trajectory even beside a wall or while holding drop input.
		Jump();
		return;
	}
	if (!IsGrappling())
	{
		Super::DoJumpStart();
	}
}

void APlayerBase::StopMovementForAttack()
{
	ActionValueY = 0.0f;
	DropValue = 0.0f;
	ConsumeMovementInputVector();
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement->IsMovingOnGround())
	{
		Movement->StopMovementImmediately();
	}
	// 공중에서는 기존 속도를 유지한다. 공격 중 새 이동 입력은 DoMove에서 차단한다.
}

void APlayerBase::DoInteract()
{
	if (IsDead() || IsHitReacting() || !IsValid(InteractionComponent))
	{
		return;
	}

	InteractionComponent->RequestInteract();
}

UCoreComponent* APlayerBase::GetCoreComponent() const
{
	return CoreComponent;
}

void APlayerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	GetWorldTimerManager().ClearTimer(HitReactionTimer);
	if (DoubleJumpTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(TAG_PlayerDoubleJump).Remove(DoubleJumpTagChangedHandle);
		DoubleJumpTagChangedHandle.Reset();
	}
	if (MoveSpeedChangedHandle.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UCombatAttributeSet::GetMoveSpeedMultiplierAttribute()).Remove(MoveSpeedChangedHandle);
		MoveSpeedChangedHandle.Reset();
	}
	GetWorldTimerManager().ClearTimer(EnergyRechargeTimerHandle);
	AbilitySystemComponent->CancelAllAbilities();
	Super::EndPlay(EndPlayReason);
}
