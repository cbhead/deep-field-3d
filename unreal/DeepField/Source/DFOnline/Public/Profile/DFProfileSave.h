#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "DFProfileSave.generated.h"

// C13 -- the player profile (unreal/PLAN/CONTRACTS/profile.md). Settings are the player's;
// progression is written only from a host-computed FDFMatchRecord (B§5.6): a client never
// declares its own XP. Every layout change bumps CurrentVersion and adds a step to
// UDFProfileMigrations; the file on disk is never read by a newer field without one.

/** A weapon blueprint: slot tag (DF.Weapon.Slot.*) -> attachment id, plus the ammo id. */
USTRUCT(BlueprintType)
struct DFONLINE_API FDFBlueprint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "DF|Profile") TMap<FGameplayTag, FName> Attachments;
	UPROPERTY(BlueprintReadWrite, Category = "DF|Profile") FName Ammo;

	bool operator==(const FDFBlueprint& Other) const { return Ammo == Other.Ammo && Attachments.OrderIndependentCompareEqual(Other.Attachments); }
};

/** What the host computed when a match ended. XP is per source (the faction played, later
 *  co-op bonuses) and capped at MaxXpPerSource before it is applied, whatever the host said. */
USTRUCT(BlueprintType)
struct DFONLINE_API FDFMatchRecord
{
	GENERATED_BODY()

	static constexpr int32 MaxXpPerSource = 60;

	UPROPERTY(BlueprintReadWrite, Category = "DF|Profile") FName MapId;
	UPROPERTY(BlueprintReadWrite, Category = "DF|Profile") FGameplayTag Tier;
	UPROPERTY(BlueprintReadWrite, Category = "DF|Profile") bool bEndless = false;
	UPROPERTY(BlueprintReadWrite, Category = "DF|Profile") int32 BestWave = 0;
	UPROPERTY(BlueprintReadWrite, Category = "DF|Profile") bool bVictory = false;
	/** Faction tag (or a later source tag) -> XP earned. */
	UPROPERTY(BlueprintReadWrite, Category = "DF|Profile") TMap<FGameplayTag, int32> XpBySource;
	UPROPERTY(BlueprintReadWrite, Category = "DF|Profile") FDateTime PlayedAt;
	UPROPERTY(BlueprintReadWrite, Category = "DF|Profile") FString HostBuild;
	/** The host's online id (C14) so a record can be traced; signed at Level 3. */
	UPROPERTY(BlueprintReadWrite, Category = "DF|Profile") FString HostId;

	/** Clamp every source into [0, MaxXpPerSource]. Returns true if anything changed. */
	bool CapXp();
};

UCLASS(BlueprintType)
class DFONLINE_API UDFProfileSave : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;
	static constexpr int32 MaxRecentMatches = 20;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DF|Profile") int32 Version = CurrentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Profile") FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Profile") FGameplayTag PreferredFaction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Profile") FString LastJoinCode;

	// Settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Settings") bool bShowDamageNumbers = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Settings") float MouseSensitivity = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Settings") int32 FieldOfView = 90;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Settings") float HudScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Settings") bool bScreenShake = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Settings") bool bReduceFlashes = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Settings") bool bTeammateOutlines = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Settings") float MasterVolume = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Settings") float MusicVolume = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Settings") float SfxVolume = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Settings") float VoiceVolume = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Settings") FName ScalabilityTier = TEXT("Medium");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Settings") float TsrPercent = 67.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Settings") bool bFlashlightAuto = true;

	// Progression (Level 1: local; written only through ApplyMatchRecord)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DF|Progression") TMap<FGameplayTag, int32> FactionXp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DF|Progression") TArray<FName> ClearedSectors;
	/** Key "<mapId>" for the campaign, "<mapId>:endless" for endless. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DF|Progression") TMap<FName, int32> BestWave;
	/** Per sector: the hardest tier cleared. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DF|Progression") TMap<FName, FGameplayTag> HighestTier;
	/** weaponId -> blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Progression") TMap<FName, FDFBlueprint> Blueprints;
	/** The last MaxRecentMatches host-computed records, newest first. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DF|Progression") TArray<FDFMatchRecord> RecentMatches;

	/** Fold a host record in: capped XP, sector cleared on a campaign victory, best wave, highest tier, recent list. */
	void ApplyMatchRecord(const FDFMatchRecord& Record);

	/** The BestWave key for a map / mode pair. */
	static FName BestWaveKey(FName MapId, bool bEndless);

	/** Standard < Hardened < Assault; unknown tags rank below Standard. */
	static int32 TierRank(const FGameplayTag& Tier);

	/** Field-by-field equality (tests and the round-trip check). */
	bool IsSameAs(const UDFProfileSave& Other) const;
};
