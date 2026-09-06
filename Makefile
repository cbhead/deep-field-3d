GODOT ?= $(HOME)/Applications/Godot_mono.app/Contents/MacOS/Godot
export PATH := $(HOME)/.dotnet:$(PATH)

.PHONY: sim test gates game run import check assets

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

## Everything CI runs: the pre-push check.
check: sim test gates game
