#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "DFPaletteSettings.generated.h"

// C8 / ADR-0017 — docs/palette.json as a developer-settings object. Gameplay code never spells a
// colour: it asks GetDefault<UDFPaletteSettings>(). The defaults below ARE palette.json 1.0
// (2026-09-06) transcribed field for field; a re-import (import_tokens.py, WS-01) may override
// any of them in DefaultGame.ini. Hex strings are sRGB and become linear at construction.
// Every FLinearColor member carries an explicit initialiser: FLinearColor's default constructor
// leaves it uninitialised and the reflection system's struct-initialisation check (LogClass:
// "StructProperty ... is not initialized properly") reports each such member at every start.
// palette.json lists only forge / ember / tempest under factions; glacier and specter get no
// entry until the palette does.

USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFPaletteSemantics
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FLinearColor Danger = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Warning = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Success = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Info = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Hp = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor HpLow = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Shield = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Armor = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Currency = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Upgrade = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Elite = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Boss = FLinearColor::Black;
};

USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFPaletteFaction
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FLinearColor Accent = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Ability = FLinearColor::Black;
};

USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFPaletteScrap
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FLinearColor Alloy = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Flux = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Plating = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Gravium = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Primecore = FLinearColor::Black;
};

/** A status swatch: a tint that repaints the albedo (chill, freeze, shred, tar), an emissive
 *  glow that rides on it (burn, shock: bEmissive with the tint as the glow), or an emissive-only
 *  status (mark, reveal: bHasTint false, Emissive set). */
USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFPaletteStatus
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) bool bHasTint = true;
	UPROPERTY(EditAnywhere) FLinearColor Tint = FLinearColor::Black;
	UPROPERTY(EditAnywhere) bool bEmissive = false;
	UPROPERTY(EditAnywhere) FLinearColor Emissive = FLinearColor::Black;
};

USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFPaletteEnemies
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FLinearColor Base = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor HpLow = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor WeakPoint = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor WardenShield = FLinearColor::Black;
};

USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFPaletteTowerBody
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FLinearColor SteelHull = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor SteelPlate = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Chassis = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Trim = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Chrome = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Brass = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Rubber = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Hazard = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor OpticGlass = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Ceramic = FLinearColor::Black;
	UPROPERTY(EditAnywhere) FLinearColor Concrete = FLinearColor::Black;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Deep Field Palette"))
class DFGAMEPLAY_API UDFPaletteSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDFPaletteSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** "#RRGGBB" (sRGB) -> linear. The one place hex is parsed. */
	static FLinearColor FromHex(const TCHAR* Hex);

	UPROPERTY(EditAnywhere, Config, Category = "Semantics") FDFPaletteSemantics Semantics;
	/** Keyed by faction content id (forge, ember, tempest, ...). */
	UPROPERTY(EditAnywhere, Config, Category = "Factions") TMap<FName, FDFPaletteFaction> Factions;
	UPROPERTY(EditAnywhere, Config, Category = "Scrap") FDFPaletteScrap Scrap;
	/** Keyed by status content id (burn, chill, ...). */
	UPROPERTY(EditAnywhere, Config, Category = "Statuses") TMap<FName, FDFPaletteStatus> Statuses;
	UPROPERTY(EditAnywhere, Config, Category = "Enemies") FDFPaletteEnemies Enemies;
	/** Keyed by elite content id (gilded, juggernaut, voltaic, swift, umbral). */
	UPROPERTY(EditAnywhere, Config, Category = "Enemies") TMap<FName, FLinearColor> EliteRims;
	UPROPERTY(EditAnywhere, Config, Category = "Towers") FDFPaletteTowerBody TowerBody;
	/** Keyed by tower content id; each tower's one energy hue. */
	UPROPERTY(EditAnywhere, Config, Category = "Towers") TMap<FName, FLinearColor> TowerEnergy;
	/** palette.json towers.emissiveIntensity — the EnergyIntensity default. */
	UPROPERTY(EditAnywhere, Config, Category = "Towers") float EnergyEmissiveIntensity = 0.95f;
	/** StatusWeight while a status tint is active (TintableView.cs blended at 0.7; C8 caps at 0.7). */
	UPROPERTY(EditAnywhere, Config, Category = "Statuses") float StatusTintWeight = 0.7f;
	UPROPERTY(EditAnywhere, Config, Category = "UI") TArray<FLinearColor> UIObsidian;
	UPROPERTY(EditAnywhere, Config, Category = "UI") TArray<FLinearColor> UISteel;
	UPROPERTY(EditAnywhere, Config, Category = "UI") TArray<FLinearColor> UIBrass;
	UPROPERTY(EditAnywhere, Config, Category = "UI") TArray<FLinearColor> UIArcane;

	/** The swatch for a status id, or null. */
	const FDFPaletteStatus* Status(FName StatusId) const { return Statuses.Find(StatusId); }
	/** The swatch for a DF.Status.* tag, or null. */
	const FDFPaletteStatus* Status(const FGameplayTag& StatusTag) const;
	/** Rim colour for an elite id / DF.Enemy.Elite.* tag; Semantics.Elite when unknown. */
	FLinearColor EliteRim(FName EliteId) const;
	FLinearColor EliteRim(const FGameplayTag& EliteTag) const;
	/** Energy hue for a tower id; white when unknown. */
	FLinearColor TowerEnergyHue(FName TowerId) const;
};
