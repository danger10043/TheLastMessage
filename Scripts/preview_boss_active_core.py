"""Source-geometry inspection renders; the Unreal assets are validated separately."""
import runpy, os, math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops
s=runpy.run_path(os.path.join(os.path.dirname(__file__),'build_boss_active_core_source.py'))
bones=s['bones'];geometry=s['geometry'];out=s['OUT'];palette={x[0]:x[1] for x in s['palette']}
def rotation(q):
    x,y,z,w=q
    return np.array([[1-2*y*y-2*z*z,2*x*y-2*z*w,2*x*z+2*y*w],[2*x*y+2*z*w,1-2*x*x-2*z*z,2*y*z-2*x*w],[2*x*z-2*y*w,2*y*z+2*x*w,1-2*x*x-2*y*y]])
camera=np.array([.5,7,3.3]);target=np.array([0,0,1.03]);forward=(target-camera);forward/=np.linalg.norm(forward);right=np.cross(forward,[0,0,1]);right/=np.linalg.norm(right);up=np.cross(right,forward)
light=np.array([-.3,-.6,.9]);light/=np.linalg.norm(light)
def render(clip='Spawn',time=2.2,size=640):
    poses=s['pose'](clip,time);world=[]
    for i,b in enumerate(bones):
        p,q,scale=poses[i];mat=np.eye(4);mat[:3,:3]=rotation(q)@np.diag(scale);mat[:3,3]=p
        world.append(mat if b['parent'] is None else world[b['parent']]@mat)
    matrices=[]
    for i,b in enumerate(bones):
        inv=np.eye(4);inv[:3,3]=-np.array(b['pos']);matrices.append(world[i]@inv)
    im=Image.new('RGB',(size,size),(19,25,34));draw=ImageDraw.Draw(im)
    def project(v):
        v=np.asarray(v)-target
        return (size*.50+np.dot(v,right)*size*.31,size*.53-np.dot(v,up)*size*.31)
    for k in range(-7,8):
        draw.line([project([k*.4,-2.8,0]),project([k*.4,2.8,0])],fill=(30,39,49))
        draw.line([project([-2.8,k*.4,0]),project([2.8,k*.4,0])],fill=(30,39,49))
    # Mask below-floor geometry, including the underground lift.
    transformed=[]
    for name,tris in geometry.items():
        for a,b,c,n,j in tris:
            mat=matrices[j];p=(np.array([a,b,c])@mat[:3,:3].T)+mat[:3,3]
            if np.max(p[:,2])<0:continue
            if np.min(p[:,2])<-.005:continue
            normal=np.cross(p[1]-p[0],p[2]-p[0]);ln=np.linalg.norm(normal)
            if ln<1e-10:continue
            normal/=ln
            if np.dot(normal,camera-p.mean(axis=0))<0 and name!='Crystal':continue
            transformed.append((np.dot(p.mean(axis=0)-camera,forward),name,p,normal))
    transformed.sort(key=lambda x:x[0],reverse=True)
    glow=Image.new('RGB',(size,size));gd=ImageDraw.Draw(glow)
    for depth,name,p,normal in transformed:
        col=np.array(palette[name]);brightness=.35+.85*max(0,np.dot(normal,light))+.18*max(0,np.dot(normal,[-.7,.5,.5]))
        if name in ['Cyan','White','Amber']:brightness=1.8
        col=np.clip(col*brightness,0,1)
        rgb=tuple(int(pow(float(v),.65)*255) for v in col)
        polygon=[project(v) for v in p]
        if name=='Crystal':
            layer=Image.new('RGBA',im.size);ImageDraw.Draw(layer).polygon(polygon,fill=(45,145,235,38));im=Image.alpha_composite(im.convert('RGBA'),layer).convert('RGB');draw=ImageDraw.Draw(im)
        else:draw.polygon(polygon,fill=rgb)
        if name in ['Cyan','White','Amber']:gd.polygon(polygon,fill=tuple(int(c*.7) for c in rgb))
    im=ImageChops.add(im,glow.filter(ImageFilter.GaussianBlur(size*.009)))
    return im
fontpath=r'C:\Windows\Fonts\arial.ttf'
font=ImageFont.truetype(fontpath,22);small=ImageFont.truetype(fontpath,16)
hero=render(size=1000);hero.save(os.path.join(out,'BossActiveCore_SourcePreview.png'))
sheet=Image.new('RGB',(1280,1320),(15,20,28));d=ImageDraw.Draw(sheet)
rows=[('Spawn',[.0,.65,1.25,2.2]),('Destroy',[0,.3,.7,2.4]),('Retract',[0,.6,1.5,2.4])]
for row,(clip,times) in enumerate(rows):
    d.text((24,row*440+12),clip.upper(),font=font,fill=(150,214,255))
    for col,t in enumerate(times):
        sheet.paste(render(clip,t,320),(col*320,row*440+50))
        d.text((col*320+20,row*440+383),'%.2f s'%t,font=small,fill=(191,204,221))
sheet.save(os.path.join(out,'BossActiveCore_AnimationContactSheet.png'))
frames=[]
for clip,duration in [('Spawn',2.2),('Destroy',2.4),('Retract',2.4)]:
    for i in range(int(duration*15)+1):
        t=i/15;im=render(clip,t,480);ImageDraw.Draw(im).text((18,16),'%s  %.2fs'%(clip.upper(),t),font=small,fill=(190,220,255));frames.append(im)
    frames.extend([frames[-1]]*8)
frames[0].save(os.path.join(out,'BossActiveCore_Animations.gif'),save_all=True,append_images=frames[1:],duration=67,loop=0)
print('SOURCE_PREVIEWS_COMPLETE')
