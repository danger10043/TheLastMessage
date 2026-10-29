#include "Character/Enemy/EnemyBase.h"
#include "Character/Enemy/EnemyAIController.h"
#include "Character/Enemy/EnemyAttributeSet.h"
#include "Character/Enemy/Drop/EnemyHealthPickup.h"
#include "Character/Enemy/EnemyProjectile.h"
#include "Character/Player/PlayerBase.h"
#include "Perception/AIPerceptionComponent.h"
#include "Framework/ActorPoolSubsystem.h"
#include "TimerManager.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimMontage.h"
#include "Materials/MaterialInterface.h"
#include "Components/WidgetComponent.h"
#include "Character/Enemy/UI/EnemyHealthBarWidget.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interface/AttackReceiverInterface.h"

AEnemyBase::AEnemyBase()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UEnemyAttributeSet>(TEXT("AttributeSet"));

	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// 사이드뷰라 몬스터는 Y(깊이) 축으로 움직이지 않는다. 장애물이나 다른 몬스터에 밀려도 Y가 벗어나지 않게 평면에 고정한다.
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->bConstrainToPlane = true;
	MovementComponent->SetPlaneConstraintNormal(FVector(0.f, 1.f, 0.f));

	// 머리 위 체력바: 화면에 항상 정면으로 보이는 스크린 스페이스 위젯. 위치는 BP에서 조정한다.
	HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBarComponent->SetupAttachment(GetCapsuleComponent());
	HealthBarComponent->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
	HealthBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarComponent->SetDrawSize(FVector2D(120.f, 14.f));
	HealthBarComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBarComponent->SetVisibility(false);
}

UAbilitySystemComponent* AEnemyBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AEnemyBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyEnemyData(EnemyData);
}

void AEnemyBase::ApplyEnemyData(const UEnemyDataAsset* Data)
{
	if (!Data)
	{
		return;
	}

	bAlwaysShowHealthBar = Data->bAlwaysShowHealthBar;
	HealthBarVisibleTime = Data->HealthBarVisibleTime;
	HitReactionCooldown = Data->HitReactionCooldown;
	bHitReactionInterruptsAttack = Data->bHitReactionInterruptsAttack;
	HitFlashDuration = Data->HitFlashDuration;
	CorpseLifeSpan = Data->CorpseLifeSpan;
	HomeReanchorHeight = Data->HomeReanchorHeight;
	PatrolRadius = Data->PatrolRadius;
	HealDropChance = Data->HealDropChance;
	HealDropAmount = Data->HealDropAmount;
	HealDropHeight = Data->HealDropHeight;
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	DefaultCapsuleCollision = GetCapsuleComponent()->GetCollisionEnabled();

	InitializeEnemy();

	OnEnemyDamaged.AddUniqueDynamic(this, &AEnemyBase::HandleDamagedEffect);
}

void AEnemyBase::InitializeEnemy()
{
	if (bInitialized)
	{
		return;
	}
	bInitialized = true;

	HomeLocation = GetActorLocation();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	ApplyLevelScaling(false); // MaxHealth, Health(가득), DamageMultiplier
	AttributeSet->InitAttackPower(GetStatConfig().AttackDamage);
	AttributeSet->InitMoveSpeed(GetStatConfig().MoveSpeed);

	GetCharacterMovement()->MaxWalkSpeed = GetStatConfig().MoveSpeed;

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		}
	}

	// 체력바는 항상 표시 모드가 아니면 맞을 때까지 숨겨 둔다.
	if (bAlwaysShowHealthBar)
	{
		ShowHealthBar();
	}
	else
	{
		HideHealthBar();
	}
}

void AEnemyBase::ApplyLevelScaling(bool bKeepHealthRatio)
{
	const FEnemyStatConfig& Stats = GetStatConfig();
	const float DamageScale = EnemyData ? EnemyData->DamageScalePerLevel : 0.2f;
	const float HealthScale = EnemyData ? EnemyData->HealthScalePerLevel : 0.15f;
	const int32 LevelOffset = FMath::Max(EnemyLevel, 1) - 1;

	// 이미 초기화된 상태에서 레벨이 바뀌면 현재 체력 비율을 유지한다.
	const float HealthRatio = (bKeepHealthRatio && AttributeSet->GetMaxHealth() > 0.f) ? AttributeSet->GetHealthPercent() : 1.f;
	const float NewMaxHealth = Stats.MaxHealth * (1.f + HealthScale * LevelOffset);

	AttributeSet->InitMaxHealth(NewMaxHealth);
	AttributeSet->InitHealth(NewMaxHealth * HealthRatio);
	AttributeSet->InitDamageMultiplier(1.f + DamageScale * LevelOffset);

	UpdateHealthBar(true);
}

