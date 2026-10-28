#include "Character/Player/PlayerBase.h"
#include "Character/Player/GrapplingHookComponent.h"
#include "Map/AreaMapComponent.h"
#include "Character/Player/CombatFeedbackComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gimmick/Component/InteractionComponent.h"
#include "Gimmick/Component/CarryComponent.h"
#include "Gimmick/Component/GravityStateComponent.h"
#include "Gimmick/Component/PushPullComponent.h"
#include "Core/CoreComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Weapon/MeleeWeaponBase.h"
#include "Weapon/WeaponImpactFeedback.h"
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
#include "Camera/PlayerCameraManager.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Engine/DamageEvents.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Projectile/ProjectileBase.h"
#include "GameFramework/GameStateBase.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Weapon/EnergyMuzzleFlash.h"

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
	RangedAimPostProcessClass = FSoftObjectPath(TEXT("/Game/Test_Jihong/Animations/ABP_PlayerRangedReady.ABP_PlayerRangedReady_C"));
	DefaultLightRangedProjectileClass = FSoftObjectPath(TEXT("/Game/Test_Jihong/Blueprints/Projectile/BP_Projectile_Plasma.BP_Projectile_Plasma_C"));
	DefaultHeavyRangedProjectileClass = FSoftObjectPath(TEXT("/Game/Test_Jihong/Blueprints/Projectile/BP_Projectile_Railgun.BP_Projectile_Railgun_C"));
	WeaponChoices.SetNum(4);
	OwnedWeaponChoices.SetNumZeroed(4);
	WeaponSelectionActions.SetNum(4);
	GravityStateComponent = CreateDefaultSubobject<UGravityStateComponent>(TEXT("GravityStateComponent"));
	CombatFeedbackComponent = CreateDefaultSubobject<UCombatFeedbackComponent>(TEXT("CombatFeedbackComponent"));
	GrapplingHookComponent = CreateDefaultSubobject<UGrapplingHookComponent>(TEXT("GrapplingHookComponent"));
	AreaMapComponent = CreateDefaultSubobject<UAreaMapComponent>(TEXT("AreaMapComponent"));
	static ConstructorHelpers::FObjectFinder<UInputAction> GrappleAction(TEXT("/Game/Main/Inputs/Actions/IA_GrapplingHook.IA_GrapplingHook"));
	GrapplingHookAction = GrappleAction.Object;
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	CombatAttributes = CreateDefaultSubobject<UCombatAttributeSet>(TEXT("CombatAttributes"));
	WeaponSlotAttributes = CreateDefaultSubobject<UWeaponSlotAttributeSet>(TEXT("WeaponSlotAttributes"));

	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	CarryComponent = CreateDefaultSubobject<UCarryComponent>(TEXT("CarryComponent"));
	PushPullComponent = CreateDefaultSubobject<UPushPullComponent>(TEXT("PushPullComponent"));

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
	DOREPLIFETIME(APlayerBase, WeaponChoices);
	DOREPLIFETIME(APlayerBase, OwnedWeaponChoices);
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

FVector APlayerBase::GetWeaponMuzzleLocation(EWeaponSlot Slot) const
{
	const UStaticMeshComponent* WeaponMesh = Slot == EWeaponSlot::Primary ? PrimaryWeaponMesh.Get()
		: Slot == EWeaponSlot::Secondary ? SecondaryWeaponMesh.Get() : nullptr;
	return WeaponMesh && WeaponMesh->DoesSocketExist(TEXT("Muzzle"))
		? WeaponMesh->GetSocketLocation(TEXT("Muzzle")) : GetActorLocation();
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

TSubclassOf<AProjectileBase> APlayerBase::GetAttackProjectileClass(const UWeaponDataAsset* Data) const
{
	if (!Data) return nullptr;
	// Explicit weapon-specific Blueprint classes always win over the player defaults.
	if (Data->ProjectileClass != AProjectileBase::StaticClass()) return Data->ProjectileClass;
	const TSoftClassPtr<AProjectileBase>& Default = Data->WeaponType == EWeaponType::HeavyRanged
		? DefaultHeavyRangedProjectileClass : DefaultLightRangedProjectileClass;
	if (UClass* Class = Default.LoadSynchronous()) return Class;
	return Data->ProjectileClass;
}

bool APlayerBase::IsRangedAiming() const
{
	const UWeaponDataAsset* Data = GetWeaponDataInSlot(VisibleWeaponSlot);
	return Data && !IsDead() && !IsHitReacting() && AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking)
		&& (Data->WeaponType == EWeaponType::LightRanged || Data->WeaponType == EWeaponType::HeavyRanged);
}

