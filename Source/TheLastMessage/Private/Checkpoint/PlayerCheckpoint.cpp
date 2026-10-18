#include "Checkpoint/PlayerCheckpoint.h"
#include "Character/Player/PlayerBase.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"

APlayerCheckpoint::APlayerCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;
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
}

bool APlayerCheckpoint::ActivateCheckpoint(APlayerBase* Player)
{
	if (!HasAuthority() || !IsValid(Player) || Player->GetWorld() != GetWorld() || !Player->IsPlayerControlled()) return false;
	if (!Player->SetCheckpoint(RespawnPoint->GetComponentTransform())) return false;
	OnCheckpointActivated.Broadcast(Player);
	return true;
}

void APlayerCheckpoint::OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APlayerBase* Player = Cast<APlayerBase>(OtherActor);
	if (Player && OtherComp == Player->GetCapsuleComponent()) ActivateCheckpoint(Player);
}
