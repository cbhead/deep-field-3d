#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Online/CoreOnline.h"
#include "DFOnlineTypes.generated.h"

// C14 value types (unreal/PLAN/CONTRACTS/online.md). Identity everywhere in gameplay is the
// provider's account id -- the EOS ProductUserId once the product exists, the Null services'
// "OSSV2-<host>-<guid>" until then -- carried as a string so it survives save games, replicated
// structs and logs. Display names are for screens only and never identify anyone.

UENUM(BlueprintType)
enum class EDFLoginMethod : uint8
{
	/** The ladder: command-line credentials (the Epic Games Store launcher's exchange code), then the
	 *  SDK's persistent token, then the account portal when a human is present. The Null services
	 *  pre-register the platform user, so Auto adopts it without a Login call. */
	Auto,
	/** -AUTH_TYPE=exchangecode -AUTH_PASSWORD=<code>, as the EGS launcher passes them. */
	ExchangeCode,
	/** The EOS Developer Authentication Tool: -DFDevAuth=<host:port>/<credential name>. */
	Developer,
	/** The refresh token the EOS SDK cached from the previous login. */
	Persistent,
	/** Browser / overlay login. */
	AccountPortal,
};

UENUM(BlueprintType)
enum class EDFConnectionState : uint8
{
	Offline,
	LoggingIn,
	LoggedIn,
	Hosting,
	Joining,
	Connected,
	Failed,
};

USTRUCT(BlueprintType)
struct DFONLINE_API FDFOnlineId
{
	GENERATED_BODY()

	/** The provider's string form of the account id. Empty means nobody. */
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online")
	FString Id;

	/** UE::Online::EOnlineServices of the provider that minted it, so the id round-trips to an FAccountId. */
	UPROPERTY()
	uint8 Services = 0;

	FDFOnlineId() = default;
	explicit FDFOnlineId(const FString& InId, uint8 InServices = 0) : Id(InId), Services(InServices) {}

	bool IsValid() const { return !Id.IsEmpty(); }
	const FString& ToString() const { return Id; }

	UE::Online::FAccountId ToAccountId() const;
	static FDFOnlineId FromAccountId(const UE::Online::FAccountId& AccountId);

	bool operator==(const FDFOnlineId& Other) const { return Id == Other.Id; }
	bool operator!=(const FDFOnlineId& Other) const { return Id != Other.Id; }
	friend uint32 GetTypeHash(const FDFOnlineId& Key) { return GetTypeHash(Key.Id); }
};

USTRUCT(BlueprintType)
struct DFONLINE_API FDFOnlineIdentity
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") FDFOnlineId Id;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") bool bLoggedIn = false;
	/** "Null" or "Epic" -- the OSSv2 provider behind the facade. */
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") FName Provider;
};

/** One party lobby (C14). The attributes are what the lobby advertises; the handshake compares Build and ContentHash. */
USTRUCT(BlueprintType)
struct DFONLINE_API FDFSessionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") bool bIsHost = false;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") FDFOnlineId Host;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") FName MapId;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") FGameplayTag Tier;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") bool bEndless = false;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") FString Build;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") FString ContentHash;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") int32 MaxPlayers = 4;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") TArray<FDFOnlineId> Members;
	/** What the services resolved for travel ("127.0.0.1:7777", "[EOS:<puid>]"); empty until JoinSession resolves it. */
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") FString ConnectString;

	/** The provider's lobby handle (not reflected; only meaningful in this process). */
	UE::Online::FLobbyId LobbyId;
};

/** A code-holder waiting for the host's verdict (DF.Message.JoinRequest -> ApproveJoin). */
USTRUCT(BlueprintType)
struct DFONLINE_API FDFJoinRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") FDFOnlineId Id;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") double RequestedAt = 0.0;
};

USTRUCT(BlueprintType)
struct DFONLINE_API FDFPresence
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "DF|Online") FName MapId;
	UPROPERTY(BlueprintReadWrite, Category = "DF|Online") int32 Wave = 0;
	UPROPERTY(BlueprintReadWrite, Category = "DF|Online") int32 Players = 0;
	UPROPERTY(BlueprintReadWrite, Category = "DF|Online") FString Status;   // "lobby", "wave", "intermission", "victory", ...
};

/** Title Storage remote config (kill switches, hotfix numbers). Defaults until EOS Title Storage exists. */
USTRUCT(BlueprintType)
struct DFONLINE_API FDFRemoteConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") bool bOnlineEnabled = true;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") bool bInvitesEnabled = true;
	/** Builds older than this are told to update (empty = no floor). */
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") FString MinimumBuild;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") int32 HotfixNumber = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") FString Notice;
	/** Named switches the game reads ("elites", "boss", "vehicles" ...); absent = on. */
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") TMap<FName, bool> KillSwitches;
	/** True once the values came from the service rather than the defaults. */
	UPROPERTY(BlueprintReadOnly, Category = "DF|Online") bool bFromService = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnSessionChanged, const FDFSessionInfo&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDFOnJoinRequest, const FDFOnlineId& /*Requester*/, const FString& /*DisplayName*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnLoginChanged, const FDFOnlineIdentity&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnInviteReceived, const FDFSessionInfo&);
/** Per-call completion: bOk and a machine-readable reason when not ("noServices", "notLoggedIn", "noSuchCode", or the provider's error id). */
DECLARE_DELEGATE_TwoParams(FDFOnlineResult, bool /*bOk*/, const FString& /*Reason*/);