void APlayerBase::UpdateRangedFacing()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (IsRangedAiming())
	{
		if (!bRangedFacingOverride) bRestoreOrientToMovement = Movement->bOrientRotationToMovement;
		bRangedFacingOverride = true;
		Movement->bOrientRotationToMovement = false;
		const FVector Aim = GetAimTargetLocation() - GetActorLocation();
		if (FMath::Abs(Aim.X) > 5.0f)
		{
			const FVector Up = -Movement->GetGravityDirection();
			const FVector Forward = FVector::VectorPlaneProject(
				Aim.X > 0.0f ? FVector::ForwardVector : -FVector::ForwardVector, Up).GetSafeNormal();
			if (!Forward.IsNearlyZero()) SetActorRotation(FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat());
		}
	}
	else if (bRangedFacingOverride)
	{
		Movement->bOrientRotationToMovement = bRestoreOrientToMovement;
		bRangedFacingOverride = false;
	}
}

void APlayerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && PendingAttackInput.IsValid()
		&& !AbilitySystemComponent->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking))
	{
		const FGameplayTag Input = PendingAttackInput;
		const EWeaponSlot Slot = Input == WeaponGameplayTags::Input_Primary ? EWeaponSlot::Primary : EWeaponSlot::Secondary;
		const bool bSameWeapon = PendingAttackWeapon.IsValid() && PendingAttackWeapon.Get() == GetWeaponInSlot(Slot);
		ClearPendingAttack();
		if (bSameWeapon && !IsDead() && !IsHitReacting()) AbilityInputPressed(Input);
	}
	UpdateMouseAim();
	if (HasAuthority() && !IsLocallyControlled() && bHasAimTarget)
	{
		// 클라이언트의 다음 갱신 전에도 이동한 캐릭터 위치를 기준으로 방향을 계산한다.
		SetAimTarget(AimTargetLocation);
	}
	UpdateRangedFacing();
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
	// Authority must evaluate hand bones even without a rendered viewport to resolve muzzle sockets.
	if (HasAuthority()) GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	if (UClass* AimClass = RangedAimPostProcessClass.LoadSynchronous())
	{
		GetMesh()->SetOverridePostProcessAnimBP(AimClass);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load ranged aim animation class: %s"),
			*RangedAimPostProcessClass.ToSoftObjectPath().ToString());
	}
	InitializePlayer();
	if (HasAuthority() && !bHasCheckpoint) SetCheckpoint(GetActorTransform());
	ScheduleProgressRestore();
}

void APlayerBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializePlayer();
	if (HasActorBegunPlay()) ScheduleProgressRestore();
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
	AbilitySystemComponent->SetNumericAttributeBase(UWeaponSlotAttributeSet::GetSharedMaxEnergyAttribute(), FMath::Max(0.0f, InitialSharedMaxEnergy));
	AbilitySystemComponent->SetNumericAttributeBase(UWeaponSlotAttributeSet::GetSharedEnergyAttribute(), WeaponSlotAttributes->GetSharedMaxEnergy());

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
	for (int32 Index = 0; Index < FMath::Min(4, WeaponSelectionActions.Num()); ++Index)
	{
		if (!WeaponSelectionActions[Index]) continue;
		EnhancedInput->BindAction(WeaponSelectionActions[Index], ETriggerEvent::Started, this, &APlayerBase::SetWeaponChoiceHeld, Index, true);
		EnhancedInput->BindAction(WeaponSelectionActions[Index], ETriggerEvent::Completed, this, &APlayerBase::SetWeaponChoiceHeld, Index, false);
		EnhancedInput->BindAction(WeaponSelectionActions[Index], ETriggerEvent::Canceled, this, &APlayerBase::SetWeaponChoiceHeld, Index, false);
	}
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

void APlayerBase::CancelWeaponInputsForUI()
{
	ClearPendingAttack();
	AbilityInputCanceled(WeaponGameplayTags::Input_Primary);
	AbilityInputCanceled(WeaponGameplayTags::Input_Secondary);
	for (int32 Index=0;Index<4;++Index) SetWeaponChoiceHeld(Index,false);
	StopJumping();ConsumeMovementInputVector();
}

