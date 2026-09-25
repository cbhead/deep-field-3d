#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "DFHeroCharacter.h"
#include "DFHeroCollision.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Movement/DFHeroMovementComponent.h"
#include "Movement/DFHeroMoveRules.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Player.* — the hero's movement numbers against the Godot player (game/scripts/Player.cs) and
// B§1.1. The Godot literals are written out here in metres so the parity is readable against the
// source; everything else is Unreal units. No world: a test world has no game mode, so a hero spawned
// in one never begins play; these read class defaults and call the rules directly.

namespace DFHeroMovementTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Tol = 1e-3f;

	FDFHeroMoveSpeeds B11()
	{
		FDFHeroMoveSpeeds S;
		S.Walk = 650.f;
		S.Sprint = 1000.f;
		S.Crouch = 300.f;
		S.Aim = 350.f;
		return S;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFHeroMoveParityTest, "DF.Unit.Player.MoveParity", DFHeroMovementTest::Flags)
bool FDFHeroMoveParityTest::RunTest(const FString&)
{
	using namespace DFHeroMovementTest;
	const UDFHeroMovementComponent* Move = GetDefault<UDFHeroMovementComponent>();

	// Player.cs: MoveSpeed 6.5, SprintSpeed 10, JumpVelocity 4.8 (m/s).
	TestEqual(TEXT("walk is Godot's 6.5 m/s"), Move->MaxWalkSpeed, 6.5f * 100.f, Tol);
	TestEqual(TEXT("sprint is Godot's 10 m/s"), Move->MaxSprintSpeed, 10.f * 100.f, Tol);
	TestEqual(TEXT("jump is Godot's 4.8 m/s"), Move->JumpZVelocity, 4.8f * 100.f, Tol);

	// B§1.1, no Godot counterpart.
	TestEqual(TEXT("crouch is 3.0 m/s"), Move->MaxWalkSpeedCrouched, 3.f * 100.f, Tol);
	TestEqual(TEXT("ADS is 3.5 m/s"), Move->MaxAimSpeed, 3.5f * 100.f, Tol);

	// The same jump only reaches the same height under the same gravity: Godot subtracts 9.8 m/s² a
	// frame; here it is the world's gravity times the component's scale.
	const float GravityZ = GetDefault<UPhysicsSettings>()->DefaultGravityZ * Move->GravityScale;
	TestEqual(TEXT("gravity is Godot's 9.8 m/s²"), GravityZ, -9.8f * 100.f, Tol);
	const float GodotApexCm = 4.8f * 4.8f / (2.f * 9.8f) * 100.f;   // 117.55 cm
	TestEqual(TEXT("a jump peaks where Godot's does"), DFHeroMove::JumpApexCm(Move->JumpZVelocity, GravityZ), GodotApexCm, 0.01f);

	// PROGRAMME.md §3.2 (player).
	TestEqual(TEXT("walkable slope is 45°"), Move->GetWalkableFloorAngle(), 45.f, Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFHeroSpeedRulesTest, "DF.Unit.Player.SpeedRules", DFHeroMovementTest::Flags)
bool FDFHeroSpeedRulesTest::RunTest(const FString&)
{
	using namespace DFHeroMovementTest;
	const FDFHeroMoveSpeeds S = B11();

	//                                                                     crouched aiming  sprint
	TestEqual(TEXT("walk"),                    DFHeroMove::MaxSpeed(S, false, false, false), 650.f, Tol);
	TestEqual(TEXT("sprint"),                  DFHeroMove::MaxSpeed(S, false, false, true),  1000.f, Tol);
	TestEqual(TEXT("crouch"),                  DFHeroMove::MaxSpeed(S, true,  false, false), 300.f, Tol);
	TestEqual(TEXT("ADS"),                     DFHeroMove::MaxSpeed(S, false, true,  false), 350.f, Tol);
	TestEqual(TEXT("crouch cancels sprint"),   DFHeroMove::MaxSpeed(S, true,  false, true),  300.f, Tol);
	TestEqual(TEXT("ADS cancels sprint"),      DFHeroMove::MaxSpeed(S, false, true,  true),  350.f, Tol);
	TestEqual(TEXT("crouched ADS: the slower"), DFHeroMove::MaxSpeed(S, true,  true,  false), 300.f, Tol);

	FDFHeroMoveSpeeds SlowAim = S;
	SlowAim.Aim = 200.f;
	TestEqual(TEXT("crouched ADS takes aim when aim is slower"), DFHeroMove::MaxSpeed(SlowAim, true, true, true), 200.f, Tol);

	TestTrue(TEXT("sprint allowed standing"), DFHeroMove::CanSprint(false, false));
	TestFalse(TEXT("no sprint crouched"), DFHeroMove::CanSprint(true, false));
	TestFalse(TEXT("no sprint aiming"), DFHeroMove::CanSprint(false, true));

	TestEqual(TEXT("no gravity, no apex"), DFHeroMove::JumpApexCm(480.f, 0.f), 0.f, Tol);
	TestEqual(TEXT("gravity's sign does not matter"), DFHeroMove::JumpApexCm(480.f, 980.f), DFHeroMove::JumpApexCm(480.f, -980.f), Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFHeroComponentSpeedTest, "DF.Unit.Player.ComponentSpeed", DFHeroMovementTest::Flags)
bool FDFHeroComponentSpeedTest::RunTest(const FString&)
{
	// A bare component, no character: IsCrouching() is false without an owner, so this checks that
	// GetMaxSpeed routes the wants through the rules in the modes that use them, and only those.
	using namespace DFHeroMovementTest;
	UDFHeroMovementComponent* Move = NewObject<UDFHeroMovementComponent>(GetTransientPackage());

	Move->MovementMode = MOVE_Walking;
	TestEqual(TEXT("walking"), Move->GetMaxSpeed(), Move->MaxWalkSpeed, Tol);

	Move->SetWantsToSprint(true);
	TestEqual(TEXT("sprinting"), Move->GetMaxSpeed(), Move->MaxSprintSpeed, Tol);
	TestTrue(TEXT("IsSprinting"), Move->IsSprinting());

	Move->MovementMode = MOVE_Falling;
	TestEqual(TEXT("sprint speed carries into the air, as in Godot"), Move->GetMaxSpeed(), Move->MaxSprintSpeed, Tol);

	Move->MovementMode = MOVE_Walking;
	Move->SetWantsToAim(true);
	TestEqual(TEXT("aiming beats sprinting"), Move->GetMaxSpeed(), Move->MaxAimSpeed, Tol);
	TestFalse(TEXT("not sprinting while aiming"), Move->IsSprinting());

	Move->MovementMode = MOVE_Flying;
	TestEqual(TEXT("modes the rules do not own keep the engine's speed"), Move->GetMaxSpeed(), Move->MaxFlySpeed, Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFHeroPredictedWantsTest, "DF.Unit.Player.PredictedWants", DFHeroMovementTest::Flags)
bool FDFHeroPredictedWantsTest::RunTest(const FString&)
{
	const uint8 EngineBits = FSavedMove_Character::FLAG_JumpPressed | FSavedMove_Character::FLAG_WantsToCrouch;
	for (int32 Case = 0; Case < 4; ++Case)
	{
		const bool bSprint = (Case & 1) != 0;
		const bool bAim = (Case & 2) != 0;
		const uint8 Packed = DFHeroMove::PackWants(bSprint, bAim);
		TestEqual(*FString::Printf(TEXT("case %d leaves jump and crouch bits alone"), Case), Packed & EngineBits, 0);

		bool bOutSprint = !bSprint;
		bool bOutAim = !bAim;
		DFHeroMove::UnpackWants(Packed | EngineBits, bOutSprint, bOutAim);
		TestTrue(*FString::Printf(TEXT("case %d sprint survives the round trip"), Case), bOutSprint == bSprint);
		TestTrue(*FString::Printf(TEXT("case %d aim survives the round trip"), Case), bOutAim == bAim);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFHeroDefaultsTest, "DF.Unit.Player.HeroDefaults", DFHeroMovementTest::Flags)
bool FDFHeroDefaultsTest::RunTest(const FString&)
{
	using namespace DFHeroMovementTest;
	const ADFHeroCharacter* Hero = GetDefault<ADFHeroCharacter>();

	TestTrue(TEXT("moves with UDFHeroMovementComponent"), Hero->GetCharacterMovement() != nullptr && Hero->GetCharacterMovement()->IsA<UDFHeroMovementComponent>());

	// Player.cs: CapsuleShape3D { Radius = 0.4, Height = 1.8 }, camera at y 1.6 above the feet, FOV 80.
	const UCapsuleComponent* Capsule = Hero->GetCapsuleComponent();
	if (!TestNotNull(TEXT("capsule"), Capsule))
	{
		return false;
	}
	TestEqual(TEXT("capsule radius 0.4 m"), Capsule->GetUnscaledCapsuleRadius(), 0.4f * 100.f, Tol);
	TestEqual(TEXT("capsule 1.8 m tall"), Capsule->GetUnscaledCapsuleHalfHeight() * 2.f, 1.8f * 100.f, Tol);
	TestTrue(TEXT("capsule profile is C16's DF_Hero"), Capsule->GetCollisionProfileName() == DFHeroCollision::Profile());

	const UCameraComponent* Camera = Hero->GetFirstPersonCamera();
	if (!TestNotNull(TEXT("first-person camera"), Camera))
	{
		return false;
	}
	const float EyeAboveFeetCm = static_cast<float>(Camera->GetRelativeLocation().Z) + Capsule->GetUnscaledCapsuleHalfHeight();
	TestEqual(TEXT("eye 1.6 m above the feet"), EyeAboveFeetCm, 1.6f * 100.f, Tol);
	TestEqual(TEXT("FOV 80°"), Camera->FieldOfView, 80.f, Tol);
	TestTrue(TEXT("camera follows the controller's pitch"), Camera->bUsePawnControlRotation != 0);
	TestTrue(TEXT("body follows the controller's yaw"), Hero->bUseControllerRotationYaw != 0);
	TestFalse(TEXT("body ignores the controller's pitch"), Hero->bUseControllerRotationPitch != 0);

	TestEqual(TEXT("pitch limit is Godot's 1.5 rad"), DFHeroMove::PitchLimitDegrees, FMath::RadiansToDegrees(1.5f), Tol);
	return true;
}

#endif
