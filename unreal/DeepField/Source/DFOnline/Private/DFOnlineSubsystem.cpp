#include "DFOnlineSubsystem.h"

#include "Backends/DFSessionBackendListenP2P.h"
#include "DFContentHash.h"
#include "DFGameInstance.h"
#include "DFGameplayTags.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Kismet/GameplayStatics.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Online/Auth.h"
#include "Online/Lobbies.h"
#include "Online/OnlineResult.h"
#include "Online/Presence.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFOnline, Log, All);

using namespace UE::Online;

const TCHAR* UDFOnlineSubsystem::OptBuild = TEXT("build");
const TCHAR* UDFOnlineSubsystem::OptContentHash = TEXT("contentHash");
const TCHAR* UDFOnlineSubsystem::OptOnlineId = TEXT("onlineId");
const FName UDFOnlineSubsystem::AttrMap(TEXT("DFMap"));
const FName UDFOnlineSubsystem::AttrTier(TEXT("DFTier"));
const FName UDFOnlineSubsystem::AttrEndless(TEXT("DFEndless"));
const FName UDFOnlineSubsystem::AttrBuild(TEXT("DFBuild"));
const FName UDFOnlineSubsystem::AttrContentHash(TEXT("DFContentHash"));
const FName UDFOnlineSubsystem::AttrJoinCode(TEXT("DFJoinCode"));
const FName UDFOnlineSubsystem::AttrApproved(TEXT("DFApproved"));
const FName UDFOnlineSubsystem::AttrMemberName(TEXT("DFName"));
const FName UDFOnlineSubsystem::LobbySchemaId(TEXT("DFLobby"));
const FName UDFOnlineSubsystem::LobbyLocalName(TEXT("DFSession"));
const FName UDFOnlineSubsystem::ReasonVersionMismatch(TEXT("versionMismatch"));
const FName UDFOnlineSubsystem::ReasonContentMismatch(TEXT("contentMismatch"));
const FName UDFOnlineSubsystem::ReasonNotInvited(TEXT("notInvited"));
const FName UDFOnlineSubsystem::ReasonBanned(TEXT("banned"));

namespace
{
	constexpr int32 GMaxPlayers = 4;
	const TCHAR* GApprovedSeparator = TEXT(",");

	FGameplayTag ConnectionTag(EDFConnectionState State)
	{
		// Config/Tags/DF_Online.ini (WS-11's own append-only tag list); a missing ini yields an empty tag, never a crash.
		const TCHAR* Leaf = TEXT("Offline");
		switch (State)
		{
		case EDFConnectionState::LoggingIn: Leaf = TEXT("LoggingIn"); break;
		case EDFConnectionState::LoggedIn:  Leaf = TEXT("LoggedIn"); break;
		case EDFConnectionState::Hosting:   Leaf = TEXT("Hosting"); break;
		case EDFConnectionState::Joining:   Leaf = TEXT("Joining"); break;
		case EDFConnectionState::Connected: Leaf = TEXT("Connected"); break;
		case EDFConnectionState::Failed:    Leaf = TEXT("Failed"); break;
		default: break;
		}
		return FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("DF.Online.Connection.%s"), Leaf)), /*ErrorIfNotFound*/ false);
	}

	FString AttributeString(const TMap<FSchemaAttributeId, FSchemaVariant>& Attributes, FName Key)
	{
		const FSchemaVariant* Value = Attributes.Find(Key);
		return Value && Value->GetType() == ESchemaAttributeType::String ? Value->GetString() : FString();
	}

	bool AttributeBool(const TMap<FSchemaAttributeId, FSchemaVariant>& Attributes, FName Key)
	{
		const FSchemaVariant* Value = Attributes.Find(Key);
		return Value && Value->GetType() == ESchemaAttributeType::Bool ? Value->GetBoolean() : false;
	}
}

UDFOnlineSubsystem* UDFOnlineSubsystem::Get(const UObject* WorldContext)
{
	if (const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr)
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UDFOnlineSubsystem>();
		}
	}
	return nullptr;
}

void UDFOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Nothing touches the services here: they are created on first use so a headless test or a
	// front-end that never goes online pays nothing, and so a missing provider is a logged refusal
	// at Login rather than a failed game-instance start.
	Backend = MakeShared<FDFSessionBackendListenP2P>();
}

void UDFOnlineSubsystem::Deinitialize()
{
	EventHandles.Reset();   // unbinds every services event
	if (Session.bValid && Services)
	{
		if (ILobbiesPtr Lobbies = Services->GetLobbiesInterface())
		{
			Lobbies->LeaveLobby({ Identity.Id.ToAccountId(), Session.LobbyId });
		}
	}
	ResetSession();
	Services.Reset();
	Backend.Reset();
	Super::Deinitialize();
}

// ---- services --------------------------------------------------------------------------------

bool UDFOnlineSubsystem::EnsureServices()
{
	if (Services)
	{
		return true;
	}
	Services = UE::Online::GetServices(EOnlineServices::Default);
	if (!Services)
	{
		// No [OnlineServices] DefaultServices in the ini: Null is the Level 1 default until the
		// EOS product exists (ADR-0003), so the facade behaves the same with or without the line.
		Services = UE::Online::GetServices(EOnlineServices::Null);
		if (Services)
		{
			UE_LOG(LogDFOnline, Log, TEXT("online services: DefaultServices not configured, using Null"));
		}
	}
	if (!Services)
	{
		UE_LOG(LogDFOnline, Warning, TEXT("online services: no provider available (OnlineServicesNull plugin missing?)"));
		return false;
	}
	UE_LOG(LogDFOnline, Log, TEXT("online services: %s"), LexToString(Services->GetServicesProvider()));
	return true;
}

FName UDFOnlineSubsystem::GetProviderName() const
{
	return Services ? FName(LexToString(Services->GetServicesProvider())) : NAME_None;
}

