# Install

> This file is a first-class deliverable: it gets updated at every milestone.

## Players (friends joining a match)

1. Download the latest release for your OS from the Releases page
   (`DeepField3D-mac.zip` or `DeepField3D-windows.zip`).
2. Unzip. On macOS the app is unsigned: **right-click → Open** the first time
   (Gatekeeper), then it opens normally.
3. Launch → enter your name → pick a faction (one per player — coordinate) →
   paste the host's Tailscale IP into JOIN.
4. You need to be on the host's tailnet. They'll send you a Tailscale invite if
   you aren't; install Tailscale, accept, done.

If you see "version mismatch" the host has a newer/older release — both grab
the latest and rejoin. Mid-match joining is fine; you'll drop in at the next
intermission with catch-up scrap.

## Developer setup (macOS)

1. **.NET 8 SDK** (user-local, no sudo):
   ```sh
   curl -fsSL https://dot.net/v1/dotnet-install.sh | bash -s -- --channel 8.0
   export PATH="$HOME/.dotnet:$PATH"   # add to your shell profile
   ```
2. **Godot 4.7.x mono (C#)**:
   ```sh
   curl -fsSL -o /tmp/godot-mono.zip \
     "https://github.com/godotengine/godot/releases/download/4.7.2-stable/Godot_v4.7.2-stable_mono_macos.universal.zip"
   unzip -o /tmp/godot-mono.zip -d ~/Applications/
   ```
   (Or `brew install --cask godot-mono dotnet-sdk` if you're fine with sudo.)
3. **Node 22+** — only for `make design-export`, which rebuilds the models from
   Claude Design's sources in `docs/design/` (see docs/ART-INTEGRATION.md). Not
   needed to build or play.
3. **GNU make** (this machine's Xcode CLT shim is broken; brew's make sidesteps it):
   ```sh
   brew install make
   export PATH="/opt/homebrew/opt/make/libexec/gnubin:$PATH"
   ```
4. Clone and verify:
   ```sh
   git clone <repo> deepfield-3d && cd deepfield-3d
   make check           # sim build + unit tests + harness gates + game build
   ./play               # windowed game (controls in the README)
   ./play --headless -- --solo foundry   # headless match, for smoke-testing
   ```

`./play` is self-contained (absolute paths to dotnet + Godot) and works from any
shell regardless of PATH; `make run` does the same but needs the PATH line above.

## Layout

| Path | What |
|---|---|
| `sim/Sim.Core` | The pure headless sim — no Godot references, ever |
| `sim/Sim.Harness` | Gate suite + PlayerBot + match runner (`make gates`) |
| `sim/Sim.Core.Tests` | xUnit unit tests |
| `game/` | Godot 4 client (graybox M0) |

## Server hosting

Arrives at M1 (dedicated server export + Tailscale invite links + runbook).
