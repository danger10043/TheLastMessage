#include "Framework/CutsceneFreezeSubsystem.h"
#include "Character/Enemy/EnemyBase.h"
#include "Character/Enemy/EnemyProjectile.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UCutsceneFreezeSubsystem* UCutsceneFreezeSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UCutsceneFreezeSubsystem>() : nullptr;
}

void UCutsceneFreezeSubsystem::SetFreezeSource(FName Source, bool bActive)
{
	if (bActive == Sources.Contains(Source))
	{
		return;
	}

	const bool bWasFrozen = IsFrozen();
	if (bActive)
	{
		Sources.Add(Source);
	}
	else
	{
		Sources.Remove(Source);
	}

	if (bWasFrozen != IsFrozen())
	{
		ApplyToWorld(IsFrozen());
	}
}

void UCutsceneFreezeSubsystem::ApplyToWorld(bool bFrozen)
{
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		It->SetCutsceneFrozen(bFrozen);
	}

	if (bFrozen)
	{
		// 날아가던 투사체가 멈춰 있다가 연출이 끝난 뒤 플레이어에게 맞지 않도록 없앤다.
		for (TActorIterator<AEnemyProjectile> It(World); It; ++It)
		{
			It->Destroy();
		}
	}
}
