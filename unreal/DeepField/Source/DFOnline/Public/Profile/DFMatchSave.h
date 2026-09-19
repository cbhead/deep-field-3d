#pragma once

#include "CoreMinimal.h"
#include "DFOnlineTypes.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "DFMatchSave.generated.h"

// C13: the between-wave autosave. The host writes one at every intermission and shares it with
// the lobby, so when the host drops any member can resume the match under a new host
// (DF.Message.HostMigrationOffered -> SaveResumed). This is the envelope; the payload structs
// below are skeletons that the owning workstreams fill in (each TODO names the owner) -- their
// serialisation is not WS-11's to decide.

/** One placed structure. TODO(WS-04): the full rig state (path levels, heat, charges, target) and the trap/barricade variants. */
USTRUCT(BlueprintType)
struct DFONLINE_API FDFSavedStructure
{
	GENERATED_BODY()

	UPROPERTY() int32 StructureId = 0;
	UPROPERTY() FName DefId;                 // tower / trap / barricade content id
	UPROPERTY() FName SocketId;
	UPROPERTY() FString OwnerId;             // FDFOnlineId string; empty = host-owned
	UPROPERTY() float HpFraction = 1.f;
	UPROPERTY() TMap<FGameplayTag, int32> PathLevels;   // DF.Tower.Path.* -> level
};

/** One player's loadout. TODO(WS-06 gunsmith, WS-03 player): weapons, attachments, ammo, melee, personal scrap, seat. */
USTRUCT(BlueprintType)
struct DFONLINE_API FDFSavedLoadout
{
	GENERATED_BODY()

	UPROPERTY() FDFOnlineId Player;
	UPROPERTY() FGameplayTag Faction;
	UPROPERTY() int32 Level = 1;
	UPROPERTY() int32 PersonalScrap = 0;
	UPROPERTY() TArray<FName> WeaponIds;
	UPROPERTY() TMap<FName, FString> WeaponBlueprints;   // weaponId -> serialised FDFBlueprint (WS-06 decides the form)
};

UCLASS(BlueprintType)
class DFONLINE_API UDFMatchSave : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY() int32 Version = CurrentVersion;

	// Identity of the match: enough for a new host to open the same map at the same point.
	UPROPERTY() FName MapId;
	UPROPERTY() FGameplayTag Tier;
	UPROPERTY() bool bEndless = false;
	UPROPERTY() int32 WaveIndex = 0;        // the next wave to start
	UPROPERTY() int32 Lap = 0;
	UPROPERTY() int32 Seed = 0;             // the wave plan / endless injection seed
	UPROPERTY() FDateTime SavedAt;
	UPROPERTY() FString HostBuild;          // must match on resume (the C14 handshake applies to the save too)
	UPROPERTY() FString ContentHash;
	UPROPERTY() FDFOnlineId Host;
	UPROPERTY() TArray<FDFOnlineId> Members;

	// Match state. TODO(DFMatch / WS-02): money, lives, threat, team scrap, lane and mutable edge states as the owners define them.
	UPROPERTY() int32 Money = 0;
	UPROPERTY() int32 Lives = 0;
	UPROPERTY() float Threat = 1.f;
	UPROPERTY() int32 TeamScrap = 0;
	UPROPERTY() TMap<FName, FName> MutableStates;   // mutable id -> state (WS-09/WS-24)

	UPROPERTY() TArray<FDFSavedStructure> Structures;   // TODO(WS-04)
	UPROPERTY() TArray<FDFSavedLoadout> Loadouts;       // TODO(WS-06, WS-03)

	/** True when this save can be resumed by the given build / content (the handshake, applied to the file). */
	bool IsCompatibleWith(const FString& Build, const FString& InContentHash) const
	{
		return Version == CurrentVersion && HostBuild == Build && ContentHash == InContentHash;
	}
};
