// GENERATED VALUES — every colour below is docs/palette.json (1.0, 2026-09-06) transcribed field
// for field, and is the interim DA_Palette (C8 / ADR-0017: gameplay code asks this object, never
// spells a colour). Do not hand-edit a hex here: change docs/palette.json and regenerate — the
// RFC in ws-02-gameplay-core.md asks WS-01's import_tokens.py to write this constructor (or the
// [/Script/DFGameplay.DFPaletteSettings] section of DefaultGame.ini, which overrides it) so the
// palette has one source. FromHex turns sRGB hex into linear at construction.
#include "Tint/DFPaletteSettings.h"

#include "DFGameplayLocalTags.h"

FLinearColor UDFPaletteSettings::FromHex(const TCHAR* Hex)
{
	return FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
}

namespace
{
	FDFPaletteStatus TintStatus(const TCHAR* Tint, bool bEmissive)
	{
		FDFPaletteStatus S;
		S.bHasTint = true;
		S.Tint = UDFPaletteSettings::FromHex(Tint);
		S.bEmissive = bEmissive;
		S.Emissive = bEmissive ? S.Tint : FLinearColor::Black;
		return S;
	}

	FDFPaletteStatus EmissiveOnlyStatus(const TCHAR* Emissive)
	{
		FDFPaletteStatus S;
		S.bHasTint = false;
		S.Tint = FLinearColor::Black;
		S.bEmissive = true;
		S.Emissive = UDFPaletteSettings::FromHex(Emissive);
		return S;
	}

	FDFPaletteFaction Faction(const TCHAR* Accent, const TCHAR* Ability)
	{
		FDFPaletteFaction F;
		F.Accent = UDFPaletteSettings::FromHex(Accent);
		F.Ability = UDFPaletteSettings::FromHex(Ability);
		return F;
	}
}

