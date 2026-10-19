"""Generate a rigid-skinned sci-fi core and three glTF skeletal clips, in meters."""
import math, itertools, json, struct, os
from collections import defaultdict

ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT=os.path.join(ROOT,'SourceAssets','BossActiveCore')
os.makedirs(OUT,exist_ok=True)
TAU=math.tau
def add(a,b): return tuple(x+y for x,y in zip(a,b))
def sub(a,b): return tuple(x-y for x,y in zip(a,b))
def mul(a,s): return tuple(x*s for x in a)
def dot(a,b): return sum(x*y for x,y in zip(a,b))
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def unit(a): return mul(a,1/max(1e-12,math.sqrt(dot(a,a))))
def lerp(a,b,t): return add(mul(a,1-t),mul(b,t))
def clamp(t): return max(0.,min(1.,t))
def smooth(t): t=clamp(t);return t*t*(3-2*t)
def radial(r,z,a): return (r*math.cos(a),r*math.sin(a),z)
def quat(axis,angle): return (*mul(unit(axis),math.sin(angle/2)),math.cos(angle/2))
IDENT=(0,0,0,1)
bones=[]
def bone(name,parent,pos):
    i=len(bones);bones.append(dict(name=name,parent=parent,pos=pos));return i
root=bone('root',None,(0,0,0))
lift=bone('platform_lift',root,(0,0,0))
base=bone('base_housing',lift,(0,0,0))
piston=bone('core_piston',lift,(0,0,.63))
energy=bone('energy_column',piston,(0,0,.93))
core=bone('core_float',lift,(0,0,1.61))
arms=[]
for i in range(3):
    a=math.radians(30)+i*TAU/3
    lower=bone('claw_%d_lower'%(i+1),lift,radial(.48,.48,a))
    upper=bone('claw_%d_upper'%(i+1),lower,radial(.78,1.24,a))
    arms.append((a,lower,upper))

# Geometry is in world bind coordinates; each vertex has a single rigid influence.
geometry=defaultdict(list)
def triangle(a,b,c,mat,joint):
    n=unit(cross(sub(b,a),sub(c,a)))
    geometry[mat].append((a,b,c,n,joint))
def polygon(points,mat,joint):
    for i in range(1,len(points)-1): triangle(points[0],points[i],points[i+1],mat,joint)
def cylinder(a,b,r,mat,joint,n=16,r2=None,caps=True):
    axis=unit(sub(b,a));u=unit(cross(axis,(0,0,1) if abs(axis[2])<.9 else (0,1,0)));v=cross(axis,u)
    r2=r if r2 is None else r2
    rings=[[add(p,add(mul(u,rad*math.cos(k*TAU/n)),mul(v,rad*math.sin(k*TAU/n)))) for k in range(n)] for p,rad in [(a,r),(b,r2)]]
    for i in range(n):
        k=(i+1)%n
        polygon([rings[0][i],rings[0][k],rings[1][k],rings[1][i]],mat,joint)
    if caps:
        polygon(list(reversed(rings[0])),mat,joint);polygon(rings[1],mat,joint)
def ring(z,r,w,h,mat,joint,n=12):
    for i in range(n):
        a=i*TAU/n;b=(i+1)*TAU/n
        p=[radial(r-w,z,a),radial(r,z,a),radial(r,z,b),radial(r-w,z,b)]
        q=[add(v,(0,0,h)) for v in p]
        polygon(list(reversed(p)),mat,joint);polygon(q,mat,joint)
        for j in range(4):polygon([p[j],p[(j+1)%4],q[(j+1)%4],q[j]],mat,joint)
def radial_panel(a,outline,thickness,mat,joint):
    # outline is CCW in radial-distance/height coordinates.
    tangent=(-math.sin(a),math.cos(a),0)
    sides=[[add(radial(r,z,a),mul(tangent,t)) for r,z in outline] for t in [-thickness/2,thickness/2]]
    polygon(sides[0],mat,joint);polygon(list(reversed(sides[1])),mat,joint)
    for i in range(len(outline)):
        j=(i+1)%len(outline);polygon([sides[0][i],sides[1][i],sides[1][j],sides[0][j]],mat,joint)
