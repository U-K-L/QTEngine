#Import scene JSON back into a Blender scene.
import bpy
import os
import json
import math
from mathutils import Euler, Vector

bl_info = {
    "name": "Unigma Scene Importer",
    "author": "UKL",
    "blender": (4, 0, 0),
    "version": (0, 0, 1),
    "location": "File > Import",
    "description": "Import Unigma Engine scene JSON into the current Blender scene",
    "category": "Import"
}


class UnigmaImporter(bpy.types.Operator):

    bl_idname = "import_scene.unigma_importer"
    bl_label = "Import Unigma Scene"
    bl_options = {'REGISTER', 'UNDO'}

    filepath: bpy.props.StringProperty(subtype='FILE_PATH')
    filter_glob: bpy.props.StringProperty(default="*.json", options={'HIDDEN'})

    def invoke(self, context, event):
        # Default to the .blend file's directory.
        blend_dir = bpy.path.abspath("//")
        if blend_dir:
            blend_name = os.path.splitext(os.path.basename(bpy.data.filepath))[0]
            default_path = os.path.join(blend_dir, f"{blend_name}.json")
            if os.path.isfile(default_path):
                self.filepath = default_path
            else:
                self.filepath = blend_dir
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        if not os.path.isfile(self.filepath):
            self.report({'ERROR'}, f"File not found: {self.filepath}")
            return {'CANCELLED'}

        with open(self.filepath, 'r') as f:
            scene_data = json.load(f)

        scene = context.scene
        updated = 0
        skipped = []

        for gobj in scene_data.get("GameObjects", []):
            name = gobj.get("name")
            if not name:
                continue

            obj = scene.objects.get(name)
            if obj is None:
                skipped.append(name)
                continue

            # Position (use local).
            pos = gobj.get("position", {}).get("local", {})
            if pos:
                obj.location = (pos.get("x", 0), pos.get("y", 0), pos.get("z", 0))

            # Rotation (use local, radians).
            rot = gobj.get("rotation", {}).get("local", {})
            if rot:
                obj.rotation_euler = Euler((rot.get("x", 0), rot.get("y", 0), rot.get("z", 0)))

            # Scale (use local).
            scl = gobj.get("scale", {}).get("local", {})
            if scl:
                obj.scale = (scl.get("x", 1), scl.get("y", 1), scl.get("z", 1))

            # Custom properties (Midtone, Highlight, etc.).
            for prop_name, prop_value in gobj.get("CustomProperties", {}).items():
                if isinstance(prop_value, list) and len(prop_value) == 4:
                    setattr(obj, prop_name, prop_value)

            # Emission (update light data if it's a light).
            em = gobj.get("Emission", {})
            if obj.type == 'LIGHT' and em:
                obj.data.color = (em.get("r", 1), em.get("g", 1), em.get("b", 1))
                obj.data.energy = em.get("a", 1000)

            # Light type.
            if obj.type == 'LIGHT' and "LightType" in gobj:
                light_type_map = {0: 'SUN', 1: 'POINT', 2: 'SPOT', 3: 'AREA'}
                lt = light_type_map.get(int(gobj["LightType"]))
                if lt:
                    obj.data.type = lt

            # Camera fields from Components.CameraComp.
            if obj.type == 'CAMERA':
                cam_comp = gobj.get("Components", {}).get("CameraComp", {})
                if cam_comp:
                    if "FOV" in cam_comp:
                        obj.data.angle = math.radians(float(cam_comp["FOV"]))
                    if "NearClip" in cam_comp:
                        obj.data.clip_start = float(cam_comp["NearClip"])
                    if "FarClip" in cam_comp:
                        obj.data.clip_end = float(cam_comp["FarClip"])
                    if "CameraType" in cam_comp:
                        obj.data.type = 'ORTHO' if cam_comp["CameraType"] == "Orthogonal" else 'PERSP'
                    if "OrthographicSize" in cam_comp:
                        obj.data.ortho_scale = float(cam_comp["OrthographicSize"])

            updated += 1

        msg = f"Updated {updated} object(s)."
        if skipped:
            msg += f" Skipped (not found): {', '.join(skipped)}"
        self.report({'INFO'}, msg)
        print(msg)
        return {'FINISHED'}


def menu_func_import(self, context):
    self.layout.operator(
        UnigmaImporter.bl_idname,
        text="Unigma Scene Import"
    )

def register():
    bpy.utils.register_class(UnigmaImporter)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)

def unregister():
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    bpy.utils.unregister_class(UnigmaImporter)

if __name__ == "__main__":
    register()
