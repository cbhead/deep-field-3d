# Needs INT — the EOS configuration block (WS-11, ADR-0003)

Status: **documented, not applied**. The Epic developer-portal product for Deep Field does not
exist yet, so `DefaultEngine.ini` stays on the Null services and the Ip net driver. When the
product exists, INT applies the blocks below (they are the only change: `UDFOnlineSubsystem`
resolves `GetServices(EOnlineServices::Default)` and never names a provider in code). Secrets
never go into a committed ini — see "Where the secrets live".

Sources read for this document (UE 5.8.2 launcher build):
`Engine/Plugins/Online/OnlineServicesEpicCommon/.../OnlineServicesEpicCommonPlatformFactory.cpp`
(the `[OnlineServices.EOS]` keys), `EOSShared/.../EOSSDKManager.cpp` (`[EOSSDK.Platform.<Name>]`),
`SocketSubsystemEOS/.../SocketSubsystemEOS.cpp`, `SocketEOS.cpp`, `NetDriverEOS.h`,
`OnlineServicesEOSGS/Config/Engine.ini` (`[OnlineServices.EOS.Lobbies]` service descriptors),
`Engine/Config/BaseEngine.ini` (`[OnlineServices.Lobbies]` LobbyBase schema),
`OnlineServicesInterface/Private/Online/OnlineServicesRegistry.cpp` (`DefaultServices`).

## 1. The provider switch — `DefaultEngine.ini`

```ini
[OnlineServices]
; OSSv2 provider enum names (CoreOnline.h LexFromString): "Null" | "Epic". The EOS plugin's
; *config section* name is "EOS" ([OnlineServices.EOS.*]); the *provider* name is "Epic".
DefaultServices=Epic
```

The Null value that ships today is the same key, so switching is one word. `UDFOnlineSubsystem`
logs the provider it resolved at first use (`online services: Epic`).

## 2. Product credentials — `DefaultEngine.ini` (ids) + a local, untracked ini (secrets)

Two equivalent shapes are read by the engine. Use the platform-config shape: it keys the same
block for `OnlineServicesEOS` and `SocketSubsystemEOS`, and it lets the secret live in a
separate file.

```ini
[EOSSDK]
DefaultPlatformConfigName=DeepField

[EOSSDK.Platform.DeepField]
ProductId=<from the portal: Product Settings -> Product ID>
SandboxId=<the Dev sandbox id, p-XXXX..; Live sandbox in the packaged build>
DeploymentId=<the deployment id inside that sandbox>
ClientId=<the Game Client credential's Client ID, xyza...>
; ClientSecret is REQUIRED by the SDK for a client credential -- but not here: see "Where the secrets live".
; ClientEncryptionKey=<64 hex chars> ; only once Title Storage / Player Data Storage are used (WS-11 L2)
; RelyingPartyURI=                   ; unused at Level 1
; CacheBaseSubdirectory=DeepField    ; optional; the SDK cache under the project's Saved dir

[OnlineServices.EOS]
PlatformConfigName=DeepField
```

Alternative shape, read when `PlatformConfigName` is absent (`OnlineServicesEpicCommonPlatformFactory.cpp:83-121`):
`[OnlineServices.EOS] ProductId= SandboxId= DeploymentId= ClientId= ClientSecret=`. Same keys; do not use both.

Command-line overrides the SDK manager honours (useful for the Dev sandbox on a Live build):
`-EpicSandboxId=` / `-EpicSandboxIdOverride=` and `-EpicDeploymentId=` / `-EpicDeploymentIdOverride=`.

### Where the secrets live

`ClientSecret` (and later `ClientEncryptionKey`) go in an **untracked** `unreal/DeepField/Config/DefaultEngine.local.ini`?
No — the engine does not merge that name. The supported options are:

1. **Per-machine user ini** (developers): `~/Library/Application Support/Epic/UnrealEngine/...`? Not project-scoped either.
   Use the engine's platform override layer instead: `unreal/DeepField/Config/Mac/MacEngine.ini` /
   `Config/Windows/WindowsEngine.ini` with only the `[EOSSDK.Platform.DeepField] ClientSecret=` line,
   and add both paths to `.gitignore` (INT owns `.gitignore`). CI injects the same file from a secret.
2. **Packaged builds**: the secret must be present in the cooked ini (the EOS SDK needs it on the client);
   `BaseGame.ini`'s `IniKeyDenylist` strips `EncryptionKey`-named keys, which is why the SDK now reads
   `ClientEncryptionKey`. Accept that a Game Client secret ships in the build (Epic's model: the secret
   only scopes what the *client* credential may do; it is not a server secret). Keep the Game *Server*
   credential (Level 2) out of clients entirely.

