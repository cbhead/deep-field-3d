#include "DFTowerTestDummy.h"
#include "Misc/AutomationTest.h"
#include "Rig/DFTowerDefinition.h"
#include "Rig/DFTowerRig.h"
#include "Rig/DFTowerRigComponent.h"
#include "Testing/DFTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Tower.Rig* — C9 (rig.md): the aim math, the component's turret / spin / stage behaviour.
// No DA_Tower_<id> has been imported yet, so the component runs without meshes: angles, settle and
// stage bookkeeping are what is pinned here; what the meshes look like is WS-30's.

namespace DFTowerRigTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	UDFTowerRigComponent* MakeRig(FDFTestWorld& World, FName TowerId, EDFTowerKind Kind, const UDFTowerDefinition* Definition = nullptr)
	{
		AActor* Owner = World.SpawnActor<ADFTowerTestDummy>();
		if (!Owner)
		{
			return nullptr;
		}
		UDFTowerRigComponent* Rig = NewObject<UDFTowerRigComponent>(Owner);
		Rig->RegisterComponent();
		Rig->Configure(TowerId, Kind, Definition);
		return Rig;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerRigAnglesTest, "DF.Unit.Tower.RigAngles", DFTowerRigTest::Flags)
bool FDFTowerRigAnglesTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerRig;
	const FVector O = FVector::ZeroVector;
	TestEqual(TEXT("straight ahead: yaw 0"), DesiredAngles(O, 0.f, FVector(100.f, 0.f, 0.f)).X, 0.0, 1e-3);
	TestEqual(TEXT("straight ahead: pitch 0"), DesiredAngles(O, 0.f, FVector(100.f, 0.f, 0.f)).Y, 0.0, 1e-3);
	TestEqual(TEXT("to the right (+Y): yaw 90"), DesiredAngles(O, 0.f, FVector(0.f, 100.f, 0.f)).X, 90.0, 1e-3);
	TestEqual(TEXT("the tower's own yaw is taken off"), DesiredAngles(O, 90.f, FVector(0.f, 100.f, 0.f)).X, 0.0, 1e-3);
	TestEqual(TEXT("45 degrees up"), DesiredAngles(O, 0.f, FVector(100.f, 0.f, 100.f)).Y, 45.0, 1e-3);
	TestEqual(TEXT("behind is +180, not -180"), DesiredAngles(O, 0.f, FVector(-100.f, 0.f, 0.f)).X, 180.0, 1e-3);

	const FDFTowerRigLimits Lance = ManifestLimitsFor(TEXT("lance"));
	TestEqual(TEXT("lance pitch floor"), Lance.PitchMinDeg, -8.f);
	TestEqual(TEXT("lance pitch ceiling"), Lance.PitchMaxDeg, 26.f);
	TestEqual(TEXT("lance traverse"), Lance.TraverseDegPerSec, 90.f);
	TestEqual(TEXT("lance elevate"), Lance.ElevateDegPerSec, 60.f);
	TestEqual(TEXT("nova's 22 degree floor"), ManifestLimitsFor(TEXT("nova")).PitchMinDeg, 22.f);
	TestEqual(TEXT("skywatch traverses fast"), ManifestLimitsFor(TEXT("skywatch")).TraverseDegPerSec, 200.f);
	TestEqual(TEXT("filament"), ManifestLimitsFor(TEXT("filament")).ElevateDegPerSec, 160.f);
	TestEqual(TEXT("anything else: the defaults"), ManifestLimitsFor(TEXT("arc")).TraverseDegPerSec, FDFTowerRigLimits().TraverseDegPerSec);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerRigSlewTest, "DF.Unit.Tower.RigSlewRatesSeamAndLimits", DFTowerRigTest::Flags)
