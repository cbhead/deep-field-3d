# Needs INT — the EOS configuration block (WS-11, ADR-0003)

Status: **documented, not applied**. Revised 2026-09-27 on the GPU box, after the owner created the
portal organization and read the EOS Developer Agreement. Three changes from the first version:

1. **No EOS value goes into git, not even the ids** (section 2). The agreement lets you share Credentials
   ("the digital keys provided by Epic to you that enable you to integrate and access the Services")
   only with Licensed EOS Developers (Developer Agreement 3.3.2), and this repository is public.
   Shipping them inside the game is permitted distribution. Publishing them in a public repository is
   not something the agreement covers, so they stay out.
2. **The Live sandbox**, not Dev (section 2). The portal now enables only Live at first; Dev and Stage
   stay disabled until the Epic Games Store publishing tools or store achievements are used.
3. **Windows only** (ADR-0028). The Mac paths are gone; the SDK's location and the Dev Auth Tool's
   source are verified on the box (section 6).

The consequence of 1: **everything EOS lives in one untracked file**, `unreal/DeepField/Config/Windows/WindowsEngine.ini`,
the provider switch and the net-driver switch included. A machine without that file (CI, the runner,
any other clone) runs exactly as today: Null services and the Ip net driver. `DefaultEngine.ini` does not
change, and already says the keys never live in it. `UDFOnlineSubsystem` resolves
`GetServices(EOnlineServices::Default)` and never names a provider in code, so the file is the whole switch.

Sources read for the first version (UE 5.8.2 launcher build; the box runs 5.8.3):
`Engine/Plugins/Online/OnlineServicesEpicCommon/.../OnlineServicesEpicCommonPlatformFactory.cpp`
(the `[OnlineServices.EOS]` keys), `EOSShared/.../EOSSDKManager.cpp` (`[EOSSDK.Platform.<Name>]`),
`SocketSubsystemEOS/.../SocketSubsystemEOS.cpp`, `SocketEOS.cpp`, `NetDriverEOS.h`,
`OnlineServicesEOSGS/Config/Engine.ini` (`[OnlineServices.EOS.Lobbies]` service descriptors),
`Engine/Config/BaseEngine.ini` (`[OnlineServices.Lobbies]` LobbyBase schema),
`OnlineServicesInterface/Private/Online/OnlineServicesRegistry.cpp` (`DefaultServices`).

## 1. Where the file lives, and why there

`unreal/DeepField/Config/Windows/WindowsEngine.ini` is the engine's Windows platform layer: it loads after
`DefaultEngine.ini` and overrides it, and packaging stages it into a Windows build, which needs the
values (the SDK reads them on the client). It is **gitignored** (`unreal/DeepField/.gitignore`, added
2026-09-27 before the file existed). So Windows-only Engine settings that *should* be tracked never go in
this file; they go in `DefaultEngine.ini`, in a Windows-specific section.

Who has it: the owner, on the box, typed in by hand (a session never handles the secret, and does not
need the ids either). CI, when a lane needs EOS, writes it from a repository secret (section 7). Nobody
else: another developer gets their own copy from the owner, and only once they are a Licensed EOS
Developer in the portal organization.

Before any commit near it: `git check-ignore -v unreal/DeepField/Config/Windows/WindowsEngine.ini` must name
the rule, and `git grep -n -I -E "ProductId=|SandboxId=|DeploymentId=|ClientId=|ClientSecret="` must find
only this RFC's placeholders.

## 2. The file

Values come from the Developer Portal: **Product Settings**.

- Product ID and the Live sandbox's ID: the *SDK Download & Credentials* tab.
- The Deployment ID: *Sandboxes*, **Live**, *Deployments*; create one if the list is empty.
- The Client ID and Client Secret: the *Clients* tab. First a client **policy** of type
  **GameClient**, which never has trusted-server rights (PROGRAMME.md 4.1, P0). Then a **client**
  using that policy.

