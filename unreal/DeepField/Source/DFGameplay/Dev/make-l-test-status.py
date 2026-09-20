"""Build WS-02's L_Test_Status content (PROGRAMME.md §5.2 WS-02 DoD: "L_Test_Status shows chill+burn
-> thermalShock 12% via cue"). Everything it writes is WS-02's (OWNERSHIP.md: Content/DF/Gameplay/Cues;
the level sits at the Gameplay root pending an ownership row — see the WS file's Needs INT):

  /Game/DF/Gameplay/Cues/GC_DF_Status                 GameplayCue.DF.Status               UDFGameplayCueNotify_Status   (fallback for every status cue)
  /Game/DF/Gameplay/Cues/GC_DF_Reaction               GameplayCue.DF.Reaction             UDFGameplayCueNotify_Reaction (fallback for every reaction cue)
  /Game/DF/Gameplay/Cues/GC_DF_Reaction_ThermalShock  GameplayCue.DF.Reaction.ThermalShock (the leaf the DoD names)
  /Game/DF/Gameplay/L_Test_Status                     floor, sun, sky light, player start, one ADFStatusTestRig

The cue tags are written by name into UDFGameplayCueNotify::CueTagName (FGameplayTag itself is not
scriptable) and verified after the save. Run headless
through the editor lock, after the editor target has been built:

  unreal/Build/editor-lock.sh "$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd" unreal/DeepField/DeepField.uproject \
      -run=pythonscript -script=unreal/DeepField/Source/DFGameplay/Dev/make-l-test-status.py \
      -nullrhi -unattended -nop4 -nosplash -NoSound

Idempotent: re-running overwrites the assets and the level.
"""
import unreal

CUES_DIR = "/Game/DF/Gameplay/Cues"
LEVEL = "/Game/DF/Gameplay/L_Test_Status"
CUES = [
    ("GC_DF_Status", unreal.DFGameplayCueNotify_Status, "GameplayCue.DF.Status"),
    ("GC_DF_Reaction", unreal.DFGameplayCueNotify_Reaction, "GameplayCue.DF.Reaction"),
    ("GC_DF_Reaction_ThermalShock", unreal.DFGameplayCueNotify_Reaction, "GameplayCue.DF.Reaction.ThermalShock"),
]


def cue_tag_of(blueprint) -> str:
    cdo = unreal.get_default_object(blueprint.generated_class())
    return str(cdo.get_editor_property("gameplay_cue_tag").get_editor_property("tag_name"))


def make_cue(name: str, parent, expected_tag: str) -> None:
    path = f"{CUES_DIR}/{name}"
    lib = unreal.EditorAssetLibrary
    if lib.does_asset_exist(path):
        if not lib.delete_asset(path):
            raise RuntimeError(f"could not delete {path}")
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, CUES_DIR, unreal.Blueprint, factory)
    if bp is None:
        raise RuntimeError(f"could not create {path}")
    # FGameplayTag is not scriptable; UDFGameplayCueNotify resolves the NAME into GameplayCueTag.
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("cue_tag_name", expected_tag)
    tag = cue_tag_of(bp)
    if not lib.save_loaded_asset(bp):
        raise RuntimeError(f"save failed: {path}")
    if tag != expected_tag:
        raise RuntimeError(f"{path}: GameplayCueTag is '{tag}', expected '{expected_tag}' (is the tag in Config/Tags/DF_Gameplay.ini?)")
    unreal.log(f"{path}: {tag} ({parent.__name__})")


def make_level() -> None:
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
    unreal.log(f"created {LEVEL}")


def main() -> None:
    for name, parent, tag in CUES:
        make_cue(name, parent, tag)
    make_level()
    for name, _, _ in CUES:
        if not unreal.EditorAssetLibrary.does_asset_exist(f"{CUES_DIR}/{name}"):
            raise RuntimeError(f"missing after save: {CUES_DIR}/{name}")
    if not unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
        raise RuntimeError(f"missing after save: {LEVEL}")
    unreal.log("L_Test_Status content: OK")


main()