void APlayerBase::AbilityInputPressed(FGameplayTag InputTag)
{
	const EWeaponSlot AssignmentSlot = InputTag == WeaponGameplayTags::Input_Primary ? EWeaponSlot::Primary
		: InputTag == WeaponGameplayTags::Input_Secondary ? EWeaponSlot::Secondary : EWeaponSlot::None;
	const int32 Choice = GetHeldWeaponChoice();
	if (AssignmentSlot != EWeaponSlot::None && Choice != INDEX_NONE)
	{
		// A binding click never starts an attack, even if assignment is currently blocked.
		AssignmentClickInputs.AddTag(InputTag);
		if (HasAuthority()) AssignWeaponChoice(Choice, AssignmentSlot);
		else ServerAssignWeaponChoice(Choice, AssignmentSlot);
		return;
	}
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
	// The initial hold delay is also cancelable by the opposite slot, before charge HUD/audio starts.
	EWeaponSlot WaitingSlot = EWeaponSlot::None;
	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		const UGA_WeaponAttackBase* Attack = Cast<UGA_WeaponAttackBase>(Spec.GetPrimaryInstance());
		if (Spec.IsActive() && Attack && Attack->IsWaitingForAttackRelease())
		{
			WaitingSlot = Spec.GetDynamicSpecSourceTags().HasTagExact(WeaponGameplayTags::Input_Primary)
				? EWeaponSlot::Primary : EWeaponSlot::Secondary;
			break;
		}
	}
	if (WaitingSlot != EWeaponSlot::None && RequestedSlot != EWeaponSlot::None && RequestedSlot != WaitingSlot
		&& IsValid(GetWeaponInSlot(RequestedSlot)) && !Handles.IsEmpty())
	{
		// 반대 슬롯 입력은 발사 전 차지만 취소한다. 비용과 쿨다운은 실제 공격 실행 때 적용된다.
		const FGameplayTag ChargingInput = WaitingSlot == EWeaponSlot::Primary
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
	if (RequestedSlot != EWeaponSlot::None && WaitingSlot == EWeaponSlot::None
		&& AbilitySystemComponent->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking))
	{
		PendingAttackInput = InputTag;
		PendingAttackWeapon = GetWeaponInSlot(RequestedSlot);
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
	if (PendingAttackInput == InputTag) ClearPendingAttack();
	if (AssignmentClickInputs.HasTagExact(InputTag))
	{
		AssignmentClickInputs.RemoveTag(InputTag);
		return;
	}
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
	UpdateRangedFacing();
	RefreshPrimaryWeaponVisual();
	ForceNetUpdate();
}

void APlayerBase::OnRep_VisibleWeaponSlot()
{
	RefreshPrimaryWeaponVisual();
	OnWeaponDataChanged.Broadcast();
}

UWeaponDataAsset* APlayerBase::GetWeaponChoice(int32 Index) const
{
	return Index >= 0 && Index < 4 && WeaponChoices.IsValidIndex(Index) ? WeaponChoices[Index].Get() : nullptr;
}

bool APlayerBase::HasWeaponChoice(int32 Index) const
{
	const UWeaponDataAsset* Data = GetWeaponChoice(Index);
	return Data && ((OwnedWeaponChoices.IsValidIndex(Index) && OwnedWeaponChoices[Index])
		|| GetWeaponDataInSlot(EWeaponSlot::Primary) == Data || GetWeaponDataInSlot(EWeaponSlot::Secondary) == Data);
}

bool APlayerBase::AcquireWeaponChoice(int32 Index)
{
	if (!HasAuthority() || !IsValid(GetWeaponChoice(Index))) return false;
	OwnedWeaponChoices.SetNum(4);
	OwnedWeaponChoices[Index] = true;
	OnWeaponDataChanged.Broadcast();
	ForceNetUpdate();
	return true;
}

EWeaponSlot APlayerBase::GetWeaponChoiceAssignment(int32 Index) const
{
	const UWeaponDataAsset* Data = GetWeaponChoice(Index);
	if (!Data) return EWeaponSlot::None;
	if (GetWeaponDataInSlot(EWeaponSlot::Primary) == Data) return EWeaponSlot::Primary;
	if (GetWeaponDataInSlot(EWeaponSlot::Secondary) == Data) return EWeaponSlot::Secondary;
	return EWeaponSlot::None;
}