void AEnemyBase::SetEnemyLevel(int32 NewLevel)
{
	EnemyLevel = FMath::Max(NewLevel, 1);
	if (bInitialized)
	{
		ApplyLevelScaling();
	}
}

float AEnemyBase::GetScaledDamage(float BaseDamage) const
{
	const float Multiplier = AttributeSet ? AttributeSet->GetDamageMultiplier() : 1.f;
	return BaseDamage * (Multiplier > 0.f ? Multiplier : 1.f);
}

void AEnemyBase::ApplyEnemyDamage(AActor* Target, float Damage, FGameplayTag DamageTypeTag)
{
	if (!Target || Damage <= 0.f)
	{
		return;
	}

	// 공격 수신 기믹이면 인터페이스로 공격 전달
	if (IAttackReceiverInterface* Receiver = Cast<IAttackReceiverInterface>(Target))
	{
		// this는 공격한 몬스터 자신
		Receiver->ReceiveAttackDamage(Damage, this);

		// 기믹 처리가 끝났으므로 아래의 일반 피해 처리는 생략
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (DamageEffectClass && TargetASC)
	{
		FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
		Context.AddSourceObject(this);

		// GE 레벨에 몬스터 레벨을 넘겨 커브 테이블 방식도 쓸 수 있게 한다.
		const FGameplayEffectSpecHandle SpecHandle =
			AbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, static_cast<float>(FMath::Max(EnemyLevel, 1)), Context);

		if (SpecHandle.IsValid())
		{
			if (DamageSetByCallerTag.IsValid())
			{
				SpecHandle.Data->SetSetByCallerMagnitude(DamageSetByCallerTag, Damage);
			}

			if (DamageTypeTag.IsValid())
			{
				SpecHandle.Data->AddDynamicAssetTag(DamageTypeTag);
			}

			AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
			return;
		}
	}

	// 예비 경로: GE가 없거나 대상에 ASC가 없으면 엔진 기본 피해를 사용한다.
	UGameplayStatics::ApplyDamage(Target, Damage, GetController(), this, nullptr);
}

void AEnemyBase::SetHomeLocation(const FVector& NewHome)
{
	HomeLocation = NewHome;

	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		AIC->RefreshHomeLocation();
	}
}

void AEnemyBase::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (IsDead() || bInPool)
	{
		return;
	}

	if (FMath::Abs(GetActorLocation().Z - HomeLocation.Z) > HomeReanchorHeight)
	{
		SetHomeLocation(GetActorLocation());
	}
}

float AEnemyBase::ModifyIncomingDamage(float Damage, const FGameplayEffectSpec& Spec) const
{
	if (RangedDamageMultiplier < 1.f)
	{
		static const FName RangedTagName(TEXT("Damage.Category.Player.Ranged"));
		const FGameplayTag RangedTag = FGameplayTag::RequestGameplayTag(RangedTagName, false);
		if (RangedTag.IsValid() && Spec.GetDynamicAssetTags().HasTagExact(RangedTag))
		{
			return Damage * RangedDamageMultiplier;
		}
	}

	return Damage;
}

void AEnemyBase::SetEnemyState(EEnemyState NewState)
{
	if (CurrentState == NewState || IsDead())
	{
		return;
	}

	CurrentState = NewState;
	OnEnemyStateChanged.Broadcast(NewState);
}

void AEnemyBase::BindTargetPlayerEvents(AActor* Target)
{
	APlayerBase* Player = Cast<APlayerBase>(Target);
	if (!Player || BoundPlayer.Get() == Player)
	{
		return;
	}

	if (APlayerBase* OldPlayer = BoundPlayer.Get())
	{
		OldPlayer->OnPlayerDied.RemoveDynamic(this, &AEnemyBase::HandleTargetPlayerDied);
	}

	Player->OnPlayerDied.AddUniqueDynamic(this, &AEnemyBase::HandleTargetPlayerDied);
	BoundPlayer = Player;
}

