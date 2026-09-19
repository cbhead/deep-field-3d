"""Create /Game/DF/Dev/L_Dev_Empty — the WS-00 smoke level: a 200 m floor, a player start,
sun, sky and fog. Run inside the editor:

  UnrealEditor-Cmd DeepField.uproject -run=pythonscript -script=tools/ue-bridge/ue/make_dev_level.py -nullrhi -unattended

Idempotent: re-running overwrites the level. Real maps are built by build_level.py from
level.json + terrain.json (CONTRACTS/map-authoring-3d.md), not by hand.
"""
import unreal

LEVEL = "/Game/DF/Dev/L_Dev_Empty"


def main() -> None:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if not les.new_level(LEVEL):
        raise RuntimeError(f"could not create {LEVEL}")

    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    floor = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50))
    floor.set_actor_label("Floor")
    floor.static_mesh_component.set_static_mesh(cube)
    floor.set_actor_scale3d(unreal.Vector(200, 200, 1))          # 200 m x 200 m x 1 m
    floor.static_mesh_component.set_collision_profile_name("DF_Graybox")
    floor.set_mobility(unreal.ComponentMobility.STATIC)

    start = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 120))
    start.set_actor_label("PlayerStart")

    sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(-55, -30, 0))
    sun.set_actor_label("Sun")
    sun.light_component.set_intensity(8.0)

    sky = eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    sky.set_actor_label("SkyAtmosphere")
    skylight = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 400))
    skylight.set_actor_label("SkyLight")
    skylight.light_component.set_editor_property("real_time_capture", True)
    fog = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
    fog.set_actor_label("HeightFog")

    if not les.save_current_level():
        raise RuntimeError("save failed")
    unreal.log(f"created {LEVEL}")


main()