void UDFOnlineSubsystem::BindServiceEvents()
{
	if (EventHandles.Num() > 0 || !Services)
	{
		return;
	}
	if (IAuthPtr Auth = Services->GetAuthInterface())
	{
		EventHandles.Add(Auth->OnLoginStatusChanged().Add(this, [this](const FAuthLoginStatusChanged& E) { HandleLoginStatusChanged(E); }));
	}
	if (ILobbiesPtr Lobbies = Services->GetLobbiesInterface())
	{
		EventHandles.Add(Lobbies->OnLobbyMemberJoined().Add(this, [this](const FLobbyMemberJoined& E) { HandleLobbyMemberJoined(E); }));
		EventHandles.Add(Lobbies->OnLobbyMemberLeft().Add(this, [this](const FLobbyMemberLeft& E) { HandleLobbyMemberLeft(E); }));
		EventHandles.Add(Lobbies->OnLobbyLeft().Add(this, [this](const FLobbyLeft& E) { HandleLobbyLeft(E); }));
		EventHandles.Add(Lobbies->OnLobbyLeaderChanged().Add(this, [this](const FLobbyLeaderChanged& E) { HandleLobbyLeaderChanged(E); }));
		EventHandles.Add(Lobbies->OnLobbyAttributesChanged().Add(this, [this](const FLobbyAttributesChanged& E) { HandleLobbyAttributesChanged(E); }));
		EventHandles.Add(Lobbies->OnLobbyInvitationAdded().Add(this, [this](const FLobbyInvitationAdded& E) { HandleLobbyInvitationAdded(E); }));
		EventHandles.Add(Lobbies->OnUILobbyJoinRequested().Add(this, [this](const FUILobbyJoinRequested& E) { HandleUILobbyJoinRequested(E); }));
	}
}

// ---- identity ----------------------------------------------------------------------------------

void UDFOnlineSubsystem::Login(EDFLoginMethod Method, FDFOnlineResult OnDone)
{
	if (IsLoggedIn())
	{
		OnDone.ExecuteIfBound(true, FString());
		return;
	}
	if (!EnsureServices())
	{
		FailLogin(TEXT("noServices"), OnDone);
		return;
	}
	SetConnectionState(EDFConnectionState::LoggingIn);

	// The Null services register the platform user at startup and their Login is NotImplemented;
	// EOS reports an account that PersistentAuth already restored the same way. Either is a login.
	if (TryAdoptPlatformUser())
	{
		OnDone.ExecuteIfBound(true, FString());
		return;
	}

	TArray<EDFLoginMethod> Ladder;
	if (Method == EDFLoginMethod::Auto)
	{
		// Command-line credentials (the EGS launcher), then the cached token, then a human.
		Ladder = { EDFLoginMethod::Auto, EDFLoginMethod::Persistent };
		if (!FApp::IsUnattended())
		{
			Ladder.Add(EDFLoginMethod::AccountPortal);
		}
	}
	else
	{
		Ladder = { Method };
	}
	LoginStep(MoveTemp(Ladder), 0, OnDone);
}

void UDFOnlineSubsystem::LoginStep(TArray<EDFLoginMethod> Ladder, int32 Index, FDFOnlineResult OnDone)
{
	IAuthPtr Auth = Services ? Services->GetAuthInterface() : nullptr;
	if (!Auth || !Ladder.IsValidIndex(Index))
	{
		FailLogin(Auth ? TEXT("noCredentials") : TEXT("noAuthInterface"), OnDone);
		return;
	}
	const EDFLoginMethod Method = Ladder[Index];

	FAuthLogin::Params Params;
	Params.PlatformUserId = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
	switch (Method)
	{
	case EDFLoginMethod::Auto:
		Params.CredentialsType = LoginCredentialsType::Auto;   // EOS reads -AUTH_TYPE/-AUTH_LOGIN/-AUTH_PASSWORD itself
		break;
	case EDFLoginMethod::ExchangeCode:
	{
		FString Code;
		FParse::Value(FCommandLine::Get(), TEXT("AUTH_PASSWORD="), Code);
		Params.CredentialsType = LoginCredentialsType::ExchangeCode;
		Params.CredentialsToken.Emplace<FString>(Code);
		break;
	}
	case EDFLoginMethod::Developer:
	{
		// -DFDevAuth=<host:port>/<credential> ; defaults match the Dev Auth Tool's own defaults.
		FString Spec = TEXT("localhost:8081/DFDev");
		FParse::Value(FCommandLine::Get(), TEXT("DFDevAuth="), Spec);
		FString Address, Credential;
		if (!Spec.Split(TEXT("/"), &Address, &Credential))
		{
			Address = Spec;
			Credential = TEXT("DFDev");
		}
		Params.CredentialsType = LoginCredentialsType::Developer;
		Params.CredentialsId = Address;
		Params.CredentialsToken.Emplace<FString>(Credential);
		break;
	}
	case EDFLoginMethod::Persistent:
		Params.CredentialsType = LoginCredentialsType::PersistentAuth;
		break;
	case EDFLoginMethod::AccountPortal:
		Params.CredentialsType = LoginCredentialsType::AccountPortal;
		break;
	}

	UE_LOG(LogDFOnline, Log, TEXT("login: %s via %s"), *Params.CredentialsType.ToString(), *GetProviderName().ToString());
	Auth->Login(MoveTemp(Params)).OnComplete(this, [this, Ladder = MoveTemp(Ladder), Index, OnDone](const TOnlineResult<FAuthLogin>& Result) mutable
	{
		if (Result.IsOk())
		{
			CompleteLogin(*Result.GetOkValue().AccountInfo);
			OnDone.ExecuteIfBound(true, FString());
			return;
		}
		const FOnlineError& Error = Result.GetErrorValue();
		UE_LOG(LogDFOnline, Log, TEXT("login step %d failed: %s"), Index, *Error.GetLogString());
		if (Ladder.IsValidIndex(Index + 1))
		{
			LoginStep(MoveTemp(Ladder), Index + 1, OnDone);
		}
		else
		{
			FailLogin(Error.GetErrorId(), OnDone);
		}
	});
}