void AEnemyBase::UnbindTargetPlayerEvents()
{
	if (APlayerBase* Player = BoundPlayer.Get())
	{
		Player->OnPlayerDied.RemoveDynamic(this, &AEnemyBase::HandleTargetPlayerDied);
	}
	BoundPlayer.Reset();

	if (APlayerBase* Player = DeadPlayer.Get())
	{
		Player->OnPlayerRespawned.RemoveDynamic(this, &AEnemyBase::HandleTargetPlayerRespawned);
	}
	DeadPlayer.Reset();
}

void AEnemyBase::HandleTargetPlayerDied(APlayerBase* Player)
{
	if (IsDead() || bInPool || !Player || Player != CurrentTarget)
	{
		return;
	}

	// 부활하면 다시 추격할 수 있도록 부활 이벤트도 받아 둔다.
	Player->OnPlayerRespawned.AddUniqueDynamic(this, &AEnemyBase::HandleTargetPlayerRespawned);
	DeadPlayer = Player;

	// 진행 중인 공격을 취소하고 타겟을 해제한 뒤 정찰로 복귀한다. 죽은 위치를 조사하러 가지는 않는다.
	OnTargetDied();
}

void AEnemyBase::HandleTargetPlayerRespawned(APlayerBase* Player)
{
	if (!Player)
	{
		return;
	}

	Player->OnPlayerRespawned.RemoveDynamic(this, &AEnemyBase::HandleTargetPlayerRespawned);
	DeadPlayer.Reset();

	if (IsDead() || bInPool || CurrentTarget)
	{
		return;
	}

	// 부활한 곳이 이 몬스터의 감지 거리 안이고 아직 시야에 감지된 상태라면(새 감지 이벤트가 오지 않으므로) 바로 다시 추격한다.
	if (FVector::Dist(Player->GetActorLocation(), GetActorLocation()) > GetStatConfig().DetectRange)
	{
		return;
	}

	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		if (UAIPerceptionComponent* Perception = AIC->GetAIPerceptionComponent())
		{
			FActorPerceptionBlueprintInfo Info;
			if (Perception->GetActorsPerception(Player, Info))
			{
				for (const FAIStimulus& Stimulus : Info.LastSensedStimuli)
				{
					if (Stimulus.WasSuccessfullySensed())
					{
						OnPlayerDetected(Player);
						break;
					}
				}
			}
		}
	}
}

void AEnemyBase::SetCurrentTarget(AActor* NewTarget)
{
	CurrentTarget = NewTarget;

	// 타겟이 바뀌거나 사라지면 이전 플레이어의 이벤트를 해제하고, 새 타겟이 플레이어면 연결한다.
	if (!NewTarget || BoundPlayer.Get() != NewTarget)
	{
		if (APlayerBase* OldPlayer = BoundPlayer.Get())
		{
			OldPlayer->OnPlayerDied.RemoveDynamic(this, &AEnemyBase::HandleTargetPlayerDied);
		}
		BoundPlayer.Reset();
	}
	BindTargetPlayerEvents(NewTarget);

	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		AIC->UpdateTargetActor(NewTarget);
	}
}

void AEnemyBase::OnPlayerDetected(AActor* Player)
{
	if (IsDead() || bInPool || !Player)
	{
		return;
	}

	// 이미 죽은 플레이어(시체)는 감지해도 추격하지 않는다.
	if (const APlayerBase* PlayerCharacter = Cast<APlayerBase>(Player))
	{
		if (PlayerCharacter->IsDead())
		{
			return;
		}
	}

	SetCurrentTarget(Player);
	SetEnemyState(EEnemyState::Chase);
}

void AEnemyBase::OnPlayerLost()
{
	if (IsDead())
	{
		return;
	}

	CancelAttack();
	SetCurrentTarget(nullptr);
	SetEnemyState(EEnemyState::Patrol);
}

void AEnemyBase::OnTargetDied()
{
	// TODO: 진행 중인 공격(몽타주·타이머·충전·돌진) 취소는 공격 GA 도입 시 여기서 처리
	OnPlayerLost();
}

float AEnemyBase::GetDistanceToTarget() const
{
	return CurrentTarget ? GetDistanceTo(CurrentTarget) : MAX_FLT;
}

bool AEnemyBase::CanAttackTarget() const
{
	if (!CurrentTarget || IsDead() || bIsAttacking || !GetWorld())
	{
		return false;
	}

	const bool bInRange = GetDistanceToTarget() <= GetStatConfig().AttackRange;
	const bool bCooldownDone = (GetWorld()->GetTimeSeconds() - LastAttackTime) >= GetAttackCooldown();
	return bInRange && bCooldownDone;
}

