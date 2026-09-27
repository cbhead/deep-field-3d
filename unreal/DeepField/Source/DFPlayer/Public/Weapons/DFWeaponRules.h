#pragma once

#include "CoreMinimal.h"

struct FDFWeaponRow;

/**
 * The hero's gun as pure functions (WS-03, B§1.2): trigger, fire rate, magazine and reload. A port of
 * the sim's player weapon, `Step.cs` ApplyPlayerHit (the cooldown, the dry pull that starts a reload
 * instead of firing, a round per shot), BeginReload (not while reloading, not when full) and the player
 * update (the reload timer refills the magazine when it runs out), with the Godot client's trigger
 * (`Player.cs` TickWeapons: an automatic gun fires while the button is held, a semi-automatic once per
 * pull). UDFHeroWeaponComponent holds one FDFWeaponState on each machine that runs the gun and asks
 * these functions what happens; so do the tests.
 *
 * Two deliberate changes from the sim, both PROGRAMME.md §3.2 rule 2 ("shots, not hits"):
 * - Every shot spends a round, a miss included. The sim spent one only on a hit it accepted, because
 *   the Godot client reported hits and never misses, so a miss was free there.
 * - The server checks the rate of reported shots against ShotsPerSecond × 1.05 with a small burst
 *   allowance (FDFShotBudget) rather than the sim's exact cooldown, so two shots that network jitter
 *   delivers close together are not refused.
 *
 * Units: seconds, and centimetres for range.
 */

/** One gun's numbers, from its FDFWeaponRow (weapons.json). */
struct FDFWeaponRules
{
	float Damage = 0.f;
	float ShotsPerSecond = 1.f;
	float RangeCm = 0.f;
	int32 MagazineSize = 12;
	float ReloadSeconds = 1.6f;
	/** Fires while the trigger is held; otherwise once per pull (weapons.json "automatic"). */
	bool bAutomatic = true;
};

/** One machine's view of the gun in hand. */
struct FDFWeaponState
{
	/** Rounds in the magazine (the sim's Magazine[weaponId]). */
	int32 Rounds = 0;
	/** Seconds until the next shot may leave (the sim's WeaponCooldown). */
	float Cooldown = 0.f;
	/** Seconds of reload left; above 0 while reloading (the sim's ReloadTimer). */
	float ReloadLeft = 0.f;
	/** The trigger was held last step: a semi-automatic gun needs it released before it fires again. */
	bool bTriggerWasHeld = false;
};

/** What one step of the trigger did. */
struct FDFWeaponStep
{
	/** A round left the barrel this step (at most one per step). */
	bool bFired = false;
	/** A pull on an empty magazine started the reload (DF.Message.ReloadStarted). */
	bool bReloadStarted = false;
	/** The reload finished this step: the magazine is full (DF.Message.Reloaded). */
	bool bReloaded = false;
};

/** The host's allowance of shots from one client: refills at ShotsPerSecond × RateSlack, capped at BurstShots. */
struct FDFShotBudget
{
	float Shots = 0.f;
};

/** Why the host accepted or refused a shot a client reported. */
enum class EDFShotVerdict : uint8
{
	Accepted,
	/** No gun, or a gun with no range or damage (no content row). */
	NoWeapon,
	/** The hero is down or waiting to respawn. */
	NotUp,
	/** The origin is too far from where the host has the shooter's eye, or the direction is not one. */
	BadShot,
	/** Faster than the gun fires (the budget is spent). */
	TooFast,
	/** The host's magazine is empty (the host starts the reload, as the sim's dry pull does). */
	Empty,
	/** The host is still reloading, beyond the grace. */
	Reloading,
};

