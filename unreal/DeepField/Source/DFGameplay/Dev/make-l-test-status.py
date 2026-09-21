"""Build WS-02's DoD level (PROGRAMME.md §5.2 WS-02: "L_Test_Status shows chill+burn -> thermalShock
12% via cue"):

  /Game/DF/Dev/L_Test_Status   a 40 m floor, a player start, sun, sky light, one ADFStatusTestRig

The rig (Source/DFGameplay/Public/Dev/DFStatusTestRig.h) does the rest at play: it chills and burns
itself every cycle, the status component detonates thermalShock, and the native
UDFGameplayCueNotify_Reaction draws the burst and forwards the cue to UDFMessageBus. No cue asset
is needed (UDFGameplayCueNotify_Base::RegisterNativeCues). DF.Func.Status.ThermalShockInLevel opens
this map. Run headless through the editor lock, after the editor target has been built:

  unreal/Build/editor-lock.sh "$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd" unreal/DeepField/DeepField.uproject \
      -run=pythonscript -script=unreal/DeepField/Source/DFGameplay/Dev/make-l-test-status.py \
      -nullrhi -unattended -nop4 -nosplash -NoSound

Idempotent: re-running overwrites the level. The script lives beside the module (tools/ue-bridge/ue/
is WS-30's). The LEVEL IT WRITES IS NOT COMMITTED: Content/DF/Dev is WS-15's glob, so a .umap from a
[WS-02] PR is an ownership-check violation until INT adds the row the WS file's Needs INT asks for.
DF.Func.Status.ThermalShockInLevel therefore opens this level when it is on disk and otherwise falls
back to WS-00's committed /Game/DF/Dev/L_Dev_Empty and spawns the rig itself — run this script when
you want to LOOK at the DoD (the debug sphere, the label), not to make the test pass.
"""
import unreal

LEVEL = "/Game/DF/Dev/L_Test_Status"


def main() -> None:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not les.new_level(LEVEL):
        raise RuntimeError(f"could not create {LEVEL}")

    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    floor = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50))
    floor.set_actor_label("Floor")
    floor.static_mesh_component.set_static_mesh(cube)
    floor.set_actor_scale3d(unreal.Vector(40, 40, 1))            # 40 m x 40 m x 1 m
    floor.static_mesh_component.set_collision_profile_name("DF_Graybox")
    floor.set_mobility(unreal.ComponentMobility.STATIC)

    start = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-600, 0, 120), unreal.Rotator(0, 0, 0))
    start.set_actor_label("PlayerStart")

    sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(-55, -30, 0))
    sun.set_actor_label("Sun")
    sun.light_component.set_intensity(8.0)
    skylight = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 400))
    skylight.set_actor_label("SkyLight")
    skylight.light_component.set_editor_property("real_time_capture", True)

    rig = eas.spawn_actor_from_class(unreal.DFStatusTestRig, unreal.Vector(0, 0, 100))
    rig.set_actor_label("StatusRig")

    if not les.save_current_level():
        raise RuntimeError("save failed")
    if not unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
        raise RuntimeError(f"missing after save: {LEVEL}")
    unreal.log(f"created {LEVEL}: OK")


main()