void AEnemyBase::StartAttackCooldown()
{
	if (GetWorld())
	{
		LastAttackTime = GetWorld()->GetTimeSeconds();
	}
}

void AEnemyBase::StartAttack()
{
	if (IsDead() || bIsAttacking)
	{
		return;
	}

	// 기본 구현: 별도 공격 동작이 없는 몬스터는 쿨다운만 소모한다. 서브클래스가 오버라이드한다.
	StartAttackCooldown();
}

void AEnemyBase::CancelAttack()
{
	if (bIsAttacking && AttackMontage)
	{
		StopAnimMontage(AttackMontage);
	}

	bIsAttacking = false;
}

void AEnemyBase::SpawnEffectAtLocation(UNiagaraSystem* System, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	if (System && GetWorld())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, Location, Rotation, Scale);
	}
}

void AEnemyBase::SpawnEffectAtSocket(UNiagaraSystem* System, FName SocketName,
	const FVector& FallbackLocalOffset, const FRotator& FallbackRotation)
{
	if (!System)
	{
		return;
	}

	if (!SocketName.IsNone() && GetMesh() && GetMesh()->DoesSocketExist(SocketName))
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(System, GetMesh(), SocketName, FVector::ZeroVector,
			FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
		return;
	}

	SpawnEffectAtLocation(System, GetActorTransform().TransformPosition(FallbackLocalOffset), FallbackRotation);
}

FVector AEnemyBase::GetMuzzleLocation(FName SocketName, const FVector& LocalOffset) const
{
	if (!SocketName.IsNone() && GetMesh() && GetMesh()->DoesSocketExist(SocketName))
	{
		return GetMesh()->GetSocketLocation(SocketName);
	}

	return GetActorTransform().TransformPosition(LocalOffset);
}

void AEnemyBase::HandleDamagedEffect(float Damage)
{
	SpawnEffectAtLocation(HitEffect, GetActorLocation());
	StartHitFlash();
	PlayHitReaction();

	UpdateHealthBar(false);
	ShowHealthBar();
}

UEnemyHealthBarWidget* AEnemyBase::GetHealthBarWidget() const
{
	if (!HealthBarComponent)
	{
		return nullptr;
	}

	if (!HealthBarComponent->GetUserWidgetObject())
	{
		HealthBarComponent->InitWidget(); // 위젯이 아직 만들어지지 않았으면 만든다.
	}

	return Cast<UEnemyHealthBarWidget>(HealthBarComponent->GetUserWidgetObject());
}

void AEnemyBase::UpdateHealthBar(bool bInstant)
{
	if (!AttributeSet)
	{
		return;
	}

	if (UEnemyHealthBarWidget* Widget = GetHealthBarWidget())
	{
		Widget->SetHealthPercent(AttributeSet->GetHealthPercent(), bInstant);
	}
}

void AEnemyBase::ShowHealthBar()
{
	if (!HealthBarComponent || !GetWorld())
	{
		return;
	}

	HealthBarComponent->SetVisibility(true);

	// 맞을 때마다 타이머를 다시 시작해 계속 맞는 동안은 유지한다.
	if (!bAlwaysShowHealthBar)
	{
		GetWorldTimerManager().SetTimer(HealthBarTimerHandle, this, &AEnemyBase::HideHealthBar, HealthBarVisibleTime, false);
	}
}

void AEnemyBase::HideHealthBar()
{
	if (HealthBarComponent)
	{
		HealthBarComponent->SetVisibility(false);
	}
}

void AEnemyBase::StartHitFlash()
{
	if (!HitFlashMaterial || !GetMesh() || !GetWorld() || bInPool)
	{
		return;
	}

	GetMesh()->SetOverlayMaterial(HitFlashMaterial);

	// 연속 피격 때는 타이머를 다시 시작해 번쩍임을 이어 간다.
	GetWorldTimerManager().SetTimer(HitFlashTimerHandle, this, &AEnemyBase::EndHitFlash, HitFlashDuration, false);
}

void AEnemyBase::EndHitFlash()
{
	if (GetMesh())
	{
		GetMesh()->SetOverlayMaterial(nullptr);
	}
}

