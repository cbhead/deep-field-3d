# C8 — Palette and tint material contract

**Canonical:** `Content/DF/Core/DA_Palette` (imported from `docs/palette.json` by `tools/ue-bridge/ue/import_tokens.py`), the master materials under `Content/DF/Materials/Master/`, the three MPCs under `Content/DF/Core/`, and `unreal/DeepField/Source/DFGameplay/Public/Tint/UDFTintComponent.h`. **Owner:** WS-00 defines the names; WS-31 implements the materials. **Rule:** R. Colour semantics are `docs/PALETTE.md` (ADR-0017): gameplay code never contains a colour literal — it asks `DA_Palette`.

## Master materials
| Master | Used by | Instance parameters gameplay may set (by name) |
|---|---|---|
| `M_TowerBody` | tower chassis, stages, traps, sockets, barricade | `EnergyHue`, `EnergyIntensity` (0.95 default), `Powered` (0/1), `HeatRamp` (0–1; filament emissive 0.3→2.0), `Damage` (0–1 from StructureHp), `Wear` (0–1 per level bought), `GhostPreview` (0/1) |
| `M_Enemy` (+ `M_Enemy_Stealth` translucent sibling) | enemies, boss | `HpFrac` (1→0: albedo → `Palette.Danger #C93B28` at weight 0.6; leaks <0.4; sparks NS <0.25), `StatusTint` (LinearColor), `StatusWeight` (0–0.7), `StatusEmissive` (LinearColor; burn/shock only), `MarkEmissive` (0/1 → #E3BC66 rim), `RevealEmissive` (0/1 → #7FE65A rim), `FreezeAmount`, `TarAmount`, `PoisonAmount`, `Stealth` (0–1), `EyeGlow` (×1 day / ×3 night), `EliteRimColor`, `EliteRimPower`, `EliteFresnelWidth`, `Enrage`, `Phase` (1–3), `Shielded` (0/1), `Dissolve` (0–1); slot 2 `MI_Enemy_WeakPoint`: `WeakPointEmissive` |
| `M_Hero`, `M_FPArms` | third-person heroes, first-person arms | `FactionAccent`, `Downed`, `AbilityCharge`, `Wetness`, `Frost` |
| `M_WeaponHero` | weapons, melee, attachments | `AmmoHue`, `InfusionHue`, `Heat`, `PackAPunchLevel` |
| `M_Environment_Layered`, `M_Landscape_<Map>`, `M_Foliage`, `M_Glass`, `M_Water_*` | environments | read MPCs only |
| `M_Decal_*` (`DM_Scorch Crater Frost Tar Corrode Oil Puddle Tyre Coverage`) | decals via `BP_DecalPool` (cap 256) | `Fill` (tar), `Radius`, `NextRadius`, `Hue` (coverage) |
| `M_VFX_*` (`Additive Translucent_Lit Beam Ribbon Distortion Mesh_Emissive`), `PPM_RevealPulse`, `PPM_HeatShimmer`, `PPM_Downed` | Niagara, post | via Niagara user params (C10) / MPCs |
| `M_UI_Chamfer` (`Chamfer`, `Bevel`, `Glow`), `M_UI_ModelWell`, `M_UI_Hazard` | UMG | style assets |

## Material Parameter Collections
- `MPC_Conditions`: `FogDensity, FogHeightFalloff, FogAlbedo, NightAmount, SunIntensityScale, SunColor, MoonIntensity, CloudCoverage, Wetness, SnowAmount, HeatShimmer, WindStrength, WindDir, LightningFlash, DryAmount, IceAmount` — written only by `BP_ConditionDirector` blending a `DA_ConditionPreset_<id>` over 8 s at intermission.
- `MPC_Match`: `WavePhase (0 build/1 wave/2 boss), Threat, CoreHpFrac, OverdriveOrigin, OverdriveRadius, RevealPulseOrigin, RevealPulseRadius, CryoFieldOrigin, CryoFieldRadius, IgnitionOrigin, IgnitionRadius`.
- `MPC_Player`: `FlashlightOn, ViewerFaction, ReduceFlashes, TeammateOutline` (per local viewer).
- `MPC_DF_Palette`: every `palette.json` entry as a named vector (semantics, factions, scrap, statuses, tower energy hues, elite rims) for materials that need a swatch without an instance parameter.

## The component
`UDFTintComponent` (one per enemy/hero/tower/vehicle actor; runs on every machine from replicated state): `SetHp(float)`, `SetStatus(EChannel, FGameplayTag Id, float Magnitude)`, `ClearStatus(EChannel)`, `SetElite(FGameplayTag)`, `SetStealth(float)`, `SetMarked(bool)`, `SetRevealed(bool)`, `SetEnergy(FLinearColor, float)`, `SetDamage(float)`, `SetWear(float)`, `SetPhase(int)`, `SetDissolve(float)`. It resolves precedence exactly as `TintableView.cs` did (Control > Thermal > Movement for `StatusTint`; mark/reveal emissive-only so they coexist), reads colours from `DA_Palette`, and writes **Custom Primitive Data** (preferred; no dynamic instance churn) or MID parameters for materials that need them. `DF.Unit.TintContract` drives every parameter through the component and checks the written values.

## Palette keys (`DA_Palette`, from `docs/palette.json`)
`Semantics.{Danger,Warning,Success,Info,Hp,HpLow,Shield,Armor,Currency,Upgrade,Elite,Boss}`, `Factions.<id>.{Accent,Ability}`, `Scrap.{Alloy,Flux,Plating,Gravium,Primecore}`, `Statuses.<id>` (+ `Emissive` flag), `Enemies.{Base,WeakPoint,WardenShield}`, `Elites.<id>`, `TowerBody.{SteelHull,SteelPlate,Chassis,Trim,Chrome,Brass,Hazard,Concrete}`, `Towers.<id>.Energy`, `UI.{Obsidian[],Steel[],Brass[],Arcane[]}`.
