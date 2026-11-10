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
#include "GameFramework/WorldSettings.h"
#include "Widgets/SViewport.h"
#include "Map/AreaMapComponent.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
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
	// 지금 클릭하면 훅이 실제로 걸리는 경우에만 청록 커서. 커서 조회는 프레임당 여러 번 올 수 있어 캐시한다.
	struct FGrappleCursorCache { TWeakObjectPtr<const APlayerHUD> Owner; uint64 Frame = MAX_uint64; double NextTime = 0; bool bReady = false; };
	static FGrappleCursorCache Cache;
	if (!ShouldUseGameplayCursor()) return false;
	const APlayerController* PC = GetOwningPlayerController();
	const APlayerBase* Player = PC ? Cast<APlayerBase>(PC->GetPawn()) : nullptr;
	const UGrapplingHookComponent* Grapple = Player ? Player->FindComponentByClass<UGrapplingHookComponent>() : nullptr;
	if (!Grapple) return false;
	const UWorld* World = GetWorld();
	const double Now = World->GetRealTimeSeconds();
	if (Cache.Owner.Get() == this && (Cache.Frame == GFrameCounter || Now < Cache.NextTime)) return Cache.bReady;
	// 충돌이 준비된 맵은 매 프레임, 숨은 장식 메시를 임시로 켜야 하는 맵은 비용이 커서 0.1초 간격
	const bool bAuthoredCollision = World->GetWorldSettings()->ActorHasTag(TEXT("GrappleCollisionAuthored"));
	Cache.Owner = this; Cache.Frame = GFrameCounter; Cache.NextTime = bAuthoredCollision ? 0.0 : Now + 0.1;
	Cache.bReady = Grapple->CanGrappleAt(Player->GetAimTargetLocation());
	return Cache.bReady;
}

bool APlayerHUD::ShouldUseGameplayCursor() const
{
	const APlayerController* PC = GetOwningPlayerController();
	const APlayerBase* Player = PC ? Cast<APlayerBase>(PC->GetPawn()) : nullptr;
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const TSharedPtr<SViewport> ViewportWidget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
	// Interactive menus keep the normal cursor; the HUD itself does not intercept mouse hits.
	return bShowHUD && PC && PC->IsLocalController() && PC->bShowMouseCursor
		&& Player && !Player->IsDead()
		&& !UGameplayStatics::IsGamePaused(this) && Viewport && !Viewport->IgnoreInput()
		&& ViewportWidget.IsValid();
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