void AEnemyBase::PlayHitReaction()
{
	if (!HitMontage || !GetWorld() || IsDead() || bInPool)
	{
		return;
	}

	// 치명타는 사망 몽타주가 재생되므로 피격 몽타주를 재생하지 않는다.
	if (AttributeSet && AttributeSet->GetHealth() <= 0.f)
	{
		return;
	}

	if (bIsAttacking && !bHitReactionInterruptsAttack)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastHitReactionTime < HitReactionCooldown)
	{
		return;
	}
	LastHitReactionTime = Now;

	if (bIsAttacking)
	{
		CancelAttack(); // 진행 중인 공격(몽타주, 타이머, 이펙트)을 취소한다.
	}

	PlayAnimMontage(HitMontage);
}

void AEnemyBase::PlayAttackMontage()
{
	if (AttackMontage)
	{
		PlayAnimMontage(AttackMontage);
	}
}

void AEnemyBase::FinishAttack()
{
	bIsAttacking = false;
	StartAttackCooldown();
}

void AEnemyBase::FaceTarget()
{
	if (!CurrentTarget)
	{
		return;
	}

	FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.f;
	if (!ToTarget.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.f, ToTarget.Rotation().Yaw, 0.f));
	}
}

FVector AEnemyBase::ComputeAimDirection(const FVector& MuzzleLocation) const
{
	if (!CurrentTarget)
	{
		return FVector::ZeroVector;
	}

	// 총구 소켓은 애니메이션에 따라 Y(깊이)가 몬스터 평면에서 벗어난다. 투사체는 몬스터가 있는 평면에서 쏘므로 같은 기준으로 계산한다.
	FVector From = MuzzleLocation;
	From.Y = GetActorLocation().Y;
	return (CurrentTarget->GetActorLocation() - From).GetSafeNormal();
}

AEnemyProjectile* AEnemyBase::SpawnEnemyProjectile(const FEnemyProjectileConfig& Config, const FVector& MuzzleLocation, const FVector& FixedDirection)
{
	UClass* ProjectileClass = Config.ProjectileClass.Get();
	if (!ProjectileClass || !GetWorld())
	{
		return nullptr;
	}

	// 고정 방향이 있으면 타겟 없이도 그 방향으로 쏜다. 없으면 지금 타겟 방향을 쓴다.
	const FVector Direction = FixedDirection.IsNearlyZero() ? ComputeAimDirection(MuzzleLocation) : FixedDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return nullptr;
	}

	// 투사체는 몬스터가 있는 Y 평면에서 발사한다. 소켓 위치의 Y가 애니메이션 때문에 벗어나 있으면 플레이어와 평면이 어긋나 지나쳐 버린다.
	FVector SpawnLocation = MuzzleLocation;
	SpawnLocation.Y = GetActorLocation().Y;

	// 투사체는 오브젝트 풀에서 꺼내 쓴다(풀이 없으면 새로 만든다).
	const FTransform SpawnTransform(Direction.Rotation(), SpawnLocation);
	AEnemyProjectile* Projectile = nullptr;

	if (UActorPoolSubsystem* Pool = GetWorld()->GetSubsystem<UActorPoolSubsystem>())
	{
		Projectile = Cast<AEnemyProjectile>(Pool->AcquireActor(ProjectileClass, SpawnTransform, this, this));
	}
	else
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Projectile = GetWorld()->SpawnActor<AEnemyProjectile>(ProjectileClass, SpawnTransform, Params);
	}

	if (!Projectile)
	{
		return nullptr;
	}

	Projectile->InitializeProjectile(this, GetScaledDamage(GetStatConfig().AttackDamage));
	Projectile->SetProjectileSpeed(Config.Speed);
	Projectile->SetProjectileLifetime(Config.Lifetime);
	Projectile->LaunchProjectile(Direction);
	return Projectile;
}

void AEnemyBase::TryDropHeal()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority() || !bCanDropOnDeath || !HealDropClass || HealDropChance <= 0.f)
	{
		return;
	}

	if (FMath::FRand() > HealDropChance)
	{
		return;
	}

	// 죽은 자리 아래의 바닥을 찾아 그 위에 놓는다. 바닥을 못 찾으면(벽·천장에서 죽음 등) 죽은 자리에 그대로 놓는다.
	FVector Location = GetActorLocation();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyHealDrop), false, this);
	FHitResult Hit;
	if (World->LineTraceSingleByObjectType(Hit, Location, Location - FVector(0.f, 0.f, 500.f),
		FCollisionObjectQueryParams(ECC_WorldStatic), QueryParams))
	{
		Location.Z = Hit.ImpactPoint.Z + HealDropHeight;
	}

	const FTransform SpawnTransform(FRotator::ZeroRotator, Location);
	AEnemyHealthPickup* Pickup = World->SpawnActorDeferred<AEnemyHealthPickup>(
		HealDropClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		return;
	}

	Pickup->InitPickup(HealDropAmount);
	Pickup->FinishSpawning(SpawnTransform);
}

