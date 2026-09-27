"""Create (or refresh) the default Enhanced Input assets, CONTRACTS/collision-input.md (C16).

    UnrealEditor-Cmd <DeepField.uproject> -run=pythonscript -script=<this file> -unattended -nullrhi

Writes Content/DF/Core/Input/IMC_DF_Default and the actions ADFHeroCharacter binds (IA_Move, IA_Look,
IA_Jump, IA_Sprint, IA_Crouch, IA_Aim, IA_Fire, IA_Reload) and ADFPlayerController binds (IA_Build:
hold E on a socket; IA_Sell: hold X 0.7 s on a tower), with C16's defaults: the Godot bindings (WASD
and the arrow keys, Space, Shift, mouse, LMB / RMB, R, hold E / X) and a gamepad from day one. Safe to
re-run: existing assets are updated in place, and the mapping context's keys are rewritten from the
table below, so the table is the source of truth. The rest of C16's actions (IA_Melee, IA_Upgrade,
...) arrive with the features that use them.

ADFHeroCharacter::Move reads X as strafe and Y as forward; Look adds X to yaw and Y to pitch.
"""
import unreal

PATH = "/Game/DF/Core/Input"
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary


def fail(msg):
    unreal.log_error("make-default-input: " + msg)
    raise SystemExit(1)


def load_or_create(name, cls, factory):
    full = "%s/%s" % (PATH, name)
    if lib.does_asset_exist(full):
        asset = lib.load_asset(full)
    else:
        asset = tools.create_asset(name, PATH, cls, factory)
    if asset is None:
        fail("could not create or load " + full)
    return asset


def action(name, value_type, description):
    ia = load_or_create(name, unreal.InputAction, unreal.InputAction_Factory())
    ia.set_editor_property("value_type", value_type)
    ia.set_editor_property("action_description", description)
    return ia


def key(name):
    k = unreal.Key()
    k.import_text(name)
    if str(k.export_text()) in ("", "None"):
        fail("unknown key name " + name)
    return k


# Modifiers are instanced into the mapping context's package.
def negate(outer, x=True, y=True, z=True):
    m = unreal.new_object(unreal.InputModifierNegate, outer)
    m.set_editor_property("x", x)
    m.set_editor_property("y", y)
    m.set_editor_property("z", z)
    return m


def swizzle_yxz(outer):
    m = unreal.new_object(unreal.InputModifierSwizzleAxis, outer)
    m.set_editor_property("order", unreal.InputAxisSwizzle.YXZ)
    return m


def dead_zone(outer):
    return unreal.new_object(unreal.InputModifierDeadZone, outer)


def hold(ia, seconds):
    """C16's hold-to-act: triggers once, after Seconds held (the action owns the trigger)."""
    t = unreal.new_object(unreal.InputTriggerHold, ia)
    t.set_editor_property("hold_time_threshold", seconds)
    t.set_editor_property("is_one_shot", True)
    ia.set_editor_property("triggers", [t])
    return ia


AXIS2D = unreal.InputActionValueType.AXIS2D
BOOL = unreal.InputActionValueType.BOOLEAN

move = action("IA_Move", AXIS2D, "Walk: X strafes right, Y walks forward")
look = action("IA_Look", AXIS2D, "Look: X turns, Y pitches")
jump = action("IA_Jump", BOOL, "Jump")
sprint = action("IA_Sprint", BOOL, "Sprint while held")
crouch = action("IA_Crouch", BOOL, "Crouch while held")
aim = action("IA_Aim", BOOL, "Aim down sights while held")
fire = action("IA_Fire", BOOL, "Fire: an automatic gun fires while held, a semi-automatic once per pull")
reload = action("IA_Reload", BOOL, "Reload the gun in hand")
build =hold(action("IA_Build", BOOL, "Hold on a free socket: build there"), 0.25)
sell = hold(action("IA_Sell", BOOL, "Hold on a tower: sell it"), 0.7)

imc = load_or_create("IMC_DF_Default", unreal.InputMappingContext, unreal.InputMappingContext_Factory())
imc.set_editor_property("context_description", "On foot (C16 defaults: the Godot bindings plus a gamepad)")

# (action, key, modifiers) -- the source of truth for IMC_DF_Default.
TABLE = [
    (move, "W", lambda o: [swizzle_yxz(o)]),
    (move, "S", lambda o: [swizzle_yxz(o), negate(o)]),
    (move, "D", lambda o: []),
    (move, "A", lambda o: [negate(o)]),
    (move, "Up", lambda o: [swizzle_yxz(o)]),
    (move, "Down", lambda o: [swizzle_yxz(o), negate(o)]),
    (move, "Right", lambda o: []),
    (move, "Left", lambda o: [negate(o)]),
    (move, "Gamepad_Left2D", lambda o: [dead_zone(o)]),
    (look, "Mouse2D", lambda o: []),   # no Y negate: tested 2026-09-27, negated read as inverted
    (look, "Gamepad_Right2D", lambda o: [dead_zone(o)]),
    (jump, "SpaceBar", lambda o: []),
    (jump, "Gamepad_FaceButton_Bottom", lambda o: []),
    (sprint, "LeftShift", lambda o: []),
    (sprint, "Gamepad_LeftThumbstick", lambda o: []),
    (crouch, "LeftControl", lambda o: []),
    (crouch, "C", lambda o: []),
    (crouch, "Gamepad_FaceButton_Right", lambda o: []),
    (aim, "RightMouseButton", lambda o: []),
    (aim, "Gamepad_LeftTrigger", lambda o: []),
    (fire, "LeftMouseButton", lambda o: []),
    (fire, "Gamepad_RightTrigger", lambda o: []),
    (reload, "R", lambda o: []),
    (reload, "Gamepad_FaceButton_Top", lambda o: []),   # face-left is build's
    (build, "E", lambda o: []),
    (build, "Gamepad_FaceButton_Left", lambda o: []),
    (sell, "X", lambda o: []),
    (sell, "Gamepad_DPad_Down", lambda o: []),
]

mappings = []
for ia, key_name, mods in TABLE:
    m = unreal.EnhancedActionKeyMapping()
    m.set_editor_property("action", ia)
    m.set_editor_property("key", key(key_name))
    m.set_editor_property("modifiers", mods(imc))
    mappings.append(m)

data = imc.get_editor_property("default_key_mappings")
data.set_editor_property("mappings", mappings)
imc.set_editor_property("default_key_mappings", data)

for asset in (move, look, jump, sprint, crouch, aim, fire, reload, build, sell, imc):
    if not lib.save_loaded_asset(asset, only_if_is_dirty=False):
        fail("could not save " + asset.get_path_name())

# Read back what was saved, so the log says what the assets hold.
written = imc.get_editor_property("default_key_mappings").get_editor_property("mappings")
unreal.log("make-default-input: IMC_DF_Default has %d mappings" % len(written))
for m in written:
    unreal.log("make-default-input:   %s <- %s (%d modifiers)" % (
        m.get_editor_property("action").get_name(), m.get_editor_property("key").export_text(),
        len(m.get_editor_property("modifiers"))))
