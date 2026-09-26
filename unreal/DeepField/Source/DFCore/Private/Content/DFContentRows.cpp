#include "Content/DFContentRows.h"

float FDFStatusRow::Magnitude() const
{
	// Statuses.cs:32-40 — the number "strongest wins" compares within a channel.
	switch (Channel)
	{
	case EDFStatusChannel::Movement:      return 1.f - SpeedFactor;
	case EDFStatusChannel::Thermal:
	case EDFStatusChannel::Toxin:         return DamagePerSecond;
	case EDFStatusChannel::Vulnerability: return DamageTakenFactor - 1.f;
	case EDFStatusChannel::Defense:       return -ArmorDelta;
	case EDFStatusChannel::Control:       return MaxDurationSeconds;
	case EDFStatusChannel::Tether:        return PullSpeed;
	case EDFStatusChannel::Detection:     return 1.f;
	}
	return 0.f;
}
