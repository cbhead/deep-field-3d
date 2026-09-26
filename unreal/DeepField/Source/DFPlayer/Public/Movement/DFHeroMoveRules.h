#pragma once

#include "CoreMinimal.h"
#include "Hero/DFHeroLife.h"

/**
 * How fast a hero moves, as pure functions (WS-03, B§1.1). The movement component asks these; so do
 * the tests, so the rule is written once.
 *
 * Walk, sprint and jump are parity with the Godot player (`game/scripts/Player.cs`: MoveSpeed 6.5,
 * SprintSpeed 10, JumpVelocity 4.8, gravity 9.8 m/s²). Crouch 3.0 and ADS 3.5 are new in B§1.1 and
 * have no Godot counterpart. Units are Unreal's: centimetres, cm/s, cm/s².
 */
struct FDFHeroMoveSpeeds
{
	float Walk = 0.f;
	float Sprint = 0.f;
	float Crouch = 0.f;
	float Aim = 0.f;
};

namespace DFHeroMove
{
	// ---- B§1.1 numbers -------------------------------------------------------------------------

	constexpr float WalkCmPerSec   = 650.f;    // Player.cs MoveSpeed 6.5
	constexpr float SprintCmPerSec = 1000.f;   // Player.cs SprintSpeed 10
	constexpr float JumpZCmPerSec  = 480.f;    // Player.cs JumpVelocity 4.8
	constexpr float CrouchCmPerSec = 300.f;    // B§1.1 crouch 3.0
	constexpr float AimCmPerSec    = 350.f;    // B§1.1 ADS 3.5
	constexpr float DownedCrawlCmPerSec = 100.f;   // B§1.14 downed crawl 1 m/s

	/**
	 * Godot writes the hero's velocity directly every physics frame, so it starts, stops and turns
	 * instantly, in the air as well as on the ground. The CMC accelerates instead; this is high enough
	 * that sprint speed is reached in 0.05 s (three frames at 60 Hz). Whether the Unreal hero should
	 * keep Godot's weightless feel is an open question in ws-03-player.md, not a settled rule.
	 */
	constexpr float ParityAccelerationCmPerSec2 = 20000.f;

	/** PROGRAMME.md §3.2 (player): CMC walkable slope 45°. (Step 45 cm is the CMC's own default.) */
	constexpr float WalkableFloorDegrees = 45.f;

	// ---- body and view (Player.cs) -------------------------------------------------------------

	/** Godot's hero capsule is 0.4 m radius and 1.8 m tall; Unreal's capsule is measured from its centre. */
	constexpr float CapsuleRadiusCm     = 40.f;
	constexpr float CapsuleHalfHeightCm = 90.f;

	/** Godot's camera sits 1.6 m above the feet with an 80° field of view. */
	constexpr float EyeHeightAboveFeetCm = 160.f;
	constexpr float FieldOfViewDegrees   = 80.f;

	/** Godot clamps pitch to ±1.5 rad. */
	constexpr float PitchLimitDegrees = 85.943669f;

	// ---- rules ---------------------------------------------------------------------------------

	/** Sprint is a want, not a state: it takes effect only while the hero is neither crouched nor aiming. */
	DFPLAYER_API bool CanSprint(bool bCrouched, bool bAiming);

	/**
	 * The hero's top speed on the ground and in the air (Godot's air speed is its ground speed). Crouch
	 * and ADS each cap speed and cancel sprint; together, the slower of the two applies. Sprint works in
	 * any direction, as it does in Godot.
	 */
	DFPLAYER_API float MaxSpeed(const FDFHeroMoveSpeeds& Speeds, bool bCrouched, bool bAiming, bool bWantsSprint);

	/**
	 * Top speed for a hero's life state (B§1.14): standing moves at StandingSpeed (MaxSpeed above), a
	 * downed hero crawls at DownedSpeed, and a hero waiting for the solo respawn does not move.
	 */
	DFPLAYER_API float MaxSpeedForLife(EDFHeroLife Life, float StandingSpeed, float DownedSpeed);

	/** Height a jump of JumpZ (cm/s) reaches under GravityZ (cm/s², either sign): v² / 2g. 0 without gravity. */
	DFPLAYER_API float JumpApexCm(float JumpZ, float GravityZ);

	/**
	 * Sprint and ADS are predicted like crouch: the client's want travels with each saved move in
	 * FSavedMove_Character's custom flags (FLAG_Custom_0 sprint, FLAG_Custom_1 aim), and the server
	 * replays the move with the same want. These two functions are that packing, and nothing else sets
	 * those bits.
	 */
	DFPLAYER_API uint8 PackWants(bool bWantsSprint, bool bWantsAim);
	DFPLAYER_API void UnpackWants(uint8 Flags, bool& bOutWantsSprint, bool& bOutWantsAim);
}
