GODOT ?= $(HOME)/Applications/Godot_mono.app/Contents/MacOS/Godot
export PATH := $(HOME)/.dotnet:$(PATH)

.PHONY: sim test gates game run import check audit assets usage usage-list

## Build the pure sim (standalone — enforces the no-Godot boundary).
sim:
	dotnet build sim/Sim.Core

## Unit tests.
test:
	dotnet test

## Harness gate suite (the 671-gate culture lives here).
gates:
	dotnet run --project sim/Sim.Harness

## Build the Godot game project.
game:
	dotnet build game/DeepField.Game.csproj

## Run the game windowed.
run: game
	"$(GODOT)" --path game

## (Re)import Godot resources headlessly.
import:
	"$(GODOT)" --headless --import game

## Art delivery status: what the design brief names vs what's in game/assets/.
assets:
	@./tools/asset-report.sh --list

## Which delivered assets does the game actually consume?
usage:
	@./tools/asset-usage.sh

usage-list:
	@./tools/asset-usage.sh --list

## Build one view of every def and check the names against the brief. Needs
## Godot, so it is slower than the rest — but leaving it out of `check` meant
## adding two factions passed locally and failed in CI on the hero models
## nobody had named yet. A pre-push check that does not run what CI runs is
## not a pre-push check.
audit: game
	@"$(GODOT)" --headless --path game -- --asset-audit > /tmp/deepfield-audit.log 2>&1 || \
		{ tail -30 /tmp/deepfield-audit.log; exit 1; }
	@grep -q "\[asset-audit\] built" /tmp/deepfield-audit.log
	@! grep -qiE "^ERROR|SCRIPT ERROR|Unhandled exception" /tmp/deepfield-audit.log
	@./tools/asset-report.sh --verify /tmp/deepfield-audit.log

## Everything CI runs: the pre-push check.
check: sim test gates game audit
