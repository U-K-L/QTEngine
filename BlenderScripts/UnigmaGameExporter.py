#Custom exporter for Unigma Engine.
import bpy
import os
import json
from mathutils import Vector

#Exporter menu options
bl_info  = {
    "name": "Unigma Game Exporter",
    "autor": "UKL",
    "blender": (4, 0, 0),
    "version": (0, 0, 1),
    "location": "File > Export",
    "description": "Export scene data for Unigma Engine",
    "category": "Export"
}

class UnigmaExporter(bpy.types.Operator):

    bl_idname = "export_scene.unigma_exporter"
    bl_label = "Export Unigma Scene"
    bl_options = {'PRESET'}
    _timer = None
    
    def invoke(self, context, event):

        blend_file_dir = bpy.path.abspath("//")
        blend_file_name_no_ext = os.path.basename(bpy.data.filepath).replace(".blend", "")
        gltf_path = os.path.join(blend_file_dir, f"{blend_file_name_no_ext}.gltf")

        bpy.ops.export_scene.gltf(
            'EXEC_DEFAULT',
            filepath=gltf_path,
            export_format='GLTF_SEPARATE',
            use_visible=True,
            export_yup=True
        )

        print("Unigma Exporter GLTF. Starting Json")
        self.exportJson()
        print("Unigma Exporter Json. Done")
        return {'FINISHED'}
    
    def make_serializable(self, obj):
        if isinstance(obj, Vector):
            return list(obj)
        if hasattr(obj, "to_list"):
            return obj.to_list()
        if isinstance(obj, dict):
            return {k: self.make_serializable(v) for k, v in obj.items()}
        if isinstance(obj, (list, tuple)):
            return [self.make_serializable(v) for v in obj]
        return obj
    
    def exportJson(self):
        scene = bpy.context.scene
        scene_name = scene.name
        print(f"Scene Name: {scene_name}")

        scene_data = {
            "SceneName": scene_name,
            "GameObjects": []
        }

        for obj in scene.objects:
            # Skip hidden objects
            if not obj.visible_get():
                continue

            object_name = obj.name
            local_position = obj.location
            world_position = obj.matrix_world.translation

            object_type = ("Camera" if obj.type == "CAMERA" else
                        "Light" if obj.type == "LIGHT" else
                        "Mesh")

            world_rotation = obj.matrix_world.to_euler()
            world_scale = obj.matrix_world.to_scale()

            # Custom properties (e.g., BaseAlbedo)
            custom_properties = {}
            outlineNames = ["BaseAlbedo", "TopAlbedo", "SideAlbedo", "InnerOutlineColor", "OuterOutlineColor"]
            for prop_name, prop_value in obj.items():
                if prop_name not in outlineNames:
                    continue
                if isinstance(prop_value, (list, tuple)):  # Handle directly serializable types
                    custom_properties[prop_name] = prop_value
                elif hasattr(prop_value, "to_list"):  # Handle IDPropertyArray
                    custom_properties[prop_name] = prop_value.to_list()
                else:
                    custom_properties[prop_name] = str(prop_value)  # Fallback for unsupported types

            # Get shader properties
            emission = [0, 0, 0, 0]
            print("Starting to process objects...")

            if obj.type == 'MESH' and obj.data.materials:
                print(f"Processing object: {obj.name}")
                for material in obj.data.materials:
                    if material and material.use_nodes:
                        for node in material.node_tree.nodes:
                            print(f"Node: {node.name}, Type: {node.type}")
                            if node.type == 'BSDF_PRINCIPLED':  # Check for Principled BSDF node
                                print("Found Principled BSDF node")
                                if "Emission Strength" in node.inputs:  # Check for "Emission" input
                                    emission = list(node.inputs["Emission Color"].default_value)
                                    emission[3] = node.inputs["Emission Strength"].default_value
                                    print(f"Object: {obj.name}, Emission: {emission}")
                                else:
                                    # Debug available inputs if "Emission" is missing
                                    print(f"Emission input not found in node: {node.name}")
                                    print(f"Available inputs: {[input.name for input in node.inputs]}")


            if obj.type == 'LIGHT':
                light_data = obj.data
                emcolor = list(light_data.color)
                emission = [emcolor[0], emcolor[1], emcolor[2], light_data.energy]
                                        
            parent_name = obj.parent.name if obj.parent else None
            
            compOut = {}
            for comp in obj.components:
                settings = {}
                for k in comp.keys():
                    v = comp[k]
                    # convert mathutils.Vector → list
                    if isinstance(v, Vector):
                        settings[k] = list(v)
                    else:
                        settings[k] = v
                compOut.setdefault(comp.name, []).append(settings)

            serializableComponents = self.make_serializable(compOut)
            # Add info to the scene data structure
            scene_data["GameObjects"].append({
                "name": object_name,
                # The script will fill "MeshID" later after reading the glTF
                "position": {
                    "local": {
                        "x": local_position.x,
                        "y": local_position.y,
                        "z": local_position.z
                    },
                    "world": {
                        "x": world_position.x,
                        "y": world_position.y,
                        "z": world_position.z
                    }
                },
                "rotation": {
                    "local": {
                        "x": obj.rotation_euler.x,
                        "y": obj.rotation_euler.y,
                        "z": obj.rotation_euler.z
                    },
                    "world": {
                        "x": world_rotation.x,
                        "y": world_rotation.y,
                        "z": world_rotation.z
                    }
                },
                "scale": {
                    "local": {
                        "x": obj.scale.x,
                        "y": obj.scale.y,
                        "z": obj.scale.z
                    },
                    "world": {
                        "x": world_scale.x,
                        "y": world_scale.y,
                        "z": world_scale.z
                    }
                },
                "Emission": {
                    "r": emission[0],
                    "g": emission[1],
                    "b": emission[2],
                    "a": emission[3]
                },
                "type": object_type,
                "parent": parent_name,
                "Components": serializableComponents,
                "CustomProperties": custom_properties
            })

        blend_file_dir = bpy.path.abspath("//")
        blend_file_name_no_ext = os.path.basename(bpy.data.filepath).replace(".blend", "")

        # JSON export path for our custom scene data
        output_file_path = os.path.join(blend_file_dir, f"{blend_file_name_no_ext}.json")

        gltf_path = os.path.join(blend_file_dir, f"{blend_file_name_no_ext}.gltf")
        if os.path.isfile(gltf_path):
            with open(gltf_path, 'r') as gf:
                gltf_data = json.load(gf)

            node_name_to_mesh_index = {}
            for node in gltf_data.get("nodes", []):
                if "name" in node and "mesh" in node:
                    node_name = node["name"]
                    mesh_idx = node["mesh"]
                    node_name_to_mesh_index[node_name] = mesh_idx

            for gobj in scene_data["GameObjects"]:
                if gobj["type"] == "Mesh":
                    gobj_name = gobj["name"]
                    if gobj_name in node_name_to_mesh_index:
                        gobj["MeshID"] = node_name_to_mesh_index[gobj_name]
                    else:
                        gobj["MeshID"] = None
        else:
            print(f"No .gltf file found at: {gltf_path}.\nSkipping MeshID assignment.")
        
        # Finally, write out the combined data to JSON
        with open(output_file_path, 'w') as outfile:
            json.dump(scene_data, outfile, indent=4)



# ---- Add Operator to File > Export menu ----
def menu_func_export(self, context):
    self.layout.operator(
        UnigmaExporter.bl_idname,
        text="Unigma Scene Export"
    )

def register():
    bpy.utils.register_class(UnigmaExporter)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)

def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
    bpy.utils.unregister_class(UnigmaExporter)

if __name__ == "__main__":
    register()