bool UDFOnlineSubsystem::TryAdoptPlatformUser()
{
	IAuthPtr Auth = Services ? Services->GetAuthInterface() : nullptr;
	if (!Auth)
	{
		return false;
	}
	FAuthGetLocalOnlineUserByPlatformUserId::Params Params;
	Params.PlatformUserId = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
	TOnlineResult<FAuthGetLocalOnlineUserByPlatformUserId> Result = Auth->GetLocalOnlineUserByPlatformUserId(MoveTemp(Params));
	if (Result.IsOk() && IsOnlineStatus(Result.GetOkValue().AccountInfo->LoginStatus))
	{
		CompleteLogin(*Result.GetOkValue().AccountInfo);
		return true;
	}
	// No account on the primary user: any account the provider already holds will do (a headless
	// run whose input-device mapper handed the Null user to another platform user id).
	TOnlineResult<FAuthGetAllLocalOnlineUsers> All = Auth->GetAllLocalOnlineUsers({});
	if (All.IsOk())
	{
		for (const TSharedRef<FAccountInfo>& Account : All.GetOkValue().AccountInfo)
		{
			if (IsOnlineStatus(Account->LoginStatus))
			{
				CompleteLogin(*Account);
				return true;
			}
		}
	}
	return false;
}

void UDFOnlineSubsystem::CompleteLogin(const FAccountInfo& Info)
{
	Identity.Id = FDFOnlineId::FromAccountId(Info.AccountId);
	Identity.DisplayName.Reset();
	if (const FSchemaVariant* Name = Info.Attributes.Find(AccountAttributeData::DisplayName))
	{
		Identity.DisplayName = Name->GetString();
	}
	Identity.bLoggedIn = true;
	Identity.Provider = GetProviderName();
	BindServiceEvents();
	UE_LOG(LogDFOnline, Log, TEXT("logged in: %s (%s) via %s"), *Identity.Id.ToString(), *Identity.DisplayName, *Identity.Provider.ToString());
	SetConnectionState(EDFConnectionState::LoggedIn);
	OnLoginChanged.Broadcast(Identity);
}

void UDFOnlineSubsystem::FailLogin(const FString& Reason, FDFOnlineResult OnDone)
{
	UE_LOG(LogDFOnline, Warning, TEXT("login failed: %s"), *Reason);
	Identity = FDFOnlineIdentity();
	SetConnectionState(EDFConnectionState::Failed);
	OnLoginChanged.Broadcast(Identity);
	OnDone.ExecuteIfBound(false, Reason);
}

void UDFOnlineSubsystem::Logout(FDFOnlineResult OnDone)
{
	if (!IsLoggedIn() || !Services)
	{
		OnDone.ExecuteIfBound(true, FString());
		return;
	}
	Leave();
	IAuthPtr Auth = Services->GetAuthInterface();
	const FAccountId AccountId = Identity.Id.ToAccountId();
	// Local state clears now, whatever the provider says: a Null provider cannot log out
	// (NotImplemented) but the facade's answer to "who am I" must still become "nobody".
	Identity = FDFOnlineIdentity();
	SetConnectionState(EDFConnectionState::Offline);
	OnLoginChanged.Broadcast(Identity);
	if (!Auth)
	{
		OnDone.ExecuteIfBound(true, FString());
		return;
	}
	Auth->Logout({ AccountId }).OnComplete(this, [OnDone](const TOnlineResult<FAuthLogout>& Result)
	{
		if (Result.IsError())
		{
			UE_LOG(LogDFOnline, Verbose, TEXT("provider logout: %s"), *Result.GetErrorValue().GetLogString());
		}
		OnDone.ExecuteIfBound(true, FString());
	});
}

void UDFOnlineSubsystem::HandleLoginStatusChanged(const FAuthLoginStatusChanged& Event)
{
	if (!IsLoggedIn() || FDFOnlineId::FromAccountId(Event.AccountInfo->AccountId) != Identity.Id)
	{
		return;
	}
	if (!IsOnlineStatus(Event.LoginStatus))
	{
		UE_LOG(LogDFOnline, Warning, TEXT("logged out by the provider (%s)"), LexToString(Event.LoginStatus));
		Leave();
		Identity = FDFOnlineIdentity();
		SetConnectionState(EDFConnectionState::Offline);
		OnLoginChanged.Broadcast(Identity);
	}
}

// ---- session -----------------------------------------------------------------------------------

void UDFOnlineSubsystem::CreateInviteOnlySession(FName MapId, FGameplayTag Tier, bool bEndless, FDFOnlineResult OnDone)
{
	if (!IsLoggedIn() || !Services)
	{
		OnDone.ExecuteIfBound(false, TEXT("notLoggedIn"));
		return;
	}
	ILobbiesPtr Lobbies = Services->GetLobbiesInterface();
	if (!Lobbies)
	{
		OnDone.ExecuteIfBound(false, TEXT("noLobbies"));
		return;
	}
	if (Session.bValid)
	{
		Leave();
	}
	JoinCode.Reset();
	const FString FirstCode = JoinCode.Current(Now());
	PublishedJoinCodeGeneration = JoinCode.GetGeneration();

	FCreateLobby::Params Params;
	Params.LocalAccountId = Identity.Id.ToAccountId();
	Params.LocalName = LobbyLocalName;
	Params.SchemaId = LobbySchemaId;
	Params.bPresenceEnabled = true;     // the lobby friends see and the overlay joins
	Params.MaxMembers = GMaxPlayers;
	Params.JoinPolicy = ELobbyJoinPolicy::InvitationOnly;
	Params.Attributes.Add(AttrMap, FSchemaVariant(MapId.ToString()));
	Params.Attributes.Add(AttrTier, FSchemaVariant(Tier.ToString()));
	Params.Attributes.Add(AttrEndless, FSchemaVariant(bEndless));
	Params.Attributes.Add(AttrBuild, FSchemaVariant(GetBuildLabel()));
	Params.Attributes.Add(AttrContentHash, FSchemaVariant(GetContentHash()));
	Params.Attributes.Add(AttrJoinCode, FSchemaVariant(FirstCode));
	Params.Attributes.Add(AttrApproved, FSchemaVariant(FString()));
	Params.UserAttributes.Add(AttrMemberName, FSchemaVariant(Identity.DisplayName));

	UE_LOG(LogDFOnline, Log, TEXT("create session: map=%s tier=%s endless=%d"), *MapId.ToString(), *Tier.ToString(), bEndless);
	Lobbies->CreateLobby(MoveTemp(Params)).OnComplete(this, [this, OnDone](const TOnlineResult<FCreateLobby>& Result)
	{
		if (Result.IsError() || !Result.GetOkValue().Lobby)
		{
			const FString Reason = Result.IsError() ? Result.GetErrorValue().GetErrorId() : TEXT("noLobby");
			UE_LOG(LogDFOnline, Warning, TEXT("create session failed: %s"), *Reason);
			JoinCode.Reset();
			SetConnectionState(EDFConnectionState::LoggedIn);
			OnDone.ExecuteIfBound(false, Reason);
			return;
		}
		ApplyLobby(*Result.GetOkValue().Lobby, /*bIsHost*/ true);
		Approved.Reset();
		Banned.Reset();
		Pending.Reset();
		Approved.Add(Identity.Id);
		SetConnectionState(EDFConnectionState::Hosting);
		OnSessionChanged.Broadcast(Session);
		OnDone.ExecuteIfBound(true, FString());
	});
}

