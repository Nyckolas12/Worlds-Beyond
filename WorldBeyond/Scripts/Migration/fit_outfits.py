"""
Worlds Beyond - snug-fit the demigods' outfits to their MetaHuman bodies.

The Streetwear hoodie / sweatshirt / pants were made for a bigger body, so they float off Angel and Ji-Woong. This pulls
loose cloth in towards the body (keeping a small gap and part of the original looseness so folds survive), pushes
cloth that clips into the body back out, and leaves alone what is meant to hang free (the hood, anything far away).

Run with the editor closed (needs the GeometryScripting plugin, enabled in WorldBeyond.uproject):
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/fit_outfits.py -unattended -nosplash -NullRHI

- Writes <Outfit>_Fitted next to each outfit (the original is never touched) and puts it on the character Blueprint.
- Re-running starts again from the originals: tune the numbers below and run it again.
- To go back to the original outfits, in PowerShell run `$env:BEYOND_FIT_REVERT = "1"` first, then the script
  (then `Remove-Item Env:BEYOND_FIT_REVERT`).
- Report in Saved/MigrationBackups/last_run_fit.txt. Blueprints are backed up before they are saved.
"""
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import BACKUP_DIR, backup, load, log, warn, write_report  # noqa: E402

CHARACTERS = ["/Game/WorldsBeyond/Characters/Ji-Woong/BP_Ji-Woong",
              "/Game/WorldsBeyond/Characters/Angel/BP_Angel"]
FITTED_SUFFIX = "_Fitted"

# All distances in cm, measured from the body's skin
MIN_GAP = 0.6           # cloth closer than this (or inside the body) is pushed out to it
SNUG = 1.5              # looseness that is always kept as it is
KEEP = 0.4              # share of the looseness beyond SNUG that is kept (0 = skin tight, 1 = unchanged)
MAX_PULL = 5.0          # no vertex moves in further than this
MAX_PUSH = 3.0          # ...or out further than this
FREE_FROM = 10.0        # cloth this far from the body starts being left alone...
FREE_BEYOND = 15.0      # ...and from here on is not moved at all (hood, drawstrings)
SMOOTH_ITERATIONS = 6   # the displacement is smoothed over the mesh so seams and folds keep their shape
PROTECT_BONES = ("neck", "head")  # vertices mostly skinned to these keep their shape (hood, collar)
PROTECT_TOP = 12.0      # without bone weights: the top of the body this deep (neck) is protected instead
SKIP_MATERIALS = ("sneaker", "shoe", "boot")  # footwear keeps its shape
MISALIGNED_FRACTION = 0.5  # more than this share of the outfit far from the body: it isn't fitted to it, skip


# ---------------------------------------------------------------- Geometry Script access

REQUIRED_FUNCTIONS = ("copy_mesh_from_skeletal_mesh", "copy_mesh_to_skeletal_mesh", "build_bvh_for_mesh",
                      "find_nearest_point_on_mesh", "get_all_vertex_positions", "get_all_triangle_indices",
                      "get_triangle_face_normal", "get_mesh_bounding_box", "set_all_mesh_vertex_positions",
                      "convert_vector_list_to_array", "convert_array_to_vector_list", "convert_triangle_list_to_array",
                      "recompute_normals")
# Bone weights only sharpen the hood / collar protection; without them it goes by height
OPTIONAL_FUNCTIONS = ("get_all_bones_info", "get_vertex_bone_weights")


def _gs():
    """
    The Geometry Script functions this script uses, looked up by function name across every Geometry Script library
    (their Python class names differ between engine versions). Nothing is changed if one is missing.
    """
    libraries = [getattr(unreal, name) for name in dir(unreal) if name.startswith("GeometryScript")]
    libraries.sort(key=lambda lib: 0 if isinstance(lib, type) and issubclass(lib, unreal.BlueprintFunctionLibrary) else 1)
    functions = {}
    for function in REQUIRED_FUNCTIONS + OPTIONAL_FUNCTIONS:
        for library in libraries:
            if hasattr(library, function):
                functions[function] = getattr(library, function)
                break
    missing = [f for f in REQUIRED_FUNCTIONS if f not in functions]
    if missing:
        raise RuntimeError("Geometry Script functions not found: %s. Is the GeometryScripting plugin enabled? Libraries found: %s"
                           % (", ".join(missing), ", ".join(sorted(l.__name__ for l in libraries if isinstance(l, type))) or "none"))
    return functions


