#include "Map/AreaMapRegion.h"
#include "Map/AreaMapComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AAreaMapRegion::AAreaMapRegion()
{
	RegionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("RegionBounds"));
	SetRootComponent(RegionBounds);
	RegionBounds->SetBoxExtent(FVector(1200,500,700));
	RegionBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RegionBounds->SetGenerateOverlapEvents(false);
	RegionBounds->SetCanEverAffectNavigation(false);
	RegionBounds->SetHiddenInGame(true);

	// 마스크를 쓰는 방만 BeginPlay에서 틱을 켠다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded()) MaskMesh = PlaneMesh.Object;
}

void AAreaMapRegion::BeginPlay()
{
	Super::BeginPlay();
	if (!bUseMask) return;

	if (!MaskMesh || !MaskMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AreaMapRegion] %s: Use Mask가 켜져 있지만 Mask Material=%s, Mask Mesh=%s 입니다. 마스크를 만들지 못했습니다."),
			*GetName(), MaskMaterial ? TEXT("있음") : TEXT("없음"), MaskMesh ? TEXT("있음") : TEXT("없음"));
		return;
	}

	MaskComponent = NewObject<UStaticMeshComponent>(this, TEXT("RegionMask"));
	MaskComponent->SetStaticMesh(MaskMesh);
	MaskComponent->SetMobility(EComponentMobility::Movable);
	MaskComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MaskComponent->SetGenerateOverlapEvents(false);
	MaskComponent->SetCanEverAffectNavigation(false);
	MaskComponent->SetCastShadow(false);
	MaskComponent->bReceivesDecals = false;
	// 방 박스의 크기나 회전에 영향받지 않고 월드 좌표로 직접 배치한다.
	MaskComponent->SetUsingAbsoluteLocation(true);
	MaskComponent->SetUsingAbsoluteRotation(true);
	MaskComponent->SetUsingAbsoluteScale(true);
	MaskComponent->SetupAttachment(GetRootComponent());
	MaskComponent->RegisterComponent();
	MaskMaterialInstance = MaskComponent->CreateDynamicMaterialInstance(0, MaskMaterial);

	// 머티리얼에 투명도 파라미터가 없으면 서서히 사라지지 않고 한 번에 사라진다.
	float ParameterValue = 0.f;
	if (!MaskMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(MaskOpacityParameter), ParameterValue))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AreaMapRegion] %s: Mask Material에 스칼라 파라미터 '%s'이(가) 없습니다. 이 파라미터를 Opacity에 연결하지 않으면 막이 서서히 걷히지 않고 한 번에 사라집니다."),
			*GetName(), *MaskOpacityParameter.ToString());
	}

	MaskAlpha = 1.f;
	bMaskInitialized = false;
	SetActorTickEnabled(true);
}

void AAreaMapRegion::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateMask(DeltaTime);
}