void UDFOnlineSubsystem::InviteFriend(const FDFOnlineId& Friend, FDFOnlineResult OnDone)
{
	if (!IsHosting() || !Services)
	{
		OnDone.ExecuteIfBound(false, TEXT("notHosting"));
		return;
	}
	ILobbiesPtr Lobbies = Services->GetLobbiesInterface();
	if (!Lobbies || !Friend.IsValid())
	{
		OnDone.ExecuteIfBound(false, TEXT("invalidTarget"));
		return;
	}
	// An invite is the host's approval: the friend may connect the moment they accept.
	Approved.Add(Friend);
	PublishApprovedList();
	Lobbies->InviteLobbyMember({ Identity.Id.ToAccountId(), Session.LobbyId, Friend.ToAccountId() }).OnComplete(this, [this, Friend, OnDone](const TOnlineResult<FInviteLobbyMember>& Result)
	{
		if (Result.IsError())
		{
			// Null has no invitations (NotImplemented); the approval stands so a LAN join still works.
			UE_LOG(LogDFOnline, Log, TEXT("invite %s: %s"), *Friend.ToString(), *Result.GetErrorValue().GetLogString());
			OnDone.ExecuteIfBound(false, Result.GetErrorValue().GetErrorId());
			return;
		}
		OnDone.ExecuteIfBound(true, FString());
	});
}

FString UDFOnlineSubsystem::GetJoinCode()
{
	if (!IsHosting())
	{
		return FString();
	}
	const FString Code = JoinCode.Current(Now());
	if (JoinCode.GetGeneration() != PublishedJoinCodeGeneration)
	{
		PublishJoinCode();   // the timer rotated it; tell the lobby
	}
	return Code;
}

void UDFOnlineSubsystem::RotateJoinCode()
{
	if (!IsHosting())
	{
		return;
	}
	JoinCode.Rotate(Now());
	PublishJoinCode();
	OnSessionChanged.Broadcast(Session);
}

bool UDFOnlineSubsystem::IsJoinCodeValid(const FString& Candidate) const
{
	return IsHosting() && JoinCode.Matches(Candidate, Now());
}

void UDFOnlineSubsystem::PublishJoinCode()
{
	PublishedJoinCodeGeneration = JoinCode.GetGeneration();
	WriteLobbyAttribute(AttrJoinCode, JoinCode.Current(Now()));
}

void UDFOnlineSubsystem::JoinByCode(const FString& RawCode, FDFOnlineResult OnDone)
{
	if (!IsLoggedIn() || !Services)
	{
		OnDone.ExecuteIfBound(false, TEXT("notLoggedIn"));
		return;
	}
	const FString Code = FDFJoinCode::Normalize(RawCode);
	if (!FDFJoinCode::IsWellFormed(Code))
	{
		OnDone.ExecuteIfBound(false, TEXT("malformedCode"));
		return;
	}
	ILobbiesPtr Lobbies = Services->GetLobbiesInterface();
	if (!Lobbies)
	{
		OnDone.ExecuteIfBound(false, TEXT("noLobbies"));
		return;
	}
	if (Session.bValid)
	{
		Leave();
	}
	SetConnectionState(EDFConnectionState::Joining);

	FFindLobbies::Params Params;
	Params.LocalAccountId = Identity.Id.ToAccountId();
	Params.MaxResults = 8;
	Params.Filters.Add(FFindLobbySearchFilter{ AttrJoinCode, ESchemaAttributeComparisonOp::Equals, FSchemaVariant(Code) });
	Lobbies->FindLobbies(MoveTemp(Params)).OnComplete(this, [this, Code, OnDone](const TOnlineResult<FFindLobbies>& Result)
	{
		if (Result.IsError())
		{
			UE_LOG(LogDFOnline, Warning, TEXT("join by code: search failed: %s"), *Result.GetErrorValue().GetLogString());
			SetConnectionState(EDFConnectionState::LoggedIn);
			OnDone.ExecuteIfBound(false, Result.GetErrorValue().GetErrorId());
			return;
		}
		// The provider filtered on EOS; the LAN search returns everything, so filter again here.
		TSharedPtr<const FLobby> Match;
		for (const TSharedRef<const FLobby>& Lobby : Result.GetOkValue().Lobbies)
		{
			if (AttributeString(Lobby->Attributes, AttrJoinCode) == Code)
			{
				Match = Lobby;
				break;
			}
		}
		if (!Match)
		{
			UE_LOG(LogDFOnline, Log, TEXT("join by code: no lobby for %s"), *Code);
			SetConnectionState(EDFConnectionState::LoggedIn);
			OnDone.ExecuteIfBound(false, TEXT("noSuchCode"));
			return;
		}
		FJoinLobby::Params Join;
		Join.LocalAccountId = Identity.Id.ToAccountId();
		Join.LocalName = LobbyLocalName;
		Join.LobbyId = Match->LobbyId;
		Join.bPresenceEnabled = true;
		Join.UserAttributes.Add(AttrMemberName, FSchemaVariant(Identity.DisplayName));
		Services->GetLobbiesInterface()->JoinLobby(MoveTemp(Join)).OnComplete(this, [this, OnDone](const TOnlineResult<FJoinLobby>& JoinResult)
		{
			if (JoinResult.IsError() || !JoinResult.GetOkValue().Lobby)
			{
				const FString Reason = JoinResult.IsError() ? JoinResult.GetErrorValue().GetErrorId() : TEXT("noLobby");
				UE_LOG(LogDFOnline, Warning, TEXT("join by code: join failed: %s"), *Reason);
				SetConnectionState(EDFConnectionState::LoggedIn);
				OnDone.ExecuteIfBound(false, Reason);
				return;
			}
			// A member now, but not yet a player: the host's ApproveJoin decides (DF.Message.JoinApproved).
			ApplyLobby(*JoinResult.GetOkValue().Lobby, /*bIsHost*/ false);
			OnSessionChanged.Broadcast(Session);
			OnDone.ExecuteIfBound(true, FString());
		});
	});
}

