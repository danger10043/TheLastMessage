#include "Core/CorePickup.h"

#include "Character/Player/PlayerBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CoreComponent.h"
#include "Core/CoreRewardSession.h"
#include "DataAsset/CoreDataAsset.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Templates/UnrealTemplate.h"
#include "UI/CoreChoiceWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogCoreReward, Log, All);

ACorePickup::ACorePickup()
{
	// 대기 움직임과 흡수 연출을 Tick에서 처리한다. 일시정지 중에는 함께 멈춘다.
	PrimaryActorTick.bCanEverTick = true;

	// 메시를 루트로 설정하여 액터의 배치 위치와 회전을 메시에도 적용한다.
	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	SetRootComponent(CoreMesh);

	// 메시는 외형만 표시하도록 충돌과 오버랩 감지를 끈다.
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PickupCollision = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollision"));
	PickupCollision->SetupAttachment(CoreMesh);
	PickupCollision->InitSphereRadius(50.0f);
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupCollision->SetGenerateOverlapEvents(true);

	// DefaultEngine.ini의 CorePickup 채널을 사용한다. 플레이어 캡슐만 이 채널에 Overlap으로 응답해야 한다.
	PickupCollision->SetCollisionObjectType(ECC_GameTraceChannel2);
	PickupCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 획득 범위에 들어온 대상을 자동 획득 처리로 연결한다.
	PickupCollision->OnComponentBeginOverlap.AddDynamic(this, &ACorePickup::OnPickupBeginOverlap);
}

void ACorePickup::EnsurePickupId()
{
	if (IsTemplate() || PickupId.IsValid()) return;
#if WITH_EDITOR
	// 기존 배치도 에디터에서 로드하면 안정적인 ActorGuid를 한 번 복사해 런타임 속성에 보관한다.
	// 엔진의 ActorGuid 자체는 패키징 빌드에 없으므로 저장 API에서 직접 사용하지 않는다.
	if (GetActorGuid().IsValid())
	{
		PickupId = GetActorGuid();
		return;
	}
#endif
	PickupId = FGuid::NewGuid();
}

void ACorePickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsurePickupId();
}

void ACorePickup::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	EnsurePickupId();
#endif
}

#if WITH_EDITOR
void ACorePickup::PostEditImport()
{
	Super::PostEditImport();
	// 복제/붙여넣기는 새 배치다. PIE는 이 경로를 거치지 않으므로 원본 ID를 유지한다.
	if (!IsTemplate()) PickupId = FGuid::NewGuid();
}
#endif

bool ACorePickup::QuerySavedConsumedState_Implementation() const
{
	return false;
}

void ACorePickup::RestoreConsumedState(bool bWasConsumed)
{
	// 이미 지급한 픽업을 false로 되돌려 보상을 다시 지급하지 않는다. 초기화는 맵 재로드 후 새 기록으로 한다.
	if (!HasAuthority() || !bWasConsumed || bIsGrantingReward) return;
	bConsumed = true;
	bIsAbsorbing = false;
	bOpenChoiceAfterAbsorb = false;
	bHiddenForChoice = false;
	AbsorbTarget.Reset();
	ClearChoiceReward();
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}

void ACorePickup::MarkConsumed()
{
	if (bConsumed) return;
	bConsumed = true;
	OnPickupConsumed.Broadcast(PickupId);
}

void ACorePickup::OnPickupBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 코어 지급은 서버에서 처리한다.
	if (!HasAuthority() || !bPersistenceInitialized)
	{
		return;
	}

	// 플레이어의 캡슐이 들어온 경우에만 획득을 요청한다.
	APlayerBase* Player = Cast<APlayerBase>(OtherActor);
	if (!IsValid(Player) || !Player->IsPlayerControlled() || OtherComp != Player->GetCapsuleComponent())
	{
		return;
	}

	// 고정 보상은 즉시 획득하고, 선택형 보상은 화면에서 선택을 마친 뒤 지급한다.
	if (RewardMode == ECoreRewardMode::Choice)
	{
		TryOpenCoreChoice(Player);
	}
	else
	{
		TryAcquireFixedCore(Player->GetCoreComponent());
	}
}

