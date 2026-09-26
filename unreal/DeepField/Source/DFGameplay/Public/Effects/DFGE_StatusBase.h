#pragma once

#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DFGE_StatusBase.generated.h"

// C5 — CONTRACT DEVIATION, RFC'd in unreal/PLAN/workstreams/ws-02-gameplay-core.md: status.md names
// one `GE_Status_<id>` ASSET per status under Content/DF/Gameplay/GE; this branch ships instead
// "GE_Status_<id> assets -> UDFGE_Status_<Channel> C++ classes with the id tag granted dynamically":
// one infinite effect class per channel (UDFGE_Status_Movement, _Thermal, _Toxin, _Defense,
// _Vulnerability, _Control, _Tether, _Detection). The class grants DF.Status.Channel.<Channel>; the
// status id (DF.Status.<Id>) is added as a dynamic granted tag on the spec, and the row's magnitude
// arrives as DF.SetByCaller.Magnitude, so the eight classes cover every current and future status
// row without a new asset — WS-14 cues and WS-05 key on the tags, never on an asset name. Duration
// is NOT the effect's: UDFStatusComponent owns the slot timers (strongest-wins, refresh, reactions)
// and removes the effect when the slot ends, which keeps the replicated slots the single source
// clients derive tint and VFX from.
UCLASS(Abstract)
class DFGAMEPLAY_API UDFGE_StatusBase : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/** Default period of the Thermal / Toxin damage tick: the sim's 1/30 s (Balance.cs TickHz). UDFStatusComponent overwrites
	 *  the spec's Period from Balance("tickHz", 30); the tick amount is dps x period through the full damage order, so the
	 *  period IS a balance number (the post-armor floor turns a 0.2 tick into 0.5 — see FDFStatusResolver). */
	static constexpr float DotPeriodSeconds = 1.f / 30.f;

	/** The channel this class serves. */
	EDFStatusChannel GetChannel() const { return Channel; }

	/** The effect class for a channel (never null). */
	static TSubclassOf<UDFGE_StatusBase> ClassForChannel(EDFStatusChannel InChannel);

	/** The channel tag DF.Status.Channel.<Channel>. */
	static FGameplayTag ChannelTag(EDFStatusChannel InChannel);

protected:
	/** Called by each subclass constructor: infinite duration + the channel's granted tag. */
	void ConfigureChannel(EDFStatusChannel InChannel);

	/** Adds a set-by-caller (DF.SetByCaller.Magnitude) modifier on Attribute with Op. */
	void AddMagnitudeModifier(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op);

	/** Turns the effect into a periodic damage tick through UDFDamageExecution. */
	void ConfigurePeriodicDamage();

private:
	EDFStatusChannel Channel = EDFStatusChannel::Movement;
};
