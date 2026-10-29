#include "Core/CoreAssistAfterimage.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Character/CombatDamageLibrary.h"
#include "Character/Player/PlayerBase.h"
#include "Character/Enemy/EnemyBase.h"
#include "Character/Boss/BossMonster.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CoreComponent.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Engine/World.h"
#include "GAS/WeaponGameplayTags.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/StrongObjectPtr.h"

ACoreAssistAfterimage::ACoreAssistAfterimage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	SetActorEnableCollision(false);
	SetCanBeDamaged(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	CharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
	CharacterMesh->SetupAttachment(GetRootComponent());
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CharacterMesh->SetGenerateOverlapEvents(false);
	CharacterMesh->SetCastShadow(false);
	CharacterMesh->SetHiddenInGame(true);
	CharacterMesh->SetDisablePostProcessBlueprint(true);
	CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	CharacterMesh->bEnableUpdateRateOptimizations = false;
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(CharacterMesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->SetCastShadow(false);
	WeaponMesh->SetHiddenInGame(true);
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
}

UAbilitySystemComponent* ACoreAssistAfterimage::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

bool ACoreAssistAfterimage::Initialize(APlayerBase* Player, const UMeleeAttackEventData* Attack, float Damage, FName CoreId)
{
	UCoreComponent* Core = IsValid(Player) ? Player->GetCoreComponent() : nullptr;
	if (!HasAuthority() || AttackRecord || !IsValid(Player) || Player->IsDead() || !IsValid(Core) || !Core->HasCore(CoreId)
		|| !IsValid(Attack) || !IsValid(Attack->WeaponData) || !IsValid(Attack->SkeletalMesh) || !IsValid(Attack->AttackMontage)
		|| !IsValid(Attack->WeaponData->WeaponMesh) || !AfterimageMaterial || !FMath::IsFinite(Damage) || Damage <= 0.0f
		|| Attack->ActorTransform.ContainsNaN() || Attack->MeshTransform.ContainsNaN()
		|| (Attack->WeaponData->WeaponType != EWeaponType::LightMelee && Attack->WeaponData->WeaponType != EWeaponType::HeavyMelee)) return false;
	if (!Attack->AttackSection.IsNone() && Attack->AttackMontage->GetSectionIndex(Attack->AttackSection) == INDEX_NONE) return false;

	FCoreAfterimageAppearance Visual;
	Visual.CharacterMesh = Attack->SkeletalMesh;
	Visual.WeaponMesh = Attack->WeaponData->WeaponMesh;
	Visual.Montage = Attack->AttackMontage;
	Visual.Section = Attack->AttackSection;
	Visual.CharacterTransform = Player->GetMesh()->GetComponentTransform().GetRelativeTransform(GetActorTransform());
	Visual.WeaponSocket = Attack->WeaponData->AttachSocketName;
	Visual.WeaponTransform = Attack->WeaponData->AttachOffset;
	// 실제 장착된 메시에서 소켓과 보정을 가져와 플레이어의 대체 소켓 설정도 반영한다.
	TInlineComponentArray<UStaticMeshComponent*> Attachments(Player);
	for (const UStaticMeshComponent* Attachment : Attachments)
	{
		if (Attachment->GetAttachParent() == Player->GetMesh() && Attachment->GetStaticMesh() == Visual.WeaponMesh
			&& Attachment->IsVisible() && !Attachment->bHiddenInGame)
		{
			Visual.WeaponSocket = Attachment->GetAttachSocketName();
			Visual.WeaponTransform = Attachment->GetRelativeTransform();
			break;
		}
	}
	if (Visual.WeaponSocket.IsNone() || !Player->GetMesh()->DoesSocketExist(Visual.WeaponSocket)) return false;

	Appearance = Visual;
	AttackRecord = const_cast<UMeleeAttackEventData*>(Attack);
	StrikeDamage = Damage;
	SourcePlayer = Player;
	SourceCore = Core;
	SourceCoreId = CoreId;
	Player->OnPlayerDied.AddDynamic(this, &ACoreAssistAfterimage::HandlePlayerDied);
	Player->OnDestroyed.AddDynamic(this, &ACoreAssistAfterimage::HandleSourceDestroyed);
	Core->OnCoreChanged.AddDynamic(this, &ACoreAssistAfterimage::HandleCoreChanged);
	return true;
}

void ACoreAssistAfterimage::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	if (HasAuthority())
	{
		MeleeHitHandle = AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(WeaponGameplayTags::Event_MeleeHit)
			.AddUObject(this, &ACoreAssistAfterimage::HandleMeleeHit);
	}
	StartPresentation();
}

void ACoreAssistAfterimage::OnRep_Appearance()
{
	StartPresentation();
}