namespace DFWeapon
{
	/** §3.2 rule 2: the host allows a shot rate up to ShotsPerSecond × 1.05. */
	constexpr float RateSlack = 1.05f;
	/** Balance.cs HitRangeSlack (B§1.2): the host traces RangeCm × 1.15, its sanity bound on the client's range. */
	constexpr float RangeSlack = 1.15f;
	/** The budget's cap: one shot delivered early by jitter, after one delivered late, still goes through. */
	constexpr float BurstShots = 2.f;
	/** B§1.2's ≤200 ms rewind, applied to the reload: the owner starts its reload half a round trip before
	 *  the host hears of it, so it also finishes first; a shot that arrives this close to the end of the
	 *  host's reload finishes it and fires, rather than being refused. */
	constexpr float ReloadGraceSeconds = 0.2f;
	/** How far a reported shot origin may be from the host's copy of the shooter's eye (cm): a sprinting
	 *  hero is a metre ahead of the host's copy at 100 ms, and a crouch moves the eye too. */
	constexpr float MaxOriginErrorCm = 200.f;

	/** The row's numbers in these units (RangeMeters to cm). */
	DFPLAYER_API FDFWeaponRules RulesFromRow(const FDFWeaponRow& Row);

	/** A full magazine, nothing cooling down, not reloading. */
	DFPLAYER_API FDFWeaponState Loaded(const FDFWeaponRules& Rules);

	DFPLAYER_API bool IsReloading(const FDFWeaponState& State);

	/** How far through the reload, 0..1 (0 when not reloading). */
	DFPLAYER_API float ReloadFraction(const FDFWeaponState& State, const FDFWeaponRules& Rules);

	/** Seconds between two shots (1 / ShotsPerSecond). */
	DFPLAYER_API float ShotInterval(const FDFWeaponRules& Rules);

	/** Start a reload if one is worth starting: not while reloading, not with a full magazine (Step.cs BeginReload). True if it started. */
	DFPLAYER_API bool BeginReload(FDFWeaponState& State, const FDFWeaponRules& Rules);

	/** Run the reload timer down by DeltaSeconds; true on the step it runs out and the magazine is refilled. */
	DFPLAYER_API bool AdvanceReload(FDFWeaponState& State, const FDFWeaponRules& Rules, float DeltaSeconds);

	/**
	 * One step of the owner's gun: the reload runs, the cooldown runs, and the trigger acts. A held
	 * trigger (a fresh pull for a semi-automatic) fires one round when the cooldown is spent and the
	 * magazine has one; on an empty magazine it starts the reload instead (Step.cs ApplyPlayerHit's dry
	 * pull, which also waits for the cooldown). Nothing fires while reloading. The cooldown carries the
	 * part of a frame it overran into the next shot, so a held trigger fires at ShotsPerSecond on
	 * average whatever the frame rate, and it does not bank shots while the trigger is up.
	 */
	DFPLAYER_API FDFWeaponStep Step(FDFWeaponState& State, const FDFWeaponRules& Rules, float DeltaSeconds, bool bTriggerHeld);

	/** A fresh budget: BurstShots. */
	DFPLAYER_API FDFShotBudget FullBudget();

	/** The host's budget refills at ShotsPerSecond × RateSlack per second, up to BurstShots. */
	DFPLAYER_API void RefillBudget(FDFShotBudget& Budget, const FDFWeaponRules& Rules, float DeltaSeconds);

	/**
	 * The host's check of a shot a client reported, against the host's own state of that client's gun.
	 * In order: still reloading (a reload within ReloadGraceSeconds of its end finishes now and the
	 * shot goes on), an empty magazine, the budget. Accepted spends a round and a shot of budget;
	 * a refusal changes nothing (starting the reload on Empty is the caller's).
	 */
	DFPLAYER_API EDFShotVerdict HostAcceptShot(FDFWeaponState& State, FDFShotBudget& Budget, const FDFWeaponRules& Rules);

	/** A reported origin within MaxOriginErrorCm of where the host has the shooter's eye. */
	DFPLAYER_API bool IsOriginPlausible(const FVector& ReportedOrigin, const FVector& HostEye);
}
