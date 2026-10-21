#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_MeleeHit.h"
#include "Animation/AnimNotifyState_MeleeTrail.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraRibbonRendererProperties.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMeleeTrailAssetsTest, "TheLastMessage.Weapon.MeleeTrailAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMeleeTrailAssetsTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : { TEXT("Pipe"), TEXT("Hammer") })
	{
		const FString Path = FString(TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_")) + Name;
		UWeaponDataAsset* Data = LoadObject<UWeaponDataAsset>(nullptr, *Path);
		if (!TestNotNull(TEXT("Melee weapon exists"), Data)) continue;
		UNiagaraSystem* Trail = Data->MeleeTrailEffect;
		if (!TestNotNull(TEXT("Melee weapon has its own ribbon asset"), Trail)) continue;
#if WITH_EDITOR
		// Imported systems can require a deferred compile after engine-version migration
		Trail->RequestCompile(false);
		Trail->WaitForCompilationComplete(true, false);
#endif
		TestTrue(TEXT("Ribbon Niagara compiles successfully"), Trail->IsValid());
		TestEqual(TEXT("Ribbon has one emitter without smoke"), Trail->GetEmitterHandles().Num(), 1);
		for (const FNiagaraEmitterHandle& Handle : Trail->GetEmitterHandles())
		{
			FVersionedNiagaraEmitterData* Emitter = Handle.GetEmitterData();
			if (!TestNotNull(TEXT("Ribbon emitter exists"), Emitter)) continue;
			TestFalse(TEXT("Old trail points remain in world space"), Emitter->bLocalSpace);
			TestEqual(TEXT("Only a ribbon renderer is present"), Emitter->GetRenderers().Num(), 1);
			if (Emitter->GetRenderers().Num() == 1)
			{
				const auto* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Emitter->GetRenderer(0));
				if (TestNotNull(TEXT("Renderer is a ribbon rather than a sprite"), Ribbon))
					TestNotNull(TEXT("Ribbon has a fade material"), Ribbon->Material.Get());
			}
		}
		TestTrue(TEXT("Trail is gated by animation window"), Data->bUseMeleeTrailNotifyWindow);
		TestFalse(TEXT("Trail is attached near the weapon tip"), Data->MeleeTrailOffset.GetLocation().IsNearlyZero());
		if (!TestNotNull(TEXT("Weapon has attack montage"), Data->AttackMontage.Get())) continue;
		int32 HitCount = 0, WindowCount = 0;
		for (const FAnimNotifyEvent& Event : Data->AttackMontage->Notifies)
		{
			if (Event.Notify && Event.Notify->IsA<UAnimNotify_MeleeHit>()) ++HitCount;
			if (Event.NotifyStateClass && Event.NotifyStateClass->IsA<UAnimNotifyState_MeleeTrail>())
			{
				++WindowCount;
				TestTrue(TEXT("Trail window has a short positive duration"), Event.GetDuration() > 0 && Event.GetDuration() <= 0.4f);
				TestTrue(TEXT("Trail window stays within montage"), Event.GetTriggerTime() >= 0 && Event.GetEndTriggerTime() <= Data->AttackMontage->GetPlayLength());
			}
		}
		TestTrue(TEXT("Montage has hit notifications"), HitCount > 0);
		TestEqual(TEXT("Every strike has exactly one trail window"), WindowCount, HitCount);
	}
	return true;
}
#endif
