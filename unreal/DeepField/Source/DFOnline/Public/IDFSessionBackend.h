#pragma once

#include "CoreMinimal.h"

class UWorld;

// C14's transport seam. Level 1 is a listen host (ADR-0004): over the Ip net driver today and,
// once the EOS product exists, the EOS P2P net driver with relays forced -- same backend, a
// config change (unreal/PLAN/rfcs/needs-int-eos-config.md). Level 2 adds a dedicated-server
// backend behind this same interface; the facade never learns which one it is talking to.
class DFONLINE_API IDFSessionBackend
{
public:
	virtual ~IDFSessionBackend() = default;

	virtual FName GetName() const = 0;

	/** Host: travel to MapId as a listen server ("<map>?listen<Options>"). */
	virtual bool StartHost(UWorld* World, FName MapId, const FString& Options) = 0;

	/** Client: travel to the host. ConnectString is what the services resolved ("127.0.0.1:7777",
	 *  "[EOS:<puid>]"); Options carries the C14 handshake (UDFOnlineSubsystem::MakeJoinOptions). */
	virtual bool Join(UWorld* World, const FString& ConnectString, const FString& Options) = 0;

	/** Drop the connection / stop listening and return to the front end. Safe to call when idle. */
	virtual void Leave(UWorld* World) = 0;

	/** The net driver this backend expects (the smoke asserts it; the EOS switch changes it). */
	virtual FName GetNetDriverClassName() const = 0;

	virtual bool IsDedicated() const { return false; }
};