bool FDFTowerRigSlewTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerRig;
	const FDFTowerRigLimits Lance = ManifestLimitsFor(TEXT("lance"));   // 90 deg/s traverse, 60 elevate

	float Yaw = 0.f, Pitch = 0.f;
	TestFalse(TEXT("half a second toward yaw 90: not there"), Slew(Yaw, Pitch, FVector2D(90.0, 0.0), Lance, 0.5f));
	TestEqual(TEXT("45 degrees turned"), Yaw, 45.f, 1e-3f);
	TestTrue(TEXT("another half second: settled"), Slew(Yaw, Pitch, FVector2D(90.0, 0.0), Lance, 0.5f));
	TestEqual(TEXT("on 90"), Yaw, 90.f, 1e-3f);

	// Across the seam: 170 to -170 is 20 degrees the short way, not 340 the long way.
	Yaw = 170.f;
	Slew(Yaw, Pitch, FVector2D(-170.0, 0.0), Lance, 0.1f);
	TestEqual(TEXT("9 degrees toward the seam"), Yaw, 179.f, 1e-3f);
	Slew(Yaw, Pitch, FVector2D(-170.0, 0.0), Lance, 0.1f);
	TestEqual(TEXT("and over it"), Yaw, -172.f, 1e-3f);
	TestTrue(TEXT("settles on the far side"), Slew(Yaw, Pitch, FVector2D(-170.0, 0.0), Lance, 0.1f));

	// A limited turret never crosses the seam: it goes back the long way and stops at its limit.
	FDFTowerRigLimits Limited = Lance;
	Limited.bUnlimitedYaw = false;
	Limited.YawMinDeg = -90.f;
	Limited.YawMaxDeg = 90.f;
	Yaw = 80.f;
	Slew(Yaw, Pitch, FVector2D(-170.0, 0.0), Limited, 1.f);
	TestEqual(TEXT("back through 0 at 90 deg/s"), Yaw, -10.f, 1e-3f);
	TestTrue(TEXT("settled at the -90 limit, as near as it can get"), Slew(Yaw, Pitch, FVector2D(-170.0, 0.0), Limited, 1.f));
	TestEqual(TEXT("-90"), Yaw, -90.f, 1e-3f);

	// Pitch band: nova cannot aim below 22 degrees (its 5 m minimum range in geometry).
	const FDFTowerRigLimits Nova = ManifestLimitsFor(TEXT("nova"));
	Yaw = 0.f;
	Pitch = 40.f;
	Slew(Yaw, Pitch, FVector2D(0.0, 0.0), Nova, 10.f);
	TestEqual(TEXT("level is below nova's floor: it stops at 22"), Pitch, 22.f, 1e-3f);
	Slew(Yaw, Pitch, FVector2D(0.0, 89.0), Nova, 10.f);
	TestEqual(TEXT("and its ceiling is 70"), Pitch, 70.f, 1e-3f);
	Pitch = 22.f;
	Slew(Yaw, Pitch, FVector2D(0.0, 52.0), Nova, 0.5f);
	TestEqual(TEXT("elevating at 30 deg/s"), Pitch, 37.f, 1e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerRigComponentTest, "DF.Unit.Tower.RigTurretSpinAndSearch", DFTowerRigTest::Flags)
