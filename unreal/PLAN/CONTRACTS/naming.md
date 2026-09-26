# C7 — Naming

**Canonical:** this document (the regex below) — **it is the only enforcement today**. The `DFEditor` asset validator and `tools/ue-bridge/ue/validate_content.py` are both still to build (WS-30; `DFEditor` currently holds only the DFLevelImport / DFMapValidate / DFTerrainImport commandlets), so nothing mechanically checks a prefix yet. **Owner:** WS-00. **Rule:** A (a new prefix is appended here, and to the validator once it exists, in one `contract-append` PR).

**Asset names:** `^(SM|SK|SKEL|PHYS|ABP|AS|AM|BS|CR|IKR|M|MI|MF|ML|MPC|T|NS|NE|BP|DT|DA|WBP|ST|S|MS|SC|ATT|L|DL|PCG|GC|RT|C|F|IP|IMC|IA)_[A-Z][A-Za-z0-9]*(_[A-Z0-9][A-Za-z0-9]*)*$`

| Prefix | Type | Example |
|---|---|---|
| `SM_` / `SK_` / `SKEL_` / `PHYS_` | static mesh / skeletal mesh / skeleton / physics asset | `SM_Tower_Lance_Yaw`, `SK_Enemy_Drifter`, `SKEL_Enemy_Biped` |
| `ABP_` / `AS_` / `AM_` / `BS_` / `CR_` / `IKR_` | anim blueprint / sequence / montage / blend space / control rig / IK retargeter | `AM_FPArms_Reload_MagFed_Rifle` |
| `M_` / `MI_` / `MF_` / `ML_` / `MPC_` | material / instance / function / layer / parameter collection | `M_TowerBody`, `MI_Tower_Lance_Body`, `MPC_Conditions` |
| `T_` | texture, suffix by role: `_BC` (sRGB), `_N` (BC5), `_ORM` (linear R occ G rough B metal), `_MSK` (linear packed AO/curv/ID-A/ID-B), `_E` emissive, `_M` mask, `_ID` | `T_Enemy_Drifter_MSK` |
| `NS_` / `NE_` | Niagara system / emitter | `NS_Status_Burn` |
| `BP_` / `DT_` / `DA_` / `WBP_` / `ST_` | blueprint / data table / data asset / widget / slate style | `DA_Tower_Lance`, `WBP_Hud` |
| `S_` / `MS_` / `SC_` / `ATT_` | sound wave / MetaSound source / sound class / attenuation | `MS_Tower_Lance_Fire` |
| `L_` / `DL_` / `PCG_` / `GC_` / `RT_` / `C_` / `F_` / `IP_` / `IMC_` / `IA_` | level / data layer / PCG graph / geometry collection / render target / curve / font / Interchange pipeline / input mapping context / input action | `L_Foundry_Terrain`, `GC_Wall` |

**Folder-by-prefix** (`unreal/DeepField/Content/DF/`): `Core Data/{Tables,Defs/<Domain>} Gameplay/{GE,GA_Base,Cues} Heroes/<faction> Characters/{FPArms/<set>,Boss,_Shared} Enemies/{<id>,_Shared,Elites} Towers/<id>/{Stages/<path>} World/{Traversal,Sockets,Traps,Destructibles,Pickups} Weapons/{Ranged/<id>,Melee/<id>,Attachments,Ammo,MeleeMods} Vehicles/<id> Maps/<Map> Env/{Kits/<kit>,<Map>/{Structures,Props,PCG,Lighting}} Materials/{Master,Functions,Layers,Decals,VFX,UI} Textures/Shared VFX/{Niagara/<Category>,Emitters,Meshes} Audio/{MetaSounds,SFX/<Category>,Music,Attenuation} UI/{Widgets,Kit,Screens/<Screen>,Icons,Fonts,Styles,Studio} Online Dev/Tests`. `/Game/Megascans/` is the Fab plugin's and is never moved or edited.

**Levels:** `L_<Map>` persistent + `L_<Map>_Gameplay`, `_Terrain`, `_Art`, `_Lighting`, `_Audio`; test levels `L_Test_<Name>`; `L_Dev_Empty`, `L_UIStudio`, `L_MainMenu`.

**Content ids** (rows, tags, DA suffixes): lower camelCase exactly as the sim spelled them (`emberPistol`); tag and DA forms are PascalCase (`DF.Weapon.EmberPistol`, `DA_Weapon_EmberPistol`). Stage modules: `SM_Tower_<Id>_<Path>_S02..S10` (`S01` is the chassis itself and has no asset).

**Sockets** (mesh sockets, fixed names per category — validator-checked): towers `S_Yaw` (on Foot), `S_Pitch` (on Yaw), `S_Muzzle`, `S_Crown`, `S_Hit`, `S_Stage_<Path>` (on the part that path's modules mount to), `S_Spin` (aura), `S_Link_1..6` (overclock); enemies `status`, `overhead`, `eye`, `elite`, `weak_01..`, family extras (`shield`, `sac`, `plate_a/b/c`, `siege`, `mound`); heroes `weapon_r`, `melee_r`, `backpack`, `flashlight`, `overhead`; arms `weapon`, `hand_l_magazine`, `flashlight`; weapons `muzzle`, `eject`, `magwell`, `mount_barrel|muzzle|optic|magazine|stock|underbarrel|infusion`; melee `mount_edge|grip|infusion|counterweight|chargecell`; vehicles `seat_driver`, `seat_passenger`, `eye`, `exhaust`, `headlight_l|r`, `hitch`; traversal `zip_sheave`, `interact`, `membrane`.

**C++:** modules `DF<Area>`; classes `ADF*`, `UDF*`, `FDF*`, `EDF*`, `IDF*`; messages `FDFMsg_<Name>`; tests `DF.<Layer>.<Area>.<Name>`.

**Text files:** `unreal/content/json/<table>.json`, `unreal/content/levels/<map>.level.json`, `unreal/content/terrain/<map>.terrain.json`; scripts kebab-case under `unreal/Build/` and `tools/ue-bridge/`.
