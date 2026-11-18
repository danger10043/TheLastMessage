#if WITH_DEV_AUTOMATION_TESTS
#include "Core/CoreRewardSession.h"

#include "AbilitySystemComponent.h"
#include "Core/CoreComponent.h"
#include "DataAsset/CoreDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerfectGuardRewardPrerequisiteTest,
	"TheLastMessage.Core.Rewards.PerfectGuardPrerequisite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPerfectGuardRewardPrerequisiteTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AActor* Owner = World->SpawnActor<AActor>();
	UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Owner);
	ASC->RegisterComponent();
	ASC->InitAbilityActorInfo(Owner, Owner);
	UCoreComponent* Core = NewObject<UCoreComponent>(Owner);
	Core->RegisterComponent();
	Core->Initialize(ASC);

	auto MakeCore = [](const TCHAR* Id)
	{
		UCoreDataAsset* Data = NewObject<UCoreDataAsset>();
		Data->CoreId = FName(Id);
		Data->CoreCategory = ECoreCategory::Ability;
		Data->MaxLevel = 5;
		return Data;
	};
	UCoreDataAsset* Guard = MakeCore(TEXT("Core_Guard"));
	UCoreDataAsset* Perfect = MakeCore(TEXT("Core_PerfectGuard"));
	UCoreRewardSession* Session = NewObject<UCoreRewardSession>();

	TestTrue(TEXT("Perfect Guard alone is excluded without Guard"),
		Session->Start(Core, {Perfect}, ECoreCategory::Ability) == ECoreRewardStartResult::NoCandidates);
	TestFalse(TEXT("No choice session remains active"), Session->IsActive());
	TestTrue(TEXT("An unoffered Perfect Guard cannot be selected"),
		Session->SelectCore(Perfect->CoreId) == ECoreAcquireResult::NotOffered);

	TArray<UCoreDataAsset*> Pool = {Perfect, MakeCore(TEXT("Other_A")), MakeCore(TEXT("Other_B")),
		MakeCore(TEXT("Other_C")), MakeCore(TEXT("Other_D"))};
	for (int32 Attempt = 0; Attempt < 16; ++Attempt)
	{
		TestTrue(TEXT("Other ability rewards remain available without Guard"),
			Session->Start(Core, Pool, ECoreCategory::Ability) == ECoreRewardStartResult::Started);
		for (const FCoreChoiceEntry& Choice : Session->GetChoiceEntries())
		{
			TestTrue(TEXT("Initial choices exclude Perfect Guard"), Choice.CoreData != Perfect);
		}
		UCoreDataAsset* Replacement = Session->RerollCoreChoice(0);
		TestNotNull(TEXT("The remaining ordinary core can be rerolled"), Replacement);
		TestTrue(TEXT("Rerolls exclude Perfect Guard"), Replacement != Perfect);
		Session->Cancel();
	}

	TestTrue(TEXT("Guard acquisition succeeds"), IsCoreAcquireSuccess(Core->AcquireCore(Guard)));
	TestTrue(TEXT("Perfect Guard becomes available after Guard"),
		Session->Start(Core, {Perfect}, ECoreCategory::Ability) == ECoreRewardStartResult::Started);
	const TArray<FCoreChoiceEntry> Choices = Session->GetChoiceEntries();
	TestEqual(TEXT("Perfect Guard is offered"), Choices.Num(), 1);
	if (Choices.Num() == 1) TestTrue(TEXT("Offered data is Perfect Guard"), Choices[0].CoreData == Perfect);
	TestTrue(TEXT("Perfect Guard selection succeeds with Guard"),
		IsCoreAcquireSuccess(Session->SelectCore(Perfect->CoreId)));
	TestTrue(TEXT("Perfect Guard upgrades remain available"),
		Session->Start(Core, {Perfect}, ECoreCategory::Ability) == ECoreRewardStartResult::Started);
	Session->Cancel();

	Core->ResetOwnedCores();
	TestTrue(TEXT("Resetting ownership excludes Perfect Guard again"),
		Session->Start(Core, {Perfect}, ECoreCategory::Ability) == ECoreRewardStartResult::NoCandidates);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
