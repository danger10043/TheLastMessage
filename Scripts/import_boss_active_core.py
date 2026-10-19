import unreal, os, json
ROOT=r'C:\UnrealProject\TheLastMessage'
SOURCE=os.path.join(ROOT,'SourceAssets','BossActiveCore')
DEST='/Game/_ExternalAssets/CustomMesh/BossActiveCore'
unreal.EditorAssetLibrary.make_directory(DEST)

pipeline=unreal.InterchangeGenericAssetsPipeline()
pipeline.set_editor_property('use_source_name_for_asset',False)
pipeline.mesh_pipeline.set_editor_property('import_static_meshes',False)
pipeline.mesh_pipeline.set_editor_property('import_skeletal_meshes',True)
pipeline.mesh_pipeline.set_editor_property('create_physics_asset',False)
pipeline.material_pipeline.set_editor_property('import_materials',False)
pipeline.animation_pipeline.set_editor_property('import_animations',True)
pipeline.animation_pipeline.set_editor_property('use30_hz_to_bake_bone_animation',True)
stack=unreal.InterchangePipelineStackOverride()
stack.add_pipeline(pipeline)
task=unreal.AssetImportTask()
for key,value in dict(filename=os.path.join(SOURCE,'SK_BossActiveCore.glb'),destination_path=DEST,automated=True,save=True,replace_existing=True,options=stack).items():task.set_editor_property(key,value)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
assets=[unreal.load_asset(p) for p in unreal.EditorAssetLibrary.list_assets(DEST,recursive=True,include_folder=False)]
unreal.log('CORE_IMPORTED_ASSETS '+str([(a.get_name(),a.get_class().get_name()) for a in assets]))
meshes=[a for a in assets if isinstance(a,unreal.SkeletalMesh)]
assert len(meshes)==1,'Expected one skeletal mesh'
mesh=meshes[0]
def rename(asset,name):
    if asset.get_name()!=name:
        assert unreal.EditorAssetLibrary.rename_loaded_asset(asset,DEST+'/'+name)
    return asset
rename(mesh,'SK_BossActiveCore')
skeleton=mesh.get_editor_property('skeleton')
assert skeleton
unreal.EditorAssetLibrary.save_loaded_asset(skeleton,False)

lib=unreal.MaterialEditingLibrary
def material(name,color,metallic=0.,roughness=.4,glow=0.,opacity=None,animated=False):
    path=DEST+'/M_BossCore_'+name
    m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_BossCore_'+name,DEST,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('used_with_skeletal_mesh',True)
    if opacity is not None:
        m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
        m.set_editor_property('two_sided',False)
    if glow>0:m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    def node(cls,x,y):return lib.create_material_expression(m,getattr(unreal,cls),x,y)
    def link(a,b,pin):assert lib.connect_material_expressions(a,'',b,pin)
    def scalar(name,value,x,y):
        e=node('MaterialExpressionScalarParameter',x,y);e.set_editor_property('parameter_name',name);e.set_editor_property('default_value',value);return e
    def col(name,color,x,y):
        e=node('MaterialExpressionVectorParameter',x,y);e.set_editor_property('parameter_name',name);e.set_editor_property('default_value',unreal.LinearColor(*color,1));return e
    c=col('Tint',color,-450,-200)
    if animated:
        blue=col('BlueTint',(.015,.12,1.),-450,-60)
        time=node('MaterialExpressionTime',-1100,150)
        period=scalar('CycleSeconds',4.,-1100,260)
        div=node('MaterialExpressionDivide',-920,150);link(time,div,'A');link(period,div,'B')
        sine=node('MaterialExpressionSine',-760,150);sine.set_editor_property('period',1.);link(div,sine,'')
        half=node('MaterialExpressionMultiply',-600,150);half.set_editor_property('const_b',.5);link(sine,half,'A')
        shift=node('MaterialExpressionAdd',-440,150);shift.set_editor_property('const_b',.5);link(half,shift,'A')
        lerp=node('MaterialExpressionLinearInterpolate',-200,-150);link(c,lerp,'A');link(blue,lerp,'B');link(shift,lerp,'Alpha');c=lerp
    if glow>0:
        strength=scalar('GlowStrength',glow,-100,100)
        mult=node('MaterialExpressionMultiply',120,-150);link(c,mult,'A');link(strength,mult,'B')
        lib.connect_material_property(mult,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        lib.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
        for prop,value in [(unreal.MaterialProperty.MP_METALLIC,metallic),(unreal.MaterialProperty.MP_ROUGHNESS,roughness)]:
            s=scalar(str(prop).split('.')[-1],value,-100,100 if prop==unreal.MaterialProperty.MP_METALLIC else 250)
            lib.connect_material_property(s,'',prop)
    if opacity is not None:
        alpha=scalar('Opacity',opacity,100,300);lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m
mats=dict(
    Armor=material('Armor',(.13,.16,.20),.72,.38),
    Dark=material('Dark',(.023,.032,.045),.65,.48),
    Steel=material('Steel',(.36,.43,.51),.85,.27),
    Cyan=material('Cyan',(.07,.66,1.),glow=6.,animated=True),
    Amber=material('Amber',(1.,.27,.018),glow=3.),
    Crystal=material('Crystal',(.055,.47,1.),glow=.7,opacity=.20,animated=True),
    White=material('Nucleus',(.45,.85,1.),glow=15.))
slots=[]
for i,slot in enumerate(mesh.get_editor_property('materials')):
    name=str(slot.material_slot_name)
    key=next((k for k in mats if k.lower()==name.lower()),None)
    assert key is not None,'Unmapped material slot '+name
    slot.set_editor_property('material_interface',mats[key]);slots.append(slot)
mesh.set_editor_property('materials',slots)
mesh.set_editor_property('positive_bounds_extension',unreal.Vector(160,160,80))
mesh.set_editor_property('negative_bounds_extension',unreal.Vector(160,160,300))
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
clips={}
for a in assets:
    if not isinstance(a,unreal.AnimSequence):continue
    suffix=next((s for s in ['Spawn','Destroy','Retract'] if s.lower() in a.get_name().lower()),None)
    assert suffix,'Unknown clip '+a.get_name()
    rename(a,'AN_BossActiveCore_'+suffix)
    a.set_editor_property('enable_root_motion',False)
    assert a.get_editor_property('skeleton')==skeleton
    assert unreal.EditorAssetLibrary.save_loaded_asset(a,False)
    clips[suffix]=dict(asset=a.get_path_name(),seconds=a.get_play_length())
assert set(clips)=={'Spawn','Destroy','Retract'},str(clips)
for name,duration in [('Spawn',2.2),('Destroy',2.4),('Retract',2.4)]:assert abs(clips[name]['seconds']-duration)<.04
report=dict(mesh=mesh.get_path_name(),skeleton=skeleton.get_path_name(),clips=clips,materials={k:m.get_path_name() for k,m in mats.items()},imported=list(task.imported_object_paths))
with open(os.path.join(SOURCE,'unreal_report.json'),'w') as f:json.dump(report,f,indent=2)
unreal.log('BOSS_CORE_BUILD_SUCCESS '+json.dumps(report))
