#include "Character/Player/PlayerBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "GAS/CombatAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gimmick/Component/InteractionComponent.h"
#include "Core/CoreComponent.h"
#include "Components/CapsuleComponent.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Weapon/MeleeWeaponBase.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PlayerDead, "State.Dead");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PlayerDoubleJump, "Unlock.DoubleJump");

APlayerBase::APlayerBase()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	CombatAttributes = CreateDefaultSubobject<UCombatAttributeSet>(TEXT("CombatAttributes"));

	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));

	CoreComponent = CreateDefaultSubobject<UCoreComponent>(TEXT("CoreComponent"));

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);

	JumpMaxCount = 1;
}

UAbilitySystemComponent* APlayerBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
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
	CoreComponent->Initialize(AbilitySystemComponent);
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (AbilityClass && !AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		}
	}
}

bool APlayerBase::IsDead() const
{
	return AbilitySystemComponent->HasMatchingGameplayTag(TAG_PlayerDead);
}

void APlayerBase::HandleDeath()
{
	if (!HasAuthority() || bDeathHandled) return;
	bDeathHandled = true;
	AbilitySystemComponent->AddLooseGameplayTag(TAG_PlayerDead, 1, EGameplayTagReplicationState::TagAndCountToAll);
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
	if (!IsDead()) Super::DoMove(Forward);
}

void APlayerBase::DoDrop(float Value)
{
	if (!IsDead()) Super::DoDrop(Value);
}

void APlayerBase::DoJumpStart()
{
	if (!IsDead()) Super::DoJumpStart();
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
