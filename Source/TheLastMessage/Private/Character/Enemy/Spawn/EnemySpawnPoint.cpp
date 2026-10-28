#include "Character/Enemy/Spawn/EnemySpawnPoint.h"
#include "Character/Enemy/Spawn/EnemySpawnSubsystem.h"
#include "Framework/ActorPoolSubsystem.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"

AEnemySpawnPoint::AEnemySpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	DirectionArrow->SetupAttachment(Root);
	DirectionArrow->ArrowSize = 1.5f;
	DirectionArrow->bHiddenInGame = true;

	RangeBox = CreateDefaultSubobject<UBoxComponent>(TEXT("RangeBox"));
	RangeBox->SetupAttachment(Root);
	RangeBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RangeBox->SetGenerateOverlapEvents(false);
	RangeBox->bHiddenInGame = true;
	RangeBox->ShapeColor = FColor::Orange;
}

void AEnemySpawnPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서 스폰 범위를 박스로 보여 준다. (Y는 고정이라 얇게 표시)
	RangeBox->SetBoxExtent(FVector(FMath::Max(SpawnRangeX, 10.f), 10.f, FMath::Max(SpawnRangeZ, 10.f)));
}

void AEnemySpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	if (UEnemySpawnSubsystem* Subsystem = GetWorld()->GetSubsystem<UEnemySpawnSubsystem>())
	{
		Subsystem->RegisterSpawnPoint(this);
	}

	// 거리 기반이면 서브시스템이 플레이어가 가까워졌을 때 스폰한다.
	if (!bStreamByDistance)
	{
		SpawnEnemies();
	}
}

void AEnemySpawnPoint::UpdateStreaming(const FVector& PlayerLocation)
{
	if (!bStreamByDistance)
	{
		return;
	}

	// 사이드뷰라 Y(깊이)는 무시하고 X(좌우)·Z(높이)만 본다.
	const FVector Delta = PlayerLocation - GetActorLocation();
	const float Distance = FMath::Sqrt(FMath::Square(Delta.X) + FMath::Square(Delta.Z));

	const float Spawn = SpawnRadius;
	const float Despawn = FMath::Max(DespawnRadius, SpawnRadius);

	if (Slots.Num() == 0)
	{
		if (Distance <= Spawn)
		{
			SpawnEnemies();
			bResetArmed = false;
		}
		return;
	}

	if (Distance > Despawn)
	{
		DespawnEnemies();
		bResetArmed = false;
		return;
	}

	if (Distance <= ResetRadius)
	{
		bResetArmed = true;
	}
	else if (bResetArmed)
	{
		bResetArmed = false;
		ResetSpawn();
	}
}

void AEnemySpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UEnemySpawnSubsystem* Subsystem = World->GetSubsystem<UEnemySpawnSubsystem>())
		{
			Subsystem->UnregisterSpawnPoint(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AEnemySpawnPoint::BuildSlots()
{
	Slots.Reset();

	// 목록의 종류별 마릿수만큼 항목을 펼친다.
	TArray<int32> EntryIndices;
	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FEnemySpawnEntry& Entry = Entries[EntryIndex];
		if (!Entry.EnemyClass)
		{
			continue;
		}

		for (int32 Count = 0; Count < FMath::Max(Entry.Count, 1); ++Count)
		{
			EntryIndices.Add(EntryIndex);
		}
	}

	const int32 Total = EntryIndices.Num();
	if (Total == 0)
	{
		return;
	}

	// 종류가 한쪽으로 몰리지 않게 순서를 섞는다.
	for (int32 i = Total - 1; i > 0; --i)
	{
		const int32 SwapIndex = FMath::RandRange(0, i);
		if (SwapIndex != i)
		{
			EntryIndices.Swap(i, SwapIndex);
		}
	}

	// 범위를 마릿수만큼 칸으로 나누고 칸 안에서만 무작위로 뽑아 최소 간격을 지킨다.
	const float TotalWidth = SpawnRangeX * 2.f;
	const float SlotWidth = TotalWidth / Total;
	const float Margin = FMath::Min(MinSpacing * 0.5f, SlotWidth * 0.5f);

	if (SlotWidth < MinSpacing)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemySpawnPoint] %s: 범위(%.0f)가 좁아 최소 간격(%.0f)을 지킬 수 없습니다. 몬스터 %d마리."),
			*GetName(), TotalWidth, MinSpacing, Total);
	}

	const FVector Origin = GetActorLocation();
	for (int32 Index = 0; Index < Total; ++Index)
	{
		const FEnemySpawnEntry& Entry = Entries[EntryIndices[Index]];

		const float SlotStart = -SpawnRangeX + SlotWidth * Index;
		const float OffsetX = FMath::FRandRange(SlotStart + Margin, SlotStart + SlotWidth - Margin);
		const float OffsetZ = SpawnRangeZ > 0.f ? FMath::FRandRange(-SpawnRangeZ, SpawnRangeZ) : 0.f;

		FSpawnSlot& Slot = Slots.AddDefaulted_GetRef();
		Slot.EnemyClass = Entry.EnemyClass;
		Slot.Level = Entry.Level;
		Slot.Location = Origin + FVector(OffsetX, 0.f, OffsetZ); // Y는 스폰 포인트 값으로 고정
	}
}