bool ACorePickup::TryAcquireFixedCore(UCoreComponent* InCoreComponent)
{
	// 지급 가능한 고정 보상과 코어를 받을 컴포넌트가 준비되어 있어야 한다.
	// 서버 권한이 없거나 액터가 파괴 중이면 보상을 지급할 수 없다.
	if (!HasAuthority() || !bPersistenceInitialized || IsActorBeingDestroyed()
		|| RewardMode != ECoreRewardMode::Fixed || bConsumed || bIsGrantingReward
		|| !IsValid(InCoreComponent) || !IsValid(CoreData))
	{
		return false;
	}

	// 코어 획득 처리가 끝날 때까지 같은 보상의 추가 획득 요청을 막는다.
	TGuardValue<bool> GrantingRewardGuard(bIsGrantingReward, true);
	// 코어 획득을 시도하며, 실패하면 보상 액터를 유지하고 false를 반환한다.
	if (!IsCoreAcquireSuccess(InCoreComponent->AcquireCore(CoreData)))
	{
		return false;
	}

	// 획득 성공 후 소비 상태를 먼저 기록하고, 코어를 받은 액터에게 빨려 들어가는 연출을 시작한다.
	AActor* RewardOwner = InCoreComponent->GetOwner();
	MarkConsumed();
	if (!IsActorBeingDestroyed()) StartAbsorb(RewardOwner);
	return true;
}

bool ACorePickup::TryOpenCoreChoice(APlayerBase* Player)
{
	if (!HasAuthority() || !bPersistenceInitialized || IsActorBeingDestroyed() || bConsumed || bIsGrantingReward
		|| RewardMode != ECoreRewardMode::Choice || IsValid(ActiveChoiceWidget) || IsValid(ActiveRewardSession)
		|| !IsValid(Player) || Player->IsDead() || !ChoiceWidgetClass)
	{
		return false;
	}

	APlayerController* Controller = Cast<APlayerController>(Player->GetController());
	UCoreComponent* CoreComponent = Player->GetCoreComponent();
	if (!IsValid(Controller) || !Controller->IsLocalController() || !IsValid(CoreComponent))
	{
		return false;
	}

	TGuardValue<bool> GrantingRewardGuard(bIsGrantingReward, true);
	// Asset Manager에 등록된 코어를 보상 후보의 원본 목록으로 수집한다.
	UAssetManager& AssetManager = UAssetManager::Get();
	TArray<FPrimaryAssetId> CoreAssetIds;
	AssetManager.GetPrimaryAssetIdList(UCoreDataAsset::StaticClass()->GetFName(), CoreAssetIds);
	TArray<UCoreDataAsset*> Pool;
	for (const FPrimaryAssetId& AssetId : CoreAssetIds)
	{
		UCoreDataAsset* Data = Cast<UCoreDataAsset>(AssetManager.GetPrimaryAssetPath(AssetId).TryLoad());
		// 보상 세션에서 분류와 포함 여부를 검사하고, 획득 가능 여부는 코어 컴포넌트에 조회한다.
		if (IsValid(Data))
		{
			Pool.Add(Data);
		}
	}
	UCoreRewardSession* Session = NewObject<UCoreRewardSession>(this);
	const ECoreRewardStartResult StartResult = Session->Start(CoreComponent, Pool, ChoiceCategory);
	if (StartResult != ECoreRewardStartResult::Started)
	{
		ShowUnavailableNotice(StartResult);
		return false;
	}

	// 후보를 먼저 확보해 두면, 코어가 빨려 들어간 뒤 선택 화면을 열 수 없는 경우가 생기지 않는다.
	ChoosingPlayer = Player;
	ActiveRewardSession = Session;
	Session->OnCancelled.AddUObject(this, &ACorePickup::HandleRewardCancelled);
	Player->OnPlayerDied.AddDynamic(this, &ACorePickup::HandleChoosingPlayerDied);
	Player->OnDestroyed.AddDynamic(this, &ACorePickup::HandleChoosingPlayerDestroyed);

	// 코어가 플레이어에게 빨려 들어간 뒤 선택 화면을 연다. 연출 시간이 0이면 바로 연다.
	if (AbsorbDuration > KINDA_SMALL_NUMBER)
	{
		bOpenChoiceAfterAbsorb = true;
		StartAbsorb(Player);
		return true;
	}
	return OpenChoiceScreen();
}