def beam(a,b,width,depth,mat,joint):
    axis=unit(sub(b,a));u=unit(cross(axis,(0,0,1) if abs(axis[2])<.9 else (0,1,0)));v=cross(axis,u)
    corners=[]
    for p in [a,b]: corners.append([add(p,add(mul(u,x*width/2),mul(v,y*depth/2))) for x,y in [(-1,-1),(1,-1),(1,1),(-1,1)]])
    polygon(list(reversed(corners[0])),mat,joint);polygon(corners[1],mat,joint)
    for i in range(4):polygon([corners[0][i],corners[0][(i+1)%4],corners[1][(i+1)%4],corners[1][i]],mat,joint)

# Low hexagonal pedestal, armored belt, inset ribs and warning strips.
cylinder((0,0,.015),(0,0,.11),.68,'Armor',base,6,.64)
cylinder((0,0,.11),(0,0,.21),.61,'Dark',base,12)
cylinder((0,0,.21),(0,0,.52),.52,'Armor',base,12,.46)
ring(.12,.625,.035,.035,'Steel',base)
ring(.48,.475,.035,.035,'Steel',base)
ring(.535,.40,.09,.06,'Dark',base)
ring(.585,.38,.035,.025,'Steel',base)
for i in range(6):
    a=i*TAU/6
    radial_panel(a,[(.46,.16),(.61,.16),(.61,.28),(.49,.47),(.43,.47)],.30,'Armor',base)
    radial_panel(a,[(.612,.235),(.615,.235),(.535,.355),(.527,.355)],.20,'Amber',base)
    for j in range(3):
        z=.24+j*.052
        beam(radial(.521,z,a-.12),radial(.521,z,a+.12),.014,.016,'Dark',base)
    for side in [-1,1]:
        p=add(radial(.515,.41,a),mul((-math.sin(a),math.cos(a),0),side*.10))
        cylinder(p,add(p,mul(radial(1,0,a),.018)),.017,'Steel',base,6)
    # Short heavy anchor feet.
    radial_panel(a,[(.51,.015),(.78,.015),(.75,.07),(.55,.12)],.16,'Dark',base)

# Visible segmented actuator, inset blue socket and vertical energy tether.
cylinder((0,0,.51),(0,0,.76),.245,'Dark',piston,12)
cylinder((0,0,.69),(0,0,.94),.17,'Steel',piston,16)
ring(.82,.225,.075,.055,'Armor',piston)
ring(.925,.245,.06,.045,'Armor',piston)
cylinder((0,0,.953),(0,0,.965),.178,'Cyan',piston,24)
cylinder((0,0,.97),(0,0,1.38),.028,'Cyan',energy,12)
for i in range(8):
    a=i*TAU/8
    cylinder(radial(.198,.52,a),radial(.198,.72,a),.027,'Armor',piston,8)
    cylinder(radial(.198,.72,a),radial(.198,.79,a),.014,'Steel',piston,8)

# Three double-hinged claws with beveled silhouette, chrome pistons and inset energy strips.
for a,lower,upper in arms:
    radial_panel(a,[(.43,.39),(.60,.42),(.88,1.16),(.84,1.28),(.69,1.25),(.44,.58)],.22,'Dark',lower)
    radial_panel(a,[(.48,.55),(.58,.54),(.83,1.16),(.79,1.23),(.72,1.20)],.25,'Armor',lower)
    radial_panel(a,[(.505,.60),(.52,.60),(.744,1.15),(.725,1.15)],.256,'Cyan',lower)
    t=(-math.sin(a),math.cos(a),0)
    for side in [-1,1]:
        cylinder(add(radial(.50,.47,a),mul(t,side*.145)),add(radial(.70,.99,a),mul(t,side*.145)),.036,'Dark',lower,10)
        cylinder(add(radial(.70,.99,a),mul(t,side*.145)),add(radial(.78,1.19,a),mul(t,side*.145)),.020,'Steel',lower,10)
    for r,z in [(.49,.49),(.78,1.24)]:
        cylinder(add(radial(r,z,a),mul(t,-.15)),add(radial(r,z,a),mul(t,.15)),.09,'Steel',lower,12)
        cylinder(add(radial(r,z,a),mul(t,-.164)),add(radial(r,z,a),mul(t,-.151)),.056,'Dark',lower,8)
        cylinder(add(radial(r,z,a),mul(t,.151)),add(radial(r,z,a),mul(t,.164)),.056,'Dark',lower,8)
    radial_panel(a,[(.70,1.20),(.85,1.25),(.76,1.73),(.58,2.36),(.47,2.28),(.56,1.72)],.18,'Dark',upper)
    radial_panel(a,[(.73,1.30),(.81,1.32),(.72,1.76),(.57,2.30),(.51,2.25),(.60,1.72)],.215,'Armor',upper)
    radial_panel(a,[(.57,1.73),(.586,1.73),(.485,2.21),(.472,2.21)],.14,'Cyan',upper)
    # Raised facet at the claw tip and a steel elbow shield.
    radial_panel(a,[(.51,2.25),(.57,2.30),(.58,2.36),(.47,2.28)],.215,'Steel',upper)
    radial_panel(a,[(.69,1.23),(.83,1.27),(.80,1.43),(.66,1.39)],.23,'Steel',upper)
    for z in [1.48,1.57,1.66]:
        r=.78-(z-1.4)*.2
        radial_panel(a,[(r,z),(r+.025,z),(r+.021,z+.014),(r-.004,z+.014)],.22,'Dark',upper)

