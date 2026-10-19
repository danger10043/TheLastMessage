"""Render actual Unreal skeletal poses in an isolated preview level."""
import unreal, os, time, traceback
ROOT=r'C:\UnrealProject\TheLastMessage'
DEST='/Game/_ExternalAssets/CustomMesh/BossActiveCore'
OUT=os.path.join(ROOT,'SourceAssets','BossActiveCore','UnrealPreview')
os.makedirs(OUT,exist_ok=True)
exec(compile(open(os.path.join(ROOT,'Scripts','validate_boss_active_core.py')).read(),'validate_boss_active_core.py','exec'))
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
map_path=DEST+'/L_BossActiveCore_Preview'
if unreal.EditorAssetLibrary.does_asset_exist(map_path):
    assert level.load_level(map_path)
else:
    assert level.new_level(map_path)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for old in actors.get_all_level_actors():
    if isinstance(old,(unreal.SkeletalMeshActor,unreal.StaticMeshActor,unreal.Light,unreal.CameraActor,unreal.SceneCapture2D)):
        actors.destroy_actor(old)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
mesh=unreal.load_asset(DEST+'/SK_BossActiveCore')
actor=actors.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(0,0,0))
actor.set_actor_label('Boss Active Core - Animation Preview')
comp=actor.get_component_by_class(unreal.SkeletalMeshComponent)
comp.set_skeletal_mesh_asset(mesh)
comp.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
comp.set_update_animation_in_editor(True)
comp.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
anim=unreal.load_asset(DEST+'/AN_BossActiveCore_Spawn')
comp.set_animation(anim)
comp.set_position(anim.get_play_length(),False)
comp.override_animation_data(anim,False,False,anim.get_play_length(),1.)
floor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-5))
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
floor.set_actor_scale3d(unreal.Vector(10,10,.1))
floor.set_actor_label('Preview Floor')
mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_BossCore_PreviewFloor',DEST,unreal.Material,unreal.MaterialFactoryNew()) if not unreal.EditorAssetLibrary.does_asset_exist(DEST+'/M_BossCore_PreviewFloor') else unreal.load_asset(DEST+'/M_BossCore_PreviewFloor')
unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
c=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector)
c.set_editor_property('constant',unreal.LinearColor(.027,.035,.048,1))
unreal.MaterialEditingLibrary.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)
floor.static_mesh_component.set_material(0,mat)
sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,400),unreal.Rotator(-42,-35,0))
sun.light_component.set_editor_property('intensity',3.)
sun.light_component.set_editor_property('light_color',unreal.Color(r=210,g=225,b=255,a=255))
fill=actors.spawn_actor_from_class(unreal.RectLight,unreal.Vector(-220,280,230))
fill.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(fill.get_actor_location(),unreal.Vector(0,0,110)),False)
fill.light_component.set_editor_property('intensity',300.)
fill.light_component.set_editor_property('source_width',200.)
fill.light_component.set_editor_property('source_height',200.)
fill.light_component.set_editor_property('light_color',unreal.Color(r=140,g=190,b=255,a=255))
rim=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(100,-140,200))
rim.light_component.set_editor_property('intensity',50.)
rim.light_component.set_editor_property('light_color',unreal.Color(r=80,g=160,b=255,a=255))
camera_location=unreal.Vector(50,-560,290)
look=unreal.MathLibrary.find_look_at_rotation(camera_location,unreal.Vector(0,0,112))
camera=actors.spawn_actor_from_class(unreal.CameraActor,camera_location,look)
camera.camera_component.set_editor_property('field_of_view',40.)
capture=actors.spawn_actor_from_class(unreal.SceneCapture2D,camera_location,look)
sc=capture.get_component_by_class(unreal.SceneCaptureComponent2D)
sc.set_editor_property('fov_angle',40.)
sc.set_editor_property('capture_every_frame',False)
sc.set_editor_property('capture_on_movement',False)
sc.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
rt=unreal.RenderingLibrary.create_render_target2d(world,1000,1000,unreal.TextureRenderTargetFormat.RTF_RGBA8)
sc.set_editor_property('texture_target',rt)
settings=sc.get_editor_property('post_process_settings')
for key,val in dict(override_auto_exposure_method=True,auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,override_auto_exposure_bias=True,auto_exposure_bias=0.,override_bloom_intensity=True,bloom_intensity=.5).items():settings.set_editor_property(key,val)
sc.set_editor_property('post_process_settings',settings)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(camera_location,look)
assert level.save_current_level()
states=[('Spawn',2.2,'Active'),('Spawn',.65,'Spawn_Mid'),('Destroy',.3,'Destroy_Fracture'),('Destroy',.9,'Destroy_Mid'),('Destroy',2.4,'Destroy_End'),('Retract',.6,'Retract_Fold'),('Retract',1.5,'Retract_Mid'),('Retract',2.4,'Retract_End')]
start=time.monotonic();index=0;phase=0;next_time=start+5
def tick(dt):
    global index,phase,next_time
    try:
        now=time.monotonic()
        if now<next_time:return
        if index>=len(states):
            unreal.unregister_slate_post_tick_callback(handle)
            comp.set_animation(anim)
            comp.set_position(anim.get_play_length(),False)
            comp.override_animation_data(anim,False,False,anim.get_play_length(),1.)
            level.save_current_level()
            unreal.log('BOSS_CORE_RENDER_SUCCESS')
            unreal.SystemLibrary.quit_editor();return
        clip,t,name=states[index]
        if phase==0:
            a=unreal.load_asset(DEST+'/AN_BossActiveCore_'+clip)
            comp.set_animation(a)
            comp.set_position(t,False)
            comp.override_animation_data(a,False,False,t,1.)
            phase=1;next_time=now+1.5
        elif phase==1:
            sc.capture_scene();phase=2;next_time=now+.7
        else:
            unreal.RenderingLibrary.export_render_target(world,rt,OUT,name+'.png')
            unreal.log('CORE_CAPTURED '+name)
            index+=1;phase=0;next_time=now+.1
    except Exception:
        unreal.log_error(traceback.format_exc());unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
unreal.log('BOSS_CORE_RENDER_TICK_READY')
