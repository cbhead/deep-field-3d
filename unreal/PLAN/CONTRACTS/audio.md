# C11 — Audio event contract (ADR-0014)

**Canonical:** `Content/DF/Audio/MetaSounds/MS_*`, `Content/DF/Core/DA_AudioCueMap`, `unreal/DeepField/Source/DFAudio/Public/DFAudioSubsystem.h` (no `U` in the filename; the name WS-13's implementation must use — it does not exist yet), `Content/DF/Audio/LICENSES.md`. **Owner:** WS-13. **Rule:** A for new cue→sound mappings; R for input names and states.

- **Sources:** MetaSound sources `MS_<Category>_<Id>_<Verb>` (e.g. `MS_Tower_Lance_Fire`, `MS_Status_Burn_Loop`, `MS_Enemy_Drifter_Death`, `MS_UI_Wheel_Open`, `MS_Weather_Fog_Bed`). Exposed inputs (all optional): `Intensity` (0–1), `Heat` (0–1), `Distance` (cm), `Variant` (int), `Tier` (1–3), `Threat` (endless multiplier).
- **Cue map:** every `GameplayCue.DF.*` tag and every `DF.Message.*` that has a sound has a row in `DA_AudioCueMap { CueTag → MetaSound, Attenuation, SoundClass, bLoop, MaxConcurrent }`. `DF.Audio.EveryCueHasSound` fails on a cue with no row (a silent event is a bug, like a placeholder mesh).
- **Music:** `MS_Music_Director` on a Quartz clock; states are tags `DF.Audio.State.{Lobby,Intermission,Wave,Breach,Boss,Victory,Defeat}` set by `UDFAudioSubsystem` from match messages; stems per map layered by state and `Threat`; transitions on bar boundaries; the 8 s intermission is a fill.
- **Mix:** sound classes `SC_Master SC_Music SC_SFX SC_UI SC_Voice SC_Ambience`; submixes with Audio Modulation for weather (fog low-pass on `SC_SFX`), downed (`PPM_Downed` twin), and reduce-flash-independent. Attenuation presets `ATT_Tower ATT_Gun ATT_Enemy ATT_World ATT_UI`. Occlusion via audio occlusion traces; reverb volumes per building/tunnel.
- **Hue-coded towers:** each tower's fire/idle sound takes the same `Tier` and `Heat` as its VFX so a tower is legible blind.
- **Enemy vocabulary:** locomotion loop (anim notify per stride), hit, death, spawn, specials; elites add a layer; the shade unseen is silent.
- **Licensing:** every imported wave/pack/generated clip is a row in `LICENSES.md { asset, source, licence (CC0/CC-BY/…/generated), url, date }`. CI fails a `S_*` asset with no row.