# Regular dodecahedron divided into 12 five-sided crystalline wedges.
phi=(1+math.sqrt(5))/2
points=list(itertools.product([-1,1],repeat=3))
for a,b in itertools.product([-1,1],repeat=2):points.extend([(0,a/phi,b*phi),(a/phi,b*phi,0),(a*phi,0,b/phi)])
points=[mul(p,.43/math.sqrt(3)) for p in points]
faces={}
for ids in itertools.combinations(range(20),3):
    a,b,c=[points[i] for i in ids];n=cross(sub(b,a),sub(c,a))
    if dot(n,n)<1e-10:continue
    n=unit(n);d=dot(n,a);ds=[dot(n,p)-d for p in points]
    if min(ds)<-1e-7 and max(ds)>1e-7:continue
    on=tuple(i for i,v in enumerate(ds) if abs(v)<1e-7)
    if len(on)!=5:continue
    if d<0:n=mul(n,-1)
    center=mul(tuple(sum(points[i][j] for i in on) for j in range(3)),.2)
    u=unit(sub(points[on[0]],center));v=cross(n,u)
    faces[on]=sorted(on,key=lambda i:math.atan2(dot(sub(points[i],center),v),dot(sub(points[i],center),u)))
assert len(faces)==12
shards=[];seen_edges=set()
for k,poly in enumerate(faces.values()):
    centroid=mul(tuple(sum(points[i][j] for i in poly) for j in range(3)),.2)
    origin=add((0,0,1.61),mul(centroid,.58))
    joint=bone('crystal_shard_%02d'%(k+1),core,origin);shards.append((joint,centroid))
    outer=[add((0,0,1.61),points[i]) for i in poly]
    polygon(outer,'Crystal',joint)
    apex=(0,0,1.61)
    for i in range(5):
        j=(i+1)%5
        # Interior fracture surfaces are revealed only when the pieces separate.
        triangle(apex,outer[j],outer[i],'Crystal',joint)
        edge=tuple(sorted((poly[i],poly[j])))
        if edge not in seen_edges:
            cylinder(outer[i],outer[j],.006,'Cyan',joint,6);seen_edges.add(edge)
# Small luminous nucleus, separately collapsible on destruction.
nucleus=bone('core_nucleus',core,(0,0,1.61))
for a,b,c in [(0,1,4),(1,2,4),(2,3,4),(3,0,4),(1,0,5),(2,1,5),(3,2,5),(0,3,5)]:
    p=[(.07,0,0),(0,.07,0),(-.07,0,0),(0,-.07,0),(0,0,.10),(0,0,-.10)]
    triangle(add(p[a],(0,0,1.61)),add(p[b],(0,0,1.61)),add(p[c],(0,0,1.61)),'White',nucleus)

# glTF uses right-handed Y-up coordinates; authored Z-up points convert as (x,z,-y).
def gl(p):return (p[0],p[2],-p[1])
def glq(q):return (q[0],q[2],-q[1],q[3])
data=bytearray();views=[];accessors=[]
def accessor(values,kind,component=5126,target=None,limits=False):
    while len(data)%4:data.append(0)
    start=len(data);flat=[]
    for v in values:flat.extend(v if isinstance(v,(list,tuple)) else [v])
    fmt={5126:'f',5123:'H',5125:'I'}[component]
    data.extend(struct.pack('<'+fmt*len(flat),*flat))
    view=dict(buffer=0,byteOffset=start,byteLength=len(data)-start)
    if target:view['target']=target
    vi=len(views);views.append(view)
    ac=dict(bufferView=vi,componentType=component,count=len(values),type=kind)
    if limits:
        vv=[v if isinstance(v,(list,tuple)) else [v] for v in values]
        ac['min']=[min(v[j] for v in vv) for j in range(len(vv[0]))];ac['max']=[max(v[j] for v in vv) for j in range(len(vv[0]))]
    ai=len(accessors);accessors.append(ac);return ai