void ACoreAssistAfterimage::StartPresentation()
{
	if (!HasActorBegunPlay() || bPresentationStarted || !Appearance.CharacterMesh || !Appearance.WeaponMesh || !Appearance.Montage) return;
	if (!AfterimageMaterial || !FMath::IsFinite(PlaybackRate) || PlaybackRate <= 0.0f
		|| !FMath::IsFinite(Appearance.Montage->RateScale) || Appearance.Montage->RateScale <= 0.0f)
	{
		Destroy();
		return;
	}
	CharacterMesh->SetSkeletalMeshAsset(Appearance.CharacterMesh);
	CharacterMesh->SetRelativeTransform(Appearance.CharacterTransform);
	if (!CharacterMesh->DoesSocketExist(Appearance.WeaponSocket))
	{
		Destroy();
		return;
	}
	WeaponMesh->SetStaticMesh(Appearance.WeaponMesh);
	WeaponMesh->AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Appearance.WeaponSocket);
	WeaponMesh->SetRelativeTransform(Appearance.WeaponTransform);
	VisualMaterial = UMaterialInstanceDynamic::Create(AfterimageMaterial, this);
	if (!VisualMaterial)
	{
		Destroy();
		return;
	}
	VisualMaterial->SetVectorParameterValue(TEXT("Tint"), Tint);
	VisualMaterial->SetScalarParameterValue(TEXT("Brightness"), FMath::Max(0.0f, Brightness));
	VisualMaterial->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(Opacity, 0.0f, 1.0f));
	for (int32 Index = 0; Index < CharacterMesh->GetNumMaterials(); ++Index) CharacterMesh->SetMaterial(Index, VisualMaterial);
	for (int32 Index = 0; Index < WeaponMesh->GetNumMaterials(); ++Index) WeaponMesh->SetMaterial(Index, VisualMaterial);

	CharacterMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	UAnimSingleNodeInstance* Animation = CharacterMesh->GetSingleNodeInstance();
	if (!Animation)
	{
		Destroy();
		return;
	}
	// 캐릭터용 이동·상태 그래프 없이 공격 몽타주의 슬롯만 재생한다.
	Animation->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	Animation->SetAnimationAsset(Appearance.Montage, false, PlaybackRate);
	if (!Appearance.Section.IsNone())
	{
		Animation->RestartMontage(Appearance.Montage, Appearance.Section);
		Animation->Montage_SetNextSection(Appearance.Section, NAME_None, Appearance.Montage);
	}
	if (!Animation->Montage_IsActive(Appearance.Montage))
	{
		Destroy();
		return;
	}
	FOnMontageEnded EndDelegate = FOnMontageEnded::CreateUObject(this, &ACoreAssistAfterimage::HandleMontageEnded);
	Animation->Montage_SetEndDelegate(EndDelegate, Appearance.Montage);
	const int32 SectionIndex = Appearance.Montage->GetSectionIndex(Appearance.Section);
	const float Length = SectionIndex != INDEX_NONE ? Appearance.Montage->GetSectionLength(SectionIndex) : Appearance.Montage->GetPlayLength();
	const float Duration = Length / (PlaybackRate * Appearance.Montage->RateScale);
	if (!FMath::IsFinite(Duration) || Duration <= 0.0f)
	{
		Destroy();
		return;
	}
	PresentationEndTime = GetWorld()->GetTimeSeconds() + Duration;
	bPresentationStarted = true;
	// 첫 화면부터 선택한 공격 자세를 표시한다.
	CharacterMesh->TickAnimation(0.0f, false);
	CharacterMesh->RefreshBoneTransforms();
	CharacterMesh->SetHiddenInGame(false);
	WeaponMesh->SetHiddenInGame(false);
	SetActorTickEnabled(true);
	// 종료 알림과 별도로 수명 상한을 두어 잔상이 남지 않도록 한다.
	SetLifeSpan(Duration + Appearance.Montage->BlendOut.GetBlendTime() + 0.2f);
}

void ACoreAssistAfterimage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPresentationStarted || !VisualMaterial || FadeOutDuration <= 0.0f) return;
	const float Remaining = PresentationEndTime - GetWorld()->GetTimeSeconds();
	if (Remaining < FadeOutDuration)
	{
		VisualMaterial->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(Opacity, 0.0f, 1.0f)
			* FMath::Clamp(Remaining / FadeOutDuration, 0.0f, 1.0f));
	}
}