def _pick(result, kind):
    """
    Geometry Script calls return their target mesh first, then their out parameters, as a tuple:
    the first value of the wanted type, or the result itself when it isn't a tuple.
    """
    if isinstance(result, tuple):
        return next((r for r in result if isinstance(r, kind)), None)
    return result if isinstance(result, kind) else None


def _read_lod(gs, skeletal_mesh, lod):
    mesh = unreal.new_object(unreal.DynamicMesh)
    options = unreal.GeometryScriptCopyMeshFromAssetOptions()
    read = unreal.GeometryScriptMeshReadLOD()
    read.set_editor_property("lod_index", lod)
    outcome = _pick(gs["copy_mesh_from_skeletal_mesh"](skeletal_mesh, mesh, options, read), unreal.GeometryScriptOutcomePins)
    if outcome is not None and outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError("could not read LOD %d of %s" % (lod, skeletal_mesh.get_path_name()))
    return mesh


def _write_lod(gs, mesh, skeletal_mesh, lod):
    options = unreal.GeometryScriptCopyMeshToAssetOptions()
    for name, value in (("enable_recompute_normals", False), ("enable_recompute_tangents", True), ("replace_materials", False)):
        try:
            options.set_editor_property(name, value)
        except Exception:
            pass
    write = unreal.GeometryScriptMeshWriteLOD()
    write.set_editor_property("lod_index", lod)
    outcome = _pick(gs["copy_mesh_to_skeletal_mesh"](mesh, skeletal_mesh, options, write), unreal.GeometryScriptOutcomePins)
    if outcome is not None and outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError("could not write LOD %d of %s" % (lod, skeletal_mesh.get_path_name()))


def _positions(gs, mesh):
    vector_list = _pick(gs["get_all_vertex_positions"](mesh, False), unreal.GeometryScriptVectorList)
    return list(_pick(gs["convert_vector_list_to_array"](vector_list), unreal.Array) or [])


def _triangles(gs, mesh):
    triangle_list = _pick(gs["get_all_triangle_indices"](mesh, False), unreal.GeometryScriptTriangleList)
    return list(_pick(gs["convert_triangle_list_to_array"](triangle_list), unreal.Array) or [])


def _lod_count(skeletal_mesh):
    try:
        return unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).get_lod_count(skeletal_mesh)
    except Exception:
        pass
    try:
        return skeletal_mesh.get_lod_num()
    except Exception:
        return 1


# ---------------------------------------------------------------- the fit

def _vec(v):
    return (v.x, v.y, v.z)


