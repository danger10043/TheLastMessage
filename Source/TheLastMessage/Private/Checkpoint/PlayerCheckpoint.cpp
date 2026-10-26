#include "Checkpoint/PlayerCheckpoint.h"
#include "Character/Player/PlayerBase.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

APlayerCheckpoint::APlayerCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(100, 100, 120));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &APlayerCheckpoint::OnTriggerOverlap);
	RespawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RespawnPoint"));
	RespawnPoint->SetupAttachment(Trigger);
	RespawnArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("RespawnArrow"));
	RespawnArrow->SetupAttachment(RespawnPoint);
	RespawnArrow->ArrowColor = FColor::Green;
	TerminalRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TerminalRoot"));
	TerminalRoot->SetupAttachment(Trigger);
	// Keep the capsule-center checkpoint origin; put the prop on the floor behind it.
	TerminalRoot->SetRelativeLocation(FVector(0, -80, -90));
	TerminalBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerminalBody"));
	TerminalBody->SetupAttachment(TerminalRoot);
	TerminalBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TerminalBody->SetGenerateOverlapEvents(false);
	TerminalBody->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyAsset(TEXT("/Game/Main/Checkpoints/CommunicationTerminal/Meshes/SM_CP_Terminal.SM_CP_Terminal"));
	TerminalBody->SetStaticMesh(BodyAsset.Object);
	auto LoadMaterialSet = [](const TCHAR* State, FCheckpointSignalMaterials& Set)
	{
		auto Load = [State](const TCHAR* Slot) -> UMaterialInterface*
		{
			const FString Name = FString::Printf(TEXT("MI_CP_%s_%s"), State, Slot);
			const FString Path = FString::Printf(TEXT("/Game/Main/Checkpoints/CommunicationTerminal/Materials/%s.%s"), *Name, *Name);
			ConstructorHelpers::FObjectFinder<UMaterialInterface> Asset(*Path);
			return Asset.Object;
		};
		Set.Screen = Load(TEXT("Screen"));
		Set.Ring = Load(TEXT("Ring"));
		Set.Core = Load(TEXT("Core"));
		Set.Beacon = Load(TEXT("Beacon"));
		Set.BeaconBand = Load(TEXT("BeaconBand"));
	};
	LoadMaterialSet(TEXT("Inactive"), InactiveMaterials);
	LoadMaterialSet(TEXT("Current"), CurrentMaterials);
	LoadMaterialSet(TEXT("Visited"), VisitedMaterials);
}

void APlayerCheckpoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshTerminalVisuals();
}

void APlayerCheckpoint::BeginPlay()
{
	Super::BeginPlay();
	RefreshTerminalVisuals();
}

void APlayerCheckpoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APlayerCheckpoint, CheckpointVisualState);
}

void APlayerCheckpoint::SetCheckpointVisualActive(bool bActive)
{
	SetCheckpointVisualState(bActive ? ECheckpointVisualState::Current : ECheckpointVisualState::Inactive);
}

void APlayerCheckpoint::SetCheckpointVisualState(ECheckpointVisualState NewState)
{
	if (!HasAuthority()) return;
	CheckpointVisualState = NewState;
	RefreshTerminalVisuals();
	ForceNetUpdate();
}

void APlayerCheckpoint::OnRep_VisualState()
{
	RefreshTerminalVisuals();
}

void APlayerCheckpoint::RefreshTerminalVisuals()
{
	bCheckpointActivated = CheckpointVisualState != ECheckpointVisualState::Inactive;
	const FCheckpointSignalMaterials& Set = CheckpointVisualState == ECheckpointVisualState::Current ? CurrentMaterials
		: (CheckpointVisualState == ECheckpointVisualState::Visited ? VisitedMaterials : InactiveMaterials);
	TerminalBody->SetMaterialByName(TEXT("Screen"), Set.Screen);
	TerminalBody->SetMaterialByName(TEXT("Ring"), Set.Ring);
	TerminalBody->SetMaterialByName(TEXT("Core"), Set.Core);
	TerminalBody->SetMaterialByName(TEXT("Beacon"), Set.Beacon);
	TerminalBody->SetMaterialByName(TEXT("BeaconBand"), Set.BeaconBand);
}

bool APlayerCheckpoint::ActivateCheckpoint(APlayerBase* Player)
{
	if (!HasAuthority() || !IsValid(Player) || Player->GetWorld() != GetWorld() || !Player->IsPlayerControlled()) return false;
	if (!Player->SetCheckpoint(RespawnPoint->GetComponentTransform())) return false;
	// The shared world display has one current respawn beacon; earlier visits stay blue.
	for (TActorIterator<APlayerCheckpoint> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->CheckpointVisualState == ECheckpointVisualState::Current)
			It->SetCheckpointVisualState(ECheckpointVisualState::Visited);
	}
	SetCheckpointVisualState(ECheckpointVisualState::Current);
	OnCheckpointActivated.Broadcast(Player);
	return true;
}

void APlayerCheckpoint::OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APlayerBase* Player = Cast<APlayerBase>(OtherActor);
	if (Player && OtherComp == Player->GetCapsuleComponent()) ActivateCheckpoint(Player);
}