void UDFOnlineSubsystem::ApproveJoin(const FDFOnlineId& Requester, bool bApprove)
{
	if (!IsHosting() || !Requester.IsValid())
	{
		return;
	}
	Pending.RemoveAll([&Requester](const FDFJoinRequest& R) { return R.Id == Requester; });
	if (!bApprove)
	{
		Kick(Requester, /*bBanForSession*/ false);
		return;
	}
	Approved.Add(Requester);
	PublishApprovedList();
	// One code admits one player: whoever else was told it needs a new one from the host.
	JoinCode.Rotate(Now());
	PublishJoinCode();

	FDFMsg_Player Msg;
	Msg.Name = FName(*Requester.ToString());
	Msg.OnlineId = Requester.ToString();
	BroadcastMessage(DFTags::Message_JoinApproved, Msg);
	OnSessionChanged.Broadcast(Session);
}

void UDFOnlineSubsystem::JoinSession(const FDFSessionInfo& Info, FDFOnlineResult OnDone)
{
	if (!IsLoggedIn() || !Services)
	{
		OnDone.ExecuteIfBound(false, TEXT("notLoggedIn"));
		return;
	}
	if (!Info.LobbyId.IsValid())
	{
		OnDone.ExecuteIfBound(false, TEXT("noLobby"));
		return;
	}
	const bool bAlreadyMember = Session.bValid && Session.LobbyId == Info.LobbyId && Session.Members.Contains(Identity.Id);
	if (bAlreadyMember)
	{
		ConnectToSession(OnDone);
		return;
	}
	ILobbiesPtr Lobbies = Services->GetLobbiesInterface();
	if (!Lobbies)
	{
		OnDone.ExecuteIfBound(false, TEXT("noLobbies"));
		return;
	}
	if (Session.bValid)
	{
		Leave();
	}
	SetConnectionState(EDFConnectionState::Joining);
	FJoinLobby::Params Join;
	Join.LocalAccountId = Identity.Id.ToAccountId();
	Join.LocalName = LobbyLocalName;
	Join.LobbyId = Info.LobbyId;
	Join.bPresenceEnabled = true;
	Join.UserAttributes.Add(AttrMemberName, FSchemaVariant(Identity.DisplayName));
	Lobbies->JoinLobby(MoveTemp(Join)).OnComplete(this, [this, OnDone](const TOnlineResult<FJoinLobby>& Result)
	{
		if (Result.IsError() || !Result.GetOkValue().Lobby)
		{
			const FString Reason = Result.IsError() ? Result.GetErrorValue().GetErrorId() : TEXT("noLobby");
			UE_LOG(LogDFOnline, Warning, TEXT("join session failed: %s"), *Reason);
			SetConnectionState(EDFConnectionState::LoggedIn);
			OnDone.ExecuteIfBound(false, Reason);
			return;
		}
		ApplyLobby(*Result.GetOkValue().Lobby, /*bIsHost*/ false);
		OnSessionChanged.Broadcast(Session);
		ConnectToSession(OnDone);
	});
}

void UDFOnlineSubsystem::ConnectToSession(FDFOnlineResult OnDone)
{
	// What the services resolve is provider-shaped ("[EOS:<puid>]" for the EOS net driver); the
	// LAN lobby carries its address as an attribute instead.
	FString Connect;
	if (Services)
	{
		FGetResolvedConnectString::Params Params;
		Params.LocalAccountId = Identity.Id.ToAccountId();
		Params.LobbyId = Session.LobbyId;
		TOnlineResult<FGetResolvedConnectString> Resolved = Services->GetResolvedConnectString(MoveTemp(Params));
		if (Resolved.IsOk())
		{
			Connect = Resolved.GetOkValue().ResolvedConnectString;
		}
	}
	if (Connect.IsEmpty())
	{
		Connect = Session.ConnectString;
	}
	if (Connect.IsEmpty())
	{
		UE_LOG(LogDFOnline, Warning, TEXT("join session: no connect string for the lobby"));
		OnDone.ExecuteIfBound(false, TEXT("noConnectString"));
		return;
	}
	Session.ConnectString = Connect;
	if (!Backend || !Backend->Join(GetGameWorld(), Connect, MakeJoinOptions()))
	{
		OnDone.ExecuteIfBound(false, TEXT("backendRefused"));
		return;
	}
	SetConnectionState(EDFConnectionState::Connected);
	OnDone.ExecuteIfBound(true, FString());
}

void UDFOnlineSubsystem::Kick(const FDFOnlineId& Member, bool bBanForSession)
{
	if (!IsHosting() || !Member.IsValid() || Member == Identity.Id)
	{
		return;
	}
	Approved.Remove(Member);
	Pending.RemoveAll([&Member](const FDFJoinRequest& R) { return R.Id == Member; });
	Session.Members.Remove(Member);
	if (bBanForSession)
	{
		Banned.Add(Member);
	}
	if (ILobbiesPtr Lobbies = Services ? Services->GetLobbiesInterface() : nullptr)
	{
		Lobbies->KickLobbyMember({ Identity.Id.ToAccountId(), Session.LobbyId, Member.ToAccountId() }).OnComplete(this, [Member](const TOnlineResult<FKickLobbyMember>& Result)
		{
			if (Result.IsError())
			{
				// Null cannot kick (NotImplemented); the approval list still refuses the player at PreLogin.
				UE_LOG(LogDFOnline, Log, TEXT("kick %s: %s"), *Member.ToString(), *Result.GetErrorValue().GetLogString());
			}
		});
	}
	PublishApprovedList();
	FDFMsg_Player Msg;
	Msg.Name = FName(*Member.ToString());
	Msg.OnlineId = Member.ToString();
	BroadcastMessage(DFTags::Message_Kicked, Msg);
	OnSessionChanged.Broadcast(Session);
}

