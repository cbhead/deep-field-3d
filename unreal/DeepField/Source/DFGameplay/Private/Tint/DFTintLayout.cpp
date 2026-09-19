#include "Tint/DFTintLayout.h"

namespace DFTintLayout
{
	const TCHAR* ParameterName(int32 Index)
	{
		switch (Index)
		{
		case HpFrac:            return TEXT("HpFrac");
		case StatusTintR:       return TEXT("StatusTint.R");
		case StatusTintG:       return TEXT("StatusTint.G");
		case StatusTintB:       return TEXT("StatusTint.B");
		case StatusWeight:      return TEXT("StatusWeight");
		case StatusEmissiveR:   return TEXT("StatusEmissive.R");
		case StatusEmissiveG:   return TEXT("StatusEmissive.G");
		case StatusEmissiveB:   return TEXT("StatusEmissive.B");
		case MarkEmissive:      return TEXT("MarkEmissive");
		case RevealEmissive:    return TEXT("RevealEmissive");
		case FreezeAmount:      return TEXT("FreezeAmount");
		case TarAmount:         return TEXT("TarAmount");
		case PoisonAmount:      return TEXT("PoisonAmount");
		case Stealth:           return TEXT("Stealth");
		case EyeGlow:           return TEXT("EyeGlow");
		case EliteRimColorR:    return TEXT("EliteRimColor.R");
		case EliteRimColorG:    return TEXT("EliteRimColor.G");
		case EliteRimColorB:    return TEXT("EliteRimColor.B");
		case EliteRimPower:     return TEXT("EliteRimPower");
		case EliteFresnelWidth: return TEXT("EliteFresnelWidth");
		case Enrage:            return TEXT("Enrage");
		case Phase:             return TEXT("Phase");
		case Shielded:          return TEXT("Shielded");
		case Dissolve:          return TEXT("Dissolve");
		case EnergyHueR:        return TEXT("EnergyHue.R");
		case EnergyHueG:        return TEXT("EnergyHue.G");
		case EnergyHueB:        return TEXT("EnergyHue.B");
		case EnergyIntensity:   return TEXT("EnergyIntensity");
		case Powered:           return TEXT("Powered");
		case HeatRamp:          return TEXT("HeatRamp");
		case Damage:            return TEXT("Damage");
		case Wear:              return TEXT("Wear");
		case GhostPreview:      return TEXT("GhostPreview");
		default:                return TEXT("");
		}
	}
}