void ACoreAssistAfterimage::HandleMeleeHit(const FGameplayEventData* Payload)
{
	APlayerBase* Player = SourcePlayer.Get();
	UCoreComponent* Core = SourceCore.Get();
	if (!HasAuthority() || IsActorBeingDestroyed() || !bPresentationStarted || !IsValid(Player) || Player->IsDead()
		|| !IsValid(Core) || !Core->HasCore(SourceCoreId) || !IsValid(AttackRecord) || !AttackRecord->DamageEffectClass
		|| !Payload || Payload->EventTag != WeaponGameplayTags::Event_MeleeHit
		|| Payload->Instigator != this || Payload->OptionalObject != Appearance.Montage
		|| !FMath::IsFinite(AttackRecord->AttackRange) || AttackRecord->AttackRange <= 0.0f
		|| !FMath::IsFinite(AttackRecord->HitRadius) || AttackRecord->HitRadius <= 0.0f) return;

	const FVector Direction = FVector::VectorPlaneProject(GetActorForwardVector(), FVector::YAxisVector).GetSafeNormal();
	if (Direction.IsNearlyZero()) return;
	const FVector Start = GetActorLocation();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CoreAssistStrike), false, this);
	Query.AddIgnoredActor(Player);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, Start, Start + Direction * AttackRecord->AttackRange, FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects), FCollisionShape::MakeSphere(AttackRecord->HitRadius), Query);
	// 피해 알림 중 초기화·사망으로 잔상이 제거되어도 이번 호출의 공격 정보는 유지한다.
	TStrongObjectPtr<UMeleeAttackEventData> Record(AttackRecord);
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		const TWeakObjectPtr<AActor> Key(Target);
		if (!IsValid(Target) || HitActors.Contains(Key)) continue;
		const AEnemyBase* Enemy = Cast<AEnemyBase>(Target);
		ABossMonster* Boss = Cast<ABossMonster>(Target);
		if ((!Enemy && !Boss) || (Enemy && Enemy->IsDead()) || (Boss && Boss->GetBossState() == EBossState::Dead)) continue;
		FCollisionQueryParams WallQuery(Query);
		WallQuery.AddIgnoredActor(Target);
		FHitResult WallHit;
		if (GetWorld()->LineTraceSingleByChannel(WallHit, Start, Target->GetActorLocation(), ECC_Visibility, WallQuery)) continue;

		FVector Contact = Hit.ImpactPoint;
		FVector Normal = Hit.ImpactNormal;
		if (Hit.bStartPenetrating)
		{
			Contact = Target->GetActorLocation();
			if (UPrimitiveComponent* Component = Hit.GetComponent())
			{
				FVector Closest;
				if (Component->GetClosestPointOnCollision(Start, Closest) > 0.0f) Contact = Closest;
			}
			Normal = (Start - Contact).GetSafeNormal();
		}
		if (Normal.IsNearlyZero()) Normal = -Direction;
		// 이벤트가 같은 타격 알림을 재호출해도 중복 피해가 발생하지 않도록 먼저 기록한다.
		HitActors.Add(Key);
		const bool bApplied = UCombatDamageLibrary::ApplyFinalDamageToTarget(Player, Target, Record->DamageEffectClass,
			StrikeDamage, WeaponGameplayTags::Damage_PlayerMelee, Record->FeedbackStrength, CombatGameplayTags::Source_AssistStrike);
		if (IsActorBeingDestroyed() || !IsValid(Player) || Player->IsDead() || !SourceCore.IsValid() || !Core->HasCore(SourceCoreId)) break;
		if (bApplied) Player->MulticastWeaponImpact(Record->WeaponData, Contact, Normal, nullptr, nullptr);
		else HitActors.Remove(Key);
		if (IsActorBeingDestroyed() || !IsValid(Player) || Player->IsDead()) break;
	}
}

void ACoreAssistAfterimage::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == Appearance.Montage) Destroy();
}

void ACoreAssistAfterimage::HandlePlayerDied(APlayerBase* Player)
{
	Destroy();
}

void ACoreAssistAfterimage::HandleSourceDestroyed(AActor* Actor)
{
	Destroy();
}

void ACoreAssistAfterimage::HandleCoreChanged(FName CoreId, int32 OldLevel, int32 NewLevel)
{
	if (CoreId == SourceCoreId && NewLevel == 0) Destroy();
}

void ACoreAssistAfterimage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MeleeHitHandle.IsValid())
	{
		if (auto* Events = AbilitySystemComponent->GenericGameplayEventCallbacks.Find(WeaponGameplayTags::Event_MeleeHit)) Events->Remove(MeleeHitHandle);
		MeleeHitHandle.Reset();
	}
	if (APlayerBase* Player = SourcePlayer.Get())
	{
		Player->OnPlayerDied.RemoveDynamic(this, &ACoreAssistAfterimage::HandlePlayerDied);
		Player->OnDestroyed.RemoveDynamic(this, &ACoreAssistAfterimage::HandleSourceDestroyed);
	}
	if (UCoreComponent* Core = SourceCore.Get()) Core->OnCoreChanged.RemoveDynamic(this, &ACoreAssistAfterimage::HandleCoreChanged);
	if (UAnimInstance* Animation = CharacterMesh->GetAnimInstance())
	{
		FOnMontageEnded Empty;
		Animation->Montage_SetEndDelegate(Empty, Appearance.Montage);
	}
	AttackRecord = nullptr;
	HitActors.Reset();
	VisualMaterial = nullptr;
	Super::EndPlay(EndPlayReason);
}

void ACoreAssistAfterimage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACoreAssistAfterimage, Appearance, COND_InitialOnly);
}
