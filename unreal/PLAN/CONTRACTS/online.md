# C14 — Online API (ADR-0003)

**Canonical:** `unreal/DeepField/Source/DFOnline/Public/DFOnlineSubsystem.h`, `IDFSessionBackend.h`. **Owner:** WS-11. **Rule:** R.

```cpp
UCLASS() class UDFOnlineSubsystem : public UGameInstanceSubsystem {
  void Login(EDFLoginMethod = Auto /* EGS exchange code | Dev Auth Tool | persistent */);
  void Logout();
  FDFOnlineIdentity GetLocalIdentity();                         // ProductUserId + EpicAccountId + display name
  void CreateInviteOnlySession(FName MapId, FGameplayTag Tier, bool bEndless);   // party lobby; host = local
  void InviteFriend(const FDFOnlineId&);                        // EOS friends / overlay
  FString GetJoinCode();                                        // rotating, expires with the lobby
  void JoinByCode(const FString&);                              // host approves; DF.Message.JoinRequest
  void ApproveJoin(const FDFOnlineId&, bool);
  void JoinSession(const FDFSessionInfo&);                      // accepted invite / presence join
  void Kick(const FDFOnlineId&, bool bBanForSession);
  void Leave();
  void SetPresence(const FDFPresence&);                         // map, wave, players
  FDFRemoteConfig GetRemoteConfig();                            // Title Storage: kill switches, hotfix numbers
  FOnSessionChanged OnSessionChanged; FOnJoinRequest OnJoinRequest; FOnLoginChanged OnLoginChanged;
};
class IDFSessionBackend { /* P2P listen-server backend now (EOS relay, ForceRelays); dedicated-server backend at L2 */ };
```
- Player identity everywhere in gameplay is the **EOS `ProductUserId`** (`FDFOnlineId`) — owners, kicks, saves, logs; never a display name.
- Net driver: `SocketSubsystemEOS` / EOS P2P with relays forced; version + **content hash** carried in the session attributes and checked at `PreLogin` (`DF.Message.JoinRejected{versionMismatch|contentMismatch|notInvited|banned}`).
- `PreLogin` asks the join seams in `Source/DFCore/Public/Online/DFJoinSeams.h` (ruling R3, 2026-09-25; DFMatch cannot call DFOnline, so the question goes through DFCore): every game-instance subsystem implementing `IDFJoinValidator` (the handshake above — `UDFOnlineSubsystem`), `IDFSanctionsCheck` or `IDFAntiCheatCheck` is asked, and the first refusal's reason is PreLogin's error. Sanctions and anti-cheat have no implementer at Level 1, which is the pass-through (seams for L2/L3). The traveller's id is the `onlineId` URL option (`DFJoinSeams::OptOnlineId`).
- The spike (P1) records its verdict as an ADR: OSSv2 login + session + P2P relay working on the 5.8.2 launcher build, or the OSSv1 fallback inside this same facade.
