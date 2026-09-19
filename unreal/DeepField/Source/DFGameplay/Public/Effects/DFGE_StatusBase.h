#pragma once

#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DFGE_StatusBase.generated.h"

// C5 — GE_Status_<id> in C++: one infinite effect class per channel. The class grants
// DF.Status.Channel.<Channel>; the status id (DF.Status.<Id>) is added as a dynamic granted
// tag on the spec, and the row's magnitude arrives as DF.SetByCaller.Magnitude, so the eight
// classes cover every current and future status row without a new asset. Duration is NOT the
// effect's: UDFStatusComponent owns the slot timers (strongest-wins, refresh, reactions) and
// removes the effect when the slot ends, which keeps the replicated slots the single source
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
