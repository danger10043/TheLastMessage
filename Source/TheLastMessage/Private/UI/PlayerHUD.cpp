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
#include "UI/SaveNotificationWidget.h"
#include "Save/ProgressSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SViewport.h"
#include "Map/AreaMapComponent.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"
#include "UI/FactoryUIStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

APlayerHUD::APlayerHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SaveNotificationClass = USaveNotificationWidget::StaticClass();
	static ConstructorHelpers::FObjectFinder<UFont> TitleFont(TEXT("/Game/Main/UI/Fonts/F_TLM_Title"));
	RoomTitleFont = TitleFont.Object;
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
	if (!SaveNotification && SaveNotificationClass)
	{
		SaveNotification = CreateWidget<USaveNotificationWidget>(PC, SaveNotificationClass);
		if (SaveNotification) SaveNotification->AddToPlayerScreen(100);
		if (UGameInstance* Instance = GetGameInstance())
			Instance->GetSubsystem<UProgressSaveSubsystem>()->OnCheckpointSaveFinished.AddUniqueDynamic(this, &APlayerHUD::CheckpointSaveFinished);
	}
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
	DrawDamageVignette();
	DrawGrappleIndicator();
	DrawRoomTitle();
}

void APlayerHUD::PlayDamageVignette()
{
	const APlayerController* PC = GetOwningPlayerController();
	if (!GetWorld() || !PC || !PC->IsLocalController() || !PC->GetPawn()) return;
	DamageVignettePawn = PC->GetPawn();
	DamageVignetteStartTime = GetWorld()->GetRealTimeSeconds();
}

void APlayerHUD::DrawDamageVignette()
{
	const APlayerController* PC = GetOwningPlayerController();
	if (!Canvas || !GetWorld() || !PC || !bShowHUD || DamageVignetteStartTime < 0.0) return;
	if (!DamageVignettePawn.IsValid() || DamageVignettePawn.Get() != PC->GetPawn())
	{
		DamageVignetteStartTime = -1.0;
		return;
	}
	// Real time keeps the flash short even during combat hit stop.
	const float Age = GetWorld()->GetRealTimeSeconds() - DamageVignetteStartTime;
	constexpr float Duration = 0.45f;
	if (Age >= Duration)
	{
		DamageVignetteStartTime = -1.0;
		return;
	}
	const float Fade = 1.0f - FMath::Clamp((Age - 0.035f) / (Duration - 0.035f), 0.0f, 1.0f);
	const float Opacity = 0.65f * Fade * Fade;
	// A small vertex-colored mesh gives soft edges without a texture or material asset.
	constexpr int32 Columns = 48, Rows = 32;
	FCanvasTriangleItem Item(FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, GWhiteTexture);
	Item.TriangleList.Reset();
	Item.BlendMode = SE_BLEND_Translucent;
	auto ColorAt = [Opacity](FVector2D UV)
	{
		const float Edge = FMath::Min(FMath::Min(UV.X, 1.0 - UV.X), FMath::Min(UV.Y, 1.0 - UV.Y));
		const float Mask = FMath::Clamp(1.0f - Edge / 0.13f, 0.0f, 1.0f);
		return FLinearColor(0.65f, 0.005f, 0.012f, Opacity * Mask * Mask);
	};
	auto AddTriangle = [&](FVector2D A, FVector2D B, FVector2D C)
	{
		const FLinearColor CA = ColorAt(A), CB = ColorAt(B), CC = ColorAt(C);
		if (CA.A + CB.A + CC.A <= 0.0f) return;
		FCanvasUVTri Tri;
		const FVector2D Size(Canvas->ClipX, Canvas->ClipY);
		Tri.V0_Pos = A * Size; Tri.V1_Pos = B * Size; Tri.V2_Pos = C * Size;
		Tri.V0_Color = CA; Tri.V1_Color = CB; Tri.V2_Color = CC;
		Item.TriangleList.Add(Tri);
	};
	for (int32 Y = 0; Y < Rows; ++Y)
	for (int32 X = 0; X < Columns; ++X)
	{
		const FVector2D A(double(X) / Columns, double(Y) / Rows);
		const FVector2D B(double(X + 1) / Columns, double(Y + 1) / Rows);
		AddTriangle(A, FVector2D(B.X, A.Y), B);
		AddTriangle(A, B, FVector2D(A.X, B.Y));
	}
	Canvas->DrawItem(Item);
}

