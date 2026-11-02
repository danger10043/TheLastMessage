"""Bake a box-parallel pulling torso and lock the pushing clip's root in place."""
import unreal, math, json, shutil
from pathlib import Path
EX=unreal.AnimPoseExtensions
LOCAL=unreal.AnimPoseSpaces.LOCAL
WORLD=unreal.AnimPoseSpaces.WORLD
OPT=unreal.AnimPoseEvaluationOptions()
ROOT='/Game/Test_Jihong/Animations/PushPull/'
backup=Path(unreal.Paths.project_saved_dir())/'PushPullPoseBackup'
backup.mkdir(exist_ok=True)
for name in ['AM_Pull_BoxSides','A_Push_Heavy_01_Loop1']:
    src=Path(unreal.Paths.project_content_dir())/'Test_Jihong/Animations/PushPull'/f'{name}.uasset'
    dst=backup/src.name
    if not dst.exists():shutil.copy2(src,dst)
def v(t):return (t.x,t.y,t.z)
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def mul(a,b):return tuple(x*b for x in a)
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def length(a):return math.sqrt(dot(a,a))
def norm(a):return mul(a,1/max(length(a),1e-9))
def cross(a,b):return(a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def qt(q):return(q.x,q.y,q.z,q.w)
def qmul(a,b):
    x,y,z,w=a;X,Y,Z,W=b
    return(w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z)
def qbetween(a,b):
    a,b=norm(a),norm(b); d=dot(a,b)
    return norm((*cross(a,b),1+d))
def tr(p,b,s=LOCAL):return EX.get_bone_pose(p,b,s)
def pos(p,b):return v(tr(p,b,WORLD).translation)
def setrot(p,b,q):
    t=tr(p,b,WORLD);t.rotation=unreal.Quat(*q);p.set_bone_pose(t,b,WORLD)
def solve(p,side,target,hand_q):
    upper,lower,hand='upperarm_'+side,'lowerarm_'+side,'hand_'+side
    a,b,c=pos(p,upper),pos(p,lower),pos(p,hand)
    l1,l2=length(sub(b,a)),length(sub(c,b)); direction=norm(sub(target,a))
    dist=min(length(sub(target,a)),(l1+l2)*0.9999)
    target=add(a,mul(direction,dist))
    pole=sub(b,a); plane=sub(pole,mul(direction,dot(pole,direction)))
    if length(plane)<1e-6:plane=cross(direction,(0,0,1))
    along=(l1*l1-l2*l2+dist*dist)/(2*max(dist,1e-6))
    elbow=add(add(a,mul(direction,along)),mul(norm(plane),math.sqrt(max(0,l1*l1-along*along))))
    setrot(p,upper,qmul(qbetween(sub(b,a),sub(elbow,a)),qt(tr(p,upper,WORLD).rotation)))
    b,c=pos(p,lower),pos(p,hand)
    setrot(p,lower,qmul(qbetween(sub(c,b),sub(target,b)),qt(tr(p,lower,WORLD).rotation)))
    setrot(p,hand,hand_q)
source=unreal.load_asset(ROOT+'AS_Pull_BoxSides')
path=ROOT+'AS_Pull_BoxParallel'
asset=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(),path)
rate=unreal.FrameRate(30,1)
fps=rate.numerator/rate.denominator
count=round(source.get_play_length()*fps)+1
poses=[];angles=[];errors=[]
neutral=EX.get_anim_pose_at_time(unreal.load_asset('/Game/_ExternalAssets/Robbie_01/animation/Idle_battle'),0.0,OPT)
for i in range(count):
    p=EX.get_anim_pose_at_time(source,i/fps,OPT)
    targets={s:(pos(p,'hand_'+s),qt(tr(p,'hand_'+s,WORLD).rotation)) for s in ['l','r']}
    across=sub(pos(p,'clavicle_l'),pos(p,'clavicle_r'))
    angle=-math.atan2(across[1],across[0])
    setrot(p,'spine_01',qmul((0,0,math.sin(angle/2),math.cos(angle/2)),qt(tr(p,'spine_01',WORLD).rotation)))
    for s,(target,q) in targets.items():
        solve(p,s,target,q)
        errors.append(length(sub(pos(p,'hand_'+s),target)))
        wrist=tr(p,'hand_'+s)
        wrist.rotation=tr(neutral,'hand_'+s).rotation
        p.set_bone_pose(wrist,'hand_'+s,LOCAL)
    across=sub(pos(p,'clavicle_l'),pos(p,'clavicle_r'))
    angles.append(math.degrees(math.atan2(across[1],across[0])))
    poses.append(p)
assert max(abs(x) for x in angles)<0.1,angles
assert max(errors)<1.0,max(errors)
controller=asset.controller
controller.open_bracket('Align pulling torso parallel to box',False)
controller.set_frame_rate(rate,False)
controller.set_number_of_frames(unreal.FrameNumber(count-1),False)
tracks=set(str(x) for x in unreal.AnimationLibrary.get_animation_track_names(asset))
for bone in EX.get_bone_names(poses[0]):
    transforms=[tr(p,bone) for p in poses]
    assert controller.set_bone_track_keys(bone,[t.translation for t in transforms],[t.rotation for t in transforms],[t.scale3d for t in transforms],False)
controller.close_bracket(False)
asset.set_editor_property('enable_root_motion',False)
asset.set_editor_property('force_root_lock',True)
unreal.EditorAssetLibrary.save_loaded_asset(asset,False)
montage=unreal.load_asset(ROOT+'AM_Pull_BoxSides')
tracks=montage.get_editor_property('slot_anim_tracks')
for index,track in enumerate(tracks):
    animtrack=track.get_editor_property('anim_track');segments=animtrack.get_editor_property('anim_segments')
    for j,segment in enumerate(segments):
        segment.set_editor_property('anim_reference',asset);segments[j]=segment
    animtrack.set_editor_property('anim_segments',segments);track.set_editor_property('anim_track',animtrack);tracks[index]=track
montage.set_editor_property('slot_anim_tracks',tracks)
unreal.EditorAssetLibrary.save_loaded_asset(montage,False)
push=unreal.load_asset(ROOT+'A_Push_Heavy_01_Loop1')
push.set_editor_property('enable_root_motion',False)
push.set_editor_property('force_root_lock',True)
unreal.EditorAssetLibrary.save_loaded_asset(push,False)
# Verify compressed output, rather than only the poses before saving.
max_yaw=0;max_hand_error=0
for i in range(count):
    p=EX.get_anim_pose_at_time(asset,i/fps,OPT)
    across=sub(pos(p,'clavicle_l'),pos(p,'clavicle_r'))
    max_yaw=max(max_yaw,abs(math.degrees(math.atan2(across[1],across[0]))))
    for side in ['l','r']:max_hand_error=max(max_hand_error,length(sub(pos(p,'hand_'+side),pos(poses[i],'hand_'+side))))
assert max_yaw<0.5,(max_yaw,max_hand_error)
assert max_hand_error<0.5,max_hand_error
report={'sequence':asset.get_path_name(),'frames':count,'max_shoulder_yaw_degrees':max_yaw,'max_hand_bake_error_cm':max_hand_error,'max_hand_target_error_cm':max(errors),'push_root_locked':push.get_editor_property('force_root_lock')}
Path(unreal.Paths.project_saved_dir()+'PushPullPoseFix.json').write_text(json.dumps(report,indent=2))
unreal.log('PUSH_PULL_POSES_FIXED '+json.dumps(report))
