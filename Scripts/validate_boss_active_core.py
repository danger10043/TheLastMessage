import unreal, json, os, math
ROOT=r'C:\UnrealProject\TheLastMessage'
DEST='/Game/_ExternalAssets/CustomMesh/BossActiveCore'
mesh=unreal.load_asset(DEST+'/SK_BossActiveCore')
options=unreal.AnimPoseEvaluationOptions()
options.set_editor_property('optional_skeletal_mesh',mesh)
results={}
for clip in ['Spawn','Destroy','Retract']:
    anim=unreal.load_asset(DEST+'/AN_BossActiveCore_'+clip)
    samples=[]
    for t in [0.,anim.get_play_length()/2,anim.get_play_length()]:
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(anim,t,options)
        assert unreal.AnimPoseExtensions.is_valid(pose)
        names=unreal.AnimPoseExtensions.get_bone_names(pose)
        assert len(names)==25
        sample=dict(time=t)
        for name in ['root','platform_lift','core_float','crystal_shard_01','claw_1_upper','energy_column']:
            transform=unreal.AnimPoseExtensions.get_bone_pose(pose,name,unreal.AnimPoseSpaces.WORLD)
            sample[name]=dict(translation=[transform.translation.x,transform.translation.y,transform.translation.z],scale=[transform.scale3d.x,transform.scale3d.y,transform.scale3d.z],rotation=[transform.rotation.x,transform.rotation.y,transform.rotation.z,transform.rotation.w])
        assert max(abs(v) for v in sample['root']['translation'])<.01
        samples.append(sample)
    results[clip]=samples
assert results['Spawn'][0]['platform_lift']['translation'][2]<-250
assert abs(results['Spawn'][-1]['platform_lift']['translation'][2])<.1
assert results['Retract'][-1]['platform_lift']['translation'][2]<-250
assert max(results['Destroy'][-1]['crystal_shard_01']['scale'])<.01
assert max(results['Destroy'][-1]['energy_column']['scale'])<.01
assert abs(results['Destroy'][-1]['claw_1_upper']['rotation'][3]-results['Destroy'][0]['claw_1_upper']['rotation'][3])>.01
with open(os.path.join(ROOT,'SourceAssets','BossActiveCore','animation_validation.json'),'w') as f:json.dump(results,f,indent=2)
unreal.log('BOSS_CORE_ANIMATION_VALIDATION_SUCCESS')
