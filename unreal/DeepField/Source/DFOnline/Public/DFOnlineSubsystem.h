#pragma once

#include "CoreMinimal.h"
#include "DFJoinCode.h"
#include "DFOnlineTypes.h"
#include "GameplayTagContainer.h"
#include "Online/DFJoinSeams.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineServices.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DFOnlineSubsystem.generated.h"

class IDFSessionBackend;
namespace UE::Online
{
	struct FAccountInfo;
	struct FLobby;
	struct FLobbyMember;
	struct FAuthLoginStatusChanged;
	struct FLobbyJoined;
	struct FLobbyLeft;
	struct FLobbyMemberJoined;
	struct FLobbyMemberLeft;
	struct FLobbyLeaderChanged;
	struct FLobbyAttributesChanged;
	struct FLobbyInvitationAdded;
	struct FUILobbyJoinRequested;
}

// C14 -- the online facade (ADR-0003). Everything above this class talks Deep Field: identities,
// sessions, join codes, approvals, the handshake. Everything below it is UE::Online (OSSv2):
// GetServices(Default) resolves to the Null services until the EOS product exists and to
// OnlineServicesEOS afterwards, by config alone (unreal/PLAN/rfcs/needs-int-eos-config.md).
// The lobby is the party (C14 "party lobby; host = local"); the match itself is the listen
// server that IDFSessionBackend starts from it. Player identity is FDFOnlineId, never a name.
//
// Join-code flow (the host approves; B§5 "host trusted, clients not"):
//   host   CreateInviteOnlySession -> lobby {DFMap, DFTier, DFEndless, DFBuild, DFContentHash, DFJoinCode, DFApproved}
//   client JoinByCode(code)        -> FindLobbies(DFJoinCode == code) -> JoinLobby (a lobby member, not yet a player)
//   host   OnLobbyMemberJoined     -> DF.Message.JoinRequest + OnJoinRequest (the member is "pending")
//   host   ApproveJoin(id, true)   -> approved list (lobby attribute DFApproved), the code rotates, DF.Message.JoinApproved
//          ApproveJoin(id, false)  -> Kick
//   client sees itself in DFApproved -> DF.Message.JoinApproved -> JoinSession -> backend travel with MakeJoinOptions()
//   host   ADFGameMode::PreLogin   -> ValidateJoinOptions: versionMismatch | contentMismatch | notInvited | banned
// Invited friends (InviteFriend) are approved when the invite is sent.
UCLASS()
class DFONLINE_API UDFOnlineSubsystem : public UGameInstanceSubsystem, public IDFJoinValidator
{
	GENERATED_BODY()

public:
	static UDFOnlineSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- identity ----------------------------------------------------------------------------
	void Login(EDFLoginMethod Method = EDFLoginMethod::Auto, FDFOnlineResult OnDone = FDFOnlineResult());
	void Logout(FDFOnlineResult OnDone = FDFOnlineResult());
	const FDFOnlineIdentity& GetLocalIdentity() const { return Identity; }
	bool IsLoggedIn() const { return Identity.bLoggedIn; }
	/** "Null" or "Epic"; NAME_None before the services were touched. */
	FName GetProviderName() const;
	EDFConnectionState GetConnectionState() const { return State; }

	// ---- session -----------------------------------------------------------------------------
	void CreateInviteOnlySession(FName MapId, FGameplayTag Tier, bool bEndless, FDFOnlineResult OnDone = FDFOnlineResult());
	void InviteFriend(const FDFOnlineId& Friend, FDFOnlineResult OnDone = FDFOnlineResult());
	/** The live code (host only; empty otherwise). Rotates by itself when its lifetime runs out. */
	FString GetJoinCode();
	void RotateJoinCode();
	bool IsJoinCodeValid(const FString& Candidate) const;
	void JoinByCode(const FString& Code, FDFOnlineResult OnDone = FDFOnlineResult());
	void ApproveJoin(const FDFOnlineId& Requester, bool bApprove);
	const TArray<FDFJoinRequest>& GetPendingJoinRequests() const { return Pending; }
	/** Accepted invite / presence join / approved code: become a member if needed, then travel to the host. */
	void JoinSession(const FDFSessionInfo& Info, FDFOnlineResult OnDone = FDFOnlineResult());
	void Kick(const FDFOnlineId& Member, bool bBanForSession);
	void Leave();
	/** Host: start the listen server on the session's map through the backend. */
	bool LaunchMatch();
	const FDFSessionInfo& GetSession() const { return Session; }
	bool IsHosting() const { return Session.bValid && Session.bIsHost; }
	bool IsApproved(const FDFOnlineId& Id) const { return Approved.Contains(Id); }
	bool IsBanned(const FDFOnlineId& Id) const { return Banned.Contains(Id); }
	/** How long a join code lives without an approval (seconds; 0 = only rotates on approval). */
	void SetJoinCodeLifetime(double Seconds) { JoinCode.LifetimeSeconds = Seconds; }

	// ---- presence / remote config -------------------------------------------------------------
	void SetPresence(const FDFPresence& Presence);
	const FDFPresence& GetPresence() const { return LastPresence; }
	/** Defaults until Title Storage is wired (EOS product); see GetRemoteConfig in the .cpp for the file it will read. */
	FDFRemoteConfig GetRemoteConfig() const { return RemoteConfig; }

