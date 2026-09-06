# Deep Field 3D

First-person co-op tower defense (1–4 players, self-hosted over Tailscale).
The 3D rebuild of [deep-field-td](https://github.com/cbhead/deep-field-td):
Bloons-depth counters and machine-swept balance, Sanctum-style build-and-shoot.

**Status: M0 walking skeleton.** One enemy, one tower, one weapon, one graybox
lane — but the whole pipeline is real: deterministic 30 Hz headless sim, harness
gates with a PlayerBot, serialization resume, and an in-process Godot client.

- `make check` — sim build, unit tests, harness gates, game build
- `make run` — play the graybox (WASD/mouse, **E** build on a socket, **F** start wave, **LMB** fire)

The full build plan (architecture, netcode, milestones M0–M5) lives in the
project plan; `docs/INSTALL.md` tracks setup per milestone.
