#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DFJoinSeams.generated.h"

// C14 join seams (contract-append, INT ruling R3, 2026-09-25). ADFGameMode::PreLogin (DFMatch, layer 4)
// must ask the online layer (DFOnline, layer 5) whether a traveller may join, and layering forbids the
// direct call — so the question is asked through these interfaces, which DFCore owns and anything may
// implement. PreLogin walks the game instance's subsystems and asks every implementer; the first "no"
// refuses the join, and its reason becomes the PreLogin error the client receives.
//
//   IDFJoinValidator    the handshake: version, content hash, invited/approved, session ban
//                       (UDFOnlineSubsystem, WS-11). Reasons: versionMismatch | contentMismatch |
//                       notInvited | banned — the DF.Message.JoinRejected vocabulary.
//   IDFSanctionsCheck   platform sanctions (EOS PlayerSanctions at L2). No implementer at L1: pass-through.
//   IDFAntiCheatCheck   anti-cheat admission (EAC at L3). No implementer at L1: pass-through.
//
// The traveller's online id is the "onlineId" URL option UDFOnlineSubsystem::MakeJoinOptions writes;
// DFJoinSeams::OptOnlineId names the key so no implementer needs DFOnline to find it.

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDFJoinValidator : public UInterface
{
	GENERATED_BODY()
};

class DFCORE_API IDFJoinValidator
{
	GENERATED_BODY()

public:
	/** The host-side handshake on the travel options. False refuses the join; OutReason is the
	 *  DF.Message.JoinRejected reason and becomes PreLogin's error message. */
	virtual bool ValidateJoin(const FString& Options, FString& OutReason) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDFSanctionsCheck : public UInterface
{
	GENERATED_BODY()
};

class DFCORE_API IDFSanctionsCheck
{
	GENERATED_BODY()

public:
	/** False when the platform has sanctioned this account; OutReason is the rejection reason. */
	virtual bool PassesSanctions(const FString& OnlineId, FString& OutReason) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDFAntiCheatCheck : public UInterface
{
	GENERATED_BODY()
};

class DFCORE_API IDFAntiCheatCheck
{
	GENERATED_BODY()

public:
	/** False when anti-cheat refuses this client; OutReason is the rejection reason. */
	virtual bool PassesAntiCheat(const FString& OnlineId, FString& OutReason) = 0;
};

namespace DFJoinSeams
{
	/** The URL option carrying the traveller's online id (UDFOnlineSubsystem::OptOnlineId spells the same key). */
	inline constexpr const TCHAR* OptOnlineId = TEXT("onlineId");
}
