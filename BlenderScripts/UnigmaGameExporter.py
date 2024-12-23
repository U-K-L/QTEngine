#Custom exporter for Unigma Engine.
import bpy
import os
import json

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
        # 1) Determine the current working directory:
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
    
    def exportJson(self):
        scene = bpy.context.scene
        scene_name = scene.name
        print(f"Scene Name: {scene_name}")

        scene_data = {
            "SceneName": scene_name,
            "GameObjects": []
        }

        # Collect basic object info
        for obj in scene.objects:
            # Skip hidden objects
            if not obj.visible_get():
                continue

            object_name = obj.name
            local_position = obj.location
            world_position = obj.matrix_world.translation
            # Distinguish cameras, lights, or mesh objects
            object_type = ("Camera" if obj.type == "CAMERA" else
                        "Light" if obj.type == "LIGHT" else
                        "Mesh")

            # Extract world rotation and scale from matrix_world
            world_rotation = obj.matrix_world.to_euler()
            world_scale = obj.matrix_world.to_scale()

            # Custom properties (e.g., BaseAlbedo)
            custom_properties = {}
            for prop_name, prop_value in obj.items():
                if prop_name == "BaseAlbedo":
                    custom_properties[prop_name] = {
                        "r": prop_value[0],
                        "g": prop_value[1],
                        "b": prop_value[2],
                        "a": prop_value[3]
                    }
                else:
                    custom_properties[prop_name] = prop_value

            # Identify parent name (if any)
            parent_name = obj.parent.name if obj.parent else None

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
                "type": object_type,
                "parent": parent_name,
                "CustomProperties": custom_properties
            })

        blend_file_dir = bpy.path.abspath("//")
        blend_file_name_no_ext = os.path.basename(bpy.data.filepath).replace(".blend", "")

        # JSON export path for our custom scene data
        output_file_path = os.path.join(blend_file_dir, f"{blend_file_name_no_ext}.json")

        # 1) Try to load the matching glTF file in the same directory
        gltf_path = os.path.join(blend_file_dir, f"{blend_file_name_no_ext}.gltf")
        if os.path.isfile(gltf_path):
            with open(gltf_path, 'r') as gf:
                gltf_data = json.load(gf)

            # 2) Build a dictionary: node_name -> mesh_index
            node_name_to_mesh_index = {}
            for node in gltf_data.get("nodes", []):
                # Only assign if node has both "name" and "mesh"
                if "name" in node and "mesh" in node:
                    node_name = node["name"]
                    mesh_idx = node["mesh"]
                    node_name_to_mesh_index[node_name] = mesh_idx

            # 3) Update the scene_data game objects
            for gobj in scene_data["GameObjects"]:
                if gobj["type"] == "Mesh":
                    gobj_name = gobj["name"]
                    # If the object name matches a glTF node’s name, set MeshID
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