int32 APlayerBase::GetHeldWeaponChoice() const
{
	if (IsDead()) return INDEX_NONE;
	// Multiple held keys select the lowest number; releasing all keys clears selection.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (HeldWeaponChoices.Contains(Index)) return Index;
	}
	return INDEX_NONE;
}

void APlayerBase::SetWeaponChoiceHeld(int32 Index, bool bHeld)
{
	if (Index < 0 || Index >= 4) return;
	if (bHeld && !IsDead()) HeldWeaponChoices.Add(Index);
	else HeldWeaponChoices.Remove(Index);
}

void APlayerBase::ServerAssignWeaponChoice_Implementation(int32 Index, EWeaponSlot Slot)
{
	AssignWeaponChoice(Index, Slot);
}

bool APlayerBase::AssignWeaponChoice(int32 Index, EWeaponSlot Slot)
{
	UWeaponDataAsset* Data = GetWeaponChoice(Index);
	if (!HasAuthority() || IsDead() || IsHitReacting() || !IsValid(Data) || !HasWeaponChoice(Index)
		|| (Slot != EWeaponSlot::Primary && Slot != EWeaponSlot::Secondary)) return false;
	// Keep a running swing/charge bound to its captured weapon until it finishes.
	if (AbilitySystemComponent->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking)) return false;
	if (GetWeaponDataInSlot(Slot) == Data) return true;

	auto Remember = [this](UWeaponBase* Weapon)
	{
		if (!Weapon) return;
		CachedChoiceWeapons.AddUnique(Weapon);
	};
	Remember(PrimaryWeapon);
	Remember(SecondaryWeapon);
	UWeaponBase* Selected = nullptr;
	for (UWeaponBase* Weapon : CachedChoiceWeapons)
	{
		if (Weapon && Weapon->WeaponData == Data) { Selected = Weapon; break; }
	}
	if (!Selected)
	{
		const bool bRanged = Data->WeaponType == EWeaponType::LightRanged || Data->WeaponType == EWeaponType::HeavyRanged;
		Selected = bRanged ? static_cast<UWeaponBase*>(NewObject<URangedWeaponBase>(this))
			: static_cast<UWeaponBase*>(NewObject<UMeleeWeaponBase>(this));
		if (!Selected->Initialize(Data)) return false;
		CachedChoiceWeapons.Add(Selected);
	}
	UWeaponBase* Previous = GetWeaponInSlot(Slot);
	const EWeaponSlot OtherSlot = Slot == EWeaponSlot::Primary ? EWeaponSlot::Secondary : EWeaponSlot::Primary;
	UWeaponBase* Other = GetWeaponInSlot(OtherSlot);
	if (Other && Other->WeaponData == Data) { Selected = Other; Other = Previous; }
	PrimaryWeapon = Slot == EWeaponSlot::Primary ? Selected : Other;
	SecondaryWeapon = Slot == EWeaponSlot::Secondary ? Selected : Other;
	InitialPrimaryWeaponData = PrimaryWeapon ? PrimaryWeapon->WeaponData.Get() : nullptr;
	InitialSecondaryWeaponData = SecondaryWeapon ? SecondaryWeapon->WeaponData.Get() : nullptr;
	RefreshPrimaryWeaponVisual();
	OnWeaponDataChanged.Broadcast();
	ForceNetUpdate();
	return true;
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
	MulticastWeaponChargeSound(GetWeaponDataInSlot(Slot), Slot, Duration);
	ForceNetUpdate();
}

void APlayerBase::ClearWeaponCharge(EWeaponSlot Slot)
{
	if (!HasAuthority() || WeaponChargeState.Slot != Slot) return;
	WeaponChargeState = FPlayerWeaponChargeState();
	MulticastWeaponChargeSound(nullptr, EWeaponSlot::None, 0.0f);
	ForceNetUpdate();
}


void APlayerBase::PlayRangedShotFeedback(EWeaponSlot Slot, const UWeaponDataAsset* Data, const FTransform& ShotTransform)
{
	if (!HasAuthority() || !Data) return;
	UWeaponDataAsset* Feedback = const_cast<UWeaponDataAsset*>(Data);
	MulticastRangedShotFeedback(Slot, Data->WeaponType == EWeaponType::HeavyRanged, Feedback, Data->AttackEffect.Get(),
		Data->AttackSound.Get(), ShotTransform);
}

