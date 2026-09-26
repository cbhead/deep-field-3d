#pragma once

#include "CoreMinimal.h"

// C8 (contract-append, WS-02) — the Custom Primitive Data layout UDFTintComponent writes and the
// master materials (WS-31) read. One float per C8 instance parameter, colours as three
// consecutive floats (linear RGB), packed into the engine's 9 float4s (36 floats). Indices are
// permanent: adding a parameter appends; nothing is ever renumbered. Materials read a float
// with the CustomPrimitiveData node at the index below; a material that needs a parameter this
// layout does not carry (FactionAccent, AmmoHue, ...) stays an MID parameter for now.
//
//   float4 0: [0] HpFrac         [1..3] StatusTint.rgb
//   float4 1: [4] StatusWeight   [5..7] StatusEmissive.rgb
//   float4 2: [8] MarkEmissive   [9] RevealEmissive   [10] FreezeAmount   [11] TarAmount
//   float4 3: [12] PoisonAmount  [13] Stealth         [14] EyeGlow        [15] EliteRimColor.r
//   float4 4: [16] EliteRimColor.g [17] EliteRimColor.b [18] EliteRimPower [19] EliteFresnelWidth
//   float4 5: [20] Enrage        [21] Phase           [22] Shielded       [23] Dissolve
//   float4 6: [24..26] EnergyHue.rgb                  [27] EnergyIntensity
//   float4 7: [28] Powered       [29] HeatRamp        [30] Damage         [31] Wear
//   float4 8: [32] GhostPreview  [33..35] reserved
namespace DFTintLayout
{
	enum EIndex : int32
	{
		HpFrac = 0,
		StatusTintR = 1, StatusTintG = 2, StatusTintB = 3,
		StatusWeight = 4,
		StatusEmissiveR = 5, StatusEmissiveG = 6, StatusEmissiveB = 7,
		MarkEmissive = 8,
		RevealEmissive = 9,
		FreezeAmount = 10,
		TarAmount = 11,
		PoisonAmount = 12,
		Stealth = 13,
		EyeGlow = 14,
		EliteRimColorR = 15, EliteRimColorG = 16, EliteRimColorB = 17,
		EliteRimPower = 18,
		EliteFresnelWidth = 19,
		Enrage = 20,
		Phase = 21,
		Shielded = 22,
		Dissolve = 23,
		EnergyHueR = 24, EnergyHueG = 25, EnergyHueB = 26,
		EnergyIntensity = 27,
		Powered = 28,
		HeatRamp = 29,
		Damage = 30,
		Wear = 31,
		GhostPreview = 32,
		NumFloats = 33,
	};

	/** The C8 parameter name for an index (for the MID fallback and for tests / logs). */
	DFGAMEPLAY_API const TCHAR* ParameterName(int32 Index);
}