bool ACorePickup::OpenChoiceScreen()
{
	TGuardValue<bool> GrantingRewardGuard(bIsGrantingReward, true);
	APlayerBase* Player = ChoosingPlayer.Get();
	UCoreRewardSession* Session = ActiveRewardSession;
	APlayerController* Controller = IsValid(Player) ? Cast<APlayerController>(Player->GetController()) : nullptr;
	if (IsValid(Controller) && Controller->IsLocalController() && IsValid(Session) && Session->IsActive()
		&& !IsActorBeingDestroyed() && !Player->IsDead())
	{
		ActiveChoiceWidget = UCoreChoiceWidget::OpenCoreChoiceUI(Controller, Session->GetChoiceEntries(), ChoiceWidgetClass);
	}
	if (!IsValid(ActiveChoiceWidget) || !IsValid(Session) || !Session->IsActive() || IsActorBeingDestroyed()
		|| !IsValid(Player) || Player->IsDead() || Player->GetController() != Controller)
	{
		// 화면 준비에 실패하면 기록을 폐기해 다른 보상을 막지 않는다. 픽업은 소비하지 않고 제자리로 되돌린다.
		ClearChoiceReward();
		return false;
	}

	ActiveChoiceWidget->OnCoreSelected.AddDynamic(this, &ACorePickup::HandleCoreSelected);
	ActiveChoiceWidget->OnRerollRequested.AddDynamic(this, &ACorePickup::HandleCoreReroll);
	UpdateRerollButtons();
	ActiveChoiceWidget->OnChoiceClosed.AddDynamic(this, &ACorePickup::HandleChoiceClosed);
	return true;
}

UCoreRewardSession* ACorePickup::GetValidRewardSession()
{
	if (bConsumed || bIsGrantingReward) return nullptr;
	APlayerBase* Player = ChoosingPlayer.Get();
	if (!HasAuthority() || IsActorBeingDestroyed()
		|| !IsValid(ActiveChoiceWidget)
		|| !IsValid(Player) || Player->IsDead()
		|| !IsValid(ActiveRewardSession) || !ActiveRewardSession->IsActive()
		|| Player->GetCoreComponent() != ActiveRewardSession->GetCoreComponent()
		|| Player->GetController() != ActiveChoiceWidget->GetOwningPlayer())
	{
		ClearChoiceReward();
		return nullptr;
	}
	return ActiveRewardSession;
}

void ACorePickup::UpdateRerollButtons()
{
	if (!IsValid(ActiveChoiceWidget) || !IsValid(ActiveRewardSession)) return;
	for (int32 Index = 0; Index < CoreRewardRules::MaxChoices; ++Index)
	{
		ActiveChoiceWidget->SetRerollEnabled(Index, ActiveRewardSession->CanRerollCoreChoice(Index));
	}
}

bool ACorePickup::RefreshChoiceScreen()
{
	if (!IsValid(ActiveRewardSession) || !ActiveRewardSession->IsActive() || !IsValid(ActiveChoiceWidget)) return false;
	UCoreChoiceWidget* Widget = ActiveChoiceWidget;
	UCoreRewardSession* Session = ActiveRewardSession;
	if (!Widget->UpdateCoreChoices(Session->GetChoiceEntries()))
	{
		if (ActiveChoiceWidget == Widget) ClearChoiceReward();
		return false;
	}
	if (ActiveChoiceWidget != Widget || ActiveRewardSession != Session || !Session->IsActive()) return false;
	UpdateRerollButtons();
	return true;
}

void ACorePickup::HandleRewardCancelled()
{
	UE_LOG(LogCoreReward, Display, TEXT("%s: 코어 선택 상태를 정리합니다. 픽업은 소비하지 않습니다."), *GetName());
	ClearChoiceReward();
}