void APlayerHUD::DrawRoomTitle()
{
	const APlayerController* PC = GetOwningPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UAreaMapComponent* Map = Pawn ? Pawn->FindComponentByClass<UAreaMapComponent>() : nullptr;
	if (RoomTitlePawn.Get() != Pawn)
	{
		RoomTitlePawn = Pawn;
		AnnouncedRoom = NAME_None;
		RoomTitleEndTime = 0.0;
	}
	if (!Map || !Canvas || !RoomTitleFont) return;
	const FName RoomId = Map->GetCurrentRoomId();
	const FAreaMapRoom* Room = Map->GetRooms().FindByPredicate([RoomId](const FAreaMapRoom& Candidate)
	{
		return Candidate.Id == RoomId && !Candidate.bAutomatic && !Candidate.Name.IsEmpty();
	});
	const double Now = GetWorld()->GetTimeSeconds();
	if (AnnouncedRoom != RoomId)
	{
		AnnouncedRoom = RoomId;
		RoomTitleEndTime = Room ? Now + 2.5 : 0.0;
	}
	if (!Room || !bShowHUD || Now >= RoomTitleEndTime) return;

	const float Alpha = FMath::Clamp((RoomTitleEndTime - Now) / 0.3, 0.0, 1.0);
	const float Scale = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.55f, 2.0f);
	const FSlateFontInfo Font(RoomTitleFont, 32 * Scale, TEXT("Regular"));
	FCanvasTextItem Title(FVector2D::ZeroVector, Room->Name, Font,
		FactoryUI::Text.CopyWithNewOpacity(Alpha));
	const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Room->Name, Font);
	const FVector2D Size = TextSize + FVector2D(66, 22) * Scale;
	const FVector2D Position((Canvas->SizeX - Size.X) * 0.5, Canvas->SizeY * 0.09);
	FCanvasTileItem Background(Position, Size, FactoryUI::Panel.CopyWithNewOpacity(0.92f * Alpha));
	Background.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Background);
	FCanvasTileItem Accent(Position + FVector2D(Size.X * 0.27, Size.Y), FVector2D(Size.X * 0.46, 2 * Scale),
		FactoryUI::Cyan.CopyWithNewOpacity(Alpha));
	Accent.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Accent);
	Title.Position = Position + (Size - TextSize) * 0.5;
	Canvas->DrawItem(Title);
}

bool APlayerHUD::ShouldUseGrappleCursor() const
{
	const APlayerController* PC = GetOwningPlayerController();
	const APlayerBase* Player = PC ? Cast<APlayerBase>(PC->GetPawn()) : nullptr;
	const UGrapplingHookComponent* Grapple = Player ? Player->FindComponentByClass<UGrapplingHookComponent>() : nullptr;
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const TSharedPtr<SViewport> ViewportWidget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
	// Interactive menus keep the normal cursor; the HUD itself does not intercept mouse hits.
	return bShowHUD && PC && PC->IsLocalController() && PC->bShowMouseCursor
		&& Player && !Player->IsDead() && Grapple && Grapple->IsGrappleUnlocked()
		&& !UGameplayStatics::IsGamePaused(this) && Viewport && !Viewport->IgnoreInput()
		&& ViewportWidget.IsValid() && ViewportWidget->IsDirectlyHovered();
}

void APlayerHUD::DrawGrappleIndicator()
{
	APlayerController* PC = GetOwningPlayerController();
	const APlayerBase* Player = PC ? Cast<APlayerBase>(PC->GetPawn()) : nullptr;
	const UGrapplingHookComponent* Grapple = Player ? Player->FindComponentByClass<UGrapplingHookComponent>() : nullptr;
	if (!Canvas || !ShouldUseGrappleCursor())
	{
		bGrappleWasCoolingDown = false;
		GrappleReadyFlashEnd = 0;
		return;
	}
	FVector2D Aim;
	if (!PC->GetMousePosition(Aim.X, Aim.Y)) return;
	if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
	{
		FVector2D ViewportSize;
		GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);
		Aim -= LocalPlayer->Origin * ViewportSize;
	}
	const float Scale = FMath::Clamp(Canvas->ClipY / 1080.0f, 0.65f, 2.0f);
	const float Radius = GrappleIndicatorRadius * Scale;
	const FVector2D Center = Aim;
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
	if (GetGameInstance()) GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>()->OnCheckpointSaveFinished.RemoveDynamic(this, &APlayerHUD::CheckpointSaveFinished);
	if (SaveNotification) SaveNotification->RemoveFromParent();
	SaveNotification = nullptr;
	if (StatusWidget) { StatusWidget->SetObservedPlayer(nullptr); StatusWidget->RemoveFromParent(); }
	if (ChargeWidget) ChargeWidget->RemoveFromParent();
	StatusWidget = nullptr;
	ChargeWidget = nullptr;
	ObservedPlayer.Reset();
	Super::EndPlay(EndPlayReason);
}

void APlayerHUD::CheckpointSaveFinished(bool bSuccess)
{
	if (SaveNotification)
	{
		const UProgressSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>();
		SaveNotification->ShowResult(bSuccess, Saves->GetActiveSlot() > 0);
	}
}