void AEnemyBase::HandleDeath()
{
	if (IsDead())
	{
		return;
	}

	SetEnemyState(EEnemyState::Dead);
	CancelAttack();
	SetCurrentTarget(nullptr);
	TryDropHeal();
	UnbindTargetPlayerEvents(); // 죽은 몬스터가 플레이어 이벤트를 받지 않게 한다.
	GetWorldTimerManager().ClearTimer(HealthBarTimerHandle);
	HideHealthBar();

	// 사망 순서: AI 중지 → 어빌리티 취소 → 콜리전 비활성화 → OnEnemyDied
	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		AIC->StopEnemyAI();
	}

	AbilitySystemComponent->CancelAllAbilities();

	if (GetCharacterMovement()->IsFalling())
	{
		// 공중에서 죽으면 수평 이동만 멈추고 그대로 떨어지게 한다(허공에 멈춰 있지 않도록).
		FVector FallVelocity = GetCharacterMovement()->Velocity;
		FallVelocity.X = 0.f;
		FallVelocity.Y = 0.f;
		GetCharacterMovement()->Velocity = FallVelocity;
	}
	else
	{
		GetCharacterMovement()->DisableMovement();
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (DeathMontage)
	{
		PlayAnimMontage(DeathMontage);
	}

	SpawnEffectAtLocation(DeathEffect, GetActorLocation());

	OnEnemyDied.Broadcast(this);

	// 사망 연출이 끝난 뒤 시체를 풀로 돌려보낸다.
	if (CorpseLifeSpan > 0.f)
	{
		GetWorldTimerManager().SetTimer(CorpseTimerHandle, this, &AEnemyBase::ReturnToPool, CorpseLifeSpan, false);
	}
}

void AEnemyBase::ReturnToPool()
{
	if (bInPool)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UActorPoolSubsystem* Pool = World->GetSubsystem<UActorPoolSubsystem>())
		{
			Pool->ReleaseActor(this);
			return;
		}
	}

	Destroy();
}

void AEnemyBase::OnReturnedToPool()
{
	bInPool = true;

	GetWorldTimerManager().ClearAllTimersForObject(this);
	EndHitFlash();
	HideHealthBar();
	CancelAttack();

	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		AIC->StopEnemyAI();
	}

	StopAnimMontage();
	AbilitySystemComponent->CancelAllAbilities();

	CurrentTarget = nullptr;
	UnbindTargetPlayerEvents();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// 스폰 포인트 등이 참조를 정리하도록 알린 뒤, 이 몬스터에 걸린 외부 바인딩을 모두 끊는다.
	OnEnemyReleased.Broadcast(this);
	OnEnemyDied.Clear();
	OnEnemyReleased.Clear();
}

void AEnemyBase::OnAcquiredFromPool()
{
	bInPool = false;
	ResetEnemy();
}

void AEnemyBase::ResetEnemy()
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	EndHitFlash();
	CancelAttack();
	StopAnimMontage();

	// 상태를 처음으로 되돌린다.
	bCanDropOnDeath = true;
	CurrentTarget = nullptr;
	UnbindTargetPlayerEvents();
	CurrentState = EEnemyState::Patrol;
	LastAttackTime = -1000000.0;
	LastHitReactionTime = -1000000.0;

	// 사망 처리에서 꺼 둔 충돌과 이동을 복구한다.
	GetCapsuleComponent()->SetCollisionEnabled(DefaultCapsuleCollision);
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	GetCharacterMovement()->SetDefaultMovementMode();

	// 능력과 능력치를 다시 설정한다(능력이 중복 부여되지 않도록 먼저 비운다).
	AbilitySystemComponent->ClearAllAbilities();
	bInitialized = false;
	InitializeEnemy();

	// AI를 처음부터 다시 시작한다.
	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		AIC->RestartEnemyAI();
	}
}

void AEnemyBase::Destroyed()
{
	UnbindTargetPlayerEvents();

	// 몬스터가 제거될 때 AI 컨트롤러가 남지 않도록 함께 제거한다.
	if (AController* OwnerController = GetController())
	{
		OwnerController->Destroy();
	}

	Super::Destroyed();
}
