# C3 — Content JSON format

**Canonical:** `unreal/content/json/<table>.json`, validated against `unreal/content/schema/<table>.schema.json` in CI. **Owner:** WS-01 (format); each table's rows are owned by the workstream listed in `../OWNERSHIP.md`. **Rule:** R; bump `$schema` on a breaking change.

Per ADR-0005 the JSON is bootstrapped **once** from `sim/Sim.Core/Content` by `tools/content-export` and is thereafter the hand-edited source of truth. `tools/content-export --diff` may be run to see how far the JSON has drifted from the frozen C# spec; it never overwrites.

```json
{
  "$schema": "deepfield-content/1",
  "table": "towers",
  "generated": "2026-09-18T00:00:00Z",
  "sourceSha": "71f6059",
  "rows": [
    { "id": "lance", "kind": "Bolt", "cost": 75, "rangeMeters": 12.0, "minRangeMeters": 0.0,
      "damage": 8, "shotsPerSecond": 1.6, "projectileSpeed": 30, "structureHp": 120,
      "applies": [], "targetLayers": ["Ground"],
      "upgradePaths": [ { "id": "damage", "perLevelFactor": 1.10, "levelCosts": [40,54,73,98,132,178,240,324,438],
                          "breakpointRecipes": { "4": {"Alloy": 6, "Plating": 2}, "7": {"Plating": 4, "Flux": 3}, "10": {"Gravium": 2, "Flux": 4} },
                          "breakpoint": { "7": "pierce" } } ],
      "energyHue": "#2B5CFF" }
  ]
}
```

Conventions: keys camelCase (the importer maps to PascalCase struct fields); enums as strings matching the `UENUM` names; scrap bundles as `{ "Alloy": n, … }`; vectors `[x, y, z]` in metres, Y-up (the importer converts to Unreal cm / Z-up / +X forward); colours as `#RRGGBB`; ids are lower camelCase exactly as the sim used them; every row has an `id`; unknown keys fail validation (no silent typos).

The importer (`DFContentPipeline` commandlet, `-run=DFContentImport`) writes `Content/DF/Data/Tables/DT_<Table>.uasset`; those `.uasset`s are never hand-edited. `DF.Content.RoundTrip` re-exports the DataTables and diffs against the JSON.

Level and terrain files are separate formats: `unreal/content/levels/<map>.level.json` (`docs/MAP-AUTHORING.md` §2 extended per `map-authoring-3d.md`) and `unreal/content/terrain/<map>.terrain.json` (`map-authoring-3d.md` §2).
