"""Build a regular dodecahedron and animated translucent/emissive materials."""
import unreal, math, itertools, os, json

ROOT = r'C:\UnrealProject\TheLastMessage'
SOURCE = os.path.join(ROOT, 'SourceAssets', 'Dodecahedron')
DEST = '/Game/_ExternalAssets/CustomMesh'
os.makedirs(SOURCE, exist_ok=True)
def add(a,b): return tuple(x+y for x,y in zip(a,b))
def sub(a,b): return tuple(x-y for x,y in zip(a,b))
def mul(a,s): return tuple(x*s for x in a)
def dot(a,b): return sum(x*y for x,y in zip(a,b))
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def norm(a): return mul(a,1/math.sqrt(dot(a,a)))
phi=(1+math.sqrt(5))/2
points=list(itertools.product([-1,1],repeat=3))
for a,b in itertools.product([-1,1],repeat=2):
    points.extend([(0,a/phi,b*phi),(a/phi,b*phi,0),(a*phi,0,b/phi)])
points=[mul(p,100/math.sqrt(3)) for p in points]
polygons={}
for ids in itertools.combinations(range(20),3):
    a,b,c=[points[i] for i in ids]
    n=cross(sub(b,a),sub(c,a))
    if dot(n,n)<1e-8: continue
    n=norm(n); d=dot(n,a)
    distances=[dot(n,p)-d for p in points]
    if min(distances)<-1e-5 and max(distances)>1e-5: continue
    on=tuple(i for i,v in enumerate(distances) if abs(v)<1e-5)
    if len(on)!=5: continue
    if d<0: n=mul(n,-1)
    center=mul(tuple(sum(points[i][j] for i in on) for j in range(3)),.2)
    u=norm(sub(points[on[0]],center)); v=cross(n,u)
    polygons[on]=sorted(on,key=lambda i:math.atan2(dot(sub(points[i],center),v),dot(sub(points[i],center),u)))
assert len(polygons)==12
edges=set()
for poly in polygons.values():
    for i in range(5): edges.add(tuple(sorted((poly[i],poly[(i+1)%5]))))
assert len(edges)==30
lengths=[math.dist(points[a],points[b]) for a,b in edges]
assert max(lengths)-min(lengths)<1e-6
vertices=[]; triangles=[]; uvs=[]
def tri(a,b,c,mat):
    base=len(vertices)
    vertices.extend([a,b,c]); uvs.extend([(0,0),(1,0),(.5,1)])
    triangles.append(([base,base+1,base+2],mat))
for poly in polygons.values():
    for i in range(1,4): tri(points[poly[0]],points[poly[i]],points[poly[i+1]],'Surface')
# Actual edge geometry avoids lighting the pentagons' triangulation diagonals.
for a,b in sorted(edges):
    start,end=points[a],points[b]; axis=norm(sub(end,start))
    u=norm(cross(axis,(0,0,1) if abs(axis[2])<.9 else (0,1,0))); v=cross(axis,u)
    rings=[]
    for p in (start,end):
        rings.append([add(p,add(mul(u,.65*math.cos(i*math.tau/8)),mul(v,.65*math.sin(i*math.tau/8)))) for i in range(8)])
    for i in range(8):
        j=(i+1)%8
        tri(rings[0][i],rings[0][j],rings[1][j],'Edges')
        tri(rings[0][i],rings[1][j],rings[1][i],'Edges')
        tri(start,rings[0][j],rings[0][i],'Edges')
        tri(end,rings[1][i],rings[1][j],'Edges')
obj=os.path.join(SOURCE,'SM_Dodecahedron.obj')
with open(obj,'w') as f:
    f.write('o SM_Dodecahedron\n')
    for p in vertices: f.write('v %f %f %f\n'%p)
    for uv in uvs: f.write('vt %f %f\n'%uv)
    last=None
    for ids,mat in triangles:
        if last!=mat: f.write('usemtl '+mat+'\n');last=mat
        f.write('f '+' '.join('%d/%d'%(i+1,i+1) for i in ids)+'\n')
unreal.EditorAssetLibrary.make_directory(DEST)
task=unreal.AssetImportTask()
for key,value in dict(filename=obj,destination_path=DEST,destination_name='SM_Dodecahedron',automated=True,save=True,replace_existing=True).items(): task.set_editor_property(key,value)
options=unreal.FbxImportUI()
for key,value in dict(import_mesh=True,import_materials=False,import_textures=False,mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH).items(): options.set_editor_property(key,value)
options.static_mesh_import_data.set_editor_property('combine_meshes',True)
options.static_mesh_import_data.set_editor_property('convert_scene',False)
task.set_editor_property('options',options)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh=unreal.load_asset(DEST+'/SM_Dodecahedron')
assert isinstance(mesh,unreal.StaticMesh)
lib=unreal.MaterialEditingLibrary
def material(name,edge=False):
    path=DEST+'/'+name
    m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('two_sided',True)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    if not edge: m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    def node(cls,x,y): return lib.create_material_expression(m,getattr(unreal,cls),x,y)
    def link(a,b,pin,output=''): assert lib.connect_material_expressions(a,output,b,pin)
    def scalar(name,value,x,y):
        e=node('MaterialExpressionScalarParameter',x,y);e.set_editor_property('parameter_name',name);e.set_editor_property('default_value',value);return e
    def color(name,value,x,y):
        e=node('MaterialExpressionVectorParameter',x,y);e.set_editor_property('parameter_name',name);e.set_editor_property('default_value',unreal.LinearColor(*value,1));return e
    time=node('MaterialExpressionTime',-900,0)
    period=scalar('CycleSeconds',4.,-900,160)
    divide=node('MaterialExpressionDivide',-680,0);link(time,divide,'A');link(period,divide,'B')
    sine=node('MaterialExpressionSine',-500,0);sine.set_editor_property('period',1.);link(divide,sine,'')
    half=node('MaterialExpressionMultiply',-340,0);half.set_editor_property('const_b',.5);link(sine,half,'A')
    offset=node('MaterialExpressionAdd',-180,0);offset.set_editor_property('const_b',.5);link(half,offset,'A')
    sky=color('SkyColor',(.12,.72,1.),-400,-300)
    blue=color('BlueColor',(.015,.09,1.),-400,-180)
    lerp=node('MaterialExpressionLinearInterpolate',30,-150);link(sky,lerp,'A');link(blue,lerp,'B');link(offset,lerp,'Alpha')
    strength=scalar('GlowStrength' if edge else 'SurfaceBrightness',12. if edge else .7,30,100)
    emission=node('MaterialExpressionMultiply',250,-100);link(lerp,emission,'A');link(strength,emission,'B')
    lib.connect_material_property(emission,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if not edge:
        opacity=scalar('Opacity',.4,250,150)
        lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(m)
    assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m
surface=material('M_Dodecahedron_Translucent')
edge=material('M_Dodecahedron_Edges',True)
for i,slot in enumerate(mesh.static_materials): mesh.set_material(i,edge if 'edge' in str(slot.material_slot_name).lower() else surface)
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
report=dict(asset=mesh.get_path_name(),regular_vertices=20,pentagonal_faces=12,edges=30,edge_length_cm=lengths[0],circumradius_cm=100,triangles=len(triangles),opacity=.4,cycle_seconds=4,edge_radius_cm=.65,materials=[str(s.material_interface.get_path_name()) for s in mesh.static_materials],bounds=str(mesh.get_bounds().box_extent))
with open(os.path.join(SOURCE,'build_report.json'),'w') as f: json.dump(report,f,indent=2)
unreal.log('DODECAHEDRON_BUILD_SUCCESS '+json.dumps(report))
