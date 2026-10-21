#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Weapon/WeaponImpactFeedback.h"
#include "DataAsset/WeaponDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponImpactTransformTest, "TheLastMessage.Weapon.ImpactSurfaceTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeaponImpactTransformTest::RunTest(const FString& Parameters)
{
	const FVector Contact(100.0f, 200.0f, 300.0f);
	const FTransform Plain = WeaponImpactFeedback::MakeTransform(nullptr, Contact, FVector::UpVector);
	TestTrue(TEXT("Impact uses contact point rather than actor center"), Plain.GetLocation().Equals(Contact));
	TestTrue(TEXT("Local X follows surface normal"), Plain.GetRotation().GetForwardVector().Equals(FVector::UpVector));
	UWeaponDataAsset* Data = NewObject<UWeaponDataAsset>();
	Data->ImpactEffectOffset = FTransform(FRotator(0, 90, 0), FVector(10, 0, 0), FVector(2));
	const FTransform Offset = WeaponImpactFeedback::MakeTransform(Data, Contact, FVector::UpVector);
	TestTrue(TEXT("Local offset moves away from surface"), Offset.GetLocation().Equals(Contact + FVector(0, 0, 10)));
	TestTrue(TEXT("Weapon impact scale is preserved"), Offset.GetScale3D().Equals(FVector(2)));
	const FTransform MissingNormal = WeaponImpactFeedback::MakeTransform(nullptr, Contact, FVector::ZeroVector);
	TestFalse(TEXT("Initial overlap with no normal produces a valid transform"), MissingNormal.ContainsNaN());
	TestTrue(TEXT("Missing normal falls back to forward"), MissingNormal.GetRotation().GetForwardVector().Equals(FVector::ForwardVector));
	return true;
}
#endif
