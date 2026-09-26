# Deep Field 3D

First-person co-op tower defense for 1–4 players: Bloons-depth counters and machine-swept balance,
with Sanctum-style build-and-shoot. It is the 3D rebuild of
[deep-field-td](https://github.com/cbhead/deep-field-td).

This branch, `main`, holds two things:

- **The Unreal Engine 5.8 rebuild (active).** It targets the Epic Games Store with Epic Online
  Services, is built on the Windows GPU workstation, and Windows is its only platform (ADR-0028).
  It moved onto `main` on 2026-09-26 (ADR-0029); `unreal/main`, where it was built until then, is
  frozen.
- **The Godot 4 client it replaces (frozen).** It retires at gate G3, when `game/` is deleted.

## The Unreal rebuild

| To… | Read |
|---|---|
| set up the machine, build, test, play or package | **[unreal/README.md](unreal/README.md)**, the runbook. On Windows it is one script: `unreal\deepfield.cmd setup`, or the one-line bootstrap in the runbook on a machine with nothing on it yet. |
| see where it stands and what to pick up | [unreal/PLAN/NEXT.md](unreal/PLAN/NEXT.md), then [STATUS.md](unreal/PLAN/STATUS.md) |
| claim work and coordinate with other sessions | [unreal/PLAN/README.md](unreal/PLAN/README.md) |
| understand the plan, the architecture and the specs | [unreal/PLAN/PROGRAMME.md](unreal/PLAN/PROGRAMME.md), with the decisions in [DECISIONS.md](unreal/PLAN/DECISIONS.md) |

| Path | What |
|---|---|
| `unreal/` | the Unreal project (`DeepField/`), its text content source (`content/`), build scripts and CI checks (`Build/`), and the programme ledger (`PLAN/`) |
| `sim/` | the C# sim: the Godot client's engine, and now the frozen written spec for the rebuild's rules |
| `tools/` | the content export from the sim, the terrain tooling, and the Claude Design export |
| `docs/` | the Godot-era design and art documents, plus data the rebuild reads (`palette.json`, `gate-baseline.tsv`) |
| `game/` | the frozen Godot client |

## The Godot client (frozen)

The client that shipped M0–M4: four maps, ten enemies, eight towers and five factions over ENet,
self-hosted on [Tailscale](https://tailscale.com/download), wearing Claude Design's models and full design system. It is frozen: its CI
lane still runs when `game/`, `sim/` or `docs/` change, but no new work goes into it.

- **Install, play and verify:** [docs/INSTALL.md](docs/INSTALL.md) (prerequisites with download links, `./play`, `.\play.cmd`, `make check`).
- **Hosting a match night:** [docs/RUNBOOK-match-night.md](docs/RUNBOOK-match-night.md).
- **Colour** follows design's spec ([docs/PALETTE.md](docs/PALETTE.md)); the art delivery contract and
  its gotchas are in [docs/ART-INTEGRATION.md](docs/ART-INTEGRATION.md). Design's models are generated,
  not hand-delivered: `make design-export` rebuilds them from the three.js sources in `docs/design/`.

### For Claude Design — read this first

**What to build next is always the open forward manifest.** There is one per outstanding piece of
work. Each is a request list rather than a change log, and each is written against something that
already runs, so its numbers are measured rather than proposed:

| | |
|---|---|
| **[docs/FORWARD-MANIFEST-hero.md](docs/FORWARD-MANIFEST-hero.md)** | **open — after the 2026-09-12 drop.** Share the hero texture set instead of embedding it twenty-four times, a world model that is not the viewmodel, and rifle and tool hands to the hero standard. (Its fifth ask, the Toaster's lane module, was delivered 2026-09-13.) |
| [docs/FORWARD-MANIFEST-switchyard.md](docs/FORWARD-MANIFEST-switchyard.md) | open — separating what repeats from what punctuates, and the turnout, headwall, overbridge and portal that the map still grayboxes |
| [docs/FORWARD-MANIFEST-vfx.md](docs/FORWARD-MANIFEST-vfx.md) | open — three effects the code calls by name and gets nothing back for: the third reaction, and the two factions whose abilities were never drawn |
| [docs/FORWARD-MANIFEST-reload.md](docs/FORWARD-MANIFEST-reload.md) | **mostly delivered 2026-09-11** — magazines off the gun, off-hand poses and turret elevation landed. Still open: Ask D, the `poisonStream` and `cryoSprayer` viewmodels. |
| [docs/FORWARD-MANIFEST-toaster.md](docs/FORWARD-MANIFEST-toaster.md) | **delivered 2026-09-12** — the whole kit for sector 4 landed and the map runs on it; kept as the record of what was asked and why |

Before any of them, read [docs/MAP-AUTHORING.md](docs/MAP-AUTHORING.md): what a map is allowed to be,
what the sim can and cannot model, and the rules every map is measured against. `docs/DESIGN-BRIEF.md`
§3 is the standing name list. A model is requested by the exact name it carries there, and a wrong
name is a silent graybox rather than an error.

### Controls

| | |
|---|---|
| WASD or arrow keys / Shift / Space | move, sprint, jump |
| **hold E** at a socket | build wheel — steer with the mouse, release to build |
| **hold U** at a structure | upgrade paths (1–3), **hold X** to sell |
| LMB / RMB | fire · melee swing (arc; melee kills pay +25% scrap) |
| walk over a drop | collect scrap — it buys your weapons, attachments and ammo |
| **Q** faction ability · **hold R** revive | |
| **Tab** | armory + gunsmith · **F** start wave early · **Esc** menu |
| E on a zipline, W or ↑ on a ladder | traversal |
| **E** at a vehicle · **hold E** to take the passenger seat | drive with WASD or the arrows, **Space** handbrake, **E** to get out |
| **hold E** on a teleport pad | pick a destination from the network; stand still for the charge |

**Host** opens a party: the lobby stays up with a seats row while friends join and pick factions, and
the match starts on **Launch**. **Endless** is a toggle on the sector: the authored waves cycle with hp
and numbers still climbing, and the run ends when the core does.
