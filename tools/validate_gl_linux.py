# Linux-only optional validation. Requires Mesa EGL, NumPy and software OpenGL.
# Synthetic textures and reduced sample counts; not a hardware benchmark or production noise validation.
import os,ctypes as C,pathlib,re
os.environ['EGL_PLATFORM']='surfaceless';os.environ['MESA_GL_VERSION_OVERRIDE']='4.6';os.environ['MESA_GLSL_VERSION_OVERRIDE']='460'
E=C.CDLL('libEGL.so.1');P=C.c_void_p;I=C.c_int;U=C.c_uint
for name,rt,at in [('eglGetDisplay',P,[P]),('eglInitialize',U,[P,P,P]),('eglBindAPI',U,[U]),('eglChooseConfig',U,[P,P,P,I,P]),('eglCreateContext',P,[P,P,P,P]),('eglMakeCurrent',U,[P,P,P,P]),('eglGetProcAddress',P,[C.c_char_p])]:
 f=getattr(E,name);f.restype=rt;f.argtypes=at
D=E.eglGetDisplay(None);assert E.eglInitialize(D,None,None);assert E.eglBindAPI(0x30A2)
attrs=(I*5)(0x3040,8,0x3033,1,0x3038);cfg=P();n=I();assert E.eglChooseConfig(D,attrs,C.byref(cfg),1,C.byref(n)) and n.value
ctx=E.eglCreateContext(D,cfg,None,(I*7)(0x3098,4,0x30FB,6,0x30FD,1,0x3038));assert ctx and E.eglMakeCurrent(D,None,None,ctx)
def gl(n,r,*a):return C.CFUNCTYPE(r,*a)(E.eglGetProcAddress(n.encode()))
create=gl('glCreateShader',U,U);source=gl('glShaderSource',None,U,I,P,P);compile=gl('glCompileShader',None,U);get=gl('glGetShaderiv',None,U,U,P);log=gl('glGetShaderInfoLog',None,U,I,P,P)
path=pathlib.Path(__file__).resolve().parents[1]/'shaders'
def expand(p):return re.sub(r'#include "(.*?)"',lambda m:expand(p.parent/m[1]),p.read_text())
ss=[]
for name,typ in [('fullscreen.vert',0x8B31),('clouds.frag',0x8B30),('cloud_noise.comp',0x91B9),('accumulate.frag',0x8B30)]:
 s=create(typ);b=C.c_char_p(expand(path/name).encode());source(s,1,C.byref(b),None);compile(s);ok=I();get(s,0x8B81,C.byref(ok));buf=C.create_string_buffer(16384);log(s,len(buf),None,buf);print(name,ok.value,buf.value.decode());assert ok.value;ss.append(s)
cp=gl('glCreateProgram',U);att=gl('glAttachShader',None,U,U);link=gl('glLinkProgram',None,U);gp=gl('glGetProgramiv',None,U,U,P);lp=gl('glGetProgramInfoLog',None,U,I,P,P)
for group in [ss[:2],ss[2:3],[ss[0],ss[3]]]:
 p=cp();[att(p,s) for s in group];link(p);ok=I();gp(p,0x8B82,C.byref(ok));buf=C.create_string_buffer(16384);lp(p,len(buf),None,buf);print('link',ok.value,buf.value.decode());assert ok.value
# Render with a deterministic synthetic texture; compile/link the actual noise compute shader above.
import numpy as np
v=gl('glCreateVertexArrays',None,I,P);bva=gl('glBindVertexArray',None,U);vao=U();v(1,C.byref(vao));bva(vao)
# Link again to retain draw program.
p=cp();[att(p,s) for s in ss[:2]];link(p);gl('glUseProgram',None,U)(p)
ct=gl('glCreateTextures',None,U,I,P);ts=gl('glTextureStorage3D',None,U,I,U,I,I,I);sub=gl('glTextureSubImage3D',None,U,I,I,I,I,I,I,I,U,U,P);par=gl('glTextureParameteri',None,U,U,I);bind=gl('glBindTextureUnit',None,U,U);pix=gl('glPixelStorei',None,U,I);pix(0x0CF5,1)
textures=(U*3)();ct(0x806F,3,textures)
z,y,x=np.indices((128,128,128),dtype=np.float32);a=(np.sin(x*.09817477)+np.cos(y*.14726215)+np.sin(z*.19634954))/6+.5;a=np.uint8(np.clip(a*255,0,255));rg=np.ascontiguousarray(np.stack([a,255-a],axis=-1));
for i in range(2):
 t=textures[i];ts(t,1,0x822B,128,128,128);par(t,0x2801,0x2601);par(t,0x2800,0x2601)
 for e in [0x2802,0x2803,0x8072]:par(t,e,0x2901)
 sub(t,0,0,0,0,128,128,128,0x8227,0x1401,rg.ctypes.data);bind(4+i,t)