void ACorePickup::ShowUnavailableNotice(ECoreRewardStartResult Result) const
{
	if (Result == ECoreRewardStartResult::Started) return;

	// UMETA DisplayName은 에디터에서만 읽을 수 있으므로 분류 이름을 직접 지정한다.
	const TCHAR* CategoryName = ChoiceCategory == ECoreCategory::Ability ? TEXT("능력형") : TEXT("스탯형");
	FString Message;
	switch (Result)
	{
	case ECoreRewardStartResult::Busy:
		Message = TEXT("다른 코어 보상을 선택 중이거나 지급 처리 중입니다.");
		break;
	case ECoreRewardStartResult::AllAtMaxLevel:
		Message = FString::Printf(TEXT("%s 코어를 모두 최대 단계까지 획득했습니다."), CategoryName);
		break;
	case ECoreRewardStartResult::NoCandidates:
		Message = FString::Printf(TEXT("이 보상에서 받을 수 있는 %s 코어가 없습니다."), CategoryName);
		break;
	default:
		Message = TEXT("지금은 코어를 받을 수 없습니다. 코어 데이터 또는 지급 대상을 확인하세요.");
		break;
	}
	UE_LOG(LogCoreReward, Display, TEXT("%s: 코어 선택을 열 수 없습니다. %s"), *GetName(), *Message);

	// 다른 보상을 처리 중일 때는 곧 끝나므로 화면에는 띄우지 않는다.
	// 같은 픽업에 여러 번 닿아도 메시지가 쌓이지 않도록 픽업마다 같은 키를 사용한다.
	if (Result != ECoreRewardStartResult::Busy && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 2.5f, FColor::Yellow, Message);
	}
}

void ACorePickup::HandleCoreSelected(UCoreDataAsset* SelectedCore)
{
	UCoreRewardSession* Session = GetValidRewardSession();
	if (!Session || !IsValid(SelectedCore)) return;

	// 획득 알림 중 다른 카드가 선택되더라도 한 보상은 한 번만 지급한다.
	TGuardValue<bool> GrantingRewardGuard(bIsGrantingReward, true);
	const ECoreAcquireResult Result = Session->SelectCore(SelectedCore->CoreId);
	if (!IsCoreAcquireSuccess(Result))
	{
		if (RefreshChoiceScreen())
		{
			ActiveChoiceWidget->SetStatusMessage(Result == ECoreAcquireResult::Busy
				? NSLOCTEXT("CoreReward", "Busy", "코어를 처리 중입니다. 잠시 후 다시 선택하세요.")
				: NSLOCTEXT("CoreReward", "ChoiceUnavailable", "지급할 수 없는 선택지를 정리했습니다. 코어를 다시 선택하세요."));
		}
		return;
	}

	// 코어는 선택 화면을 열기 전에 이미 빨려 들어갔으므로 바로 제거한다.
	MarkConsumed();
	ClearChoiceReward(true);
	Destroy();
}

void ACorePickup::HandleCoreReroll(int32 ChoiceIndex)
{
	UCoreRewardSession* Session = GetValidRewardSession();
	if (!Session) return;

	TGuardValue<bool> GrantingRewardGuard(bIsGrantingReward, true);
	const bool bRerolled = Session->RerollCoreChoice(ChoiceIndex) != nullptr;
	if (RefreshChoiceScreen() && bRerolled) ActiveChoiceWidget->ResetStatusMessage();
}

void ACorePickup::HandleChoiceClosed()
{
	ClearChoiceReward();
}

void ACorePickup::HandleChoosingPlayerDied(APlayerBase* Player)
{
	ClearChoiceReward();
}

void ACorePickup::HandleChoosingPlayerDestroyed(AActor* Player)
{
	ClearChoiceReward();
}

