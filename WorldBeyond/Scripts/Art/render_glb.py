"""
Renders a GLB from three angles into one preview PNG with Blender (headless), and prints its triangle count and textures:
    blender -b --factory-startup -P Scripts/Art/render_glb.py -- <model.glb> <preview.png> [size]
Used to compare generated 3D models (Plan 6: item inspect view, loot drops, main menu relic).
"""
import math
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
src, out = argv[0], argv[1]
size = int(argv[2]) if len(argv) > 2 else 512

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]

# Stats
triangles = 0
for obj in meshes:
    obj.data.calc_loop_triangles()
    triangles += len(obj.data.loop_triangles)
images = {img.name: tuple(img.size) for img in bpy.data.images}
print("GLB %s: %d meshes, %d triangles, images %s" % (src, len(meshes), triangles, images))

# Bounds
corners = [obj.matrix_world @ Vector(c) for obj in meshes for c in obj.bound_box]
low = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
high = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
centre = (low + high) / 2
radius = max((high - low).length / 2, 1e-3)
print("bounds %s .. %s" % (tuple(round(v, 3) for v in low), tuple(round(v, 3) for v in high)))

scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = size
scene.render.resolution_y = size
scene.render.film_transparent = False
world = bpy.data.worlds.new("World")
scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.02, 0.03, 0.07, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.6

# Three-point light
for name, offset, energy in (("Key", (1.5, -1.5, 2.0), 900.0), ("Fill", (-2.0, -1.0, 0.5), 300.0), ("Rim", (0.0, 2.0, 1.5), 700.0)):
    light = bpy.data.lights.new(name, "AREA")
    light.energy = energy * radius * radius
    light.size = radius * 2
    obj = bpy.data.objects.new(name, light)
    scene.collection.objects.link(obj)
    obj.location = centre + Vector(offset) * radius * 2.5
    obj.rotation_euler = (centre - obj.location).to_track_quat("-Z", "Y").to_euler()

camera = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
scene.collection.objects.link(camera)
scene.camera = camera
camera.data.lens = 50

frames = []
for index, yaw in enumerate((-30.0, 60.0, 180.0)):
    angle = math.radians(yaw)
    direction = Vector((math.sin(angle), -math.cos(angle), 0.35)).normalized()
    camera.location = centre + direction * radius * 3.2
    camera.rotation_euler = (centre - camera.location).to_track_quat("-Z", "Y").to_euler()
    path = out.replace(".png", "_%d.png" % index)
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    frames.append(path)

# One sheet
sheet = bpy.data.images.new("Sheet", size * 3, size)
pixels = [0.0] * (size * 3 * size * 4)
for index, path in enumerate(frames):
    image = bpy.data.images.load(path)
    data = list(image.pixels)
    for y in range(size):
        row = data[y * size * 4:(y + 1) * size * 4]
        start = (y * size * 3 + index * size) * 4
        pixels[start:start + size * 4] = row
sheet.pixels = pixels
sheet.filepath_raw = out
sheet.file_format = "PNG"
sheet.save()
print("wrote", out)
