# Deep Field 3D

First-person co-op tower defense (1–4 players, self-hosted over Tailscale).
The 3D rebuild of [deep-field-td](https://github.com/cbhead/deep-field-td):
Bloons-depth counters and machine-swept balance, Sanctum-style build-and-shoot.

**Status: M3 in progress.** Two maps, ten enemies, eight towers, five factions,
and the full design system worn across all sixteen UI surfaces — Claude Design's
380 models and 58 icons replacing the graybox everywhere the code reaches.
Information warfare has landed: stealth and healing enemies, the Detector and
Filament towers, poison and detection channels, and Night/Fog conditions.
35 harness gates + 19 unit tests green.

Design's spec is the source of truth for colour ([docs/PALETTE.md](docs/PALETTE.md));
the delivery contract and its gotchas are in
[docs/ART-INTEGRATION.md](docs/ART-INTEGRATION.md).

## Play

```sh
./play                                   # windowed
./play --headless -- --server            # dedicated server (add --map switchyard)
```

`./play` is self-contained (absolute paths to dotnet + Godot) and works from any
shell. `make run` does the same if your PATH is set up — see
[docs/INSTALL.md](docs/INSTALL.md).

### Controls

| | |
|---|---|
| WASD / Shift / Space | move, sprint, jump |
| **hold E** at a socket | build wheel — steer with the mouse, release to build |
| **hold U** at a structure | upgrade paths (1–3), **hold X** to sell |
| LMB | fire · **Q** faction ability · **hold R** revive |
| **Tab** | armory + gunsmith · **F** start wave early · **Esc** menu |
| E on a zipline, W on a ladder | traversal |

## What's in it

- **Sim** (`sim/Sim.Core`) — pure C#, zero Godot references (CI enforces it):
  deterministic 30 Hz tick, seeded wave plans, status channels with reactions,
  scrap economy, faction abilities, full-world serialization.
- **Harness** (`sim/Sim.Harness`) — the gate suite and PlayerBot sweeps. No
  engine boot, so a full campaign runs in milliseconds.
- **Client** (`game/`) — Godot 4.7 + C#: FPS controller, four run modes
  (solo/host/dedicated/client), ENet netcode with drop-in join, and the UI in
  `game/scripts/ui/`.

## Verify

```sh
make check    # sim build + unit tests + harness gates + game build
make assets   # art delivered vs what the design brief names
make usage    # what the game actually consumes (docs/ASSET-USAGE.md)
```

```sh
./play -- --shot foundry /tmp/shot.png   # render a frame, for reviewing art
```

CI additionally exports the game headless and runs two smoke lanes: a dedicated
server with a client joining it, and a real solo match on each map.

The full build plan (architecture, netcode, milestones M0–M5) lives in the
project plan; `docs/RUNBOOK-match-night.md` covers hosting.