void ACorePickup::CloseChoiceScreen(bool bAnimateClose)
{
	UCoreChoiceWidget* Widget = ActiveChoiceWidget;
	APlayerBase* Player = ChoosingPlayer.Get();

	// 화면 종료 알림에서 이 함수가 다시 호출되어도 같은 상태를 정리하지 않도록 먼저 비운다.
	ActiveChoiceWidget = nullptr;
	ChoosingPlayer.Reset();
	if (IsValid(Player))
	{
		Player->OnPlayerDied.RemoveDynamic(this, &ACorePickup::HandleChoosingPlayerDied);
		Player->OnDestroyed.RemoveDynamic(this, &ACorePickup::HandleChoosingPlayerDestroyed);
	}
	if (IsValid(Widget))
	{
		Widget->OnCoreSelected.RemoveDynamic(this, &ACorePickup::HandleCoreSelected);
		Widget->OnRerollRequested.RemoveDynamic(this, &ACorePickup::HandleCoreReroll);
		Widget->OnChoiceClosed.RemoveDynamic(this, &ACorePickup::HandleChoiceClosed);
		Widget->CloseChoiceUI(bAnimateClose);
	}
}

void ACorePickup::ClearChoiceReward(bool bAnimateClose)
{
	UCoreRewardSession* Session = ActiveRewardSession;
	ActiveRewardSession = nullptr;
	if (IsValid(Session))
	{
		Session->OnCancelled.RemoveAll(this);
		Session->Cancel();
	}
	CloseChoiceScreen(bAnimateClose);

	// 선택을 마치지 못했다면 빨려 들어갔던 코어를 제자리로 되돌려 다시 획득할 수 있게 한다.
	if (!bConsumed)
	{
		RestorePickup();
	}
}

void ACorePickup::RestorePickup()
{
	if (!bOpenChoiceAfterAbsorb && !bHiddenForChoice) return;

	bOpenChoiceAfterAbsorb = false;
	bHiddenForChoice = false;
	bIsAbsorbing = false;
	AbsorbTarget.Reset();
	SetActorLocation(IdleBaseLocation);
	SetActorScale3D(IdleBaseScale);
	SetEmissive(IdleEmissive);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ACorePickup::BeginPlay()
{
	// 배치한 위치·크기를 기준으로 떠다녀야 계속 움직여도 위치가 밀리지 않는다.
	// 여러 코어가 같은 박자로 움직이지 않도록 시작 시점을 흩뜨린다.
	IdleBaseLocation = GetActorLocation();
	IdleBaseScale = GetActorScale3D();
	IdleTime = FMath::FRandRange(0.0f, 10.0f);
	if (!PickupId.IsValid())
	{
		UE_LOG(LogCoreReward, Warning, TEXT("%s: PickupId가 없습니다. 에디터에서 이 픽업의 맵을 열고 저장하세요."), *GetPathName());
	}

	// Blueprint BeginPlay 및 초기 겹침에 의한 지급보다 저장 기록을 먼저 적용한다.
	if (HasAuthority() && QuerySavedConsumedState()) RestoreConsumedState(true);

	// 머티리얼에 발광 파라미터가 있을 때만 인스턴스를 만들어 세기를 조절한다.
	UMaterialInterface* Material = bConsumed ? nullptr : CoreMesh->GetMaterial(0);
	float DefaultEmissive = 0.0f;
	if (!EmissiveParameter.IsNone() && Material
		&& Material->GetScalarParameterValue(FHashedMaterialParameterInfo(EmissiveParameter), DefaultEmissive))
	{
		CoreMaterial = CoreMesh->CreateDynamicMaterialInstance(0);
	}
	SetEmissive(IdleEmissive);
	bPersistenceInitialized = true;
	Super::BeginPlay();
	if (bConsumed)
	{
		// Super::BeginPlay가 액터 Tick을 등록하면서 다시 켤 수 있으므로 등록 후에도 끈다.
		SetActorTickEnabled(false);
		return;
	}
	if (IsActorBeingDestroyed()) return;

	// 저장 상태 적용 전 발생한 초기 겹침은 무시했으므로, 이미 범위 안에 있던 플레이어를 한 번 다시 확인한다.
	TArray<UPrimitiveComponent*> OverlappingComponents;
	PickupCollision->GetOverlappingComponents(OverlappingComponents);
	for (UPrimitiveComponent* Component : OverlappingComponents)
	{
		if (IsValid(Component))
			OnPickupBeginOverlap(PickupCollision, Component->GetOwner(), Component, 0, false, FHitResult());
	}
}

void ACorePickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 제거되는 중에는 코어를 제자리로 되돌릴 필요가 없다.
	bOpenChoiceAfterAbsorb = false;
	bHiddenForChoice = false;
	ClearChoiceReward();
	Super::EndPlay(EndPlayReason);
}