void UDFOnlineSubsystem::Leave()
{
	if (Session.bValid && Services && Identity.bLoggedIn)
	{
		if (ILobbiesPtr Lobbies = Services->GetLobbiesInterface())
		{
			Lobbies->LeaveLobby({ Identity.Id.ToAccountId(), Session.LobbyId }).OnComplete(this, [](const TOnlineResult<FLeaveLobby>& Result)
			{
				if (Result.IsError())
				{
					UE_LOG(LogDFOnline, Verbose, TEXT("leave lobby: %s"), *Result.GetErrorValue().GetLogString());
				}
			});
		}
	}
	const bool bHadSession = Session.bValid;
	ResetSession();
	if (Backend)
	{
		Backend->Leave(GetGameWorld());
	}
	SetConnectionState(Identity.bLoggedIn ? EDFConnectionState::LoggedIn : EDFConnectionState::Offline);
	if (bHadSession)
	{
		OnSessionChanged.Broadcast(Session);
	}
}

bool UDFOnlineSubsystem::LaunchMatch()
{
	if (!IsHosting() || !Backend)
	{
		return false;
	}
	return Backend->StartHost(GetGameWorld(), Session.MapId, FString());
}

void UDFOnlineSubsystem::SetSessionBackend(TSharedPtr<IDFSessionBackend> InBackend)
{
	Backend = InBackend ? InBackend : MakeShared<FDFSessionBackendListenP2P>();
}

// ---- session plumbing -------------------------------------------------------------------------

FDFSessionInfo UDFOnlineSubsystem::SessionFromLobby(const FLobby& Lobby) const
{
	FDFSessionInfo Info;
	Info.bValid = true;
	Info.LobbyId = Lobby.LobbyId;
	Info.Host = FDFOnlineId::FromAccountId(Lobby.OwnerAccountId);
	Info.MaxPlayers = Lobby.MaxMembers > 0 ? FMath::Min(Lobby.MaxMembers, GMaxPlayers) : GMaxPlayers;
	Info.MapId = FName(*AttributeString(Lobby.Attributes, AttrMap));
	Info.Tier = FGameplayTag::RequestGameplayTag(FName(*AttributeString(Lobby.Attributes, AttrTier)), /*ErrorIfNotFound*/ false);
	Info.bEndless = AttributeBool(Lobby.Attributes, AttrEndless);
	Info.Build = AttributeString(Lobby.Attributes, AttrBuild);
	Info.ContentHash = AttributeString(Lobby.Attributes, AttrContentHash);
	Info.ConnectString = AttributeString(Lobby.Attributes, TEXT("ConnectAddress"));   // the LAN lobby's address
	for (const TPair<FAccountId, TSharedRef<const FLobbyMember>>& Member : Lobby.Members)
	{
		Info.Members.AddUnique(FDFOnlineId::FromAccountId(Member.Key));
	}
	return Info;
}

void UDFOnlineSubsystem::ApplyLobby(const FLobby& Lobby, bool bIsHost)
{
	const FString KeepConnect = Session.ConnectString;
	Session = SessionFromLobby(Lobby);
	Session.bIsHost = bIsHost || Session.Host == Identity.Id;
	Session.Members.AddUnique(Identity.Id);
	if (Session.ConnectString.IsEmpty())
	{
		Session.ConnectString = KeepConnect;
	}
	ReadLobbyAttributes(Lobby);
}

void UDFOnlineSubsystem::ReadLobbyAttributes(const FLobby& Lobby)
{
	if (Session.bIsHost)
	{
		return;   // the host is the source of the code and the approved list
	}
	const FString ApprovedList = AttributeString(Lobby.Attributes, AttrApproved);
	TArray<FString> Ids;
	ApprovedList.ParseIntoArray(Ids, GApprovedSeparator, /*CullEmpty*/ true);
	Approved.Reset();
	for (const FString& Id : Ids)
	{
		Approved.Add(FDFOnlineId(Id, Identity.Id.Services));
	}
}

void UDFOnlineSubsystem::ResetSession()
{
	Session = FDFSessionInfo();
	Pending.Reset();
	Approved.Reset();
	Banned.Reset();
	JoinCode.Reset();
	PublishedJoinCodeGeneration = 0;
}

void UDFOnlineSubsystem::PublishApprovedList()
{
	TArray<FString> Ids;
	for (const FDFOnlineId& Id : Approved)
	{
		Ids.Add(Id.ToString());
	}
	Ids.Sort();
	WriteLobbyAttribute(AttrApproved, FString::Join(Ids, GApprovedSeparator));
}

void UDFOnlineSubsystem::WriteLobbyAttribute(FName Key, const FString& Value)
{
	if (!IsHosting() || !Services)
	{
		return;
	}
	ILobbiesPtr Lobbies = Services->GetLobbiesInterface();
	if (!Lobbies)
	{
		return;
	}
	FModifyLobbyAttributes::Params Params;
	Params.LocalAccountId = Identity.Id.ToAccountId();
	Params.LobbyId = Session.LobbyId;
	Params.UpdatedAttributes.Add(Key, FSchemaVariant(Value));
	Lobbies->ModifyLobbyAttributes(MoveTemp(Params)).OnComplete(this, [Key](const TOnlineResult<FModifyLobbyAttributes>& Result)
	{
		if (Result.IsError())
		{
			// Null lobbies are write-once (NotImplemented); the host's own copy is authoritative anyway.
			UE_LOG(LogDFOnline, Verbose, TEXT("lobby attribute %s: %s"), *Key.ToString(), *Result.GetErrorValue().GetLogString());
		}
	});
}

FString UDFOnlineSubsystem::MemberDisplayName(const FLobbyMember& Member) const
{
	const FString Name = AttributeString(Member.Attributes, AttrMemberName);
	return Name.IsEmpty() ? FDFOnlineId::FromAccountId(Member.AccountId).ToString() : Name;
}