void APlayerBase::MulticastRangedShotFeedback_Implementation(EWeaponSlot Slot, bool bHeavy, UWeaponDataAsset* Feedback, UNiagaraSystem* Effect, USoundBase* Sound, FTransform ShotTransform)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	UStaticMeshComponent* Attachment = Slot == EWeaponSlot::Secondary ? SecondaryWeaponMesh.Get() : PrimaryWeaponMesh.Get();
	const bool bHasSocket = Attachment && Attachment->DoesSocketExist(TEXT("Muzzle"));
	const FName Socket = bHasSocket ? FName(TEXT("Muzzle")) : NAME_None;
	const FTransform MuzzleTransform = bHasSocket ? Attachment->GetSocketTransform(Socket) : ShotTransform;
	const FTransform Offset = Feedback ? Feedback->MuzzleFlashOffset : FTransform::Identity;
	if (Feedback && Feedback->bUseEnergyFlash)
	{
		AEnergyMuzzleFlash* Flash = GetWorld()->SpawnActor<AEnergyMuzzleFlash>(MuzzleTransform.GetLocation(), MuzzleTransform.Rotator());
		if (Flash)
		{
			if (bHasSocket)
			{
				Flash->AttachToComponent(Attachment, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
				Flash->SetActorRelativeLocation(Offset.GetLocation());
				Flash->SetActorRelativeRotation(Offset.GetRotation());
			}
			else Flash->SetActorTransform(FTransform(Offset.GetRotation(), Offset.GetLocation()) * MuzzleTransform);
			// Niagara's old tiny scale is not an energy pulse radius; use Energy Size instead.
			Flash->SetActorScale3D(FVector::OneVector);
			Flash->Initialize(Feedback, bHeavy);
		}
	}
	else if (Effect)
	{
		const FTransform EffectTransform = Offset * MuzzleTransform;
		UNiagaraComponent* Flash = bHasSocket
			? UNiagaraFunctionLibrary::SpawnSystemAttached(Effect, Attachment, Socket, Offset.GetLocation(), Offset.Rotator(), Offset.GetScale3D(), EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::None)
			: UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, EffectTransform.GetLocation(), EffectTransform.Rotator(), EffectTransform.GetScale3D());
		if (Flash && Feedback && Feedback->MuzzleFlashDuration > 0.0f)
		{
			FTimerHandle FlashTimer;
			GetWorldTimerManager().SetTimer(FlashTimer, FTimerDelegate::CreateWeakLambda(Flash, [Flash]() { Flash->DestroyComponent(); }), Feedback->MuzzleFlashDuration, false);
		}
	}
	if (Sound) UGameplayStatics::PlaySoundAtLocation(this, Sound, MuzzleTransform.GetLocation(), Feedback ? Feedback->AttackSoundVolume : 1.0f,
		Feedback ? Feedback->AttackSoundPitch : 1.0f, 0.0f, Feedback ? Feedback->WeaponSoundAttenuation.Get() : nullptr);
}

void APlayerBase::StopWeaponChargeSound()
{
	if (IsValid(WeaponChargeAudio)) WeaponChargeAudio->Stop();
	WeaponChargeAudio = nullptr;
}

void APlayerBase::MulticastWeaponImpact_Implementation(UWeaponDataAsset* Data, FVector_NetQuantize Location,
	FVector_NetQuantizeNormal Normal, UNiagaraSystem* FallbackEffect, USoundBase* FallbackSound)
{
	if (GetNetMode() != NM_DedicatedServer)
		WeaponImpactFeedback::Play(this, Data, Location, Normal, FallbackEffect, FallbackSound);
}

void APlayerBase::MulticastMeleeAttackFeedback_Implementation(EWeaponSlot Slot, UWeaponDataAsset* Data)
{
	if (GetNetMode() == NM_DedicatedServer || !Data || IsDead()
		|| (Slot != EWeaponSlot::Primary && Slot != EWeaponSlot::Secondary)
		|| (Data->WeaponType != EWeaponType::LightMelee && Data->WeaponType != EWeaponType::HeavyMelee)) return;
	StopMeleeAttackFeedback();
	MeleeFeedbackSlot = Slot;
	MeleeFeedbackData = Data;
	if (Data->MeleeAttackFeedbackDelay > 0.0f)
		GetWorldTimerManager().SetTimer(MeleeFeedbackTimer, this, &APlayerBase::StartMeleeAttackFeedback, Data->MeleeAttackFeedbackDelay, false);
	else StartMeleeAttackFeedback();
}