```ini
; unreal/DeepField/Config/Windows/WindowsEngine.ini -- UNTRACKED. Never commit, paste or share it.

[OnlineServices]
; OSSv2 provider enum names (CoreOnline.h LexFromString): "Null" | "Epic". The EOS plugin's
; *config section* name is "EOS" ([OnlineServices.EOS.*]); the *provider* name is "Epic".
DefaultServices=Epic

[EOSSDK]
DefaultPlatformConfigName=DeepField
; Keeps a broken SDK install from killing the editor.
bDllLoadFailureIsFatal=false

[EOSSDK.Platform.DeepField]
ProductId=<Product ID>
SandboxId=<the Live sandbox's ID>
DeploymentId=<the Live deployment's ID>
ClientId=<the GameClient client's ID>
ClientSecret=<the GameClient client's secret>
; ClientEncryptionKey=<64 hex chars> ; only once Title Storage / Player Data Storage are used (WS-11 L2)
; CacheBaseSubdirectory=DeepField    ; optional; the SDK cache under the project's Saved dir

[OnlineServices.EOS]
PlatformConfigName=DeepField

; The P2P net driver (section 4) -- here, not in DefaultEngine.ini, so a machine without this file
; keeps the Ip driver and never tries to start EOS sockets without credentials.
[/Script/Engine.GameEngine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/SocketSubsystemEOS.NetDriverEOS",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")

[SocketSubsystemEOS]
bEnable=true
; NoRelays | AllowRelays | ForceRelays -- C14: relays forced (no NAT guessing, no IP leak between players).
RelayControl=ForceRelays
```

Alternative shape, read when `PlatformConfigName` is absent (`OnlineServicesEpicCommonPlatformFactory.cpp:83-121`):
`[OnlineServices.EOS] ProductId= SandboxId= DeploymentId= ClientId= ClientSecret=`. Same keys; do not use both.

**Other sandboxes later.** If the store's publishing tools are adopted, Dev and Stage become available.
A build can be pointed at another sandbox without a rebuild, with the overrides the SDK manager honours:
`-EpicSandboxId=` / `-EpicSandboxIdOverride=` and `-EpicDeploymentId=` / `-EpicDeploymentIdOverride=`.

**The secret in a package.** A GameClient secret ships in every client build (Epic's model: it only
scopes what the *client* credential may do; it is not a server secret). `BaseGame.ini`'s
`IniKeyDenylist` strips `EncryptionKey`-named keys, which is why the SDK reads `ClientEncryptionKey`.
Keep any Game *Server* credential (Level 2) out of clients entirely.

## 3. The lobby schema — already committed (WS-11, additive)

`[OnlineServices.Lobbies]` in `DefaultEngine.ini` declares the `DFLobby` schema (parent `LobbyBase`) with
`DFMap DFTier DFEndless DFBuild DFContentHash DFJoinCode DFApproved` on the `Lobby` category and
`DFName` on `LobbyMember`. EOS maps them onto its `LobbyServiceAttribute1..N` (the plugin's
`Config/Engine.ini` provides those); nothing else is needed for EOS. The
`[OnlineServices.Null.Lobbies]` block exists only because the Null plugin ships no service
descriptors of its own. Leave it in place; it is inert once `DefaultServices=Epic`. These are schema,
not Credentials, so they stay tracked.

Optional EOS lobby tuning (`[OnlineServices.EOS.Lobbies]`, in the untracked file): `BucketId=DeepField:<build-major>` to
partition lobby searches by build family (the facade already carries `DFBuild` as a searchable
attribute, so this is belt-and-braces).

## 4. The P2P net driver

In the file above, not in `DefaultEngine.ini`. Notes on it:

- `NetDriverEOS` is `Config=Engine`; no keys are required. `bIsUsingP2PSockets` is deprecated (always true).
- Ip-only runs still work with it: a URL that is not an EOS address (`127.0.0.1:7788`) makes the driver pass
  through to the Ip socket subsystem (`bIsPassthrough`), so `deepfield smoke` keeps working on the box
  with the file present. Confirm that when it is first applied (checklist).
- `EOS_EPacketReliability` defaults to `UnreliableUnordered`; Unreal's own reliability layer sits on top.
- `FDFSessionBackendListenP2P::GetNetDriverClassName()` reads the definition back so the smoke can
  assert which driver is live. With EOS the resolved connect string is `[EOS:<host ProductUserId>]`
  (`FOnlineServicesEOSGS::GetResolvedConnectString`), which `NetDriverEOS` turns into a P2P socket to the
  host; the host listens by opening its map with `?listen` exactly as today.

## 5. Auth — nothing more to configure for Level 1

`UDFOnlineSubsystem::Login(Auto)` runs the ladder: command-line credentials
(`-AUTH_TYPE=exchangecode -AUTH_LOGIN=unused -AUTH_PASSWORD=<code>`, what the EGS launcher passes;
`FEOSAuthLoginOptions::Create` reads them for `LoginCredentialsType::Auto`) → `PersistentAuth` →
`AccountPortal` (skipped when `-unattended`). `EDFLoginMethod::Developer` uses the Dev Auth Tool
(`-DFDevAuth=localhost:8081/<credential>`).