void UDFOnlineSubsystem::HandleLobbyMemberJoined(const FLobbyMemberJoined& Event)
{
	if (!Session.bValid || Event.Lobby->LobbyId != Session.LobbyId)
	{
		return;
	}
	const FDFOnlineId Member = FDFOnlineId::FromAccountId(Event.Member->AccountId);
	Session.Members.AddUnique(Member);
	if (Session.bIsHost && !Event.Member->bIsLocalMember && Member != Identity.Id && !IsApproved(Member))
	{
		if (IsBanned(Member))
		{
			Kick(Member, /*bBanForSession*/ true);
			return;
		}
		// A code-holder: a member of the lobby, not yet a player. The host decides.
		FDFJoinRequest& Request = Pending.AddDefaulted_GetRef();
		Request.Id = Member;
		Request.DisplayName = MemberDisplayName(*Event.Member);
		Request.RequestedAt = Now();
		FDFMsg_Player Msg;
		Msg.Name = FName(*Request.DisplayName);
		Msg.OnlineId = Member.ToString();
		BroadcastMessage(DFTags::Message_JoinRequest, Msg);
		OnJoinRequest.Broadcast(Member, Request.DisplayName);
	}
	OnSessionChanged.Broadcast(Session);
}

void UDFOnlineSubsystem::HandleLobbyMemberLeft(const FLobbyMemberLeft& Event)
{
	if (!Session.bValid || Event.Lobby->LobbyId != Session.LobbyId)
	{
		return;
	}
	const FDFOnlineId Member = FDFOnlineId::FromAccountId(Event.Member->AccountId);
	if (Event.Member->bIsLocalMember || Member == Identity.Id)
	{
		const bool bKicked = Event.Reason == ELobbyMemberLeaveReason::Kicked;
		UE_LOG(LogDFOnline, Log, TEXT("left the session (%s)"), LexToString(Event.Reason));
		ResetSession();
		if (bKicked)
		{
			FDFMsg_Player Msg;
			Msg.Name = FName(*Identity.Id.ToString());
			Msg.OnlineId = Identity.Id.ToString();
			BroadcastMessage(DFTags::Message_Kicked, Msg);
		}
		SetConnectionState(EDFConnectionState::LoggedIn);
		OnSessionChanged.Broadcast(Session);
		return;
	}
	Session.Members.Remove(Member);
	Pending.RemoveAll([&Member](const FDFJoinRequest& R) { return R.Id == Member; });
	OnSessionChanged.Broadcast(Session);
}

void UDFOnlineSubsystem::HandleLobbyLeft(const FLobbyLeft& Event)
{
	if (!Session.bValid || Event.Lobby->LobbyId != Session.LobbyId)
	{
		return;
	}
	ResetSession();
	SetConnectionState(Identity.bLoggedIn ? EDFConnectionState::LoggedIn : EDFConnectionState::Offline);
	OnSessionChanged.Broadcast(Session);
}

void UDFOnlineSubsystem::HandleLobbyLeaderChanged(const FLobbyLeaderChanged& Event)
{
	if (!Session.bValid || Event.Lobby->LobbyId != Session.LobbyId)
	{
		return;
	}
	Session.Host = FDFOnlineId::FromAccountId(Event.Leader->AccountId);
	const bool bWasHost = Session.bIsHost;
	Session.bIsHost = Session.Host == Identity.Id;
	if (Session.bIsHost && !bWasHost)
	{
		// The host-migration seam: the lobby made us leader, so the resume (UDFMatchSave) is ours to offer.
		Approved.Add(Identity.Id);
		FDFMsg_Player Msg;
		Msg.Name = FName(*Identity.Id.ToString());
		Msg.OnlineId = Identity.Id.ToString();
		BroadcastMessage(DFTags::Message_HostMigrationOffered, Msg);
	}
	OnSessionChanged.Broadcast(Session);
}

void UDFOnlineSubsystem::HandleLobbyAttributesChanged(const FLobbyAttributesChanged& Event)
{
	if (!Session.bValid || Event.Lobby->LobbyId != Session.LobbyId || Session.bIsHost)
	{
		return;
	}
	const bool bWasApproved = IsApproved(Identity.Id);
	FDFSessionInfo Fresh = SessionFromLobby(*Event.Lobby);
	Fresh.bIsHost = false;
	Fresh.ConnectString = Fresh.ConnectString.IsEmpty() ? Session.ConnectString : Fresh.ConnectString;
	Session = Fresh;
	Session.Members.AddUnique(Identity.Id);
	ReadLobbyAttributes(*Event.Lobby);
	if (!bWasApproved && IsApproved(Identity.Id))
	{
		FDFMsg_Player Msg;
		Msg.Name = FName(*Identity.Id.ToString());
		Msg.OnlineId = Identity.Id.ToString();
		BroadcastMessage(DFTags::Message_JoinApproved, Msg);
	}
	OnSessionChanged.Broadcast(Session);
}

void UDFOnlineSubsystem::HandleLobbyInvitationAdded(const FLobbyInvitationAdded& Event)
{
	if (FDFOnlineId::FromAccountId(Event.LocalAccountId) != Identity.Id)
	{
		return;
	}
	FDFSessionInfo Invite = SessionFromLobby(*Event.Lobby);
	Invite.bIsHost = false;
	UE_LOG(LogDFOnline, Log, TEXT("invite from %s to %s"), *FDFOnlineId::FromAccountId(Event.SenderId).ToString(), *Invite.MapId.ToString());
	OnInviteReceived.Broadcast(Invite);
}

void UDFOnlineSubsystem::HandleUILobbyJoinRequested(const FUILobbyJoinRequested& Event)
{
	// The platform overlay's "join": an accepted invite arrives here already resolved to a lobby.
	if (FDFOnlineId::FromAccountId(Event.LocalAccountId) != Identity.Id || !Event.Result.IsOk())
	{
		return;
	}
	JoinSession(SessionFromLobby(*Event.Result.GetOkValue()));
}

// ---- presence / remote config -----------------------------------------------------------------

