#include "Character/Boss/BossActivationCore.h"
#include "Character/Boss/BossCorePuzzleComponent.h"
#include "Character/Boss/BossCircularMissile.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"

ABossActivationCore::ABossActivationCore()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    CollisionComponent = CreateDefaultSubobject<USphereComponent>(
        TEXT("CollisionComponent"));
    SetRootComponent(CollisionComponent);
    CollisionComponent->InitSphereRadius(80.f);
    CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
    CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CollisionComponent->SetGenerateOverlapEvents(false);
    CollisionComponent->SetCanEverAffectNavigation(false);

    CoreMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CoreMesh"));
    CoreMesh->SetupAttachment(CollisionComponent);
    CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CoreMesh->SetGenerateOverlapEvents(false);
    CoreMesh->SetCanEverAffectNavigation(false);
    CoreMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    ActiveVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ActiveVFX"));
    ActiveVFX->SetupAttachment(CoreMesh);
    ActiveVFX->SetAutoActivate(false);
    ActiveVFX->SetAutoDestroy(false);

    SetActorHiddenInGame(true);
}

void ABossActivationCore::BeginPlay()
{
    Super::BeginPlay();

    for (int32 Index = 0; Index < CoreMesh->GetNumMaterials(); ++Index)
    {
        if (UMaterialInstanceDynamic* MID =
            CoreMesh->CreateDynamicMaterialInstance(Index))
        {
            DynamicMaterials.Add(MID);
        }
    }

    ResetCore();
}

bool ABossActivationCore::CanActivateFor(
    const UBossCorePuzzleComponent* InPuzzleComponent) const
{
    const auto IsConfigured = [](const UAnimMontage* Montage)
        {
            return IsValid(Montage)
                && Montage->CompositeSections.Num() == 1;
        };

    return IsValid(InPuzzleComponent)
        && (!IsValid(PuzzleComponent.Get())
            || PuzzleComponent.Get() == InPuzzleComponent)
        && IsValid(CoreMesh->GetSkeletalMeshAsset())
        && IsValid(CoreMesh->GetAnimInstance())
        && IsConfigured(SpawnMontage.Get())
        && IsConfigured(RetractMontage.Get())
        && IsConfigured(BreakMontage.Get());
}

bool ABossActivationCore::InitializeCore(
    int32 InCoreIndex, UBossCorePuzzleComponent* InPuzzleComponent)
{
    if (!HasAuthority() || !CanActivateFor(InPuzzleComponent))
    {
        return false;
    }

    ResetCore();
    CoreIndex = InCoreIndex;
    PuzzleComponent = InPuzzleComponent;
    CoreState = ECoreState::Spawning;
    bIsActive = true;

    SetActorHiddenInGame(false);
    CoreMesh->SetComponentTickEnabled(true);
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    if (!PlayCoreMontage(SpawnMontage.Get()))
    {
        ResetCore();
        return false;
    }

    if (GetNetMode() != NM_DedicatedServer && ActiveVFX->GetAsset())
    {
        ActiveVFX->Activate(true);
    }


    SetActorTickEnabled(true);
    return true;
}

bool ABossActivationCore::PlayCoreMontage(UAnimMontage* Montage)
{
    CoreMesh->bPauseAnims = false;
    MontageInstanceID = INDEX_NONE;

    UAnimInstance* Anim = CoreMesh->GetAnimInstance();
    if (!Anim || !Montage || Anim->Montage_Play(Montage) <= 0.f)
    {
        UE_LOG(LogTemp, Error,
            TEXT("%s: core montage failed to play."), *GetName());
        return false;
    }

    FAnimMontageInstance* Instance =
        Anim->GetActiveInstanceForMontage(Montage);
    if (!Instance)
    {
        Anim->Montage_Stop(0.f, Montage);
        return false;
    }

    // 에셋은 변경하지 않고 이번 재생의 마지막 포즈만 유지한다.
    Instance->bEnableAutoBlendOut = false;
    MontageInstanceID = Instance->GetInstanceID();
    Anim->Montage_SetNextSection(
        Montage->GetSectionName(0), NAME_None, Montage);
    return true;
}

void ABossActivationCore::HandleMissileHit(AActor* Missile)
{
    if (!HasAuthority() || !bIsActive || bIsBroken
        || !IsValid(Missile)
        || !Missile->IsA<ABossCircularMissile>()
        || !IsValid(PuzzleComponent.Get())
        || Missile->GetOwner() != PuzzleComponent->GetOwner()
        || !PuzzleComponent->CanAcceptHit(this))
    {
        return;
    }

    // 연출이나 성공 콜백보다 먼저 중복 피격을 차단한다.
    bIsActive = false;
    bIsBroken = true;
    ActiveVFX->DeactivateImmediate();
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CoreState = ECoreState::Breaking;

    if (IsValid(BreakEffect.Get()) && GetNetMode() != NM_DedicatedServer)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this, BreakEffect.Get(), GetActorLocation(), GetActorRotation());
    }

    PlayCoreMontage(BreakMontage.Get());
    SetActorTickEnabled(true);

    PuzzleComponent->HandleCoreDeactivated(this);
}

void ABossActivationCore::DeactivateCore()
{
    if (bIsBroken || CoreState == ECoreState::Hidden
        || CoreState == ECoreState::Retracting)
    {
        return;
    }

    bIsActive = false;
    ActiveVFX->DeactivateImmediate();
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CoreState = ECoreState::Retracting;

    if (!PlayCoreMontage(RetractMontage.Get()))
    {
        HideCore();
        return;
    }

    SetActorTickEnabled(true);
}

void ABossActivationCore::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (CoreState == ECoreState::Fading)
    {
        FadeElapsed += DeltaSeconds;
        SetCoreOpacity(1.f - FMath::Clamp(FadeElapsed / 3.f, 0.f, 1.f));

        if (FadeElapsed >= 3.f)
        {
            HideCore();
        }
        return;
    }

    UAnimInstance* Anim = CoreMesh->GetAnimInstance();
    FAnimMontageInstance* Instance =
        Anim && MontageInstanceID != INDEX_NONE
        ? Anim->GetMontageInstanceForID(MontageInstanceID) : nullptr;

    if (Instance && Instance->IsPlaying())
    {
        return;
    }

    if (CoreState == ECoreState::Retracting)
    {
        HideCore();
    }
    else if (CoreState == ECoreState::Breaking)
    {
        CoreMesh->bPauseAnims = true;
        CoreState = ECoreState::Fading;
        FadeElapsed = 0.f;
    }
    else if (CoreState == ECoreState::Spawning)
    {
        CoreMesh->bPauseAnims = true;
        CoreState = ECoreState::Active;
        SetActorTickEnabled(false);
    }
}

void ABossActivationCore::SetCoreOpacity(float Opacity)
{
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (IsValid(MID))
        {
            MID->SetScalarParameterValue(TEXT("CoreOpacity"), Opacity);
        }
    }
}

void ABossActivationCore::HideCore()
{
    bIsActive = false;
    ActiveVFX->DeactivateImmediate();
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetActorHiddenInGame(true);
    CoreState = ECoreState::Hidden;
    MontageInstanceID = INDEX_NONE;
    SetActorTickEnabled(false);

    CoreMesh->bPauseAnims = false;
    if (UAnimInstance* Anim = CoreMesh->GetAnimInstance())
    {
        Anim->StopAllMontages(0.f);
    }
    CoreMesh->SetComponentTickEnabled(false);
}

void ABossActivationCore::ResetCore()
{
    HideCore();
    bIsBroken = false;
    FadeElapsed = 0.f;
    SetCoreOpacity(1.f);
}