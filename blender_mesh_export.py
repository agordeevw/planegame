import bpy
from ctypes import *


def writeMeshResource(context, filepath, use_some_setting):
    print("running writeMeshResource...")
    f = open(filepath, 'wb')
    object = bpy.context.selected_objects[0]
    depsgraph = bpy.context.evaluated_depsgraph_get()
    object = object.evaluated_get(depsgraph)
    mesh = object.to_mesh()
    
    # engine forward is -Z
    # engine up is Y
    # engine right is X
    
    # model up is Z
    # model forward is Y
    # model right is X
    
    # write vertices
    vertices = mesh.vertices
    vertexCount = len(vertices)
    f.write(c_uint32(vertexCount))
    for i in range(0, vertexCount):
        f.write(c_float(vertices[i].co[0]))
        f.write(c_float(vertices[i].co[2]))
        f.write(c_float(-vertices[i].co[1]))
        f.write(c_float(vertices[i].normal[0]))
        f.write(c_float(vertices[i].normal[2]))
        f.write(c_float(-vertices[i].normal[1]))
    
    # collect submeshes
    submeshes = {}    
    mesh.calc_loop_triangles()
    for tri in mesh.loop_triangles:
        if tri.material_index not in submeshes:
            submeshes[tri.material_index] = []
        submeshes[tri.material_index].append(tri.vertices[0])
        submeshes[tri.material_index].append(tri.vertices[1])
        submeshes[tri.material_index].append(tri.vertices[2])

    # write indices
    indexCount = 3 * len(mesh.loop_triangles)
    f.write(c_uint32(indexCount))
    for submeshIdx in sorted(submeshes):
        for value in submeshes[submeshIdx]:
            f.write(c_uint32(value))
    
    # write submesh descriptions
    f.write(c_uint32(len(submeshes)))
    submeshIndexStart = 0
    for submeshIdx in sorted(submeshes):
        submeshIndexCount = len(submeshes[submeshIdx])
        f.write(c_uint32(submeshIndexStart))
        f.write(c_uint32(len(submeshes[submeshIdx])))
        submeshIndexStart += submeshIndexCount
        
    f.close()
    
    object.to_mesh_clear()

    return {'FINISHED'}


# ExportHelper is a helper class, defines filename and
# invoke() function which calls the file selector.
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty, EnumProperty
from bpy.types import Operator


class ExportMesh(Operator, ExportHelper):
    """This appears in the tooltip of the operator and in the generated docs"""
    bl_idname = "export_test.some_data"  # important since its how bpy.ops.import_test.some_data is constructed
    bl_label = "Export Model"

    # ExportHelper mixin class uses this
    filename_ext = ".meshresource"

    filter_glob: StringProperty(
        default="*.meshresource",
        options={'HIDDEN'},
        maxlen=255,  # Max internal buffer length, longer would be clamped.
    )

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.
    use_setting: BoolProperty(
        name="Example Boolean",
        description="Example Tooltip",
        default=True,
    )

    type: EnumProperty(
        name="Example Enum",
        description="Choose between two items",
        items=(
            ('OPT_A', "First Option", "Description one"),
            ('OPT_B', "Second Option", "Description two"),
        ),
        default='OPT_A',
    )

    def execute(self, context):
        return writeMeshResource(context, self.filepath, self.use_setting)


# Only needed if you want to add into a dynamic menu
def menu_func_export(self, context):
    self.layout.operator(ExportSomeData.bl_idname, text="Text Export Operator")


# Register and add to the "file selector" menu (required to use F3 search "Text Export Operator" for quick access).
def register():
    bpy.utils.register_class(ExportMesh)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_class(ExportMesh)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)


if __name__ == "__main__":
    register()

    # test call
    bpy.ops.export_test.some_data('INVOKE_DEFAULT')
