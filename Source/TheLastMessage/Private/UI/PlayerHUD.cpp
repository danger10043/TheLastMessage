#include "UI/PlayerHUD.h"
#include "UI/PlayerStatusWidget.h"
#include "UI/WeaponChargeWidget.h"
#include "Character/Player/PlayerBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "RenderUtils.h"
#include "Character/Player/GrapplingHookComponent.h"

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
			StatusWidget->SetVisibility(bShowHUD ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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

void APlayerHUD::DrawHUD()
{
	Super::DrawHUD();
	DrawGrappleIndicator();
}

void APlayerHUD::DrawGrappleIndicator()
{
	APlayerController* PC = GetOwningPlayerController();
	const APlayerBase* Player = PC ? Cast<APlayerBase>(PC->GetPawn()) : nullptr;
	const UGrapplingHookComponent* Grapple = Player ? Player->FindComponentByClass<UGrapplingHookComponent>() : nullptr;
	if (!Canvas || !bShowHUD || !PC || !PC->IsLocalController() || !Player || Player->IsDead()
		|| !Grapple || !Grapple->IsGrappleUnlocked())
	{
		bGrappleWasCoolingDown = false;
		GrappleReadyFlashEnd = 0;
		return;
	}
	FVector2D Aim;
	if (!PC->ProjectWorldLocationToScreen(Player->GetAimTargetLocation(), Aim, true)) return;
	const float Scale = FMath::Clamp(Canvas->ClipY / 1080.0f, 0.65f, 2.0f);
	const float Radius = GrappleIndicatorRadius * Scale;
	const FVector2D Center(
		FMath::Clamp(Aim.X + GrappleIndicatorOffset.X * Scale, Radius + 2, FMath::Max(Radius + 2, Canvas->ClipX - Radius - 2)),
		FMath::Clamp(Aim.Y + GrappleIndicatorOffset.Y * Scale, Radius + 2, FMath::Max(Radius + 2, Canvas->ClipY - Radius - 2)));
	const bool bCoolingDown = Grapple->GetCooldownRemaining() > 0;
	const float Now = GetWorld()->GetTimeSeconds();
	if (bGrappleWasCoolingDown && !bCoolingDown) GrappleReadyFlashEnd = Now + 0.22f;
	bGrappleWasCoolingDown = bCoolingDown;
	const bool bActive = Grapple->HasGrappleAnchor();
	const float Flash = FMath::Clamp((GrappleReadyFlashEnd - Now) / 0.22f, 0.0f, 1.0f);
	const FLinearColor Ready(0.2f, 0.85f, 1.0f);
	const FLinearColor Color = Flash > 0 ? FLinearColor::LerpUsingHSV(Ready, FLinearColor::White, Flash)
		: bActive ? FLinearColor(0.55f, 1.0f, 1.0f) : bCoolingDown ? FLinearColor(0.35f, 0.42f, 0.48f, 0.65f) : Ready;
	const float Progress = bCoolingDown ? Grapple->GetCooldownProgress() : 1;
	// 선 조각 대신 면을 이어 그리고 가장자리를 부드럽게 처리
	auto DrawArc = [&](float Fraction, float Width, const FLinearColor& Tint)
	{
		constexpr int32 Segments = 192;
		const float End = FMath::Clamp(Fraction, 0.0f, 1.0f) * 2 * PI;
		if (End <= 0) return;
		const float Feather = FMath::Min(0.75f, Width * 0.25f);
		const float Radii[] = { Radius - Width * 0.5f - Feather, Radius - Width * 0.5f + Feather,
			Radius + Width * 0.5f - Feather, Radius + Width * 0.5f + Feather };
		const float Alpha[] = { 0, 1, 1, 0 };
		FCanvasTriangleItem Item(FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, GWhiteTexture);
		Item.TriangleList.Reset();
		Item.TriangleList.Reserve(Segments * 6);
		Item.BlendMode = SE_BLEND_Translucent;
		auto AddTriangle = [&](FVector2D A, FVector2D B, FVector2D C, float AAlpha, float BAlpha, float CAlpha)
		{
			FCanvasUVTri Tri;
			Tri.V0_Pos = A; Tri.V1_Pos = B; Tri.V2_Pos = C;
			Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = Tint;
			Tri.V0_Color.A *= AAlpha; Tri.V1_Color.A *= BAlpha; Tri.V2_Color.A *= CAlpha;
			Item.TriangleList.Add(Tri);
		};
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const float Start = 2 * PI * Index / Segments;
			if (Start >= End) break;
			const float Next = FMath::Min(2 * PI * (Index + 1) / Segments, End);
			const FVector2D A(FMath::Cos(Start - HALF_PI), FMath::Sin(Start - HALF_PI));
			const FVector2D B(FMath::Cos(Next - HALF_PI), FMath::Sin(Next - HALF_PI));
			for (int32 Band = 0; Band < 3; ++Band)
			{
				const FVector2D InnerA = Center + A * Radii[Band], OuterA = Center + A * Radii[Band + 1];
				const FVector2D InnerB = Center + B * Radii[Band], OuterB = Center + B * Radii[Band + 1];
				AddTriangle(InnerA, OuterA, OuterB, Alpha[Band], Alpha[Band + 1], Alpha[Band + 1]);
				AddTriangle(InnerA, OuterB, InnerB, Alpha[Band], Alpha[Band + 1], Alpha[Band]);
			}
		}
		Canvas->DrawItem(Item);
	};
	DrawArc(1, 4.5f * Scale, FLinearColor(0.015f, 0.025f, 0.04f, 0.85f));
	DrawArc(Progress, (2.3f + Flash * 0.7f) * Scale, bCoolingDown ? Ready : Color);
	// 중앙은 작은 점만 표시해 원 안쪽의 여백 유지
	Canvas->K2_DrawPolygon(nullptr, Center, FVector2D(3.2f * Scale), 24, FLinearColor(0.015f, 0.025f, 0.04f, 0.85f));
	Canvas->K2_DrawPolygon(nullptr, Center, FVector2D(1.8f * Scale), 24, Color);
}

void APlayerHUD::UpdatePlayerUI()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController()) return;
	APlayerBase* Player = Cast<APlayerBase>(PC->GetPawn());
	if (ObservedPlayer.Get() != Player)
	{
		bGrappleWasCoolingDown = false;
		GrappleReadyFlashEnd = 0;
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

void APlayerHUD::SetPlayerUIVisible(bool bVisible)
{
	bShowHUD = bVisible;
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