bool FDFTowerRigComponentTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerRigTest;
	FDFTestWorld World;
	UDFTowerRigComponent* Turret = MakeRig(World, TEXT("lance"), EDFTowerKind::Bolt);
	UDFTowerRigComponent* Spinner = MakeRig(World, TEXT("detector"), EDFTowerKind::Aura);
	UDFTowerRigComponent* Wall = MakeRig(World, TEXT("barricade"), EDFTowerKind::Barricade);
	if (!TestNotNull(TEXT("rigs"), Turret) || !TestNotNull(TEXT("rigs"), Spinner) || !TestNotNull(TEXT("rigs"), Wall))
	{
		return false;
	}
	TestTrue(TEXT("a bolt tower is a turret"), Turret->GetStyle() == EDFTowerRigStyle::Turret);
	TestTrue(TEXT("an aura tower spins"), Spinner->GetStyle() == EDFTowerRigStyle::Spin);
	TestTrue(TEXT("a barricade stands"), Wall->GetStyle() == EDFTowerRigStyle::None);
	TestEqual(TEXT("lance limits when there is no DA_Tower_lance"), Turret->GetLimits().PitchMaxDeg, 26.f);
	TestEqual(TEXT("no meshes: the muzzle is the pivot, 1.5 m up"), Turret->GetMuzzleLocation(), FVector(0.f, 0.f, 150.f));

	// A body 10 m to the right, level with the pivot: yaw 90 at 90 deg/s.
	const FVector Right(0.f, 1000.f, 150.f);
	TestFalse(TEXT("turning"), Turret->AimAt(Right, 0.5f));
	TestEqual(TEXT("45 degrees in half a second"), Turret->GetYawDeg(), 45.f, 1e-3f);
	TestTrue(TEXT("settled after a second"), Turret->AimAt(Right, 0.5f));

	Turret->StartSearchWobble();
	TestTrue(TEXT("searching"), Turret->IsSearching());
	TestFalse(TEXT("never settled while searching, even on the aim"), Turret->AimAt(Right, 0.1f));
	Turret->AimAt(Right, 0.1f);   // 0.2 s: the search is over
	TestFalse(TEXT("the search ended"), Turret->IsSearching());
	bool bSettled = false;
	for (int32 i = 0; i < 10 && !bSettled; ++i)
	{
		bSettled = Turret->AimAt(Right, 1.f / 30.f);
	}
	TestTrue(TEXT("and it locks on after"), bSettled);

	Spinner->AimAt(Right, 0.5f);
	TestEqual(TEXT("engaged: 110 deg/s"), Spinner->GetSpinDeg(), 55.f, 1e-3f);
	Spinner->Idle(1.f);
	TestEqual(TEXT("idle: 25 deg/s"), Spinner->GetSpinDeg(), 80.f, 1e-3f);
	TestTrue(TEXT("a barricade is always settled"), Wall->AimAt(Right, 0.1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerRigStagesTest, "DF.Unit.Tower.RigStagesAreCumulative", DFTowerRigTest::Flags)
bool FDFTowerRigStagesTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerRigTest;
	FDFTestWorld World;
	// A definition with three "damage" modules (S02..S04) and no "range" set; no meshes imported yet.
	UDFTowerDefinition* Definition = NewObject<UDFTowerDefinition>();
	FDFTowerStageSet& Damage = Definition->Stages.Add(TEXT("damage"));
	Damage.MountPart = TEXT("Pitch");
	Damage.Modules.SetNum(3);
	Definition->Rig = DFTowerRig::ManifestLimitsFor(TEXT("lance"));

	UDFTowerRigComponent* Rig = MakeRig(World, TEXT("lance"), EDFTowerKind::Bolt, Definition);
	if (!TestNotNull(TEXT("rig"), Rig))
	{
		return false;
	}
	FDFTowerRow Row;
	Row.UpgradePaths.AddDefaulted_GetRef().Id = TEXT("damage");
	Row.UpgradePaths.AddDefaulted_GetRef().Id = TEXT("range");

	Rig->SetPathLevels(Row, TArray<int32>{ 0, 0 });
	TestEqual(TEXT("L1 is the chassis: no modules"), Rig->GetAttachedStageCount(TEXT("damage")), 0);
	Rig->SetPathLevels(Row, TArray<int32>{ 2, 1 });
	TestEqual(TEXT("L3 on damage: S02 and S03"), Rig->GetAttachedStageCount(TEXT("damage")), 2);
	TestEqual(TEXT("a path with no stage set shows nothing"), Rig->GetAttachedStageCount(TEXT("range")), 0);
	Rig->SetPathLevels(Row, TArray<int32>{ 9, 1 });
	TestEqual(TEXT("capped at the modules the definition has"), Rig->GetAttachedStageCount(TEXT("damage")), 3);
	Rig->SetPathLevels(Row, TArray<int32>{ 1, 1 });
	TestEqual(TEXT("fewer purchases take modules off"), Rig->GetAttachedStageCount(TEXT("damage")), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