	// ---- version + content-hash handshake (B§5.9) -------------------------------------------
	/** UDFGameInstance::GetBuildLabel: ProjectVersion plus the changelist when the build has one. */
	static FString GetBuildLabel();
	/** SHA-1 of the imported content tables (UDFContentHash); computed on first use, cached. */
	FString GetContentHash() const;
	bool IsContentHashComplete() const;
	void RefreshContentHash() const;
	/** "?build=<label>?contentHash=<hex>?onlineId=<id>" -- appended to every client travel URL. */
	FString MakeJoinOptions() const;
	/** The host-side check ADFGameMode::PreLogin runs on the travel options. OutError is the
	 *  DF.Message.JoinRejected reason: versionMismatch | contentMismatch | notInvited | banned. */
	UFUNCTION(BlueprintCallable, Category = "DF|Online")
	bool ValidateJoinOptions(const FString& Options, FString& OutError);
	/** IDFJoinValidator (C14 seam, DFCore): ADFGameMode::PreLogin asks this; forwards to ValidateJoinOptions. */
	virtual bool ValidateJoin(const FString& Options, FString& OutReason) override { return ValidateJoinOptions(Options, OutReason); }

	// ---- backend -------------------------------------------------------------------------------
	void SetSessionBackend(TSharedPtr<IDFSessionBackend> InBackend);
	IDFSessionBackend* GetSessionBackend() const { return Backend.Get(); }

	// ---- delegates (C14) ------------------------------------------------------------------------
	FDFOnSessionChanged OnSessionChanged;
	FDFOnJoinRequest OnJoinRequest;
	FDFOnLoginChanged OnLoginChanged;
	FDFOnInviteReceived OnInviteReceived;

	// URL option keys and lobby attribute ids (the schema in DefaultEngine.ini [OnlineServices.Lobbies]).
	static const TCHAR* OptBuild;         // "build"
	static const TCHAR* OptContentHash;   // "contentHash"
	static const TCHAR* OptOnlineId;      // "onlineId"
	static const FName AttrMap, AttrTier, AttrEndless, AttrBuild, AttrContentHash, AttrJoinCode, AttrApproved, AttrMemberName;
	static const FName LobbySchemaId;     // "DFLobby"
	static const FName LobbyLocalName;    // "DFSession"
	static const FName ReasonVersionMismatch, ReasonContentMismatch, ReasonNotInvited, ReasonBanned;

private:
	// services
	bool EnsureServices();
	UE::Online::IOnlineServices* GetServices() const { return Services.Get(); }
	void BindServiceEvents();

	// login
	void LoginStep(TArray<EDFLoginMethod> Ladder, int32 Index, FDFOnlineResult OnDone);
	bool TryAdoptPlatformUser();
	void CompleteLogin(const UE::Online::FAccountInfo& Info);
	void FailLogin(const FString& Reason, FDFOnlineResult OnDone);
	void HandleLoginStatusChanged(const UE::Online::FAuthLoginStatusChanged& Event);

	// session plumbing
	void ApplyLobby(const UE::Online::FLobby& Lobby, bool bIsHost);
	void ReadLobbyAttributes(const UE::Online::FLobby& Lobby);
	void ResetSession();
	void PublishJoinCode();
	void PublishApprovedList();
	void WriteLobbyAttribute(FName Key, const FString& Value);
	FString MemberDisplayName(const UE::Online::FLobbyMember& Member) const;
	void HandleLobbyMemberJoined(const UE::Online::FLobbyMemberJoined& Event);
	void HandleLobbyMemberLeft(const UE::Online::FLobbyMemberLeft& Event);
	void HandleLobbyLeft(const UE::Online::FLobbyLeft& Event);
	void HandleLobbyLeaderChanged(const UE::Online::FLobbyLeaderChanged& Event);
	void HandleLobbyAttributesChanged(const UE::Online::FLobbyAttributesChanged& Event);
	void HandleLobbyInvitationAdded(const UE::Online::FLobbyInvitationAdded& Event);
	void HandleUILobbyJoinRequested(const UE::Online::FUILobbyJoinRequested& Event);
	void ConnectToSession(FDFOnlineResult OnDone);
	FDFSessionInfo SessionFromLobby(const UE::Online::FLobby& Lobby) const;

	// messages
	void SetConnectionState(EDFConnectionState NewState);
	void BroadcastRejected(FName Reason, const FString& Subject, const FString& Detail);
	template <typename T> void BroadcastMessage(const FGameplayTag& Tag, const T& Payload);
	UWorld* GetGameWorld() const;
	static double Now();

	TSharedPtr<UE::Online::IOnlineServices> Services;
	TArray<UE::Online::FOnlineEventDelegateHandle> EventHandles;
	TSharedPtr<IDFSessionBackend> Backend;

	FDFOnlineIdentity Identity;
	FDFSessionInfo Session;
	FDFJoinCodeRotator JoinCode;
	int32 PublishedJoinCodeGeneration = 0;
	TArray<FDFJoinRequest> Pending;
	TSet<FDFOnlineId> Approved;
	TSet<FDFOnlineId> Banned;
	FDFPresence LastPresence;
	FDFRemoteConfig RemoteConfig;
	EDFConnectionState State = EDFConnectionState::Offline;

	// Computed on first use (the tables may load after this subsystem), so the cache is mutable behind const readers.
	mutable FString CachedContentHash;
	mutable bool bContentHashComputed = false;
	mutable bool bContentHashComplete = false;
};