// 슬래시 리본은 Niagara 시스템의 Z축을 "리본 면의 법선"으로 쓴다. 무기 메시에 그대로 붙이면 Z축이 휘두르는 동안 같이 돌아
// 리본이 옆면으로 서서 얇은 선처럼 보이므로, 회전은 월드 기준으로 고정하고 법선이 카메라를 향하게 맞춘다.
static void AlignTrailToCamera(const UObject* WorldContext, UNiagaraComponent* Trail)
{
	if (!Trail) return;
	FVector Facing = FVector::LeftVector;
	if (const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr)
		if (const APlayerController* PC = World->GetFirstPlayerController())
			if (PC->PlayerCameraManager) Facing = -PC->PlayerCameraManager->GetCameraRotation().Vector();
	Trail->SetUsingAbsoluteRotation(true);
	Trail->SetWorldRotation(FQuat::FindBetweenNormals(FVector::UpVector, Facing.GetSafeNormal()));
}

void APlayerBase::StartMeleeAttackFeedback()
{
	const UWeaponDataAsset* Data = MeleeFeedbackData;
	if (!Data || IsDead()) { StopMeleeAttackFeedback(); return; }
	UStaticMeshComponent* Attachment = MeleeFeedbackSlot == EWeaponSlot::Secondary ? SecondaryWeaponMesh.Get() : PrimaryWeaponMesh.Get();
	if (!Attachment) { StopMeleeAttackFeedback(); return; }
	auto SpawnAttached = [Attachment](UNiagaraSystem* System, FName Socket, const FTransform& Offset) -> UNiagaraComponent*
	{
		if (!System) return nullptr;
		if (!Attachment->DoesSocketExist(Socket)) Socket = NAME_None;
		return UNiagaraFunctionLibrary::SpawnSystemAttached(System, Attachment, Socket,
			Offset.GetLocation(), Offset.Rotator(), Offset.GetScale3D(), EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::None);
	};
	MeleeAttackEffect = SpawnAttached(Data->AttackEffect, Data->MeleeAttackEffectSocketName, Data->MeleeAttackEffectOffset);
	if (!Data->bUseMeleeTrailNotifyWindow)
	{
		MeleeTrailEffect = SpawnAttached(Data->MeleeTrailEffect, Data->MeleeTrailSocketName, Data->MeleeTrailOffset);
		AlignTrailToCamera(this, MeleeTrailEffect);
	}
	if (MeleeTrailEffect && Data->MeleeTrailDuration > 0.0f)
		GetWorldTimerManager().SetTimer(MeleeTrailTimer, this, &APlayerBase::StopMeleeTrail, Data->MeleeTrailDuration, false);
	if (Data->AttackSound)
	{
		const FName Socket = Attachment->DoesSocketExist(Data->MeleeAttackEffectSocketName) ? Data->MeleeAttackEffectSocketName : NAME_None;
		MeleeAttackAudio = UGameplayStatics::SpawnSoundAttached(Data->AttackSound, Attachment, Socket,
			Data->MeleeAttackEffectOffset.GetLocation(), EAttachLocation::KeepRelativeOffset, true,
			Data->AttackSoundVolume, Data->AttackSoundPitch, 0.0f, Data->WeaponSoundAttenuation);
	}
}

void APlayerBase::StopMeleeTrail()
{
	GetWorldTimerManager().ClearTimer(MeleeTrailTimer);
	if (IsValid(MeleeTrailEffect)) MeleeTrailEffect->DestroyComponent();
	MeleeTrailEffect = nullptr;
}

