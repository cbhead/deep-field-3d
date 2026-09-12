# Assets — the drop-in folder

Everything Claude Design delivers lands here. **Copying a file in is the whole
integration step.** No code change, no scene edit, no particular order.

The game asks for each asset by the exact name it carries in
[docs/DESIGN-BRIEF.md](../../docs/DESIGN-BRIEF.md). If the file is here it
renders; if it isn't, the graybox in `game/scripts/Placeholders.cs` stands in.
So a half-delivered batch is fine — the Drifter can be a finished model while
the Monolith is still a box.

## Where things go

| Folder | Name prefixes |
|---|---|
| `enemies/` | `enemy_*`, `boss_*` |
| `structures/` | `tower_*`, `trap_*`, `socket_*` |
| `weapons/` | `weapon_*`, `attach_*`, `ammo_*`, `hands_*` |
| `heroes/` | `hero_*` |
| `maps/` | `foundry_*`, `switchyard_*`, `spire_*`, `toaster_*`, `shared_*`, `prop_*` |
| `vehicles/` | `vehicle_*` — the four drivable rigs; named nodes (`seat_*`, `wheel_*`, `steer_*`) are what the code moves |
| `vfx/` | `vfx_*`, `proj_*` |
| `economy/` | `pickup_*` |
| `ui/` | `icon_*.png`, `ui_*` |

The prefix decides the folder — `game/scripts/AssetLibrary.cs` holds the table.

## Format

- **`.glb`** (glTF 2.0), 1 unit = 1 metre, Y-up / −Z forward.
- Pivot at **ground centre** for anything placed on a socket; at the **spine
  base** for enemies and heroes.
- A `.tscn` of the same name wins over the `.glb`, so a model needing an import
  tweak (collision, AnimationPlayer, material) can be wrapped without touching
  code.
- Materials must be `StandardMaterial3D` if the unit needs status tinting —
  the game modulates albedo and emission at runtime rather than replacing them
  (see `game/scripts/TintableView.cs`). ShaderMaterial is fine, it just means
  that unit owns its own look and reads status through icons and particles.

## Checking coverage

```sh
make assets      # what design has delivered vs what the brief names
```

The running game also prints `[assets] N/M resolved` on exit — that is the
other half: what the *code* actually asks for.