UDFPaletteSettings::UDFPaletteSettings()
{
	// docs/palette.json 1.0 — "semantics"
	Semantics.Danger   = FromHex(TEXT("#C93B28"));
	Semantics.Warning  = FromHex(TEXT("#E8862B"));
	Semantics.Success  = FromHex(TEXT("#7BC043"));
	Semantics.Info     = FromHex(TEXT("#2FB4BE"));
	Semantics.Hp       = FromHex(TEXT("#7BC043"));
	Semantics.HpLow    = FromHex(TEXT("#C93B28"));
	Semantics.Shield   = FromHex(TEXT("#2FB4BE"));
	Semantics.Armor    = FromHex(TEXT("#7D8BA3"));
	Semantics.Currency = FromHex(TEXT("#C89B3C"));
	Semantics.Upgrade  = FromHex(TEXT("#2FB4BE"));
	Semantics.Elite    = FromHex(TEXT("#9B5BE8"));
	Semantics.Boss     = FromHex(TEXT("#9B5BE8"));

	// "factions" (only three are authored)
	Factions.Add(TEXT("forge"),   Faction(TEXT("#C89B3C"), TEXT("#FF6F1A")));
	Factions.Add(TEXT("ember"),   Faction(TEXT("#E8622B"), TEXT("#E8622B")));
	Factions.Add(TEXT("tempest"), Faction(TEXT("#5B76E8"), TEXT("#5B76E8")));

	// "scrap"
	Scrap.Alloy     = FromHex(TEXT("#A6B2C6"));
	Scrap.Flux      = FromHex(TEXT("#2FB4BE"));
	Scrap.Plating   = FromHex(TEXT("#C89B3C"));
	Scrap.Gravium   = FromHex(TEXT("#9B5BE8"));
	Scrap.Primecore = FromHex(TEXT("#F4DCA4"));

	// "statuses" (+ emissive flag)
	Statuses.Add(TEXT("burn"),   TintStatus(TEXT("#E8622B"), true));
	Statuses.Add(TEXT("chill"),  TintStatus(TEXT("#4FC0E8"), false));
	Statuses.Add(TEXT("freeze"), TintStatus(TEXT("#A8F0F4"), false));
	Statuses.Add(TEXT("shock"),  TintStatus(TEXT("#F05AE6"), true));
	Statuses.Add(TEXT("shred"),  TintStatus(TEXT("#E9614C"), false));
	Statuses.Add(TEXT("mark"),   EmissiveOnlyStatus(TEXT("#E3BC66")));
	Statuses.Add(TEXT("reveal"), EmissiveOnlyStatus(TEXT("#7FE65A")));
	Statuses.Add(TEXT("tar"),    TintStatus(TEXT("#1B2233"), false));

	// "enemies"
	Enemies.Base         = FromHex(TEXT("#7D8BA3"));
	Enemies.HpLow        = FromHex(TEXT("#C93B28"));
	Enemies.WeakPoint    = FromHex(TEXT("#E9614C"));
	Enemies.WardenShield = FromHex(TEXT("#2FB4BE"));
	EliteRims.Add(TEXT("gilded"),     FromHex(TEXT("#C89B3C")));
	EliteRims.Add(TEXT("juggernaut"), FromHex(TEXT("#5A6880")));
	EliteRims.Add(TEXT("voltaic"),    FromHex(TEXT("#F05AE6")));
	EliteRims.Add(TEXT("swift"),      FromHex(TEXT("#65DCE4")));
	EliteRims.Add(TEXT("umbral"),     FromHex(TEXT("#9B5BE8")));

	// "towers.materials"
	TowerBody.SteelHull  = FromHex(TEXT("#606A7C"));
	TowerBody.SteelPlate = FromHex(TEXT("#9AA6B7"));
	TowerBody.Chassis    = FromHex(TEXT("#2B3A5C"));
	TowerBody.Trim       = FromHex(TEXT("#1B2436"));
	TowerBody.Chrome     = FromHex(TEXT("#C3CCD8"));
	TowerBody.Brass      = FromHex(TEXT("#B08A3E"));
	TowerBody.Rubber     = FromHex(TEXT("#14181F"));
	TowerBody.Hazard     = FromHex(TEXT("#D8A13A"));
	TowerBody.OpticGlass = FromHex(TEXT("#14314A"));
	TowerBody.Ceramic    = FromHex(TEXT("#D9D2C3"));
	TowerBody.Concrete   = FromHex(TEXT("#7A7F88"));

	// "towers.energy"
	TowerEnergy.Add(TEXT("lance"),       FromHex(TEXT("#2B5CFF")));
	TowerEnergy.Add(TEXT("skywatch"),    FromHex(TEXT("#22D3EE")));
	TowerEnergy.Add(TEXT("detector"),    FromHex(TEXT("#7FE65A")));
	TowerEnergy.Add(TEXT("nova"),        FromHex(TEXT("#F0C83A")));
	TowerEnergy.Add(TEXT("overclock"),   FromHex(TEXT("#FF6F1A")));
	TowerEnergy.Add(TEXT("filament"),    FromHex(TEXT("#FF2E4A")));
	TowerEnergy.Add(TEXT("arc"),         FromHex(TEXT("#F05AE6")));
	TowerEnergy.Add(TEXT("singularity"), FromHex(TEXT("#9B5BE8")));
	TowerEnergy.Add(TEXT("barricade"),   FromHex(TEXT("#F0C83A")));
	EnergyEmissiveIntensity = 0.95f;

	// "ui" ramps
	for (const TCHAR* Hex : { TEXT("#080B11"), TEXT("#0D1119"), TEXT("#131926"), TEXT("#1B2233"), TEXT("#242E43"), TEXT("#313C55") }) { UIObsidian.Add(FromHex(Hex)); }
	for (const TCHAR* Hex : { TEXT("#3E4A63"), TEXT("#5A6880"), TEXT("#7D8BA3"), TEXT("#A6B2C6"), TEXT("#CFD7E4"), TEXT("#ECF0F7") }) { UISteel.Add(FromHex(Hex)); }
	for (const TCHAR* Hex : { TEXT("#6F4C15"), TEXT("#9A6D1E"), TEXT("#C89B3C"), TEXT("#E3BC66"), TEXT("#F4DCA4") }) { UIBrass.Add(FromHex(Hex)); }
	for (const TCHAR* Hex : { TEXT("#12525A"), TEXT("#1B7C86"), TEXT("#2FB4BE"), TEXT("#65DCE4"), TEXT("#A8F0F4") }) { UIArcane.Add(FromHex(Hex)); }
}

const FDFPaletteStatus* UDFPaletteSettings::Status(const FGameplayTag& StatusTag) const
{
	return Status(DFGameplayLocalTags::ContentIdFromTag(StatusTag));
}

FLinearColor UDFPaletteSettings::EliteRim(FName EliteId) const
{
	if (const FLinearColor* Rim = EliteRims.Find(EliteId))
	{
		return *Rim;
	}
	return Semantics.Elite;
}

FLinearColor UDFPaletteSettings::EliteRim(const FGameplayTag& EliteTag) const
{
	return EliteRim(DFGameplayLocalTags::ContentIdFromTag(EliteTag));
}

FLinearColor UDFPaletteSettings::TowerEnergyHue(FName TowerId) const
{
	if (const FLinearColor* Hue = TowerEnergy.Find(TowerId))
	{
		return *Hue;
	}
	return FLinearColor::White;
}