void APlayerBase::BeginMeleeTrailWindow(const UAnimSequenceBase* Animation)
{
	const UWeaponDataAsset* Data = MeleeFeedbackData;
	if (GetNetMode() == NM_DedicatedServer || IsDead() || !Data || !Data->bUseMeleeTrailNotifyWindow
		|| Data->AttackMontage != Animation || !Data->MeleeTrailEffect) return;
	StopMeleeTrail();
	UStaticMeshComponent* Attachment = MeleeFeedbackSlot == EWeaponSlot::Secondary ? SecondaryWeaponMesh.Get() : PrimaryWeaponMesh.Get();
	if (!Attachment) return;
	const FName Socket = Attachment->DoesSocketExist(Data->MeleeTrailSocketName) ? Data->MeleeTrailSocketName : NAME_None;
	const FTransform& Offset = Data->MeleeTrailOffset;
	MeleeTrailEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(Data->MeleeTrailEffect, Attachment, Socket,
		Offset.GetLocation(), Offset.Rotator(), Offset.GetScale3D(), EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::None);
	AlignTrailToCamera(this, MeleeTrailEffect);
}

void APlayerBase::EndMeleeTrailWindow(const UAnimSequenceBase* Animation)
{
	if (MeleeFeedbackData && MeleeFeedbackData->bUseMeleeTrailNotifyWindow && MeleeFeedbackData->AttackMontage == Animation)
	{
		// 즉시 파괴하면 리본이 뚝 끊겨 싸구려처럼 보인다. 스폰만 멈추고 남은 파티클이 수명만큼 사라지게 둔다
		GetWorldTimerManager().ClearTimer(MeleeTrailTimer);
		if (IsValid(MeleeTrailEffect)) MeleeTrailEffect->Deactivate();
		MeleeTrailEffect = nullptr;
	}
}

void APlayerBase::StopMeleeAttackFeedback()
{
	GetWorldTimerManager().ClearTimer(MeleeFeedbackTimer);
	StopMeleeTrail();
	if (IsValid(MeleeAttackEffect)) MeleeAttackEffect->DestroyComponent();
	if (IsValid(MeleeAttackAudio)) MeleeAttackAudio->Stop();
	MeleeAttackEffect = nullptr;
	MeleeAttackAudio = nullptr;
	MeleeFeedbackData = nullptr;
	MeleeFeedbackSlot = EWeaponSlot::None;
}

void APlayerBase::MulticastStopMeleeAttackFeedback_Implementation(EWeaponSlot Slot)
{
	if (Slot == MeleeFeedbackSlot) StopMeleeAttackFeedback();
}

void APlayerBase::MulticastWeaponChargeSound_Implementation(UWeaponDataAsset* Feedback, EWeaponSlot Slot, float Duration)
{
	StopWeaponChargeSound();
	if (GetNetMode() == NM_DedicatedServer || !Feedback || !Feedback->ChargeSound || Duration <= 0.0f) return;
	USceneComponent* Attachment = Slot == EWeaponSlot::Secondary ? SecondaryWeaponMesh.Get() : PrimaryWeaponMesh.Get();
	const FName Socket = Attachment->DoesSocketExist(TEXT("Muzzle")) ? FName(TEXT("Muzzle")) : NAME_None;
	float Pitch = FMath::Max(0.1f, Feedback->ChargeSoundPitch);
	const float SoundDuration = Feedback->ChargeSound->GetDuration();
	if (Feedback->bFitChargeSoundToDuration && FMath::IsFinite(SoundDuration) && SoundDuration > 0.0f && SoundDuration < 60.0f)
		Pitch *= FMath::Clamp(SoundDuration / Duration, 0.25f, 4.0f);
	if (IsLocallyControlled() && Feedback->bLocalChargeSound2D)
	{
		WeaponChargeAudio = UGameplayStatics::SpawnSound2D(this, Feedback->ChargeSound,
			Feedback->ChargeSoundVolume * Feedback->LocalChargeSoundVolumeMultiplier, Pitch);
	}
	else
	{
		WeaponChargeAudio = UGameplayStatics::SpawnSoundAttached(Feedback->ChargeSound, Attachment, Socket,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true,
			Feedback->ChargeSoundVolume, Pitch, 0.0f, Feedback->WeaponSoundAttenuation);
	}
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
	if (bRanged)
	{
		// Warm the shared flash material while equipping/charging, not on its first 0.14-second pulse.
		if (Data->bUseEnergyFlash) Data->EnergyFlashMaterial.LoadSynchronous();
	}
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (GetWeaponChoice(Index) == Data)
		{
			OwnedWeaponChoices.SetNum(4);
			OwnedWeaponChoices[Index] = true;
		}
	}
	if (Slot == EWeaponSlot::Primary) { PrimaryWeapon = Weapon; InitialPrimaryWeaponData = Data; }
	else if (Slot == EWeaponSlot::Secondary) { SecondaryWeapon = Weapon; InitialSecondaryWeaponData = Data; }
	else return false;
	RefreshPrimaryWeaponVisual();
	OnWeaponDataChanged.Broadcast();
	return true;
}

