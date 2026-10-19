"""Run inside Unreal's Python editor or PythonScript commandlet.

Imports only the newly generated gravity-plate art folder. Does not alter levels,
existing Blueprints, game code, project settings, or trigger behavior.
"""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets' / 'GravityFlipPlate'
if not (SOURCE / 'SM_GravityFlipPlate.fbx').exists():
    SOURCE = Path(__file__).resolve().parent
if not (SOURCE / 'SM_GravityFlipPlate.fbx').exists():
    raise RuntimeError('Keep this script beside the FBX, or run it from the project Scripts folder.')
DEST = '/Game/Test_Jiho/Gimmick/Plate/GravityFlipPlate_Art'
tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
library.make_directory(DEST)


def import_file(path, name, options=None, factory=None):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(path))
    task.set_editor_property('destination_path', DEST)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    if options is not None:
        task.set_editor_property('options', options)
    if factory is not None:
        task.set_editor_property('factory', factory)
    tools.import_asset_tasks([task])
    obj = unreal.load_asset(DEST + '/' + name)
    if obj is None:
        raise RuntimeError('Import failed: %s (%s)' % (path, task.imported_object_paths))
    return obj


textures = {}
for label in ['BaseColor', 'ORM', 'Normal', 'EmissiveMask']:
    name = 'T_GravityFlipPlate_' + label
    texture = import_file(SOURCE / 'Textures' / (name + '.png'), name)
    texture.set_editor_property('srgb', label == 'BaseColor')
    if label == 'Normal':
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property('flip_green_channel', True)
    elif label != 'BaseColor':
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
    library.save_loaded_asset(texture)
    textures[label] = texture

options = unreal.FbxImportUI()
options.set_editor_property('import_mesh', True)
options.set_editor_property('import_as_skeletal', False)
options.set_editor_property('import_materials', False)
options.set_editor_property('import_textures', False)
options.set_editor_property('automated_import_should_detect_type', False)
options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
data = options.static_mesh_import_data
data.set_editor_property('combine_meshes', True)
data.set_editor_property('import_uniform_scale', 1.0)
data.set_editor_property('generate_lightmap_u_vs', False)
data.set_editor_property('auto_generate_collision', False)
data.set_editor_property('one_convex_hull_per_ucx', True)
data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
data.set_editor_property('build_nanite', True)
mesh = import_file(SOURCE / 'SM_GravityFlipPlate.fbx', 'SM_GravityFlipPlate', options, unreal.FbxFactory())
if not isinstance(mesh, unreal.StaticMesh):
    raise RuntimeError('Expected one StaticMesh; received ' + str(mesh))

mat = unreal.load_asset(DEST + '/M_GravityFlipPlate')
if mat is None:
    mat = tools.create_asset('M_GravityFlipPlate', DEST, unreal.Material, unreal.MaterialFactoryNew())
edit = unreal.MaterialEditingLibrary
edit.delete_all_material_expressions(mat)


def node(cls, x, y):
    return edit.create_material_expression(mat, cls, x, y)


for label, y in [('BaseColor', -420), ('ORM', -100), ('Normal', 240), ('EmissiveMask', 560)]:
    sample = node(unreal.MaterialExpressionTextureSample, -720, y)
    sample.set_editor_property('texture', textures[label])
    if label == 'Normal':
        sample.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    elif label != 'BaseColor':
        sample.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    if label == 'BaseColor':
        edit.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    elif label == 'ORM':
        for channel, prop in [('R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION), ('G', unreal.MaterialProperty.MP_ROUGHNESS), ('B', unreal.MaterialProperty.MP_METALLIC)]:
            edit.connect_material_property(sample, channel, prop)
    elif label == 'Normal':
        edit.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    else:
        tint = node(unreal.MaterialExpressionVectorParameter, -720, 850)
        tint.set_editor_property('parameter_name', 'GlowColor')
        tint.set_editor_property('default_value', unreal.LinearColor(.003, .20, 1.0, 1.0))
        strength = node(unreal.MaterialExpressionScalarParameter, -420, 920)
        strength.set_editor_property('parameter_name', 'EmissiveStrength')
        strength.set_editor_property('default_value', 3.0)
        tint_mult = node(unreal.MaterialExpressionMultiply, -350, 550)
        edit.connect_material_expressions(sample, 'R', tint_mult, 'A')
        edit.connect_material_expressions(tint, '', tint_mult, 'B')
        strength_mult = node(unreal.MaterialExpressionMultiply, -90, 550)
        edit.connect_material_expressions(tint_mult, '', strength_mult, 'A')
        edit.connect_material_expressions(strength, '', strength_mult, 'B')
        edit.connect_material_property(strength_mult, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.recompile_material(mat)
library.save_loaded_asset(mat)
instance = unreal.load_asset(DEST + '/MI_GravityFlipPlate')
if instance is None:
    instance = tools.create_asset('MI_GravityFlipPlate', DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
edit.set_material_instance_parent(instance, mat)
edit.set_material_instance_scalar_parameter_value(instance, 'EmissiveStrength', 3.0)
edit.set_material_instance_vector_parameter_value(instance, 'GlowColor', unreal.LinearColor(.003, .20, 1, 1))
library.save_loaded_asset(instance)
mesh.set_material(0, instance)
mesh.set_editor_property('light_map_coordinate_index', 1)
mesh.set_editor_property('light_map_resolution', 128)
library.save_loaded_asset(mesh)

subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
collision_count = subsystem.get_simple_collision_count(mesh)
if collision_count < 1:
    raise RuntimeError('Expected imported UCX collision hull')
bounds = mesh.get_bounds()
dimensions = [bounds.box_extent.x * 2, bounds.box_extent.y * 2, bounds.box_extent.z * 2]
if not (230 < dimensions[0] < 250 and 190 < dimensions[1] < 215 and 30 < dimensions[2] < 38):
    raise RuntimeError('Unexpected Unreal dimensions: ' + str(dimensions))
report = {'asset': mesh.get_path_name(), 'material': instance.get_path_name(), 'dimensions_cm': dimensions, 'simple_collision_count': collision_count, 'uv_channels': subsystem.get_num_uv_channels(mesh, 0), 'material_slots': len(mesh.static_materials), 'nanite_enabled': mesh.get_editor_property('nanite_settings').enabled}
if report['uv_channels'] != 2:
    raise RuntimeError('Expected atlas UV0 and lightmap UV1: ' + str(report))
(SOURCE / 'unreal_import_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('GRAVITY_PLATE_IMPORT_SUCCESS ' + json.dumps(report))
