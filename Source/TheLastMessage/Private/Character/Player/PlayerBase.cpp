#include "Character/Player/PlayerBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "GAS/CombatAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gimmick/Component/InteractionComponent.h"
#include "Core/CoreComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Weapon/MeleeWeaponBase.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"
#include "EnhancedInputComponent.h"
#include "GAS/Abilities/GA_AttackPrimary.h"
#include "GAS/Abilities/GA_AttackSecondary.h"
#include "InputCoreTypes.h"
#include "GAS/WeaponGameplayTags.h"
#include "Net/UnrealNetwork.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PlayerDoubleJump, "Unlock.DoubleJump");

APlayerBase::APlayerBase()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	CombatAttributes = CreateDefaultSubobject<UCombatAttributeSet>(TEXT("CombatAttributes"));

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
}

void APlayerBase::BeginPlay()
{
	Super::BeginPlay();
	InitializePlayer();
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
	// 무기 객체는 플레이어가 소유하며, 공격 수치는 DataAsset을 참조한다.
	if (!PrimaryWeapon && IsValid(InitialPrimaryWeaponData))
	{
		if (InitialPrimaryWeaponData->WeaponType == EWeaponType::LightMelee ||
			InitialPrimaryWeaponData->WeaponType == EWeaponType::HeavyMelee)
		{
			UMeleeWeaponBase* Weapon = NewObject<UMeleeWeaponBase>(this);
			if (Weapon->Initialize(InitialPrimaryWeaponData))
			{
				PrimaryWeapon = Weapon;
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: InitialPrimaryWeaponData requires a melee weapon; ranged equipment is not implemented yet."), *GetName());
		}
	}
	if (!SecondaryWeapon && IsValid(InitialSecondaryWeaponData))
	{
		if (InitialSecondaryWeaponData->WeaponType == EWeaponType::LightMelee ||
			InitialSecondaryWeaponData->WeaponType == EWeaponType::HeavyMelee)
		{
			UMeleeWeaponBase* Weapon = NewObject<UMeleeWeaponBase>(this);
			if (Weapon->Initialize(InitialSecondaryWeaponData)) SecondaryWeapon = Weapon;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: InitialSecondaryWeaponData requires a melee weapon; ranged equipment is not implemented yet."), *GetName());
		}
	}
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
	if (!HasAuthority())
	{
		ServerAbilityInput(InputTag, 1);
		return;
	}
	if (IsDead()) return;
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
	if (!HasAuthority())
	{
		ServerAbilityInput(InputTag, 0);
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
}

UWeaponBase* APlayerBase::GetWeaponInSlot(EWeaponSlot Slot) const
{
	if (Slot == EWeaponSlot::Primary) return PrimaryWeapon;
	if (Slot == EWeaponSlot::Secondary) return SecondaryWeapon;
	return nullptr;
}

void APlayerBase::AbilityInputCanceled(FGameplayTag InputTag)
{
	if (!HasAuthority())
	{
		ServerAbilityInput(InputTag, 2);
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

void APlayerBase::ServerAbilityInput_Implementation(FGameplayTag InputTag, uint8 InputState)
{
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
	return AbilitySystemComponent->HasMatchingGameplayTag(WeaponGameplayTags::State_Dead);
}

void APlayerBase::HandleDeath()
{
	if (!HasAuthority() || bDeathHandled) return;
	bDeathHandled = true;
	AbilitySystemComponent->AddLooseGameplayTag(WeaponGameplayTags::State_Dead, 1, EGameplayTagReplicationState::TagAndCountToAll);
	CleanupPlayerActions();
	OnPlayerDied.Broadcast(this);
}

void APlayerBase::CleanupPlayerActions()
{
	AbilitySystemComponent->CancelAllAbilities();
	GetWorldTimerManager().ClearTimer(EnergyRechargeTimerHandle);
	GetWorldTimerManager().ClearTimer(WallJumpTimer);
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

void APlayerBase::DoMove(float Forward)
{
	if (IsDead() || AbilitySystemComponent->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking))
	{
		ActionValueY = 0.0f;
		return;
	}
	Super::DoMove(Forward);
}

void APlayerBase::DoDrop(float Value)
{
	if (IsDead() || AbilitySystemComponent->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking))
	{
		DropValue = 0.0f;
		return;
	}
	Super::DoDrop(Value);
}

void APlayerBase::DoJumpStart()
{
	if (!IsDead() && !AbilitySystemComponent->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking))
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
	if (IsDead() || !IsValid(InteractionComponent))
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
	if (DoubleJumpTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(TAG_PlayerDoubleJump).Remove(DoubleJumpTagChangedHandle);
		DoubleJumpTagChangedHandle.Reset();
	}
	GetWorldTimerManager().ClearTimer(EnergyRechargeTimerHandle);
	AbilitySystemComponent->CancelAllAbilities();
	Super::EndPlay(EndPlayReason);
}
