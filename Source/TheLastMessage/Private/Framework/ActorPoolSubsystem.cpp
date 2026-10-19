#include "Framework/ActorPoolSubsystem.h"
#include "Interface/PoolableInterface.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

AActor* UActorPoolSubsystem::AcquireActor(UClass* ActorClass, const FTransform& Transform, AActor* Owner, APawn* Instigator)
{
	UWorld* World = GetWorld();
	if (!ActorClass || !World)
	{
		return nullptr;
	}

	AActor* Actor = nullptr;

	// 풀에 남아 있는 액터를 꺼낸다(이미 파괴된 것은 건너뛴다).
	if (FPooledActorList* List = FreeLists.Find(ActorClass))
	{
		while (!Actor && List->Actors.Num() > 0)
		{
			AActor* Candidate = List->Actors.Pop();
			if (IsValid(Candidate))
			{
				Actor = Candidate;
			}
		}
	}

	if (Actor)
	{
		Actor->SetOwner(Owner);
		Actor->SetInstigator(Instigator);

		// SpawnActor와 같은 규칙: 요청한 스케일이 기본값(1)이면 액터의 기존(BP) 스케일을 유지한다.
		FTransform FinalTransform = Transform;
		if (Transform.GetScale3D().Equals(FVector::OneVector))
		{
			FinalTransform.SetScale3D(Actor->GetActorScale3D());
		}
		Actor->SetActorTransform(FinalTransform, false, nullptr, ETeleportType::TeleportPhysics);
		Actor->SetActorHiddenInGame(false);
		Actor->SetActorEnableCollision(true);
		Actor->SetActorTickEnabled(Actor->PrimaryActorTick.bCanEverTick);
	}
	else
	{
		FActorSpawnParameters Params;
		Params.Owner = Owner;
		Params.Instigator = Instigator;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		Actor = World->SpawnActor<AActor>(ActorClass, Transform, Params);
	}

	if (!Actor)
	{
		return nullptr;
	}

	if (IPoolableInterface* Poolable = Cast<IPoolableInterface>(Actor))
	{
		Poolable->OnAcquiredFromPool();
	}

	return Actor;
}

void UActorPoolSubsystem::ReleaseActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	FPooledActorList& List = FreeLists.FindOrAdd(Actor->GetClass());
	if (List.Actors.Contains(Actor))
	{
		return; // 이미 반환됨
	}

	if (List.Actors.Num() >= MaxPoolSizePerClass)
	{
		Actor->Destroy();
		return;
	}

	if (IPoolableInterface* Poolable = Cast<IPoolableInterface>(Actor))
	{
		Poolable->OnReturnedToPool();
	}

	Actor->SetActorHiddenInGame(true);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);
	Actor->SetOwner(nullptr);

	List.Actors.Add(Actor);
}

void UActorPoolSubsystem::Prewarm(UClass* ActorClass, int32 Count)
{
	UWorld* World = GetWorld();
	if (!ActorClass || !World)
	{
		return;
	}

	for (int32 i = 0; i < Count; ++i)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (AActor* Actor = World->SpawnActor<AActor>(ActorClass, FTransform::Identity, Params))
		{
			ReleaseActor(Actor);
		}
	}
}

void UActorPoolSubsystem::Deinitialize()
{
	FreeLists.Empty();
	Super::Deinitialize();
}
