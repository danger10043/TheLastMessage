#include "Core/CoreAssistAfterimage.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Character/CombatDamageLibrary.h"
#include "Character/Player/PlayerBase.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CoreComponent.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

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
	VisualMaterial = nullptr;
	Super::EndPlay(EndPlayReason);
}

void ACoreAssistAfterimage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACoreAssistAfterimage, Appearance, COND_InitialOnly);
}