lev=np.maximum.reduce([np.roll(a,(-dz,-dy,-dx),(0,1,2)) for dz in (0,1) for dy in (0,1) for dx in (0,1)])
t=textures[2];ts(t,8,0x8229,128,128,128);par(t,0x2801,0x2700);par(t,0x2800,0x2600)
for m in range(8):
 n=lev.shape[0];lev=np.ascontiguousarray(lev);sub(t,m,0,0,0,n,n,n,0x1903,0x1401,lev.ctypes.data)
 if n>1:lev=lev.reshape(n//2,2,n//2,2,n//2,2).max(axis=(1,3,5))
bind(6,t)
# FBO and attachments
fb=U();gl('glCreateFramebuffers',None,I,P)(1,C.byref(fb));tex=(U*2)();ct(0x0DE1,2,tex)
for i in range(2):gl('glTextureStorage2D',None,U,I,U,I,I)(tex[i],1,0x8814,64,36);gl('glNamedFramebufferTexture',None,U,U,U,I)(fb,0x8CE0+i,tex[i],0)
attachments=(U*2)(0x8CE0,0x8CE1);gl('glNamedFramebufferDrawBuffers',None,U,I,P)(fb,2,attachments);gl('glBindFramebuffer',None,U,U)(0x8D40,fb);assert gl('glCheckFramebufferStatus',U,U)(0x8D40)==0x8CD5;gl('glViewport',None,I,I,I,I)(0,0,64,36)
loc=gl('glGetUniformLocation',I,U,C.c_char_p)
def ui(n,x):gl('glUniform1i',None,I,I)(loc(p,n.encode()),x)
def uf(n,*x):gl('glUniform'+str(len(x))+'f',None,I,*([C.c_float]*len(x)))(loc(p,n.encode()),*x)
for n,x in [('uCloudShape',4),('uCloudDetail',5),('uBounds',6),('uOptSteps',16),('uReferenceSteps',128),('uReferenceLight',16),('uPhaseIndex',0),('uJitterMode',0)]:ui(n,x)
for n,x in [('uResolution',(64,36)),('uPitch',(.5236,)),('uYaw',(.7854,)),('uFov',(1.0472,)),('uSunElevation',(1.1345,)),('uSunAzimuth',(1.57,)),('uTime',(12,)),('uFieldScaleRadius',(300,)),('uSkyCenter',(0,0,0)),('uAalTolerance',(.1,)),('uVolumeField',(1.25,.55,2,.28)),('uVolumeShape',(.25,.02,1.3,.8)),('uVolumeOptics',(.65,.35,.8,2.07)),('uVolumeMotion',(12,4,.8,.2)),('uVolumeSampling',(.12,.15)),('uAtmosphere',(2.5,5,.26,.27)),('uAtmosphereNorm',(1,1,1,.035))]:uf(n,*x)
gl('glUniform4i',None,I,I,I,I,I)(loc(p,b'uVolumeBudget'),64,6,2,128)
read=gl('glReadPixels',None,I,I,I,I,U,U,P);draw=gl('glDrawArrays',None,U,I,I);rb=gl('glReadBuffer',None,U);rb(0x8CE1)
images=[]
for mode in range(11):
 ui('uMode',mode);draw(4,0,3);img=np.empty((36,64,4),dtype=np.float32);read(0,0,64,36,0x1908,0x1406,img.ctypes.data);assert np.isfinite(img).all(),mode;images.append(img.copy());print('mode',mode,'mean',float(img[:,:,:3].mean()),'max',float(img.max()))
assert np.max(np.abs(images[0]-images[1]))>1e-4
assert np.max(np.abs(images[1]-images[2]))>1e-4
assert np.max(np.abs(images[1]-images[5]))>1e-4
for pitch in (0.,1.04719755,1.57079633):
 uf('uPitch',pitch)
 for sun in (.06981317,-.38397244):
  uf('uSunElevation',sun)
  for mode in range(11):
   ui('uMode',mode);draw(4,0,3);img=np.empty((36,64,4),dtype=np.float32);read(0,0,64,36,0x1908,0x1406,img.ctypes.data);assert np.isfinite(img).all(),(mode,pitch,sun)
err=gl('glGetError',U)();assert err==0,hex(err)
print('all mode smoke draws finite at horizon, 30/60/90 deg, noon/sunset/night; GL error',hex(err))
