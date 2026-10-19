#include "UI/PlayerHUD.h"
#include "UI/PlayerStatusWidget.h"
#include "UI/WeaponChargeWidget.h"
#include "Character/Player/PlayerBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/LocalPlayer.h"

APlayerHUD::APlayerHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void APlayerHUD::BeginPlay()
{
	Super::BeginPlay();
	EnsureWidgets();
	UpdatePlayerUI();
}

void APlayerHUD::EnsureWidgets()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer()) return;
	if (!StatusWidget && StatusWidgetClass)
	{
		StatusWidget = CreateWidget<UPlayerStatusWidget>(PC, StatusWidgetClass);
		if (StatusWidget)
		{
			StatusWidget->SetObservedPlayer(Cast<APlayerBase>(PC->GetPawn()));
			StatusWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			StatusWidget->AddToPlayerScreen(10);
		}
	}
	if (!ChargeWidget && ChargeWidgetClass)
	{
		ChargeWidget = CreateWidget<UWeaponChargeWidget>(PC, ChargeWidgetClass);
		if (ChargeWidget)
		{
			ChargeWidget->AddToPlayerScreen(20);
			ChargeWidget->SetDesiredSizeInViewport(ChargeWidgetSize);
			ChargeWidget->SetAlignmentInViewport(FVector2D(0.5, 1));
			ChargeWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void APlayerHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	EnsureWidgets();
	UpdatePlayerUI();
}

void APlayerHUD::UpdatePlayerUI()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController()) return;
	APlayerBase* Player = Cast<APlayerBase>(PC->GetPawn());
	if (ObservedPlayer.Get() != Player)
	{
		ObservedPlayer = Player;
		if (StatusWidget) StatusWidget->SetObservedPlayer(Player);
	}
	if (StatusWidget)
	{
		StatusWidget->SetVisibility(bShowHUD && Player ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!ChargeWidget) return;
	bool bShowCharge = bShowHUD && Player && Player->IsChargingWeapon();
	FVector2D Position = FVector2D::ZeroVector;
	if (bShowCharge)
	{
		const FVector Up = -Player->GetCharacterMovement()->GetGravityDirection();
		const FVector Anchor = Player->GetActorLocation() + Up *
			(Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + ChargeHeightAboveCapsule);
		bShowCharge = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Anchor, Position, true);
		const FVector2D ScreenSize = UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(PC).GetLocalSize();
		bShowCharge = bShowCharge && Position.X >= 0 && Position.Y >= 0
			&& Position.X <= ScreenSize.X && Position.Y <= ScreenSize.Y;
	}
	if (bShowCharge)
	{
		ChargeWidget->SetChargeProgress(Player->GetWeaponChargeRatio());
		ChargeWidget->SetPositionInViewport(Position, false);
	}
	else ChargeWidget->SetChargeProgress(0);
	ChargeWidget->SetVisibility(bShowCharge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void APlayerHUD::ShowHUD()
{
	Super::ShowHUD();
	UpdatePlayerUI();
}

void APlayerHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (StatusWidget) { StatusWidget->SetObservedPlayer(nullptr); StatusWidget->RemoveFromParent(); }
	if (ChargeWidget) ChargeWidget->RemoveFromParent();
	StatusWidget = nullptr;
	ChargeWidget = nullptr;
	ObservedPlayer.Reset();
	Super::EndPlay(EndPlayReason);
}