The committed `DefaultEngine.ini` therefore carries ids only; the secret arrives through the
platform ini (gitignored) or the CI secret.

## 3. The lobby schema — already committed (WS-11, additive)

`[OnlineServices.Lobbies]` now declares the `DFLobby` schema (parent `LobbyBase`) with
`DFMap DFTier DFEndless DFBuild DFContentHash DFJoinCode DFApproved` on the `Lobby` category and
`DFName` on `LobbyMember`. EOS maps them onto its `LobbyServiceAttribute1..N` (the plugin's
`Config/Engine.ini` provides those); nothing else is needed for EOS. The
`[OnlineServices.Null.Lobbies]` block exists only because the Null plugin ships no service
descriptors of its own — leave it in place, it is inert once `DefaultServices=Epic`.

Optional EOS lobby tuning (`[OnlineServices.EOS.Lobbies]`): `BucketId=DeepField:<build-major>` to
partition lobby searches by build family (the facade already carries `DFBuild` as a searchable
attribute, so this is belt-and-braces).

## 4. The P2P net driver — `DefaultEngine.ini`

Today (`[/Script/Engine.GameEngine]`): the `GameNetDriver` definition is `IpNetDriver`. Replace it:

```ini
[/Script/Engine.GameEngine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/SocketSubsystemEOS.NetDriverEOS",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")

[/Script/SocketSubsystemEOS.NetDriverEOS]
; NetDriverEOS is Config=Engine; no keys are required. bIsUsingP2PSockets is deprecated (always true).
; Ip-only dev runs still work: a URL that is not an EOS address ("127.0.0.1:7788") makes the driver
; pass through to the Ip socket subsystem (bIsPassthrough), so smoke-listen.sh keeps working.

[SocketSubsystemEOS]
bEnable=true
; NoRelays | AllowRelays | ForceRelays -- C14 says relays forced (no NAT guessing, no IP leak between players).
RelayControl=ForceRelays
; EOS_EPacketReliability: UnreliableUnordered (default; Unreal's own reliability layer sits on top).
; PacketReliabilityType=UnreliableUnordered
```

`FDFSessionBackendListenP2P::GetNetDriverClassName()` reads the definition back so the smoke can
assert which driver is live. With EOS the resolved connect string is `[EOS:<host ProductUserId>]`
(`FOnlineServicesEOSGS::GetResolvedConnectString`), which `NetDriverEOS` turns into a P2P socket
to the host; the host listens by opening its map with `?listen` exactly as today.

## 5. Auth — nothing to configure for Level 1

`UDFOnlineSubsystem::Login(Auto)` runs the ladder: command-line credentials
(`-AUTH_TYPE=exchangecode -AUTH_LOGIN=unused -AUTH_PASSWORD=<code>`, what the EGS launcher passes;
`FEOSAuthLoginOptions::Create` reads them for `LoginCredentialsType::Auto`) → `PersistentAuth` →
`AccountPortal` (skipped when `-unattended`). `EDFLoginMethod::Developer` uses the Dev Auth Tool
(`-DFDevAuth=localhost:8081/<credential>`). Optional keys, both under `[OnlineServices.EOS.Auth]`:
`DefaultScopes=BasicProfile,FriendsList,Presence` (the EAS scopes; must match the portal's
application permissions) and `bAutoLinkAccount=true` (link a new Epic account on first login).
`[OnlineServices.EOS.Auth.Login] EASAuthEnabled=true` keeps Epic Account Services (friends, overlay,
presence) on top of Connect; Level 1 needs it for invites.

## 6. What the Mac editor needs

The EOS SDK dylib ships inside the launcher engine (`Engine/Binaries/ThirdParty/EOSSDK/Mac`).
`[EOSSDK] bDllLoadFailureIsFatal=false` keeps a broken SDK install from killing the editor.
Nothing else: no source build required.

## Checklist for INT when applying

- [ ] Product / sandbox / deployment / client created in the portal (Game Client credential; Application with BasicProfile + FriendsList + Presence).
- [ ] Ids into `[EOSSDK.Platform.DeepField]`; `PlatformConfigName` into `[OnlineServices.EOS]`; `DefaultServices=Epic`.
- [ ] Secret via the gitignored platform ini and the CI secret; confirm `git grep ClientSecret=` finds only the placeholder comment.
- [ ] Net driver block (§4) applied; `smoke-listen.sh` still green (Ip passthrough).
- [ ] Run `unreal/Build/test.sh DF.Online` — `DF.Online.NullLogin` will now run against Epic and needs a Dev Auth Tool session (`-DFDevAuth`), or keep the tests on Null with `-ini:Engine:[OnlineServices]:DefaultServices=Null`.
- [ ] Then the WS-11 EOS checklist in `workstreams/ws-11-online.md` (two machines, relay, join-in-progress).
