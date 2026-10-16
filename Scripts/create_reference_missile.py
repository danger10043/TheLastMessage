import unreal, math, os, json

ROOT = r'C:\UnrealProject\TheLastMessage'
SOURCE = os.path.join(ROOT, 'SourceAssets', 'ReferenceMissile')
DEST = '/Game/Main/Props/ReferenceMissile'
os.makedirs(SOURCE, exist_ok=True)
verts, faces = [], []
def face(indices, mat):
    faces.append((indices, mat))
def lathe(profile, mat, n=48):
    base = len(verts)
    for x, r in profile:
        for i in range(n):
            a = 2*math.pi*i/n
            verts.append((x, r*math.cos(a), r*math.sin(a)))
    for j in range(len(profile)-1):
        for i in range(n):
            a=base+j*n+i; b=base+j*n+(i+1)%n
            c=b+n; d=a+n
            face([a,b,c,d], mat)
    face([base+i for i in reversed(range(n))], mat)
    face([base+(len(profile)-1)*n+i for i in range(n)], mat)
def prism(points, angle, thickness, mat):
    base=len(verts)
    for t in [-thickness/2, thickness/2]:
        for x,r in points:
            verts.append((x, r*math.cos(angle)-t*math.sin(angle), r*math.sin(angle)+t*math.cos(angle)))
    n=len(points)
    face([base+i for i in reversed(range(n))],mat)
    face([base+n+i for i in range(n)],mat)
    for i in range(n):
        j=(i+1)%n
        face([base+i,base+j,base+n+j,base+n+i],mat)

# Nose points along +X. Segment boundaries give crisp, texture-free bands.
lathe([(-180,6.8),(-173,10),(-155,11.7),(-50,13),(65,14),(166,14),(174,12.8)],'Body')
lathe([(174,12.8),(178,11.7),(181,8.5),(182,4),(182.2,0.1)],'Nose')
lathe([(63,14.08),(68,14.08)],'Yellow')
lathe([(27,13.78),(32,13.82)],'Yellow')
lathe([(18,13.67),(26,13.77)],'Dark')
lathe([(-181,6.7),(-176,6.7)],'Dark')
for i in range(4):
    angle=math.pi/4+i*math.pi/2
    prism([(48,13.6),(23,42),(-15,42),(7,13.6)],angle,1.25,'Dark')
    prism([(-120,11.5),(-149,41),(-172,43),(-160,10)],angle,1.5,'Body')
# Two small mounting lugs along the top of the body.
for x in [-70,5]:
    prism([(x-5,12.8),(x-5,18),(x+5,18),(x+5,12.8)],math.pi/2,6,'Dark')
obj=os.path.join(SOURCE,'SM_ReferenceMissile.obj')
with open(obj,'w') as f:
    f.write('mtllib SM_ReferenceMissile.mtl\no SM_ReferenceMissile\n')
    for v in verts: f.write('v %f %f %f\n'%v)
    for x,y,z in verts:
        f.write('vt %f %f\n'%((x+183)/366,(math.atan2(z,y)+math.pi)/(2*math.pi)))
    previous=None
    for ids,mat in faces:
        if mat!=previous: f.write('usemtl '+mat+'\n');previous=mat
        # Triangles ensure consistent import across OBJ readers.
        for k in range(1,len(ids)-1):
            f.write('f '+' '.join('%d/%d'%(v+1,v+1) for v in [ids[0],ids[k],ids[k+1]])+'\n')
with open(os.path.join(SOURCE,'SM_ReferenceMissile.mtl'),'w') as f:
    for name,color in [('Body',(0.64,0.66,0.65)),('Nose',(0.012,0.015,0.017)),('Yellow',(0.9,0.57,0.025)),('Dark',(0.07,0.08,0.085))]:
        f.write('newmtl %s\nKd %f %f %f\n\n'%((name,)+color))

unreal.EditorAssetLibrary.make_directory(DEST)
task=unreal.AssetImportTask()
task.set_editor_property('filename',obj)
task.set_editor_property('destination_path',DEST)
task.set_editor_property('destination_name','SM_ReferenceMissile')
task.set_editor_property('automated',True)
task.set_editor_property('save',True)
task.set_editor_property('replace_existing',True)
options=unreal.FbxImportUI()
options.set_editor_property('import_mesh',True)
options.set_editor_property('import_materials',False)
options.set_editor_property('import_textures',False)
options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
options.static_mesh_import_data.set_editor_property('combine_meshes',True)
options.static_mesh_import_data.set_editor_property('import_uniform_scale',1.0)
options.static_mesh_import_data.set_editor_property('convert_scene',False)
task.set_editor_property('options',options)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh=unreal.load_asset(DEST+'/SM_ReferenceMissile')
if not isinstance(mesh,unreal.StaticMesh): raise RuntimeError('Static mesh import failed: '+str(task.imported_object_paths))
materials={}
for name,color in [('Body',(0.64,0.66,0.65)),('Nose',(0.012,0.015,0.017)),('Yellow',(0.9,0.57,0.025)),('Dark',(0.07,0.08,0.085))]:
    m=unreal.load_asset(DEST+'/M_Missile_'+name)
    if m is None:
        m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_Missile_'+name,DEST,unreal.Material,unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(m)
    expr=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
    expr.set_editor_property('constant',unreal.LinearColor(*color,1))
    unreal.MaterialEditingLibrary.connect_material_property(expr,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant)
    rough.set_editor_property('r',0.52)
    unreal.MaterialEditingLibrary.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    materials[name.lower()]=m
for i,slot in enumerate(mesh.get_editor_property('static_materials')):
    name=str(slot.get_editor_property('material_slot_name')).lower()
    match=next((m for key,m in materials.items() if key in name), materials['body'])
    mesh.set_material(i,match)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
bounds=mesh.get_bounds()
report={'asset':mesh.get_path_name(),'imported':list(task.imported_object_paths),'vertices_source':len(verts),'triangles_source':sum(len(f)-2 for f,m in faces),'bounds_extent':str(bounds.box_extent),'material_slots':[str(s.material_slot_name) for s in mesh.static_materials]}
with open(os.path.join(SOURCE,'build_report.json'),'w') as f: json.dump(report,f,indent=2)
unreal.log('MISSILE_BUILD_SUCCESS '+json.dumps(report))
