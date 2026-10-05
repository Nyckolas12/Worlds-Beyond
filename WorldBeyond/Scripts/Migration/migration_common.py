"""
Helpers shared by the Worlds Beyond migration scripts (run inside UnrealEditor-Cmd -run=pythonscript).

Every asset saved through save() is first copied to Saved/MigrationBackups/<timestamp>/.
"""
import datetime
import os
import shutil

import unreal

BEL = unreal.BlueprintEditorLibrary
EAL = unreal.EditorAssetLibrary
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

CONTENT_DIR = os.path.abspath(unreal.Paths.project_content_dir())
BACKUP_DIR = os.path.join(os.path.abspath(unreal.Paths.project_saved_dir()), "MigrationBackups",
                          datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))

LOG = []
_backed_up = set()


def log(msg):
    LOG.append(msg)
    unreal.log("MIGRATE " + msg)


def warn(msg):
    LOG.append("WARNING: " + msg)
    unreal.log_warning("MIGRATE " + msg)


def write_report(file_name):
    report = os.path.join(os.path.abspath(unreal.Paths.project_saved_dir()), "MigrationBackups", file_name)
    os.makedirs(os.path.dirname(report), exist_ok=True)
    with open(report, "w", encoding="utf-8") as fh:
        fh.write("\n".join(LOG))
    log("done - report at %s" % report)


# ---------------------------------------------------------------- assets

def package_file(asset_path):
    rel = asset_path.replace("/Game/", "", 1)
    for ext in (".uasset", ".umap"):
        candidate = os.path.join(CONTENT_DIR, rel + ext)
        if os.path.exists(candidate):
            return candidate
    return None


def backup(asset_path):
    if asset_path in _backed_up:
        return
    src = package_file(asset_path)
    if src:
        dst = os.path.join(BACKUP_DIR, os.path.relpath(src, CONTENT_DIR))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
    _backed_up.add(asset_path)


def load(asset_path):
    asset = EAL.load_asset(asset_path)
    if asset is None:
        warn("could not load %s" % asset_path)
    return asset


def save(asset, asset_path):
    backup(asset_path)
    if not EAL.save_loaded_asset(asset, False):
        warn("failed to save %s" % asset_path)


def bp_class(asset_path):
    return EAL.load_blueprint_class(asset_path)


def cdo(bp):
    return unreal.get_default_object(bp.generated_class())


def tag(name):
    t = unreal.GameplayTag()
    t.import_text('(TagName="%s")' % name)
    return t


def tag_container(*names):
    c = unreal.GameplayTagContainer()
    c.import_text("(GameplayTags=(%s))" % ",".join('(TagName="%s")' % n for n in names))
    return c


def reparent(asset_path, new_parent):
    bp = load(asset_path)
    if bp is None:
        return None
    current = BEL.get_blueprint_parent_class(bp)
    if current is not None and current.get_name() == new_parent.static_class().get_name():
        return bp
    backup(asset_path)
    BEL.reparent_blueprint(bp, new_parent)
    ok = BEL.compile_blueprint(bp)
    log("reparented %s: %s -> %s (compiles=%s)" % (asset_path, current.get_name() if current else None,
                                                  new_parent.static_class().get_name(), ok))
    return bp


def set_props(bp, asset_path, **props):
    obj = cdo(bp)
    for name, value in props.items():
        try:
            obj.set_editor_property(name, value)
        except Exception as e:  # keep going; report at the end
            warn("%s: could not set %s (%s)" % (asset_path, name, e))
    save(bp, asset_path)


def ensure_blueprint(asset_path, parent):
    bp = EAL.load_asset(asset_path) if EAL.does_asset_exist(asset_path) else None
    if bp is None:
        bp = BEL.create_blueprint_asset_with_parent(asset_path, parent)
        log("created %s (%s)" % (asset_path, parent.static_class().get_name()))
    BEL.compile_blueprint(bp)
    return bp


def ensure_input_action(name):
    path = "/Game/Input/Actions/" + name
    if EAL.does_asset_exist(path):
        return load(path)
    factory = unreal.InputAction_Factory() if hasattr(unreal, "InputAction_Factory") else None
    action = ASSET_TOOLS.create_asset(name, "/Game/Input/Actions", unreal.InputAction, factory)
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    EAL.save_loaded_asset(action, False)
    log("created input action %s" % path)
    return action


def make_key(name):
    key = unreal.Key()
    key.set_editor_property("key_name", name)
    return key


def map_keys(imc_path, wanted):
    """wanted: {input action name: key name}. Adds missing mappings to the mapping context."""
    imc = load(imc_path)
    existing = {(m.get_editor_property("action").get_name() if m.get_editor_property("action") else "",
                 str(m.get_editor_property("key").get_editor_property("key_name")))
                for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings")}
    changed = False
    actions = {}
    for action_name, key_name in wanted.items():
        action = ensure_input_action(action_name)
        actions[action_name] = action
        if (action_name, key_name) not in existing:
            imc.map_key(action, make_key(key_name))
            log("mapped %s -> %s" % (key_name, action_name))
            changed = True
    if changed:
        save(imc, imc_path)
    return actions


def ensure_montage(sequence_path, montage_path):
    """A montage playing sequence_path, created next to it if it doesn't exist yet."""
    if EAL.does_asset_exist(montage_path):
        return load(montage_path)
    sequence = load(sequence_path)
    if sequence is None:
        return None
    folder, name = montage_path.rsplit("/", 1)
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", sequence)
    factory.set_editor_property("target_skeleton", sequence.get_editor_property("skeleton"))
    montage = ASSET_TOOLS.create_asset(name, folder, unreal.AnimMontage, factory)
    if montage is None:
        warn("could not create montage %s" % montage_path)
        return None
    EAL.save_loaded_asset(montage, False)
    log("created montage %s from %s" % (montage_path, sequence_path))
    return montage


def fx(system=None, sound=None, shake=None, scale=1.0, offset=None, color=None, shake_radius=2000.0):
    """unreal.BeyondFX from asset paths (missing assets are reported and left empty)."""
    value = unreal.BeyondFX()
    if system:
        asset = load(system)
        if asset:
            value.set_editor_property("system", asset)
    if sound:
        asset = load(sound)
        if asset:
            value.set_editor_property("sound", asset)
    if shake:
        shake_class = bp_class(shake)
        if shake_class:
            value.set_editor_property("camera_shake", shake_class)
            value.set_editor_property("camera_shake_radius", shake_radius)
    value.set_editor_property("scale", unreal.Vector(scale, scale, scale))
    if offset:
        value.set_editor_property("offset", unreal.Vector(*offset))
    if color:
        value.set_editor_property("override_color", True)
        value.set_editor_property("color", unreal.LinearColor(*color))
    return value
