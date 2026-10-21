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
#include "Templates/UnrealTemplate.h"
#include "UI/CoreChoiceWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogCoreReward, Log, All);

ACorePickup::ACorePickup()
{
	PrimaryActorTick.bCanEverTick = false;

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

void ACorePickup::OnPickupBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 코어 지급은 서버에서 처리한다.
	if (!HasAuthority())
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
	if (!HasAuthority() || IsActorBeingDestroyed()
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

	// 획득 성공 후 소비 상태를 먼저 기록하고, 맵의 보상 액터를 파괴한다.
	bConsumed = true;
	Destroy();
	return true;
}

bool ACorePickup::TryOpenCoreChoice(APlayerBase* Player)
{
	if (!HasAuthority() || IsActorBeingDestroyed() || bConsumed || bIsGrantingReward
		|| RewardMode != ECoreRewardMode::Choice || IsValid(ActiveChoiceWidget)
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

	ChoosingPlayer = Player;
	ActiveRewardSession = Session;
	ActiveChoiceWidget = UCoreChoiceWidget::OpenCoreChoiceUI(Controller, Session->GetChoiceEntries(), ChoiceWidgetClass);
	if (!IsValid(ActiveChoiceWidget) || !Session->IsActive() || IsActorBeingDestroyed() || !IsValid(Player) || Player->IsDead()
		|| Player->GetController() != Controller)
	{
		// 화면 준비에 실패하면 기록을 폐기해 다른 보상을 막지 않는다. 픽업 자체는 유지한다.
		ClearChoiceReward();
		return false;
	}

	ActiveChoiceWidget->OnCoreSelected.AddDynamic(this, &ACorePickup::HandleCoreSelected);
	ActiveChoiceWidget->OnRerollRequested.AddDynamic(this, &ACorePickup::HandleCoreReroll);
	Session->OnCancelled.AddUObject(this, &ACorePickup::HandleRewardCancelled);
	UpdateRerollButtons();
	ActiveChoiceWidget->OnChoiceClosed.AddDynamic(this, &ACorePickup::HandleChoiceClosed);
	Player->OnPlayerDied.AddDynamic(this, &ACorePickup::HandleChoosingPlayerDied);
	Player->OnDestroyed.AddDynamic(this, &ACorePickup::HandleChoosingPlayerDestroyed);
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

	bConsumed = true;
	ClearChoiceReward(true);
	Destroy();
}

void ACorePickup::HandleCoreReroll(int32 ChoiceIndex)
{
	UCoreRewardSession* Session = GetValidRewardSession();
	if (!Session) return;

	TGuardValue<bool> GrantingRewardGuard(bIsGrantingReward, true);
	Session->RerollCoreChoice(ChoiceIndex);
	RefreshChoiceScreen();
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
}

void ACorePickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearChoiceReward();
	Super::EndPlay(EndPlayReason);
}
