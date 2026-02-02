import os, bpy, json

if not bpy.data.filepath:
    raise RuntimeError("Save your .blend first so we know where “//” is!")

blend_dir = bpy.path.abspath("//")
json_path  = os.path.normpath(
    os.path.join(blend_dir, "..", "..", "Components", "Components.json")
)

with open(json_path) as f:
    COMPONENT_DEFS = json.load(f)


class ComponentItem(bpy.types.PropertyGroup):
    name: bpy.props.StringProperty()
    settings: bpy.props.StringProperty()

class COMPONENTS_UL_items(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        layout.label(text=item.name)

class OBJECT_OT_add_component(bpy.types.Operator):
    bl_idname = "object.add_component"
    bl_label = "Add Component"
    component: bpy.props.EnumProperty(items=[(k, k, "") for k in COMPONENT_DEFS.keys()])

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):
        obj   = context.object
        new   = obj.components.add()
        new.name = self.component

        for field, typ in COMPONENT_DEFS[self.component].items():
            # NEW: support schema objects: {"type": "...", "default": ...}
            default = None
            if isinstance(typ, dict):
                default = typ.get("default")
                typ = typ.get("type")

            if isinstance(typ, list):
                new[field] = default if default is not None else typ[0]
            elif typ == "bool":
                new[field] = default if default is not None else False
            elif typ == "float":
                new[field] = float(default) if default is not None else 0.0
            elif typ == "vector3":
                new[field] = default if default is not None else [1.0, 1.0, 1.0]
            elif typ == "int":
                new[field] = int(default) if default is not None else 0
            else:
                new[field] = default if default is not None else ""


        return {'FINISHED'}

class OBJECT_PT_components(bpy.types.Panel):
    bl_label = "Components"
    bl_idname = "OBJECT_PT_components"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    def draw(self, context):
        layout = self.layout
        obj    = context.object
        row = layout.row(align=True)
        row.operator("object.remove_component", text="Remove", icon='X')

        layout.template_list("COMPONENTS_UL_items", "", obj, "components",
                                         obj, "components_index")
        layout.operator("object.add_component")

        idx = obj.components_index
        if idx >= 0 and idx < len(obj.components):
            comp_def = COMPONENT_DEFS[obj.components[idx].name]
            comp_inst = obj.components[idx]

            for field, typ in comp_def.items():
                # this will show a checkbox for bool,
                # a number field for float/int,
                # a XYZ‐vector widget for vector3,
                # a text input for string/enum
                layout.prop(comp_inst, f'["{field}"]', text=field)

class OBJECT_OT_remove_component(bpy.types.Operator):
    bl_idname = "object.remove_component"
    bl_label = "Remove Component"
    bl_description = "Remove the selected component"

    @classmethod
    def poll(cls, context):
        obj = context.object
        return obj and obj.components and obj.components_index >= 0

    def execute(self, context):
        obj = context.object
        idx = obj.components_index
        obj.components.remove(idx)
        # clamp index
        obj.components_index = min(max(0, idx-1), len(obj.components)-1)
        return {'FINISHED'}

classes = [
    ComponentItem,
    COMPONENTS_UL_items,
    OBJECT_OT_add_component,
    OBJECT_OT_remove_component,
    OBJECT_PT_components,
]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    # only add the prop if it doesn’t already exist
    if not hasattr(bpy.types.Object, "components"):
        bpy.types.Object.components = bpy.props.CollectionProperty(type=ComponentItem)
        bpy.types.Object.components_index = bpy.props.IntProperty()


def unregister():
    # remove the prop from all Objects
    if hasattr(bpy.types.Object, "components"):
        del bpy.types.Object.components
        del bpy.types.Object.components_index

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