void ACorePickup::StartAbsorb(AActor* Target)
{
	// 연출 중에 다시 획득되지 않도록 충돌을 먼저 끈다. 소비 여부는 보상 방식에 따라 호출한 쪽에서 정한다.
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 지금 떠 있는 위치·크기·밝기에서 그대로 이어서 빨려 들어가게 한다.
	AbsorbTarget = Target;
	AbsorbStartLocation = GetActorLocation();
	AbsorbStartScale = GetActorScale3D();
	AbsorbStartEmissive = CurrentEmissive;
	AbsorbElapsed = 0.0f;
	if (!IsValid(Target) || AbsorbDuration <= KINDA_SMALL_NUMBER)
	{
		FinishAbsorb();
		return;
	}

	bIsAbsorbing = true;
	SetActorTickEnabled(true);
}

void ACorePickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsAbsorbing)
	{
		UpdateIdle(DeltaSeconds);
	}
	else if (UpdateAbsorb(DeltaSeconds))
	{
		FinishAbsorb();
	}
}

void ACorePickup::UpdateIdle(float DeltaSeconds)
{
	// 크기와 발광은 같은 사인 곡선을 사용해 함께 숨 쉬듯 바뀐다.
	IdleTime += DeltaSeconds;
	const float Bob = FMath::Sin(IdleTime * BobSpeed * UE_TWO_PI) * BobHeight;
	const float Pulse = FMath::Sin(IdleTime * PulseSpeed * UE_TWO_PI);

	SetActorLocation(IdleBaseLocation + FVector(0.0f, 0.0f, Bob));
	AddActorWorldRotation(FRotator(0.0f, SpinSpeed * DeltaSeconds, 0.0f));
	SetActorScale3D(IdleBaseScale * (1.0f + PulseScale * Pulse));
	SetEmissive(IdleEmissive * (1.0f + EmissivePulse * Pulse));
}

bool ACorePickup::UpdateAbsorb(float DeltaSeconds)
{
	AActor* Target = AbsorbTarget.Get();
	const APlayerBase* Player = Cast<APlayerBase>(Target);
	if (!IsValid(Target) || (Player && Player->IsDead()))
	{
		return true;
	}

	// 처음에는 천천히, 끝에 빠르게 빨려 들어가도록 가속 곡선을 사용한다.
	// 대상이 움직여도 따라가도록 도착 지점은 매 프레임 다시 계산한다.
	AbsorbElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(AbsorbElapsed / AbsorbDuration, 0.0f, 1.0f);
	const float Eased = Alpha * Alpha;
	const FVector TargetLocation = Target->GetActorLocation() + AbsorbTargetOffset;
	SetActorLocation(FMath::Lerp(AbsorbStartLocation, TargetLocation, Eased));

	// 크기 0은 변환 계산에서 경고가 날 수 있어 아주 작은 값에서 멈춘다.
	SetActorScale3D(AbsorbStartScale * FMath::Max(1.0f - Eased, 0.01f));
	SetEmissive(FMath::Lerp(AbsorbStartEmissive, AbsorbEmissive, Alpha));
	return Alpha >= 1.0f;
}

void ACorePickup::FinishAbsorb()
{
	bIsAbsorbing = false;
	SetActorTickEnabled(false);
	AbsorbTarget.Reset();
	if (!bOpenChoiceAfterAbsorb)
	{
		Destroy();
		return;
	}

	// 선택형 보상은 코어를 숨긴 채 선택 화면을 연다. 선택에 성공해야 픽업을 제거하고,
	// 화면을 열지 못하거나 선택이 취소되면 ClearChoiceReward에서 제자리로 되돌린다.
	bOpenChoiceAfterAbsorb = false;
	bHiddenForChoice = true;
	SetActorHiddenInGame(true);
	OpenChoiceScreen();
}

void ACorePickup::SetEmissive(float Value)
{
	CurrentEmissive = Value;
	if (CoreMaterial)
	{
		CoreMaterial->SetScalarParameterValue(EmissiveParameter, Value);
	}
}