palette=[('Armor',(.16,.19,.23),.72,.38),('Dark',(.028,.038,.052),.65,.48),('Steel',(.38,.45,.53),.85,.27),('Cyan',(.04,.65,1.),.1,.3),('Amber',(1.,.33,.025),.1,.35),('Crystal',(.025,.31,.68),.15,.2),('White',(.55,.9,1.),.1,.2)]
materials=[]
for name,col,metal,rough in palette:
    m=dict(name=name,pbrMetallicRoughness=dict(baseColorFactor=[*col,.4 if name=='Crystal' else 1],metallicFactor=metal,roughnessFactor=rough),doubleSided=name=='Crystal')
    if name=='Crystal':m['alphaMode']='BLEND'
    if name in ['Cyan','Amber','White']:m['emissiveFactor']=list(col)
    materials.append(m)
primitives=[]
for mat_index,(name,*_) in enumerate(palette):
    positions=[];normals=[];joints=[];weights=[];uv=[]
    for a,b,c,n,joint in geometry[name]:
        for p,tex in zip([a,b,c],[(0,0),(1,0),(.5,1)]):positions.append(gl(p));normals.append(gl(n));joints.append((joint,0,0,0));weights.append((1.,0.,0.,0.));uv.append(tex)
    attrs=dict(POSITION=accessor(positions,'VEC3',target=34962,limits=True),NORMAL=accessor(normals,'VEC3',target=34962),JOINTS_0=accessor(joints,'VEC4',5123,34962),WEIGHTS_0=accessor(weights,'VEC4',target=34962),TEXCOORD_0=accessor(uv,'VEC2',target=34962))
    primitives.append(dict(attributes=attrs,indices=accessor(list(range(len(positions))),'SCALAR',5125,34963),material=mat_index))
nodes=[]
for i,b in enumerate(bones):
    local=b['pos'] if b['parent'] is None else sub(b['pos'],bones[b['parent']]['pos'])
    node=dict(name=b['name'],translation=list(gl(local)))
    children=[j for j,c in enumerate(bones) if c['parent']==i]
    if children:node['children']=children
    nodes.append(node)
nodes.append(dict(name='SK_BossActiveCore',mesh=0,skin=0))
invbind=[]
for b in bones:
    x,y,z=gl(b['pos']);invbind.append((1,0,0,0,0,1,0,0,0,0,1,0,-x,-y,-z,1))
ibm=accessor(invbind,'MAT4')

