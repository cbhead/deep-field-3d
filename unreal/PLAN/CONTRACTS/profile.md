# C13 — Save / profile

**Canonical:** `unreal/DeepField/Source/DFOnline/Public/Profile/UDFProfileSave.h`, `IDFProgressionProvider.h`. **Owner:** WS-11. **Rule:** R; every change bumps `Version` and adds a migration.

```cpp
UCLASS() class UDFProfileSave : public USaveGame {
  int32 Version;                       // 1 at first ship; migrations in UDFProfileMigrations
  FString Name; FGameplayTag PreferredFaction; FString LastJoinCode;
  // settings
  bool ShowDamageNumbers; float MouseSensitivity; int32 FieldOfView; float HudScale; bool ScreenShake; bool ReduceFlashes;
  bool TeammateOutlines; float MasterVolume, MusicVolume, SfxVolume, VoiceVolume; FName ScalabilityTier; float TsrPercent; bool FlashlightAuto;
  // progression (Level 1: local; written only from a host match record — never from client-declared values)
  TMap<FGameplayTag,int32> FactionXp; TArray<FName> ClearedSectors; TMap<FName,int32> BestWave;      // key "<mapId>" and "<mapId>:endless"
  TMap<FName,FGameplayTag> HighestTier;                                                                 // per sector
  TMap<FName,FDFBlueprint> Blueprints;                                                                   // weaponId -> {slot->attachment, ammo}
  TArray<FDFMatchRecord> RecentMatches;                                                                  // last 20 host-computed records
};
```
`IDFProgressionProvider { Load(); Save(); ApplyMatchRecord(const FDFMatchRecord&); LevelFor(faction); }` — the local implementation now; a server-backed one at Level 3 consumes the same `FDFMatchRecord` (host-computed XP per source capped at 60, sector cleared, tier, best wave, signed later).

Autosave between waves for resume-under-new-host is a separate `UDFMatchSave` (world snapshot: match state, structures, players' loadouts, wave index, seed) written by the host at every intermission and shared to lobby members so any of them can host the resume.
