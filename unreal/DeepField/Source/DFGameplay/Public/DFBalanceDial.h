#pragma once

#include "CoreMinimal.h"

// A Balance.cs dial by camelCase name, read from WS-01's DT_Balance when content is loaded, else
// Default — and, unlike UDFContentSubsystem::Balance(), a dial the table lacks is a one-line
// WARNING once per process, not an error: the dials WS-02 reads that balance.json does not carry
// yet (rearThresholdDegrees, shredFrontArcLeakFactor, postArmorDamageFloor, tetherImmuneMass) are
// requested by RFC, and until they land the sim's own literal stands in. Every WS-02 rule number
// comes through here; the callers never spell a fallback twice.
namespace DFBalance
{
	DFGAMEPLAY_API float Dial(const UObject* WorldContext, FName Name, float Default);
}
