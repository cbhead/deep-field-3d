# `unreal/content` — the text source of truth

Every gameplay number lives here as JSON (ADR-0005). The Unreal DataTables under
`unreal/DeepField/Content/DF/Data/Tables/DT_*.uasset` are **generated** from these files by the
`DFContentImport` commandlet and are never hand-edited; gameplay reads them only through
`UDFContentSubsystem` (DFCore). Contracts: [C2 content-rows](../PLAN/CONTRACTS/content-rows.md),
[C3 content-json](../PLAN/CONTRACTS/content-json.md).

```
json/<table>.json        one file per table: towers, traps, enemies, statuses, reactions, factions, weapons,
                         melee, meleeAttachments, attachments, ammo, conditions, vehicles, maps, balance,
                         waves_<map> (one per map)
schema/<table>.schema.json   JSON Schema per table (waves.schema.json covers every waves_<map>)
content-ids.json         every table's ids in row order (what DF.Content.TagCoverage / RoundTrip check)
levels/legacy/           the Godot-era level briefs (input to WS-09/WS-10x, not imported by this pipeline)
```

## Editing JSON

- Keys are camelCase; the importer maps them onto the PascalCase fields of the row structs in
  `Source/DFCore/Public/Content/DFContentRows.h` (`stealth` → `bStealth`, `cost` → `Cost`).
- Enums are strings matching the `UENUM` names (`"kind": "Bolt"`, `"layer": "Air"`).
- Scrap bundles are `{"Alloy": 6, "Plating": 2}`; integer-keyed maps use string keys
  (`"breakpointRecipes": {"4": {...}}`, `"conditionSchedule": {"8": "fog"}`); `fieldMeters` is `[x, z]`
  metres; colours are `"#RRGGBB"`; `balance.json` is one row `default` whose every other key is a dial.
- Every row has an `id` (lower camelCase, the DataTable row name and the tail of its `DF.<Domain>.<Id>` tag).
  Add the id to `content-ids.json` in the same position.
- Optional fields (everything the schema does not list under `required`) may be omitted and take the
  struct default. Anything the struct does not have is an **error**: the importer refuses the table and
  names the key, so a typo can never become a silent default.
- Validate before committing: `python3 unreal/Build/validate-content-json.py`.
- Row ownership is per table (`unreal/PLAN/OWNERSHIP.md`): towers/traps → WS-04, enemies/waves → WS-05,
  weapons/melee/attachments/ammo/balance → WS-06, statuses/reactions → WS-02, factions → WS-07,
  maps/conditions/vehicles → WS-09. The format and the row structs are WS-01's.
- `tools/content-export --diff` shows how far the JSON has drifted from the frozen C# spec; it never writes.

## Running the importer

One editor process per machine (PROGRAMME.md §6.7), so always go through the lock helper:

```bash
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd"
unreal/Build/editor-lock.sh "$UE" "$PWD/unreal/DeepField/DeepField.uproject" \
  -run=DFContentImport -nullrhi -unattended -nop4 -nosplash -NoSound        # every table
  ... -run=DFContentImport -tables=towers,enemies                            # some tables
  ... -run=DFContentImport -json=/path/to/other/json                         # another source directory
```

Each table prints one line (`towers  8 rows -> /Game/DF/Data/Tables/DT_Towers (updated)`); the exit
code is 1 if any table failed, and the log names the table, row and key. Rows are replaced wholesale in
JSON order. The `.uasset`s are git-lfs `lockable`, so a checkout leaves them read-only; the importer
clears that flag itself (it is their only writer) and logs one line per table it had to. Commit the
resulting `.uasset`s (they go through Git LFS; `git lfs ls-files` shows them as pointers) together with
the JSON change; an unchanged table saves byte-identically, so a re-run on unchanged JSON leaves git clean.

## What the tests guarantee

Run them with `unreal/Build/editor-lock.sh unreal/Build/test.sh DF.Content+DF.Unit.Content`.

| Test | Guarantee |
|---|---|
| `DF.Content.RoundTrip` | For every table: the committed DataTable has the JSON's rows in the JSON's order; each row re-exported through `FJsonObjectConverter` equals the JSON row (every key on the struct, numbers within 1e-4, arrays and maps deep-equal, colours/vectors/bundles compared in their C3 shapes); every struct field the JSON omits is still at its default; `content-ids.json` matches. The first difference per table is named. |
| `DF.Content.TagCoverage` | Every id in `content-ids.json` has a native `DF.<Domain>.<Id>` tag (`DFTags::ForContentId`) under a root that exists (`DF.Tower`, `DF.Trap`, `DF.Enemy`, `DF.Status`, `DF.Reaction`, `DF.Faction`, `DF.Weapon`, `DF.Melee`, `DF.Ammo`, `DF.Condition`). Tables whose root does not exist yet (attachments, meleeAttachments) are reported as informational. |
| `DF.Content.Bindings` | For every id in every table with a `PrimaryAssetType` (DefaultGame.ini), the binding `UDFContentDefinition` is resolved through `UDFContentSubsystem::Definition`. A missing binding is a warning (`unbound: Tower lance` — bindings are other workstreams' deliverables); a binding with `bPlaceholder=true`, or one whose `ContentId`/`PrimaryType` disagree with its id, fails. |
| `DF.Unit.Content.SubsystemLoads` | `UDFContentSubsystem::LoadTables()` finds every table and a few known values come back (`Tower("lance")->Cost == 75`, `Enemy("ram")->EnrageSpeedFactor == 1.6`, `Balance("startingMoney") == 250`, `Waves("foundry").Num() == 22`). |

Source: `unreal/DeepField/Source/DFContentPipeline` (editor module; commandlet `UDFContentImportCommandlet`,
table → struct mapping in `DFContentTables.cpp`, key/shape rules in `DFContentJsonShaper.cpp`).
