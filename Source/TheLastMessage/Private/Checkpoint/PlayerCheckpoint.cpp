#include "Checkpoint/PlayerCheckpoint.h"
#include "Character/Player/PlayerBase.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Save/ProgressSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/SecureHash.h"

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
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &APlayerCheckpoint::OnTriggerOverlap);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &APlayerCheckpoint::OnTriggerEndOverlap);
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
	InteractionOutline = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InteractionOutline"));
	InteractionOutline->SetupAttachment(TerminalBody);
	InteractionOutline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionOutline->SetGenerateOverlapEvents(false);
	InteractionOutline->SetCanEverAffectNavigation(false);
	InteractionOutline->SetCastShadow(false);
	InteractionOutline->SetVisibility(false);

}

void APlayerCheckpoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshTerminalVisuals();
}

void APlayerCheckpoint::BeginPlay()
{
	Super::BeginPlay();
	SetInteractionHighlight(false);
	if (GetGameInstance()) GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>()->ApplyCheckpointState(this);
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
	InteractionOutline->SetStaticMesh(TerminalBody->GetStaticMesh());
	for (int32 Index = 0; Index < InteractionOutline->GetNumMaterials(); ++Index)
		InteractionOutline->SetMaterial(Index, InteractionOutlineMaterial);
	bCheckpointActivated = CheckpointVisualState != ECheckpointVisualState::Inactive;
	const FCheckpointSignalMaterials& Set = CheckpointVisualState == ECheckpointVisualState::Current ? CurrentMaterials
		: (CheckpointVisualState == ECheckpointVisualState::Visited ? VisitedMaterials : InactiveMaterials);
	if (Set.Screen) TerminalBody->SetMaterialByName(TEXT("Screen"), Set.Screen);
	if (Set.Ring) TerminalBody->SetMaterialByName(TEXT("Ring"), Set.Ring);
	if (Set.Core) TerminalBody->SetMaterialByName(TEXT("Core"), Set.Core);
	if (Set.Beacon) TerminalBody->SetMaterialByName(TEXT("Beacon"), Set.Beacon);
	if (Set.BeaconBand) TerminalBody->SetMaterialByName(TEXT("BeaconBand"), Set.BeaconBand);
}

bool APlayerCheckpoint::ActivateCheckpoint(APlayerBase* Player)
{
	if (GetGameInstance() && GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>()->IsRestoringProgress()) return false;
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
	if (UGameInstance* Instance = GetGameInstance())
		Instance->GetSubsystem<UProgressSaveSubsystem>()->SaveCheckpoint(Player, this);
	return true;
}

FGuid APlayerCheckpoint::GetPersistentId() const
{
	if (PersistentId.IsValid()) return PersistentId;
	// Placed actor paths are stable across sessions. Strip the editor's PIE prefix.
	FGuid Result;
	FGuid::ParseExact(FMD5::HashAnsiString(*UWorld::RemovePIEPrefix(GetPathName())), EGuidFormats::Digits, Result);
	return Result;
}

void APlayerCheckpoint::OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APlayerBase* Player = Cast<APlayerBase>(OtherActor);
	if (Player && OtherComp == Player->GetCapsuleComponent())
	{
		if (Player->IsLocallyControlled())
		{
			TouchingPlayers.Add(Player);
			SetInteractionHighlight(true);
		}
		ActivateCheckpoint(Player);
	}
}

void APlayerCheckpoint::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APlayerBase* Player = Cast<APlayerBase>(OtherActor);
	if (!Player || OtherComp != Player->GetCapsuleComponent()) return;
	TouchingPlayers.Remove(Player);
	bool bTouching = false;
	for (const TWeakObjectPtr<APlayerBase>& TouchingPlayer : TouchingPlayers)
		bTouching |= TouchingPlayer.IsValid() && TouchingPlayer->IsLocallyControlled();
	SetInteractionHighlight(bTouching);
}

void APlayerCheckpoint::SetInteractionHighlight(bool bVisible)
{
	InteractionOutline->SetVisibility(bVisible && InteractionOutlineMaterial && TerminalBody->GetStaticMesh());
	for (int32 Index = 0; Index < InteractionOutline->GetNumMaterials(); ++Index)
	{
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(InteractionOutline->GetMaterial(Index));
		if (!Material) Material = InteractionOutline->CreateDynamicMaterialInstance(Index);
		if (Material) Material->SetScalarParameterValue(TEXT("InteractionGlow"), bVisible ? 1.f : 0.f);
	}
}