void AEnemySpawnPoint::SpawnSlot(FSpawnSlot& Slot)
{
	UWorld* World = GetWorld();
	if (!Slot.EnemyClass || !World)
	{
		return;
	}

	const FTransform SpawnTransform(GetActorRotation(), Slot.Location);

	// 풀에서 꺼낸다. 풀이 없으면 직접 스폰한다.
	AEnemyBase* Enemy = nullptr;
	if (UActorPoolSubsystem* Pool = World->GetSubsystem<UActorPoolSubsystem>())
	{
		Enemy = Cast<AEnemyBase>(Pool->AcquireActor(Slot.EnemyClass, SpawnTransform));
	}
	else
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Enemy = World->SpawnActor<AEnemyBase>(Slot.EnemyClass, SpawnTransform, Params);
	}

	if (!Enemy)
	{
		return;
	}

	// 초기화(체력 가득)가 끝난 뒤 레벨을 적용한다.
	Enemy->SetEnemyLevel(Slot.Level);
	Enemy->OnEnemyDied.AddUniqueDynamic(this, &AEnemySpawnPoint::HandleSpawnedEnemyDied);
	Enemy->OnEnemyReleased.AddUniqueDynamic(this, &AEnemySpawnPoint::HandleSpawnedEnemyReleased);

	Slot.Enemy = Enemy;
	Slot.bSpawned = true;
}

void AEnemySpawnPoint::SpawnEnemies()
{
	if (Slots.Num() > 0)
	{
		return; // 이미 스폰됨
	}

	BuildSlots();
	for (FSpawnSlot& Slot : Slots)
	{
		SpawnSlot(Slot);
	}
}

void AEnemySpawnPoint::DespawnEnemies()
{
	for (FSpawnSlot& Slot : Slots)
	{
		ReleaseSlotEnemy(Slot);
	}

	Slots.Reset();
}

void AEnemySpawnPoint::ResetSpawn()
{
	switch (ResetMode)
	{
	case EEnemyResetMode::RespawnAll:
		// 모두 제거하고 위치를 새로 무작위로 정해 다시 스폰한다.
		DespawnEnemies();
		SpawnEnemies();
		break;

	case EEnemyResetMode::RespawnDeadOnly:
		if (Slots.Num() == 0)
		{
			SpawnEnemies();
			break;
		}

		// 살아 있는 몬스터는 그대로 두고, 죽은 몬스터만 원래 자리에 다시 스폰한다.
		for (FSpawnSlot& Slot : Slots)
		{
			const AEnemyBase* Enemy = Slot.Enemy.Get();
			if (Enemy && !Enemy->IsDead() && !Enemy->IsPooled())
			{
				continue;
			}

			ReleaseSlotEnemy(Slot); // 아직 남아 있는 시체는 풀로 돌려보낸다.
			SpawnSlot(Slot);
		}
		break;

	case EEnemyResetMode::None:
		break;
	}
}

void AEnemySpawnPoint::ResetForPlayerRespawn()
{
	DespawnEnemies();
	bResetArmed = false;

	// 거리 기반이 아닌 구역은 서브시스템이 다시 스폰해 주지 않으므로 여기서 새로 스폰한다.
	if (!bStreamByDistance)
	{
		SpawnEnemies();
	}
}

int32 AEnemySpawnPoint::GetAliveEnemyCount() const
{
	int32 Alive = 0;
	for (const FSpawnSlot& Slot : Slots)
	{
		const AEnemyBase* Enemy = Slot.Enemy.Get();
		if (Enemy && !Enemy->IsDead() && !Enemy->IsPooled())
		{
			++Alive;
		}
	}
	return Alive;
}

void AEnemySpawnPoint::ReleaseSlotEnemy(FSpawnSlot& Slot)
{
	if (AEnemyBase* Enemy = Slot.Enemy.Get())
	{
		Enemy->OnEnemyDied.RemoveDynamic(this, &AEnemySpawnPoint::HandleSpawnedEnemyDied);
		Enemy->OnEnemyReleased.RemoveDynamic(this, &AEnemySpawnPoint::HandleSpawnedEnemyReleased);
		Enemy->ReturnToPool(); // 이미 풀에 있으면 아무 일도 하지 않는다.
	}

	Slot.Enemy.Reset();
	Slot.bSpawned = false;
}

void AEnemySpawnPoint::HandleSpawnedEnemyDied(AEnemyBase* Enemy)
{
	OnSpawnedEnemyDied.Broadcast(Enemy);
}

void AEnemySpawnPoint::HandleSpawnedEnemyReleased(AEnemyBase* Enemy)
{
	// 풀로 돌아간 몬스터를 계속 들고 있으면 다른 구역이 재사용했을 때 잘못 참조한다.
	for (FSpawnSlot& Slot : Slots)
	{
		if (Slot.Enemy.Get() == Enemy)
		{
			Slot.Enemy.Reset();
		}
	}
}
