import bpy
from bpy.props import FloatVectorProperty

# Define default values for custom properties
custom_property_defaults = {
    "BaseAlbedo": (1.0, 1.0, 1.0, 1.0),
    "TopAlbedo": (1.0, 1.0, 1.0, 1.0),
    "SideAlbedo": (1.0, 1.0, 1.0, 1.0),
    "InnerOutlineColor": (0.01, 0.01, 0.01, 1.0),
    "OuterOutlineColor": (0.01, 0.01, 0.01, 1.0),
}

# Define custom properties
custom_properties = {
    "BaseAlbedo": FloatVectorProperty(
        name="Base Albedo",
        description="Base color for albedo",
        default=(1.0, 1.0, 1.0, 1.0),
        size=4,
        subtype='COLOR',
        min=0.0,
        max=1.0
    ),
    "TopAlbedo": FloatVectorProperty(
        name="Top Albedo",
        description="Top color for albedo",
        default=(1.0, 1.0, 1.0, 1.0),
        size=4,
        subtype='COLOR',
        min=0.0,
        max=1.0
    ),
    "SideAlbedo": FloatVectorProperty(
        name="Side Albedo",
        description="Side color for albedo",
        default=(1.0, 1.0, 1.0, 1.0),
        size=4,
        subtype='COLOR',
        min=0.0,
        max=1.0
    ),
    "InnerOutlineColor": FloatVectorProperty(
        name="Inner Outline Color",
        description="Inner outline color",
        default=(0.01, 0.01, 0.01, 1.0),
        size=4,
        subtype='COLOR',
        min=0.0,
        max=1.0
    ),
    "OuterOutlineColor": FloatVectorProperty(
        name="Outer Outline Color",
        description="Outer outline color",
        default=(0.01, 0.01, 0.01, 1.0),
        size=4,
        subtype='COLOR',
        min=0.0,
        max=1.0
    )
}

# Operator to add custom properties to selected objects
class OBJECT_OT_AddCustomProperties(bpy.types.Operator):
    bl_idname = "object.add_custom_unigma_properties"
    bl_label = "Add Unigma Properties"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        # Register properties dynamically to all selected objects
        for prop_name, prop_def in custom_properties.items():
            if not hasattr(bpy.types.Object, prop_name):
                setattr(bpy.types.Object, prop_name, prop_def)

        for obj in context.selected_objects:
            for prop_name, default_value in custom_property_defaults.items():
                if not obj.get(prop_name):
                    setattr(obj, prop_name, default_value)

        self.report({'INFO'}, "Unigma properties added to selected objects.")
        return {'FINISHED'}

def menu_func(self, context):
    self.layout.operator(OBJECT_OT_AddCustomProperties.bl_idname)

def register():
    bpy.utils.register_class(OBJECT_OT_AddCustomProperties)
    bpy.types.VIEW3D_MT_object.append(menu_func)

def unregister():
    bpy.utils.unregister_class(OBJECT_OT_AddCustomProperties)
    bpy.types.VIEW3D_MT_object.remove(menu_func)

if __name__ == "__main__":
    register()
