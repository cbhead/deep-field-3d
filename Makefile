GODOT ?= $(HOME)/Applications/Godot_mono.app/Contents/MacOS/Godot
export PATH := $(HOME)/.dotnet:$(PATH)

.PHONY: sim test gates game run import check audit assets usage usage-list design-export map-validate model-validate gate-baseline

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
##
## --path game --import, not --import game: Godot 4.7 silently no-ops on the
## latter and every asset stays unimported. CI already had this right; this
## target did not, and a fresh pull of 547 models played as grey boxes.
import:
	"$(GODOT)" --headless --path game --import

## Art delivery status: what the design brief names vs what's in game/assets/.
assets:
	@./tools/asset-report.sh --list

## Rebuild every model, icon and manifest from Claude Design's sources in
## docs/design/. Needs a browser (the skyboxes are shaders baked to a texture)
## and node; writes straight into game/assets/ and docs/.
##   make design-export              # the whole drop
##   make design-export ONLY=lance   # one tower — chassis and its 30 stages
##   make design-export ONLY=drifter # one model, by item id or file stem
##   make design-export ONLY=vfx     # a whole category
design-export:
	@./tools/design-export.sh $(if $(ONLY),--only "$(ONLY)")

## Every delivered model against the contract in docs/ART-INTEGRATION.md.
## No Godot, no browser, no dependencies — it reads the glTF directly.
##   make model-validate            # the report
##   ./tools/model-validate.py --list     # every violation
##   ./tools/model-validate.py --refresh  # re-record the pivot ratchet
model-validate:
	@./tools/model-validate.py

## Every map against the rules in docs/MAP-AUTHORING.md §4.
map-validate:
	@./tools/map-validate.sh

## The numbers behind the balance gates, not their verdicts -> docs/gate-baseline.tsv.
## Commit the diff with the change that caused it, saying whether it is a
## re-baseline (hashes and margins move, verdicts do not) or a regression.
gate-baseline:
	@dotnet run --project sim/Sim.Harness --configuration Release -- --baseline

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
check: sim test gates model-validate game audit
