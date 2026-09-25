#pragma once

#include "CoreMinimal.h"

/**
 * Downed, bleedout, revive and the solo respawn, as pure functions (WS-03, B§1.14, ADR-0024). A port
 * of the sim's hero half: `Step.cs` ApplyRevive, the downed/respawn branch of the player update and
 * RespawnPlayer, and the wave-boundary respawn in UpdateWaves. UDFHeroStateComponent holds one
 * FDFHeroLifeState per player and asks these functions what happens; so do the tests.
 *
 * One deliberate change from the sim, which B§1.14 asks for ("revive server-timed"): the sim adds one
 * tick of progress per Revive command and decays progress by half a tick every tick, whether or not
 * anyone is reviving (its own comment says decay is for "if nobody is holding"). The time a revive
 * takes therefore depends on how often the client sends the command. Here progress is server time:
 * it grows at 1 s per second while a reviver holds, and decays at half that only while nobody does,
 * so a held revive takes ReviveSeconds, and only one reviver's hold counts.
 *
 * Units: seconds, and centimetres for range.
 */
enum class EDFHeroLife : uint8
{
	/** Standing (the sim's Alive). */
	Up,
	/** On the ground, bleeding out or bled out, until revived or respawned at the wave boundary. */
	Downed,
	/** Alone in the match at 0 hp: no one can revive, so a timer respawns them (SoloRespawnSeconds). */
	Respawning,
};

/** What happened when a hero's health reached 0. */
enum class EDFHeroDown : uint8
{
	Downed,
	SoloRespawn,
	/** Not standing, so there was nothing to take down (the sim skips contact damage then). */
	Ignored,
};

/** The numbers. Defaults are `Balance.cs` (and DT_Balance: reviveSeconds, bleedoutSeconds, soloRespawnSeconds, reviveRangeMeters). */
struct FDFHeroLifeRules
{
	float ReviveSeconds = 4.f;
	float BleedoutSeconds = 30.f;
	float SoloRespawnSeconds = 10.f;
	float ReviveRangeCm = 250.f;
	/** Progress lost per second while nobody holds the revive, as a fraction of a second (Step.cs: Dt × 0.5). */
	float ReviveDecayRate = 0.5f;
	/** Health a revived hero gets back, as a fraction of max (Step.cs: PlayerMaxHp × 0.5). */
	float RevivedHpFraction = 0.5f;
};

/** Health regeneration state (Step.cs player update): seconds until regen may start. */
struct FDFHeroRegen
{
	float DelayLeft = 0.f;
};

struct FDFHeroLifeState
{
	EDFHeroLife Life = EDFHeroLife::Up;
	/** Seconds of bleedout left while Downed; 0 once bled out (the hero stays down until the wave boundary). */
	float BleedoutLeft = 0.f;
	/** Seconds until the solo respawn while Respawning. */
	float RespawnLeft = 0.f;
	/** Seconds of revive progress while Downed; revived at ReviveSeconds. */
	float ReviveProgress = 0.f;
};

namespace DFHeroLife
{
	struct FTickResult
	{
		/** Progress reached ReviveSeconds this step: the hero is Up again, at RevivedHpFraction health. */
		bool bRevived = false;
		/** The solo respawn timer ran out this step: the hero is Up again, at full health, at the hero spawn. */
		bool bRespawned = false;
	};

	/** Standing: may revive others, take a seat, be taken down. */
	DFPLAYER_API bool IsUp(const FDFHeroLifeState& State);

	/** Downed with bleedout still running (DF.Player.State.Bleeding). */
	DFPLAYER_API bool IsBleeding(const FDFHeroLifeState& State);

	/**
	 * Health reached 0. With two or more players connected the hero goes down and starts bleeding out;
	 * alone, no one could revive them, so the solo respawn timer starts instead (Step.cs). Both are a
	 * PlayerDowned to the rest of the game. Only a standing hero can be taken down.
	 */
	DFPLAYER_API EDFHeroDown Deplete(FDFHeroLifeState& State, const FDFHeroLifeRules& Rules, int32 ConnectedPlayers);

	/**
	 * One host step of DeltaSeconds. bReviveHeld means exactly one standing reviver is holding the
	 * revive within range this step (UDFHeroStateComponent decides that). Downed: bleedout runs down to
	 * 0; a held revive adds DeltaSeconds of progress and revives at ReviveSeconds, otherwise progress
	 * decays at ReviveDecayRate. Respawning: the timer runs down and respawns at 0. Up: nothing.
	 */
	DFPLAYER_API FTickResult Tick(FDFHeroLifeState& State, const FDFHeroLifeRules& Rules, float DeltaSeconds, bool bReviveHeld);

	/** A reviver DistanceCm away is close enough (Balance.cs ReviveRangeMeters, inclusive as the sim's `>` refusal). */
	DFPLAYER_API bool InReviveRange(float DistanceCm, const FDFHeroLifeRules& Rules);

	/**
	 * The hero bled out and is still down: WS-28 respawns them when the intermission ends and the next
	 * wave begins (Step.cs UpdateWaves). A hero still bleeding stays down into the next wave.
	 */
	DFPLAYER_API bool ShouldRespawnAtWaveBoundary(const FDFHeroLifeState& State);

	/** Back to standing with every timer cleared (Step.cs RespawnPlayer; full health and the spawn point are the caller's). */
	DFPLAYER_API void Respawn(FDFHeroLifeState& State);

	/** The hero took damage: regen waits RegenDelaySeconds again (Step.cs: RegenDelay = PlayerRegenDelaySeconds). */
	DFPLAYER_API void NoteDamaged(FDFHeroRegen& Regen, float RegenDelaySeconds);

	/**
	 * One step of regeneration for a standing hero; returns the new health. The delay runs down first,
	 * and on the step it reaches 0 health already grows, at RegenPerSecond up to MaxHealth (Step.cs).
	 * A hero at 0 health does not regenerate: getting up is a revive or a respawn.
	 */
	DFPLAYER_API float RegenStep(FDFHeroRegen& Regen, float Health, float MaxHealth, float RegenPerSecond, float DeltaSeconds);
}