# Animation poses are local to each bone. Root remains fixed for floor placement.
def pose(clip,t):
    result={}
    def setbone(i,delta=(0,0,0),rotation=IDENT,scale=(1,1,1)):
        b=bones[i];local=b['pos'] if b['parent'] is None else sub(b['pos'],bones[b['parent']]['pos'])
        result[i]=(add(local,delta),rotation,scale)
    for i in range(len(bones)):setbone(i)
    if clip=='Spawn':
        rise=smooth(t/1.25)
        z=-2.65*(1-rise)+.055*math.sin(max(0,t-1.1)*18)*math.exp(-max(0,t-1.1)*7) if t>1.1 else -2.65*(1-rise)
        setbone(lift,(0,0,z))
        opening=1-smooth((t-.6)/1.05)
        for a,lower,upper in arms:
            tangent=(-math.sin(a),math.cos(a),0)
            setbone(lower,rotation=quat(tangent,-.24*opening))
            setbone(upper,rotation=quat(tangent,-.62*opening))
        appear=smooth((t-.95)/.6)
        setbone(core,(0,0,-.18*(1-appear)),scale=(max(.001,appear),)*3)
        setbone(energy,scale=(max(.001,appear),)*3)
    elif clip=='Destroy':
        hit=smooth(t/.14);breakup=smooth((t-.12)/.5)
        setbone(lift,(.025*math.sin(t*60)*math.exp(-t*6),0,-.10*smooth((t-.2)/.7)))
        for i,(a,lower,upper) in enumerate(arms):
            tangent=(-math.sin(a),math.cos(a),0)
            setbone(lower,rotation=quat(tangent,(.68+.11*i)*smooth((t-.08-i*.03)/.75)))
            setbone(upper,rotation=quat(tangent,(.45+.18*i)*smooth((t-.15)/.65)))
        for k,(j,centroid) in enumerate(shards):
            u=unit(centroid);dt=max(0,t-.13)
            # Ballistic trajectories settle at the floor, then shards extinguish.
            z=max(-1.48, (.8+u[2]*.9)*dt-2.4*dt*dt)
            delta=(u[0]*(.8+.05*k)*min(dt,1.),u[1]*(.8+.05*k)*min(dt,1.),z)
            s=max(.001,1-smooth((t-1.6)/.7))
            setbone(j,delta,quat(unit((.3+k*.1,1,.6)),min(dt,1.25)*(1+k*.21)),(s,s,s))
        s=max(.001,1-smooth((t-.08)/.25));setbone(nucleus,scale=(s,)*3)
        s=max(.001,1-smooth((t-.1)/.2));setbone(energy,scale=(s,)*3)
        setbone(piston,(0,0,-.23*smooth((t-.2)/.8)))
    elif clip=='Retract':
        closing=smooth((t-.1)/.85)
        for a,lower,upper in arms:
            tangent=(-math.sin(a),math.cos(a),0)
            setbone(lower,rotation=quat(tangent,-.24*closing))
            setbone(upper,rotation=quat(tangent,-.62*closing))
        s=max(.001,1-smooth((t-.1)/.7));setbone(core,(0,0,-.2*(1-s)),scale=(s,)*3);setbone(energy,scale=(s,)*3)
        setbone(piston,(0,0,-.20*smooth((t-.4)/.5)))
        setbone(lift,(0,0,-2.65*smooth((t-.85)/1.35)))
    return result
animations=[]
for clip,duration in [('Spawn',2.2),('Destroy',2.4),('Retract',2.4)]:
    frames=int(round(duration*30))+1;times=[i/30 for i in range(frames)]
    poses=[pose(clip,t) for t in times];samplers=[];channels=[]
    time_ac=accessor(times,'SCALAR',limits=True)
    for j in range(len(bones)):
        for path,idx,kind in [('translation',0,'VEC3'),('rotation',1,'VEC4'),('scale',2,'VEC3')]:
            values=[p[j][idx] for p in poses]
            if all(v==values[0] for v in values[1:]) and (path!='translation' or j==root):continue
            values=[gl(v) for v in values] if path=='translation' else [glq(v) for v in values] if path=='rotation' else values
            out=accessor(values,kind);si=len(samplers)
            samplers.append(dict(input=time_ac,output=out,interpolation='LINEAR'))
            channels.append(dict(sampler=si,target=dict(node=j,path=path)))
    animations.append(dict(name='AN_BossActiveCore_'+clip,samplers=samplers,channels=channels))
gltf=dict(asset=dict(version='2.0',generator='BossActiveCore procedural rig builder'),scene=0,scenes=[dict(nodes=[0,len(bones)])],nodes=nodes,meshes=[dict(name='SK_BossActiveCore',primitives=primitives)],skins=[dict(name='SKEL_BossActiveCore',joints=list(range(len(bones))),skeleton=0,inverseBindMatrices=ibm)],materials=materials,animations=animations,buffers=[dict(byteLength=len(data))],bufferViews=views,accessors=accessors)
js=json.dumps(gltf,separators=(',',':')).encode();js+=b' '*((-len(js))%4)
data+=b'\x00'*((-len(data))%4)
path=os.path.join(OUT,'SK_BossActiveCore.glb')
with open(path,'wb') as f:
    f.write(struct.pack('<III',0x46546c67,2,12+8+len(js)+8+len(data)));f.write(struct.pack('<II',len(js),0x4e4f534a));f.write(js);f.write(struct.pack('<II',len(data),0x004e4942));f.write(data)
report=dict(bones=len(bones),triangles=sum(len(v) for v in geometry.values()),materials=[m['name'] for m in materials],clips={a['name']:d for a,d in zip(animations,[2.2,2.4,2.4])},floor_pivot=True,crystal_faces=12,crystal_edges=len(seen_edges),source=path)
with open(os.path.join(OUT,'source_report.json'),'w') as f:json.dump(report,f,indent=2)
print(json.dumps(report,indent=2))
