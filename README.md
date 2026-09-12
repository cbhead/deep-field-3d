# Deep Field 3D

First-person co-op tower defense (1–4 players, self-hosted over Tailscale).
The 3D rebuild of [deep-field-td](https://github.com/cbhead/deep-field-td):
Bloons-depth counters and machine-swept balance, Sanctum-style build-and-shoot.

**Status: M4 in progress.** Four maps, ten enemies, eight towers, five factions,
and the full design system worn across all sixteen UI surfaces — Claude Design's
547 models and 65 icons replacing the graybox everywhere the code reaches,
turrets driven on design's yaw/pitch rigs.
Information warfare has landed: stealth and healing enemies, the Detector and
Filament towers, poison and detection channels, and Night/Fog conditions.
The Toaster adds the three things a farm three fields wide needs: routes with
warp gates that throw a wave across the property mid-walk, four drivable
vehicles whose handling is read off the road under them, and a network of
teleport pads for the player — and its kit has landed: instanced terrain,
roads and treeline, four enterable houses, the four vehicle rigs, the warp
gate. The weapons were rebuilt to the hero standard in the same drop, and
reloads now play on the delivered magazines and off-hand poses.
48 harness gates + 90 unit tests green.

Design's spec is the source of truth for colour ([docs/PALETTE.md](docs/PALETTE.md));
the delivery contract and its gotchas are in
[docs/ART-INTEGRATION.md](docs/ART-INTEGRATION.md).

### For Claude Design — read this first

**What to build next is always the open forward manifest.** One per outstanding
piece of work, each a request list rather than a change log, each written
against a thing that already runs so its numbers are measured rather than
proposed:

| | |
|---|---|
| **[docs/FORWARD-MANIFEST-hero.md](docs/FORWARD-MANIFEST-hero.md)** | **open — after the 2026-09-12 drop.** Share the hero texture set instead of embedding it twenty-four times, a world model that is not the viewmodel, rifle and tool hands to the hero standard, and the Toaster's lane module. |
| [docs/FORWARD-MANIFEST-toaster.md](docs/FORWARD-MANIFEST-toaster.md) | **delivered 2026-09-12** — the whole kit for sector 4 landed and the map runs on it; kept as the record of what was asked and why |
| [docs/FORWARD-MANIFEST-switchyard.md](docs/FORWARD-MANIFEST-switchyard.md) | open — separating what repeats from what punctuates, and the turnout, headwall, overbridge and portal that map still grayboxes |
| [docs/FORWARD-MANIFEST-reload.md](docs/FORWARD-MANIFEST-reload.md) | open — magazines that exist off the gun, off-hand poses, and turret elevation |

Before any of them, [docs/MAP-AUTHORING.md](docs/MAP-AUTHORING.md): what a map
is allowed to be, what the sim can and cannot model, and the rules every map is
measured against. `docs/DESIGN-BRIEF.md` §3 is the standing name list — a model
is requested by the exact name it carries there, and a wrong name is a silent
graybox rather than an error.

## Play

```sh
./play                                   # windowed
./play --headless -- --server            # dedicated server (add --map switchyard)
```

`./play` is self-contained (absolute paths to dotnet + Godot) and works from any
shell; on Windows, `.\play.cmd` takes the same flags. `make run` does the same
if your PATH is set up — see [docs/INSTALL.md](docs/INSTALL.md).

**Host** opens a party: the lobby stays up with a seats row while friends join
and pick factions, and the match starts on **Launch**. **Endless** is a toggle
on the sector — the authored waves cycle with hp and numbers still climbing,
the HUD shows the threat multiplier and your best wave, and the run ends when
the core does.

### Controls

| | |
|---|---|
| WASD / Shift / Space | move, sprint, jump |
| **hold E** at a socket | build wheel — steer with the mouse, release to build |
| **hold U** at a structure | upgrade paths (1–3), **hold X** to sell |
| LMB / RMB | fire · melee swing (arc; melee kills pay +25% scrap) |
| walk over a drop | collect scrap — it buys your weapons, attachments and ammo |
| **Q** faction ability · **hold R** revive | |
| **Tab** | armory + gunsmith · **F** start wave early · **Esc** menu |
| E on a zipline, W on a ladder | traversal |
| **E** at a vehicle · **hold E** to take the passenger seat | drive with WASD, **Space** handbrake, **E** to get out |
| **hold E** on a teleport pad | pick a destination from the network; stand still for the charge |

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

Design's models are generated, not hand-delivered: the three.js sources live in
`docs/design/` and `make design-export` rebuilds every GLB, icon and manifest
from them (see [docs/ART-INTEGRATION.md](docs/ART-INTEGRATION.md)).

```sh
./play -- --shot foundry /tmp/shot.png   # render a frame, for reviewing art
```

CI additionally exports the game headless and runs two smoke lanes: a dedicated
server with a client joining it, and a real solo match on each map.

The full build plan (architecture, netcode, milestones M0–M5) lives in the
project plan; `docs/RUNBOOK-match-night.md` covers hosting.