def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _scale(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _len(a):
    return math.sqrt(_dot(a, a))


def _smoothstep(edge0, edge1, x):
    t = min(max((x - edge0) / (edge1 - edge0), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def _protected_vertices(gs, mesh, count):
    """Vertices mostly skinned to the neck / head (hood, collar); None if the mesh has no readable bone weights."""
    if "get_all_bones_info" not in gs or "get_vertex_bone_weights" not in gs:
        return None
    try:
        info = gs["get_all_bones_info"](mesh)
        bones = _pick(info, unreal.Array) or []
        names = {b.get_editor_property("index"): str(b.get_editor_property("name")).lower() for b in bones}
        protected = {i for i, n in names.items() if n.startswith(PROTECT_BONES)}
        profile = unreal.GeometryScriptBoneWeightProfile()
        result = set()
        for vid in range(count):
            out = gs["get_vertex_bone_weights"](mesh, vid, profile)
            bone_weights = _pick(out, unreal.Array) or []
            total = sum(w.get_editor_property("weight") for w in bone_weights) or 1.0
            if sum(w.get_editor_property("weight") for w in bone_weights if w.get_editor_property("bone_index") in protected) / total > 0.5:
                result.add(vid)
        return result
    except Exception as e:
        warn("bone weights unreadable (%s); protecting the top of the body by height instead" % e)
        return None


def _fit_lod(gs, cloth, body, body_bvh, body_top, label):
    positions = [_vec(p) for p in _positions(gs, cloth)]
    count = len(positions)
    protected = _protected_vertices(gs, cloth, count)

    displacement = [(0.0, 0.0, 0.0)] * count
    weight = [1.0] * count
    far = 0
    options = unreal.GeometryScriptSpatialQueryOptions()
    for vid, p in enumerate(positions):
        result = gs["find_nearest_point_on_mesh"](body, body_bvh, unreal.Vector(*p), options)
        nearest = _pick(result, unreal.GeometryScriptTrianglePoint)
        if nearest is None or not nearest.get_editor_property("valid"):
            weight[vid] = 0.0
            continue
        surface = _vec(nearest.get_editor_property("position"))
        normal_result = gs["get_triangle_face_normal"](body, nearest.get_editor_property("triangle_id"))
        normal = _vec(_pick(normal_result, unreal.Vector))

        offset = _sub(p, surface)
        distance = _len(offset)
        signed = distance if _dot(offset, normal) >= 0.0 else -distance
        if signed > FREE_BEYOND:
            far += 1

        if signed < MIN_GAP:
            # Clipping into the body: out along the skin normal
            target = surface[0] + normal[0] * MIN_GAP, surface[1] + normal[1] * MIN_GAP, surface[2] + normal[2] * MIN_GAP
            move = _sub(target, p)
            limit = MAX_PUSH
        elif signed > SNUG:
            wanted = SNUG + (signed - SNUG) * KEEP
            move = _scale(offset, (wanted - signed) / distance)
            limit = MAX_PULL
        else:
            move = (0.0, 0.0, 0.0)
            limit = 0.0
        length = _len(move)
        if length > limit > 0.0:
            move = _scale(move, limit / length)
        displacement[vid] = move

        w = 1.0 - _smoothstep(FREE_FROM, FREE_BEYOND, signed)
        if protected is not None:
            if vid in protected:
                w = 0.0
        else:
            w *= _smoothstep(body_top - PROTECT_TOP, body_top - PROTECT_TOP * 2.0, p[2]) if PROTECT_TOP > 0 else 1.0
        weight[vid] = w

    if count and far > count * MISALIGNED_FRACTION:
        raise RuntimeError("%s: %d of %d vertices are more than %.0f cm off the body - not fitted to this body, skipped"
                           % (label, far, count, FREE_BEYOND))

    displacement = [_scale(d, w) for d, w in zip(displacement, weight)]

    # Smooth the field over the mesh so neighbouring vertices move together (no creases, seams stay closed)
    neighbours = [set() for _ in range(count)]
    for tri in _triangles(gs, cloth):
        a, b, c = tri.x, tri.y, tri.z
        if max(a, b, c) < count:
            neighbours[a].update((b, c))
            neighbours[b].update((a, c))
            neighbours[c].update((a, b))
    for _ in range(SMOOTH_ITERATIONS):
        smoothed = []
        for vid in range(count):
            near = neighbours[vid]
            if not near or weight[vid] <= 0.0:
                smoothed.append(displacement[vid])
                continue
            avg = [0.0, 0.0, 0.0]
            for n in near:
                d = displacement[n]
                avg[0] += d[0]
                avg[1] += d[1]
                avg[2] += d[2]
            k = 1.0 / len(near)
            own = displacement[vid]
            smoothed.append(((own[0] + avg[0] * k) * 0.5, (own[1] + avg[1] * k) * 0.5, (own[2] + avg[2] * k) * 0.5))
        displacement = smoothed

    moved = [unreal.Vector(p[0] + d[0], p[1] + d[1], p[2] + d[2]) for p, d in zip(positions, displacement)]
    gs["set_all_mesh_vertex_positions"](cloth, _pick(gs["convert_array_to_vector_list"](moved), unreal.GeometryScriptVectorList))
    gs["recompute_normals"](cloth, unreal.GeometryScriptCalculateNormalsOptions())

    pulled = [_len(d) for d in displacement]
    log("%s: %d vertices, average move %.2f cm, largest %.2f cm" % (label, count, sum(pulled) / max(count, 1), max(pulled or [0.0])))


def fit_outfit(gs, original, body):
    """<original>_Fitted, rebuilt from the original against body."""
    original_path = original.get_path_name().split(".")[0]
    fitted_path = original_path + FITTED_SUFFIX
    eal = unreal.EditorAssetLibrary
    # An earlier run's copy is reused (the Blueprint points at it); every LOD is rewritten from the original below
    fitted = eal.load_asset(fitted_path) if eal.does_asset_exist(fitted_path) else eal.duplicate_asset(original_path, fitted_path)
    if fitted is None:
        raise RuntimeError("could not create %s" % fitted_path)

    body_mesh = _read_lod(gs, body, 0)
    # Out parameters come back as return values in Python: (mesh, bvh)
    bvh_result = gs["build_bvh_for_mesh"](body_mesh)
    bvh = _pick(bvh_result, unreal.GeometryScriptDynamicMeshBVH)
    box = _pick(gs["get_mesh_bounding_box"](body_mesh), unreal.Box)
    body_top = box.max.z

    for lod in range(_lod_count(original)):
        cloth = _read_lod(gs, original, lod)
        _fit_lod(gs, cloth, body_mesh, bvh, body_top, "%s LOD%d" % (original.get_name(), lod))
        _write_lod(gs, cloth, fitted, lod)

    eal.save_loaded_asset(fitted, False)
    return fitted


# ---------------------------------------------------------------- character Blueprints

def _mesh_property(component):
    for name in ("skeletal_mesh_asset", "skeletal_mesh"):
        try:
            return name, component.get_editor_property(name)
        except Exception:
            continue
    return None, None


def _components(bp):
    """The Blueprint's skeletal mesh components, each once (the subobject listing can repeat them)."""
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    seen = set()
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        component = lib.get_object(lib.get_data(handle))
        if isinstance(component, unreal.SkeletalMeshComponent) and component.get_path_name() not in seen:
            seen.add(component.get_path_name())
            yield component


def _is_footwear(mesh):
    for material in mesh.get_editor_property("materials"):
        interface = material.get_editor_property("material_interface")
        names = (str(material.get_editor_property("material_slot_name")), interface.get_name() if interface else "")
        if any(word in name.lower() for name in names for word in SKIP_MATERIALS):
            return True
    return False


def process_character(gs, bp_path, revert):
    bp = load(bp_path)
    if bp is None:
        return
    components = list(_components(bp))
    body = None
    for component in components:
        _, mesh = _mesh_property(component)
        if component.get_name().startswith("Body") and mesh:
            body = mesh

    changed = False
    fitted_by_original = {}  # original path -> fitted mesh, or None when it was skipped / failed
    for component in components:
        prop, mesh = _mesh_property(component)
        if not mesh or "/Clothing/" not in mesh.get_path_name():
            continue
        original_path = mesh.get_path_name().split(".")[0]
        if original_path.endswith(FITTED_SUFFIX):
            original_path = original_path[:-len(FITTED_SUFFIX)]
        original = load(original_path)
        if original is None:
            continue

        if revert:
            if original != mesh:
                component.set_editor_property(prop, original)
                log("%s: %s back to %s" % (bp_path, component.get_name(), original.get_name()))
                changed = True
            continue

        if original_path not in fitted_by_original:
            fitted_by_original[original_path] = None
            if _is_footwear(original):
                log("%s: %s is footwear, left as it is" % (bp_path, original.get_name()))
            elif body is None:
                warn("%s: no Body mesh found, can't fit %s" % (bp_path, original.get_name()))
            else:
                try:
                    fitted_by_original[original_path] = fit_outfit(gs, original, body)
                except Exception as e:
                    warn("%s: %s not fitted (%s)" % (bp_path, original.get_name(), e))
        fitted = fitted_by_original[original_path]
        if fitted is None:
            continue
        component.set_editor_property(prop, fitted)
        log("%s: %s now wears %s" % (bp_path, component.get_name(), fitted.get_name()))
        changed = True

    if changed:
        backup(bp_path)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        if not unreal.EditorAssetLibrary.save_loaded_asset(bp, False):
            warn("failed to save %s" % bp_path)


def main():
    log("backups -> %s" % BACKUP_DIR)
    revert = os.environ.get("BEYOND_FIT_REVERT") == "1"
    try:
        gs = _gs()
    except RuntimeError as e:
        warn(str(e))
        write_report("last_run_fit.txt")
        return
    for path in CHARACTERS:
        try:
            process_character(gs, path, revert)
        except Exception as e:  # keep going with the other character; report at the end
            warn("%s: %s" % (path, e))
    write_report("last_run_fit.txt")


main()
