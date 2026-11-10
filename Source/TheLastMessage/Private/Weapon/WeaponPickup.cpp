#include "Weapon/WeaponPickup.h"
#include "Character/Player/PlayerBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CorePickupStateSubsystem.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponPickup, Log, All);

AWeaponPickup::AWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = true;

	PickupRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PickupRoot"));
	SetRootComponent(PickupRoot);

	// 외형 메시는 떠다니고 돌기만 한다. 충돌은 따로 둔 구체가 맡는다.
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(PickupRoot);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetGenerateOverlapEvents(false);
	PickupMesh->SetCastShadow(false);

	PickupCollision = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollision"));
	PickupCollision->SetupAttachment(PickupRoot);
	PickupCollision->InitSphereRadius(80.0f);
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupCollision->SetGenerateOverlapEvents(true);

	// 코어 픽업과 같은 전용 채널(DefaultEngine.ini의 CorePickup)을 쓴다. 플레이어 캡슐만 Overlap으로 응답한다.
	PickupCollision->SetCollisionObjectType(ECC_GameTraceChannel2);
	PickupCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupCollision->OnComponentBeginOverlap.AddDynamic(this, &AWeaponPickup::OnPickupBeginOverlap);
}

// ---------------------------------------------------------------------------
// 배치 ID
// ---------------------------------------------------------------------------

void AWeaponPickup::EnsurePickupId()
{
	if (IsTemplate() || PickupId.IsValid()) return;
#if WITH_EDITOR
	// 에디터에서 로드한 배치는 안정적인 ActorGuid를 한 번 복사해 둔다.
	if (GetActorGuid().IsValid())
	{
		PickupId = GetActorGuid();
		return;
	}
#endif
	PickupId = FGuid::NewGuid();
}

void AWeaponPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsurePickupId();
	ApplyDisplayMesh();
}

void AWeaponPickup::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	EnsurePickupId();
#endif
}

#if WITH_EDITOR
void AWeaponPickup::PostEditImport()
{
	Super::PostEditImport();
	// 복제하거나 붙여넣은 것은 새 배치다. 새 ID를 받는다.
	if (!IsTemplate()) PickupId = FGuid::NewGuid();
}
#endif

UCorePickupStateSubsystem* AWeaponPickup::GetPickupStateSubsystem() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UCorePickupStateSubsystem>() : nullptr;
}

// ---------------------------------------------------------------------------
// 생명 주기
// ---------------------------------------------------------------------------

void AWeaponPickup::ApplyDisplayMesh()
{
	if (!PickupMesh) return;
	if (bUseWeaponMesh && IsValid(WeaponData) && IsValid(WeaponData->WeaponMesh))
	{
		PickupMesh->SetStaticMesh(WeaponData->WeaponMesh);
	}
	PickupMesh->SetRelativeScale3D(DisplayScale);
}

void AWeaponPickup::BeginPlay()
{
	if (!PickupId.IsValid())
	{
		UE_LOG(LogWeaponPickup, Warning, TEXT("%s: PickupId가 없습니다. 에디터에서 이 픽업의 맵을 열고 저장하세요."), *GetPathName());
	}
	if (!IsValid(WeaponData))
	{
		UE_LOG(LogWeaponPickup, Warning, TEXT("%s: Weapon Data가 비어 있습니다. 주울 무기를 지정하세요."), *GetPathName());
	}

	IdleTime = FMath::FRandRange(0.0f, 10.0f);
	ApplyDisplayMesh();

	// 저장 기록을 먼저 적용한다. 이미 획득한 픽업은 보이지 않게 한다.
	if (HasAuthority())
	{
		const UCorePickupStateSubsystem* State = GetPickupStateSubsystem();
		if (State && State->IsPickupConsumed(PickupId)) bConsumed = true;
	}
	bPersistenceInitialized = true;

	Super::BeginPlay();
	if (bConsumed)
	{
		HideConsumedPickup();
		return;
	}

	// 기록을 적용하기 전에 생긴 겹침은 무시했으므로, 이미 범위 안에 있던 플레이어를 한 번 다시 확인한다.
	TArray<UPrimitiveComponent*> OverlappingComponents;
	PickupCollision->GetOverlappingComponents(OverlappingComponents);
	for (UPrimitiveComponent* Component : OverlappingComponents)
	{
		if (IsValid(Component))
			OnPickupBeginOverlap(PickupCollision, Component->GetOwner(), Component, 0, false, FHitResult());
	}
}

void AWeaponPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AutoEquipTimer);
	Super::EndPlay(EndPlayReason);
}

void AWeaponPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	IdleTime += DeltaSeconds;
	const float Bob = FMath::Sin(IdleTime * BobSpeed * UE_TWO_PI) * BobHeight;
	PickupMesh->SetRelativeLocation(FVector(0.0f, 0.0f, Bob));
	if (!FMath::IsNearlyZero(SpinSpeed))
	{
		PickupMesh->AddRelativeRotation(FRotator(0.0f, SpinSpeed * DeltaSeconds, 0.0f));
	}
}

// ---------------------------------------------------------------------------
// 획득
// ---------------------------------------------------------------------------