void AAreaMapRegion::UpdateMask(float DeltaTime)
{
	if (!MaskComponent || !MaskMaterialInstance) return;
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pawn) return;

	const FVector PlayerLocation = Pawn->GetActorLocation();
	const FBox2D Bounds = GetMapBounds();

	// 방 안에 있으면 걷고, 밖이면 덮는다. 다시 덮지 않는 설정이면 한 번 탐사한 방은 계속 걷어 둔다.
	bool bOpen = Bounds.ExpandBy(FVector2D(MaskOpenMargin, MaskOpenMargin)).IsInside(FVector2D(PlayerLocation.X, PlayerLocation.Z));
	if (!bOpen && !bCoverAgainWhenLeft && !RegionId.IsNone())
	{
		if (const UAreaMapComponent* Map = Pawn->FindComponentByClass<UAreaMapComponent>()) bOpen = Map->IsExplored(RegionId);
	}
	const float Target = bOpen ? 0.f : 1.f;

	// 막의 위치와 크기. 플레이어가 서 있는 평면에 놓고, 카메라 쪽으로 옮긴 만큼 같은 화면 영역을 덮도록 크기를 줄인다.
	const FBox2D Padded = Bounds.ExpandBy(FVector2D(MaskPadding, MaskPadding));
	FVector2D Min = Padded.Min, Max = Padded.Max;
	const double PlaneY = PlayerLocation.Y + MaskOffsetY;
	if (!FMath::IsNearlyZero(MaskOffsetY))
	{
		if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const FVector CameraLocation = Camera->GetCameraLocation();
			const double RegionDepth = FMath::Abs(CameraLocation.Y - PlayerLocation.Y);
			const double MaskDepth = FMath::Max(FMath::Abs(CameraLocation.Y - PlaneY), 10.0);
			if (RegionDepth > 10.0)
			{
				const double Scale = MaskDepth / RegionDepth;
				const FVector2D CameraPoint(CameraLocation.X, CameraLocation.Z);
				Min = CameraPoint + (Min - CameraPoint) * Scale;
				Max = CameraPoint + (Max - CameraPoint) * Scale;
			}
		}
	}
	const FVector2D Size = Max - Min;
	const FVector2D Center = (Min + Max) * 0.5;
	// Plane을 세워서 카메라(+Y쪽)를 향하게 한다. Y 스케일이 세로(Z) 길이가 된다.
	MaskComponent->SetWorldLocationAndRotation(FVector(Center.X, PlaneY, Center.Y), FRotator(0.f, 0.f, -90.f));
	MaskComponent->SetWorldScale3D(FVector(FMath::Max(Size.X, 1.0) / 100.0, FMath::Max(Size.Y, 1.0) / 100.0, 1.0));

	// 처음에는 바로 맞추고, 이후에는 서서히 바꾼다.
	if (!bMaskInitialized)
	{
		MaskAlpha = Target;
		bMaskInitialized = true;
	}
	else
	{
		const float Duration = Target < MaskAlpha ? MaskRevealTime : MaskCoverTime;
		const float Speed = Duration > KINDA_SMALL_NUMBER ? 1.f / Duration : 1000.f;
		// 틱 간격이 길어도 한 번에 크게 뛰지 않게 시간을 제한한다.
		MaskAlpha = FMath::FInterpConstantTo(MaskAlpha, Target, FMath::Min(DeltaTime, 0.1f), Speed);
	}
	// 일정한 속도로 움직이는 값을 부드러운 곡선(처음과 끝이 느린)으로 바꿔서 투명도로 쓴다. 중간에 방향이 바뀌어도 끊김이 없다.
	const float Opacity = MaskAlpha * MaskAlpha * (3.f - 2.f * MaskAlpha);
	MaskMaterialInstance->SetScalarParameterValue(MaskOpacityParameter, Opacity);
	MaskComponent->SetVisibility(Opacity > 0.001f);

	// 변화가 없을 때는 갱신을 줄인다. 카메라에 맞춰 크기를 보정하는 경우에는 매 프레임 갱신한다.
	const bool bSettled = FMath::IsNearlyEqual(MaskAlpha, Target) && FMath::IsNearlyZero(MaskOffsetY);
	SetActorTickInterval(bSettled ? 0.05f : 0.f);
}

void AAreaMapRegion::CaptureLayoutBounds()
{
	AuthoredLayoutBounds.Init();
	for (const AActor* Actor : LayoutActors)
	{
		if (IsValid(Actor)) AuthoredLayoutBounds += Actor->GetComponentsBoundingBox(true);
	}
}

FBox2D AAreaMapRegion::GetMapBounds() const
{
	const FBox Base = RegionBounds->Bounds.GetBox();
	FBox Current(ForceInit);
	for (const AActor* Actor : LayoutActors) if (IsValid(Actor)) Current += Actor->GetComponentsBoundingBox(true);
	if (!Current.IsValid || !AuthoredLayoutBounds.IsValid)
		return FBox2D(FVector2D(Base.Min.X,Base.Min.Z),FVector2D(Base.Max.X,Base.Max.Z));
	const FVector ReferenceSize = AuthoredLayoutBounds.GetSize();
	const FVector CurrentSize = Current.GetSize();
	const double ScaleX = ReferenceSize.X > 1 ? CurrentSize.X / ReferenceSize.X : 1;
	const double ScaleZ = ReferenceSize.Z > 1 ? CurrentSize.Z / ReferenceSize.Z : 1;
	return FBox2D(FVector2D(Current.Min.X + (Base.Min.X-AuthoredLayoutBounds.Min.X)*ScaleX,
		Current.Min.Z + (Base.Min.Z-AuthoredLayoutBounds.Min.Z)*ScaleZ),
		FVector2D(Current.Min.X + (Base.Max.X-AuthoredLayoutBounds.Min.X)*ScaleX,
		Current.Min.Z + (Base.Max.Z-AuthoredLayoutBounds.Min.Z)*ScaleZ));
}
