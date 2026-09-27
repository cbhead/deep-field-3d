"""Turn each imported map's PlayerStart to face its build pads (WS-09, 2026-09-27).

    UnrealEditor-Cmd <DeepField.uproject> -run=pythonscript -script=<this file> -unattended -nullrhi

DFLevelImportCommandlet placed every PlayerStart at yaw 0, so on L_Testlane the hero spawned looking at
empty ground, away from the lane. The importer now faces the pads' centroid; this applies the same
rule to the maps already imported, in place, without re-importing them: for every
/Game/DF/Maps/<Map>/L_<Map> (the persistent level holds the PlayerStart; the pads are in its
_Gameplay sublevel, which loads with it), set the start's yaw toward the pads' centroid and save. Safe to re-run (a start already facing the pads is left alone).
The .umap files are Git LFS and checked out read-only: clear the flag on the ones you mean to change.
"""
import math
import unreal

MAPS = ["Testlane", "Foundry", "Spire", "Switchyard", "Toaster"]
changed = 0
for name in MAPS:
    path = "/Game/DF/Maps/%s/L_%s" % (name, name)
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log_warning("face-player-starts: %s does not exist" % path)
        continue
    world = unreal.EditorLoadingAndSavingUtils.load_map(path)
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
    pads = [a for a in actors if a.get_class().get_name() == "DFSocket"]
    if not starts or not pads:
        unreal.log("face-player-starts: %s: %d starts, %d pads; nothing to do" % (name, len(starts), len(pads)))
        continue
    cx = sum(p.get_actor_location().x for p in pads) / len(pads)
    cy = sum(p.get_actor_location().y for p in pads) / len(pads)
    dirty = False
    for start in starts:
        loc = start.get_actor_location()
        yaw = math.degrees(math.atan2(cy - loc.y, cx - loc.x))
        old = start.get_actor_rotation().yaw
        if abs(((yaw - old) + 180.0) % 360.0 - 180.0) < 0.5:
            continue
        start.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)
        unreal.log("face-player-starts: %s: %s yaw %.1f -> %.1f (toward %d pads)" % (name, start.get_actor_label(), old, yaw, len(pads)))
        dirty = True
    if dirty:
        if not unreal.EditorLoadingAndSavingUtils.save_map(world, path):
            unreal.log_error("face-player-starts: could not save %s (read-only?)" % path)
            raise SystemExit(1)
        changed += 1
unreal.log("face-player-starts: %d map(s) changed" % changed)