void AWeaponPickup::OnPickupBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 무기 지급은 서버에서만 처리한다. 저장 기록을 적용하기 전의 겹침은 BeginPlay에서 다시 확인한다.
	if (!HasAuthority() || !bPersistenceInitialized || bConsumed) return;

	// 플레이어의 캡슐이 들어온 경우에만 획득한다.
	APlayerBase* Player = Cast<APlayerBase>(OtherActor);
	if (!IsValid(Player) || !Player->IsPlayerControlled() || Player->IsDead() || OtherComp != Player->GetCapsuleComponent()) return;

	TryGrant(Player);
}

bool AWeaponPickup::TryGrant(APlayerBase* Player)
{
	if (!IsValid(WeaponData)) return false;

	// 이 무기가 플레이어의 몇 번 무기(1~4번 키)인지 찾는다.
	int32 WeaponIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Player->WeaponChoices.Num(); ++Index)
	{
		if (Player->WeaponChoices[Index] == WeaponData)
		{
			WeaponIndex = Index;
			break;
		}
	}
	if (WeaponIndex == INDEX_NONE)
	{
		UE_LOG(LogWeaponPickup, Warning, TEXT("%s: %s이(가) 플레이어의 Weapon Choices에 없습니다. 플레이어 블루프린트에 먼저 등록하세요."),
			*GetPathName(), *GetNameSafe(WeaponData));
		return false;
	}

	if (!Player->AcquireWeaponChoice(WeaponIndex)) return false;

	MarkConsumed();
	if (bAutoEquip) BeginAutoEquip(Player, WeaponIndex);

	if (PickupSound) UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	if (PickupEffect) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), PickupEffect, GetActorLocation());
	BP_OnWeaponPickedUp(Player, WeaponData);

	// 체크포인트로 돌아가면 다시 보여 줄 수 있도록 액터는 남기고 숨기기만 한다.
	HideConsumedPickup();
	return true;
}

void AWeaponPickup::MarkConsumed()
{
	if (bConsumed) return;
	bConsumed = true;
	// 코어 픽업과 같은 기록에 남긴다. 체크포인트 저장과 불러오기가 이 기록을 함께 처리한다.
	if (UCorePickupStateSubsystem* State = GetPickupStateSubsystem()) State->SetPickupConsumed(PickupId, true);
}

void AWeaponPickup::HideConsumedPickup()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	if (PickupCollision) PickupCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWeaponPickup::ShowPickup()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
	if (PickupCollision) PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	if (PickupMesh) PickupMesh->SetRelativeLocation(FVector::ZeroVector);
}

void AWeaponPickup::RestoreCheckpointConsumption(bool bWasConsumed)
{
	if (!HasAuthority()) return;
	GetWorldTimerManager().ClearTimer(AutoEquipTimer);
	AutoEquipPlayer.Reset();

	bConsumed = bWasConsumed;
	if (bConsumed) HideConsumedPickup();
	else ShowPickup();
}

// ---------------------------------------------------------------------------
// 자동 장착
// ---------------------------------------------------------------------------

void AWeaponPickup::BeginAutoEquip(APlayerBase* Player, int32 WeaponIndex)
{
	AutoEquipPlayer = Player;
	AutoEquipIndex = WeaponIndex;
	AutoEquipAttempts = 0;
	if (TryAutoEquip()) return;

	// 공격 중, 피격 중처럼 지금 장착할 수 없으면 잠시 뒤 다시 시도한다.
	GetWorldTimerManager().SetTimer(AutoEquipTimer, this, &AWeaponPickup::RetryAutoEquip, 0.25f, true);
}

void AWeaponPickup::RetryAutoEquip()
{
	++AutoEquipAttempts;
	if (TryAutoEquip() || AutoEquipAttempts >= 12)
	{
		GetWorldTimerManager().ClearTimer(AutoEquipTimer);
		AutoEquipPlayer.Reset();
	}
}

bool AWeaponPickup::TryAutoEquip()
{
	APlayerBase* Player = AutoEquipPlayer.Get();
	if (!IsValid(Player) || Player->IsDead()) return true;

	// 이미 장착된 무기면 할 일이 없다.
	if (Player->GetWeaponChoiceAssignment(AutoEquipIndex) != EWeaponSlot::None) return true;

	// 비어 있는 슬롯에 넣는다(왼쪽 클릭, 오른쪽 클릭 순). 둘 다 차 있으면 보유만 한다.
	EWeaponSlot TargetSlot = EWeaponSlot::None;
	if (!IsValid(Player->GetWeaponDataInSlot(EWeaponSlot::Primary))) TargetSlot = EWeaponSlot::Primary;
	else if (!IsValid(Player->GetWeaponDataInSlot(EWeaponSlot::Secondary))) TargetSlot = EWeaponSlot::Secondary;
	if (TargetSlot == EWeaponSlot::None) return true;

	if (!Player->AssignWeaponChoice(AutoEquipIndex, TargetSlot)) return false;

	// 새로 든 무기가 바로 보이게 한다.
	Player->ShowWeaponInSlot(TargetSlot);
	return true;
}
