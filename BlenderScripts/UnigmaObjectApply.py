bl_info = {
    "name": "Unigma Object Properties",
    "author": "UKL",
    "blender": (4, 0, 0),
    "version": (0, 0, 1),
    "location": "View3D > Object Menu",
    "description": "Add Unigma custom properties (albedos, outlines) to objects",
    "category": "Object"
}

import bpy
from bpy.props import FloatVectorProperty

custom_property_defaults = {
    "Midtone": (1.0, 1.0, 1.0, 1.0),
    "Highlight": (1.0, 1.0, 1.0, 1.0),
    "Shadow": (1.0, 1.0, 1.0, 1.0),
    "InnerOutlineColor": (0.01, 0.01, 0.01, 1.0),
    "OuterOutlineColor": (0.01, 0.01, 0.01, 1.0),
}

custom_properties = {
    "Midtone": FloatVectorProperty(name="Midtone", default=(1.0, 1.0, 1.0, 1.0), size=4, subtype='COLOR', min=0.0, max=1.0),
    "Highlight": FloatVectorProperty(name="Highlight", default=(1.0, 1.0, 1.0, 1.0), size=4, subtype='COLOR', min=0.0, max=1.0),
    "Shadow": FloatVectorProperty(name="Shadow", default=(1.0, 1.0, 1.0, 1.0), size=4, subtype='COLOR', min=0.0, max=1.0),
    "InnerOutlineColor": FloatVectorProperty(name="Inner Outline Color", default=(0.01, 0.01, 0.01, 1.0), size=4, subtype='COLOR', min=0.0, max=1.0),
    "OuterOutlineColor": FloatVectorProperty(name="Outer Outline Color", default=(0.01, 0.01, 0.01, 1.0), size=4, subtype='COLOR', min=0.0, max=1.0)
}

class OBJECT_OT_AddCustomProperties(bpy.types.Operator):
    bl_idname = "object.add_custom_unigma_properties"
    bl_label = "Add Unigma Properties"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        for prop_name, prop_def in custom_properties.items():
            if not hasattr(bpy.types.Object, prop_name):
                setattr(bpy.types.Object, prop_name, prop_def)
        for obj in context.selected_objects:
            for prop_name, default_value in custom_property_defaults.items():
                if not obj.get(prop_name):
                    setattr(obj, prop_name, default_value)
        self.report({'INFO'}, "Unigma properties added to selected objects.")
        return {'FINISHED'}

class OBJECT_PT_unigma_panel(bpy.types.Panel):
    bl_label = "Unigma Properties"
    bl_idname = "OBJECT_PT_unigma_panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Object"

    def draw(self, context):
        layout = self.layout
        obj = context.object
        if obj:
            layout.prop(obj, "Midtone")
            layout.prop(obj, "Highlight")
            layout.prop(obj, "Shadow")
            layout.prop(obj, "InnerOutlineColor")
            layout.prop(obj, "OuterOutlineColor")

def menu_func(self, context):
    self.layout.operator(OBJECT_OT_AddCustomProperties.bl_idname)

def register():
    bpy.utils.register_class(OBJECT_OT_AddCustomProperties)
    bpy.utils.register_class(OBJECT_PT_unigma_panel)
    bpy.types.VIEW3D_MT_object.append(menu_func)

def unregister():
    bpy.utils.unregister_class(OBJECT_OT_AddCustomProperties)
    bpy.utils.unregister_class(OBJECT_PT_unigma_panel)
    bpy.types.VIEW3D_MT_object.remove(menu_func)

if __name__ == "__main__":
    register()