void UDFOnlineSubsystem::SetPresence(const FDFPresence& Presence)
{
	LastPresence = Presence;
	if (!IsLoggedIn() || !Services)
	{
		return;
	}
	IPresencePtr PresenceInterface = Services->GetPresenceInterface();
	if (!PresenceInterface)
	{
		return;
	}
	TSharedRef<FUserPresence> User = MakeShared<FUserPresence>();
	User->AccountId = Identity.Id.ToAccountId();
	User->Status = EUserPresenceStatus::Online;
	User->GameStatus = EUserPresenceGameStatus::PlayingThisGame;
	User->Joinability = Session.bValid ? EUserPresenceJoinability::InviteOnly : EUserPresenceJoinability::Private;
	User->StatusString = Presence.MapId.IsNone()
		? Presence.Status
		: FString::Printf(TEXT("%s wave %d (%d/%d)"), *Presence.MapId.ToString(), Presence.Wave, Presence.Players, GMaxPlayers);
	User->RichPresenceString = User->StatusString;
	PresenceInterface->UpdatePresence({ User->AccountId, User }).OnComplete(this, [](const TOnlineResult<FUpdatePresence>& Result)
	{
		if (Result.IsError())
		{
			UE_LOG(LogDFOnline, Verbose, TEXT("presence: %s"), *Result.GetErrorValue().GetLogString());
		}
	});
}

// GetRemoteConfig returns the defaults. Once the EOS product exists it reads Title Storage file
// "df-remote-config.json" through ITitleFile (EnumerateFiles -> ReadFile) after login and fills
// FDFRemoteConfig from it (bFromService = true); kill switches gate features, MinimumBuild gates
// the handshake. The Null provider has no title files, so the defaults are the Level 1 answer.

// ---- handshake ---------------------------------------------------------------------------------

FString UDFOnlineSubsystem::GetBuildLabel()
{
	return UDFGameInstance::GetBuildLabel();
}

FString UDFOnlineSubsystem::GetContentHash() const
{
	if (!bContentHashComputed)
	{
		RefreshContentHash();
	}
	return CachedContentHash;
}

bool UDFOnlineSubsystem::IsContentHashComplete() const
{
	GetContentHash();
	return bContentHashComplete;
}

void UDFOnlineSubsystem::RefreshContentHash() const
{
	EDFContentHashSource Source = EDFContentHashSource::None;
	CachedContentHash = UDFContentHash::ComputeForProjectSource(Source);
	bContentHashComplete = Source != EDFContentHashSource::None;
	bContentHashComputed = true;
	UE_LOG(LogDFOnline, Log, TEXT("content hash: %s (%s)"), *CachedContentHash,
		Source == EDFContentHashSource::Tables ? TEXT("imported tables") : Source == EDFContentHashSource::JsonSource ? TEXT("unreal/content/json, no tables imported") : TEXT("nothing to hash"));
}

FString UDFOnlineSubsystem::MakeJoinOptions() const
{
	return FString::Printf(TEXT("?%s=%s?%s=%s?%s=%s"),
		OptBuild, *GetBuildLabel(),
		OptContentHash, *GetContentHash(),
		OptOnlineId, *Identity.Id.ToString());
}

bool UDFOnlineSubsystem::ValidateJoinOptions(const FString& Options, FString& OutError)
{
	OutError.Reset();
	const FString Build = UGameplayStatics::ParseOption(Options, OptBuild);
	const FString Hash = UGameplayStatics::ParseOption(Options, OptContentHash);
	const FString IdString = UGameplayStatics::ParseOption(Options, OptOnlineId);
	const FDFOnlineId Id(IdString, Identity.Id.Services);

	if (Build.IsEmpty() && Hash.IsEmpty() && !IsHosting())
	{
		// A bare "open <ip>" from the INT smoke or a developer: no session to protect, so let it in and say so.
		UE_LOG(LogDFOnline, Warning, TEXT("join without handshake options (dev join); a hosted session would refuse it"));
		return true;
	}
	if (Build != GetBuildLabel())
	{
		OutError = ReasonVersionMismatch.ToString();
		BroadcastRejected(ReasonVersionMismatch, IdString, FString::Printf(TEXT("you are on %s, the host is on %s"), *Build, *GetBuildLabel()));
		return false;
	}
	if (Hash != GetContentHash())
	{
		OutError = ReasonContentMismatch.ToString();
		BroadcastRejected(ReasonContentMismatch, IdString, TEXT("content tables differ from the host's"));
		return false;
	}
	if (IsBanned(Id))
	{
		OutError = ReasonBanned.ToString();
		BroadcastRejected(ReasonBanned, IdString, TEXT("kicked from this session"));
		return false;
	}
	if (IsHosting() && !IsApproved(Id))
	{
		OutError = ReasonNotInvited.ToString();
		BroadcastRejected(ReasonNotInvited, IdString, TEXT("not invited or approved by the host"));
		return false;
	}
	return true;
}

// ---- messages / helpers -------------------------------------------------------------------------

void UDFOnlineSubsystem::SetConnectionState(EDFConnectionState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	FDFMsg_Tagged Msg;
	Msg.Tag = ConnectionTag(NewState);
	Msg.Id = FName(*UEnum::GetDisplayValueAsText(NewState).ToString());
	BroadcastMessage(DFTags::Message_ConnectionState, Msg);
}

void UDFOnlineSubsystem::BroadcastRejected(FName Reason, const FString& Subject, const FString& Detail)
{
	UE_LOG(LogDFOnline, Log, TEXT("join rejected: %s (%s) %s"), *Reason.ToString(), *Subject, *Detail);
	FDFMsg_Rejected Msg;
	Msg.Reason = Reason;
	Msg.Subject = FName(*Subject);
	Msg.Detail = Detail;
	BroadcastMessage(DFTags::Message_JoinRejected, Msg);
}

template <typename T>
void UDFOnlineSubsystem::BroadcastMessage(const FGameplayTag& Tag, const T& Payload)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDFMessageBus* Bus = GI->GetSubsystem<UDFMessageBus>())
		{
			Bus->Broadcast(Tag, Payload);
		}
	}
}

UWorld* UDFOnlineSubsystem::GetGameWorld() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetWorld() : nullptr;
}

double UDFOnlineSubsystem::Now()
{
	return FPlatformTime::Seconds();
}
