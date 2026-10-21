#include "Core/CorePickup.h"

#include "Character/Player/PlayerBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CoreComponent.h"
#include "DataAsset/CoreDataAsset.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerController.h"
#include "Templates/UnrealTemplate.h"
#include "UI/CoreChoiceWidget.h"

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
	// 선택을 보류하지 않으므로, 새 화면을 열 때마다 현재 보유 상태를 기준으로 추첨한다.
	UAssetManager& AssetManager = UAssetManager::Get();
	TArray<FPrimaryAssetId> CoreAssetIds;
	AssetManager.GetPrimaryAssetIdList(UCoreDataAsset::StaticClass()->GetFName(), CoreAssetIds);
	TArray<UCoreDataAsset*> Pool;
	for (const FPrimaryAssetId& AssetId : CoreAssetIds)
	{
		UCoreDataAsset* Data = Cast<UCoreDataAsset>(AssetManager.GetPrimaryAssetPath(AssetId).TryLoad());
		// 분류·무작위 보상 포함 여부·강화 가능 여부는 코어 컴포넌트에서 검사한다.
		if (IsValid(Data))
		{
			Pool.Add(Data);
		}
	}
	const TArray<UCoreDataAsset*> Choices = CoreComponent->GenerateCoreChoices(Pool, ChoiceCategory);
	if (Choices.IsEmpty()) return false;

	ChoosingPlayer = Player;
	ChoosingCoreComponent = CoreComponent;
	ActiveChoiceWidget = UCoreChoiceWidget::OpenCoreChoiceUI(Controller, Choices, ChoiceWidgetClass);
	if (!IsValid(ActiveChoiceWidget) || IsActorBeingDestroyed() || !IsValid(Player) || Player->IsDead()
		|| Player->GetController() != Controller)
	{
		// 화면 준비에 실패하면 기록을 폐기해 다른 보상을 막지 않는다. 픽업 자체는 유지한다.
		ClearChoiceReward();
		return false;
	}

	ActiveChoiceWidget->OnCoreSelected.AddDynamic(this, &ACorePickup::HandleCoreSelected);
	ActiveChoiceWidget->OnRerollRequested.AddDynamic(this, &ACorePickup::HandleCoreReroll);
	UpdateRerollButtons();
	ActiveChoiceWidget->OnChoiceClosed.AddDynamic(this, &ACorePickup::HandleChoiceClosed);
	Player->OnPlayerDied.AddDynamic(this, &ACorePickup::HandleChoosingPlayerDied);
	Player->OnDestroyed.AddDynamic(this, &ACorePickup::HandleChoosingPlayerDestroyed);
	return true;
}

UCoreComponent* ACorePickup::GetValidChoosingCoreComponent() const
{
	APlayerBase* Player = ChoosingPlayer.Get();
	UCoreComponent* CoreComponent = ChoosingCoreComponent.Get();
	if (!HasAuthority() || IsActorBeingDestroyed() || bConsumed || bIsGrantingReward
		|| !IsValid(ActiveChoiceWidget)
		|| !IsValid(Player) || Player->IsDead() || !IsValid(CoreComponent)
		|| Player->GetCoreComponent() != CoreComponent
		|| Player->GetController() != ActiveChoiceWidget->GetOwningPlayer())
	{
		return nullptr;
	}
	return CoreComponent;
}

void ACorePickup::UpdateRerollButtons()
{
	UCoreComponent* CoreComponent = ChoosingCoreComponent.Get();
	if (!IsValid(ActiveChoiceWidget) || !IsValid(CoreComponent)) return;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		ActiveChoiceWidget->SetRerollEnabled(Index, CoreComponent->CanRerollCoreChoice(Index));
	}
}

void ACorePickup::HandleCoreSelected(UCoreDataAsset* SelectedCore)
{
	UCoreComponent* CoreComponent = GetValidChoosingCoreComponent();
	if (!CoreComponent || !IsValid(SelectedCore)) return;

	// 획득 알림 중 다른 카드가 선택되더라도 한 보상은 한 번만 지급한다.
	TGuardValue<bool> GrantingRewardGuard(bIsGrantingReward, true);
	if (!IsCoreAcquireSuccess(CoreComponent->SelectCore(SelectedCore->CoreId)))
	{
		// 지급에 실패하면 화면과 선택지를 유지하여 다른 코어를 선택할 수 있게 한다.
		return;
	}

	bConsumed = true;
	ClearChoiceReward(true);
	Destroy();
}

void ACorePickup::HandleCoreReroll(int32 ChoiceIndex)
{
	UCoreComponent* CoreComponent = GetValidChoosingCoreComponent();
	if (!CoreComponent) return;

	TGuardValue<bool> GrantingRewardGuard(bIsGrantingReward, true);
	UCoreChoiceWidget* Widget = ActiveChoiceWidget;
	UCoreDataAsset* Replacement = CoreComponent->RerollCoreChoice(ChoiceIndex);
	if (Replacement && !Widget->UpdateCoreChoice(ChoiceIndex, Replacement))
	{
		// 후보와 화면이 어긋난 상태로 선택하지 못하도록 현재 선택을 정리한다.
		ClearChoiceReward();
		return;
	}
	// 카드의 강조 알림에서 화면이 닫혔다면, 종료된 선택 화면을 다시 갱신하지 않는다.
	if (ActiveChoiceWidget == Widget) UpdateRerollButtons();
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
	ChoosingCoreComponent.Reset();
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
	if (UCoreComponent* CoreComponent = ChoosingCoreComponent.Get(); IsValid(CoreComponent))
	{
		CoreComponent->CancelCoreChoices();
	}
	CloseChoiceScreen(bAnimateClose);
}

void ACorePickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearChoiceReward();
	Super::EndPlay(EndPlayReason);
}