void APlayerBase::NotifyWeaponEnergySpent()
{
	if (HasAuthority()) LastSharedEnergyUseTime = GetWorld()->GetTimeSeconds();
}

void APlayerBase::RechargeWeaponEnergy()
{
	const double Now = GetWorld()->GetTimeSeconds();
	const double Previous = LastEnergyRechargeTime;
	LastEnergyRechargeTime = Now;
	if (!HasAuthority() || IsDead()) return;
	const float RechargeMultiplier = CombatAttributes->GetEnergyRechargeRateMultiplier();
	if (!FMath::IsFinite(RechargeMultiplier) || RechargeMultiplier <= 0.0f) return;
	const double Start = FMath::Max(Previous, LastSharedEnergyUseTime + FMath::Max(0.0f, SharedEnergyRechargeDelay));
	const float Amount = float(FMath::Max(0.0, Now - Start)) * SharedEnergyRechargeRate * RechargeMultiplier;
	if (!FMath::IsFinite(Amount) || Amount <= 0.0f) return;
	const float Current = WeaponSlotAttributes->GetSharedEnergy();
	const float Maximum = WeaponSlotAttributes->GetSharedMaxEnergy();
	if (Current < Maximum) AbilitySystemComponent->ApplyModToAttribute(UWeaponSlotAttributeSet::GetSharedEnergyAttribute(), EGameplayModOp::Additive, FMath::Min(Amount, Maximum - Current));
}

void APlayerBase::AbilityInputCanceled(FGameplayTag InputTag)
{
	if (PendingAttackInput == InputTag) ClearPendingAttack();
	if (AssignmentClickInputs.HasTagExact(InputTag))
	{
		AssignmentClickInputs.RemoveTag(InputTag);
		return;
	}
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
	if (IsHitReacting() || !HitReactMontage || GetWorld()->GetTimeSeconds() < HitReactionImmunityEndTime) return;
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
		HitReactionImmunityEndTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, HitReactionImmunityDuration);
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
	// Keep gravity active so an airborne death can settle on the ground.
	ConsumeMovementInputVector();
	GetCharacterMovement()->ClearAccumulatedForces();
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
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
	const FCollisionShape SpawnShape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	const FVector SpawnUp = -CheckpointGravity.GetSafeNormal(SMALL_NUMBER, FVector(0, 0, -1));
	bool bPlaced = false;
	// Search above an occupied checkpoint without reviving inside another actor.
	for (float Height = 0.0f; Height <= 2000.0f; Height += 50.0f)
	{
		const FVector Candidate = CheckpointTransform.GetLocation() + SpawnUp * Height;
		if (GetWorld()->OverlapBlockingTestByProfile(Candidate, CheckpointTransform.GetRotation(),
			Capsule->GetCollisionProfileName(), SpawnShape, Query)) continue;
		if (TeleportTo(Candidate, CheckpointTransform.Rotator()))
		{
			bPlaced = true;
			break;
		}
	}
	if (!bPlaced) return false;
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	// Keep equipment, core grants and permanent effects on the existing ASC.
	const bool bGravityReversed = AbilitySystemComponent->HasMatchingGameplayTag(
		FGameplayTag::RequestGameplayTag(TEXT("State.Gravity.Reversed")));
	// ToggleGravityState requires a living player; reset movement after restoring gravity.
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
	HeldWeaponChoices.Reset();
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
	MulticastWeaponChargeSound(nullptr, EWeaponSlot::None, 0.0f);
	// 사망 이벤트가 전달되기 전에 운반 물체와 효과 정리
	if (IsValid(CarryComponent))
	{
		CarryComponent->ClearCarryState();
	}

	WeaponChargeState = FPlayerWeaponChargeState();
	bGrappleCancelJumpAvailable = false;

	GrapplingHookComponent->ClearAnchor();

	FinishHitReaction();
	HitReactionImmunityEndTime = 0.0;

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
	StopMeleeAttackFeedback();
	StopWeaponChargeSound();
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