Optional keys, in the untracked file, under `[OnlineServices.EOS.Auth]`:
`DefaultScopes=BasicProfile,FriendsList,Presence` (the EAS scopes; they must match the portal
application's permissions) and `bAutoLinkAccount=true` (link a new Epic account on first login).
`[OnlineServices.EOS.Auth.Login] EASAuthEnabled=true` keeps Epic Account Services (friends, overlay,
presence) on top of Connect; Level 1 needs it for invites.

**Epic Account Services is not a standard service.** The Standard Services addendum covers Connect,
Lobbies, Sessions, Peer-to-Peer, Player Data and Title Storage (everything Level 1 uses besides EAS).
EAS, the application with BasicProfile / FriendsList / Presence, comes with its own terms and a brand
review in the portal. The brand review gates public users, not the organization's own accounts.

## 6. The box: verified 2026-09-26

- **The SDK** ships with the launcher engine at `Engine\Binaries\Win64\EOSSDK-Win64-Shipping.dll` (not
  under `Engine\Binaries\ThirdParty\EOSSDK`, as this document first guessed; the source copy is
  `Engine\Source\ThirdParty\EOSSDK\SDK\Bin\`). The first Windows package stages it and loads it:
  `LogEOSShared: FEOSSDKManager::Initialize Initializing EOSSDK Version:1.19.1.2`.
- **The Dev Auth Tool** does **not** ship with the launcher engine
  (`Engine\Source\ThirdParty\EOSSDK\SDK\Tools` is absent). Download the EOS SDK from the portal (*Product
  Settings*, *SDK Download & Credentials*) and use its `Tools\EOS_DevAuthTool-win32-x64-*.zip`. Unzip it
  outside the repository.
- The project enables `EOSShared`, `OnlineServicesEOS`, `SocketSubsystemEOS` and `OnlineServicesNull`.
  No source build is needed.

## 7. CI

No lane needs EOS today, and the tests stay on Null (below). When one does, it takes a repository
**secret** (not a variable), for example `EOS_WINDOWS_ENGINE_INI`, holding the whole file. A step writes
it to `unreal\DeepField\Config\Windows\WindowsEngine.ini` before the build, and deletes it when the job
ends (`if: always()`). The runner keeps its workspace between runs (`clean: false`), so without the
delete the file would outlive the job. GitHub withholds secrets from fork pull requests, and the job
already refuses them (ci.md, runner step 5).

## 8. The agreement, as it touches this plan

The owner read the EOS Developer Agreement (v4, 2024-02-15) and the Standard Services addendum (v7,
2025-06-17) on 2026-09-27. Not legal advice; the points that change how we work:

- **No EOS values in git** (3.3.2), as above.
- **An end-user license before anyone else gets a build** (3.1(c)): the EOS runtime may be distributed
  only inside the game, to end users bound by a EULA that disclaims Epic's warranties and liability. The
  first playtest package given to a second machine (G2) is distribution; a store release gets the
  store's EULA, a zip does not. A WS-15 packaging item.
- **Everyone signs in as themselves** (3.3.1): the second machine for the relay test needs a second
  person's Epic account, added to the organization or as a tester. No session ever uses the owner's account.
- **Never commit EOS SDK files** (4.1(c), 3.1(c)): the SDK is licensed under Epic's terms. The repository's
  MIT license does not pull it under other terms only because the SDK is not in the repository.

## Checklist for INT when applying

- [x] Organization and product created in the portal (the owner, 2026-09-27).
- [ ] The Live sandbox's deployment; a GameClient **policy** and a client using it; an Epic Account Services
      application with BasicProfile + FriendsList + Presence, linked to that client; Epic Games enabled
      under *Identity Providers* for the Live sandbox.
- [ ] `git check-ignore` names the rule for `Config/Windows/WindowsEngine.ini`, **then** the owner creates the
      file from section 2 and types the values in.
- [ ] `git status` shows nothing new, and the `git grep` in section 1 finds only placeholders.
- [ ] The Dev Auth Tool from the portal's SDK download, running, with a credential named (for example `df1`).
- [ ] `unreal\deepfield smoke`: the listen-host smoke still passes with the file present (the Ip passthrough of section 4).
- [ ] `unreal\deepfield test DF.Online`: with the file present, the tests run against Epic, and
      `DF.Online.NullLogin` needs a Dev Auth Tool session. To keep them on Null on the box, run
      `UnrealEditor-Cmd` directly with `-ini:Engine:[OnlineServices]:DefaultServices=Null`
      (`unreal/README.md` §5.1 has the full line); CI and the runner are on Null anyway (no file).
- [ ] Then WS-11's EOS checklist (`workstreams/ws-11-online.md`): Dev Auth login to a ProductUserId, an
      invite-only EOSGS lobby with the `DFLobby` attributes, and then two machines over the relay (a second
      person, a second Epic account, and a EULA in the package they are given).
