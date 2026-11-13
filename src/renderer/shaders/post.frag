#version 450
// Second post stage (compose). Reads the scene (linear RGB, planar depth in
// alpha) and the occlusion stage (rgb bounce light, a open fraction), then
// denoises occlusion with a depth-aware 4x4 filter, adds bounce light,
// aerial haze, volumetric lamp scattering, bloom and the grade. Its result
// feeds both headless readback and the window composite.
layout(set=0,binding=0) uniform sampler2D sceneImage;
layout(set=0,binding=1) uniform sampler2D occlusionImage;
layout(set=0,binding=2) uniform sampler2D shadowImage;
layout(set=0,binding=3) uniform sampler2D exposureImage;
layout(set=0,binding=4) uniform sampler2D materialImage;
// The bloom chain's top level (half size): the sum of six glare widths.
layout(set=0,binding=5) uniform sampler2D bloomImage;
// The room reflection probes (GpuRenderer captureReflections), the off-screen fallback for SSR.
layout(set=0,binding=6) uniform sampler2DArray reflectionAtlas;
layout(std430,set=0,binding=7) readonly buffer LampGrid {vec4 origin;ivec4 size;ivec4 flags;vec4 eye;vec4 basis;vec4 sun;vec4 sunColor;vec4 cascades[12];vec4 flashes[4];vec4 flashTones[4];vec4 scatter;vec4 probeInfo;vec4 probeVolumes[16];vec4 reflections[144];} lampGrid;
// The look's values (LookTuning.h, GpuRenderer writeLook): the screen reflections' strength (LookA.x),
// the screen march's roughest finish (LookB.x), a debug switch (LookC.y: 2 no screen march).
#define LookA lampGrid.reflections[141]
#define LookB lampGrid.reflections[142]
#define LookC lampGrid.reflections[143]
layout(push_constant) uniform PostParams {
 vec4 camera;   // x AO strength (negative: debug view), y aspect, z focal, w radius (m)
 vec4 haze;     // rgb linear haze colour, w density per metre
 vec4 frame;    // xy texel size, z bloom strength, w saturation
 vec4 grade;    // x shadow lift, y warm highlights, z cool shadows, w far depth
 vec4 lights[4];// view-space lamp positions (xyz) and power (w)
} p;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outColor;

float depthAt(vec2 at){return texture(sceneImage,clamp(at,vec2(0.0),vec2(1.0))).a;}
vec3 viewPosition(vec2 at,float depth){
 vec2 ndc=at*2.0-1.0;
 return vec3(ndc.x*depth/p.camera.z,-ndc.y*depth/(p.camera.z*p.camera.y),depth);
}
// Neighbours on the viewmodel (negative depth) or sky stand in as this surface.
float normalDepth(vec2 at,float fallback){float d=depthAt(at);return d>0.02?d:fallback;}
vec3 viewNormal(vec2 at,vec3 center){
  // 2-pixel baseline: half-precision depth steps otherwise tilt reflection normals.
 vec2 t=p.frame.xy*2.0;
 vec3 r=viewPosition(at+vec2(t.x,0),normalDepth(at+vec2(t.x,0),center.z)),l=viewPosition(at-vec2(t.x,0),normalDepth(at-vec2(t.x,0),center.z));
 vec3 d=viewPosition(at+vec2(0,t.y),normalDepth(at+vec2(0,t.y),center.z)),u=viewPosition(at-vec2(0,t.y),normalDepth(at-vec2(0,t.y),center.z));
 vec3 dx=abs(r.z-center.z)<abs(center.z-l.z)?r-center:center-l;
 vec3 dy=abs(d.z-center.z)<abs(center.z-u.z)?d-center:center-u;
 vec3 n=normalize(cross(dy,dx));
 return dot(n,center)>0.0?-n:n;
}
// The occlusion stage rotates its kernel on a 4x4 Bayer grid; averaging the
// 4x4 neighbourhood with similar depth removes that pattern without bleeding
// across silhouettes (denoisedBoth below).

// Lamp visibility and coloured shafts from the shadow stage, filtered like the occlusion.
vec4 denoisedShadow(float depth){
 vec4 sum=vec4(0.0);float weight=0.0;
 vec2 base=(floor(uv/p.frame.xy-1.5)+0.5)*p.frame.xy;
 for(int y=0;y<4;++y)for(int x=0;x<4;++x){
  vec2 at=base+vec2(x,y)*p.frame.xy;
  float w=exp(-abs(texture(sceneImage,at).a-depth)/(0.04*depth+0.02));
  sum+=texture(shadowImage,at)*w;weight+=w;
 }
 return weight>0.0?sum/weight:vec4(1.0,0.0,0.0,0.0);
}
// Both filters at once: the same 16 depth taps and weights feed occlusion and
// lamp visibility, so a surface pixel reads the depth once instead of twice.
void denoisedBoth(float depth,out vec4 occlusion,out vec4 shadow){
 vec4 occlusionSum=vec4(0.0);vec4 shadowSum=vec4(0.0);float weight=0.0;
 vec2 base=(floor(uv/p.frame.xy-1.5)+0.5)*p.frame.xy;
 for(int y=0;y<4;++y)for(int x=0;x<4;++x){
  vec2 at=base+vec2(x,y)*p.frame.xy;
  float sampleDepth=texture(sceneImage,at).a;
  float w=exp(-abs(sampleDepth-depth)/(0.04*depth+0.02));
  occlusionSum+=texture(occlusionImage,at)*w;shadowSum+=texture(shadowImage,at)*w;weight+=w;
 }
 occlusion=weight>0.0?occlusionSum/weight:texture(occlusionImage,uv);
 shadow=weight>0.0?shadowSum/weight:vec4(1.0,0.0,0.0,0.0);
}

// Single scattering from a point lamp along the view ray, integrated in
// closed form (inverse-square falloff), stopping at the visible surface.
float lampScatter(vec3 rayDirection,float rayLength,vec4 lamp){
 if(lamp.w<=0.0)return 0.0;
 float along=dot(lamp.xyz,rayDirection);
 float h2=max(dot(lamp.xyz,lamp.xyz)-along*along,0.04);float h=sqrt(h2);
 float glow=(atan((rayLength-along)/h)-atan(-along/h))/h;
 return glow*lamp.w;
}


// Reflection probes: six 90-degree faces per room probe, captured by the scene pass (face order and
// orientation as GpuRenderer captureReflections draws them), a mip chain for rough surfaces.
// The mip a GGX lobe of roughness r reads (as scene.frag's probeLod).
float probeLod(float r){float a=r*r;return clamp(log2(max(159.0*a,1e-4)),0.0,lampGrid.probeInfo.z-1.0);}
// The split-sum table's analytic fit (Karis, mobile), for the weight of the screen march's swap.
vec2 envBrdfApprox(float NoV,float r){const vec4 c0=vec4(-1.0,-0.0275,-0.572,0.022);const vec4 c1=vec4(1.0,0.0425,1.04,-0.04);vec4 q=r*c0+c1;float a004=min(q.x*q.x,exp2(-9.28*NoV))*q.x+q.y;return vec2(-1.04,1.04)*a004+q.zw;}
const vec3 CaptureForward[6]=vec3[6](vec3(1,0,0),vec3(-1,0,0),vec3(0,1,0),vec3(0,-1,0),vec3(0,0,1),vec3(0,0,-1));
const vec3 CaptureRight[6]=vec3[6](vec3(0,1,0),vec3(0,-1,0),vec3(-1,0,0),vec3(1,0,0),vec3(0,1,0),vec3(0,1,0));
const vec3 CaptureUp[6]=vec3[6](vec3(0,0,1),vec3(0,0,1),vec3(0,0,1),vec3(0,0,1),vec3(-1,0,0),vec3(1,0,0));
// What the room's probe sees along R from p: inside the room's box the ray is carried to the box
// wall it meets first and that point looked up from the probe (parallax), so a floor reflects the
// wall across from it, not the one behind the probe. a is the probe's weight (1 inside its box,
// fading over 6 m outside it); sky is the share of the texel that saw no surface (outdoors).
vec4 probeReflection(vec3 p,vec3 R,float roughness,out float sky){
 sky=0.0;int count=int(lampGrid.probeInfo.y);int best=-1;float bestScore=1e30;
 for(int i=0;i<count;++i){vec4 c=lampGrid.reflections[3*i],lo=lampGrid.reflections[3*i+1],hi=lampGrid.reflections[3*i+2];
  vec3 outside=max(max(lo.xyz-p,p-hi.xyz),vec3(0.0)),d=p-c.xyz;float score=dot(d,d)*0.05+dot(outside,outside)*4.0;
  if(score<bestScore){bestScore=score;best=i;}}
 if(best<0)return vec4(0.0);
 vec4 c=lampGrid.reflections[3*best],lo=lampGrid.reflections[3*best+1],hi=lampGrid.reflections[3*best+2];
 float away=length(max(max(lo.xyz-p,p-hi.xyz),vec3(0.0)));
 // A room's walls are its box's faces, so a point on a wall sits on the box's edge, and depth's steps
 // put it a hair inside or outside from one pixel to the next: an on/off parallax there drew streaks.
 // Points within half a metre of the box are carried just inside it and always take the parallax.
 vec3 dir=R;
 if(away<0.5&&LookC.y<0.5){vec3 q=clamp(p,lo.xyz+0.02,hi.xyz-0.02);vec3 r=R+vec3(1e-5);vec3 t=max((hi.xyz-q)/r,(lo.xyz-q)/r);dir=q+R*max(min(min(t.x,t.y),t.z),0.0)-c.xyz;}
 vec3 a=abs(dir);int face=a.x>=a.y&&a.x>=a.z?(dir.x>0.0?0:1):a.y>=a.z?(dir.y>0.0?2:3):(dir.z>0.0?4:5);
 float z=max(dot(dir,CaptureForward[face]),1e-5);
 vec2 st=clamp(vec2(0.5+0.5*dot(dir,CaptureRight[face])/z,0.5-0.5*dot(dir,CaptureUp[face])/z),vec2(0.0),vec2(1.0));
 // Blurred down the mips with roughness: a mirror reads the face itself, a brushed or worn finish
 // only the room's broad light and dark.
 vec4 s=textureLod(reflectionAtlas,vec3(st,c.w*6.0+float(face)),probeLod(roughness));
 sky=smoothstep(300.0,900.0,s.a);
 return vec4(s.rgb,clamp(1.0-away/6.0,0.0,1.0));
}

// A view-space vector in world space (worldDirection without the normalising).
vec3 worldVector(vec3 v){vec4 b=lampGrid.basis;float forward=v.z*b.z-v.y*b.w;return vec3(-v.x*b.y+forward*b.x,v.x*b.x+forward*b.y,v.y*b.z+v.z*b.w);}
vec3 worldDirection(vec3 v){
 vec4 b=lampGrid.basis;float forward=v.z*b.z-v.y*b.w;
 return normalize(vec3(-v.x*b.y+forward*b.x,v.x*b.x+forward*b.y,v.y*b.z+v.z*b.w));
}
float cloudHash(ivec2 c){uvec2 q=uvec2(c+ivec2(65536));uint h=q.x*1664525u+q.y*1013904223u;h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;return float(h&0xffffffu)/16777216.0;}
float cloudNoise(vec2 p){ivec2 i=ivec2(floor(p));vec2 f=fract(p);f=f*f*(3.0-2.0*f);
 return mix(mix(cloudHash(i),cloudHash(i+ivec2(1,0)),f.x),mix(cloudHash(i+ivec2(0,1)),cloudHash(i+ivec2(1,1)),f.x),f.y);}
float cloudFbm(vec2 p){float a=0.5,s=0.0;mat2 m=mat2(1.6,1.2,-1.2,1.6);for(int i=0;i<5;++i){s+=a*cloudNoise(p);p=m*p+vec2(3.1,1.7);a*=0.5;}return s;}
bool zoneSky(){return lampGrid.flags.z==1;}
// Open-zone sky: hazy blue between broken cumulus, a soft high veil (slight
// overcast), clouds lit on the sun side with silver edges, and a warm sun that
// dims when a cloud crosses it. cloudAt is also the shafts' sun visibility.
float zoneCloud(vec3 dir,out float toward){
 vec3 sunDir=lampGrid.sun.xyz;float time=lampGrid.eye.w;
 vec3 p=lampGrid.eye.xyz+dir*((900.0-lampGrid.eye.z)/max(dir.z,0.02));
 vec2 q=p.xy*0.00085+vec2(time*0.0030,time*0.0011);
 float cover=cloudFbm(q);
 toward=cloudFbm(q+sunDir.xy*0.05)-cover;
 return smoothstep(0.44,0.66,cover)*smoothstep(0.0,0.12,dir.z);
}
vec3 zoneSkyColor(vec3 dir){
 vec3 sunDir=lampGrid.sun.xyz,sunCol=lampGrid.sunColor.rgb;
 float up=clamp(dir.z,-1.0,1.0);
 vec3 zenith=vec3(0.20,0.38,0.68),horizon=vec3(0.68,0.73,0.76),ground=vec3(0.40,0.42,0.38);
 vec3 sky=up>0.0?mix(horizon,zenith,pow(up,0.6)):mix(horizon,ground,clamp(-up*5.0,0.0,1.0));
 float cosSun=max(dot(dir,sunDir),0.0);
 // Haze glows round the sun (broad Mie lobe), brightest toward the horizon.
 sky+=sunCol*(0.10*pow(cosSun,4.0)+0.30*pow(cosSun,32.0))*(1.0-0.4*max(up,0.0));
 float toward=0.0,cloud=dir.z>0.0?zoneCloud(dir,toward):0.0;
 float lit=clamp(0.55-toward*5.0,0.0,1.0);
 vec3 cloudColor=mix(vec3(0.52,0.55,0.60),vec3(1.0,0.97,0.92),lit);
 // Silver lining: thin cloud edges near the sun glow.
 cloudColor+=sunCol*pow(cosSun,12.0)*(1.0-cloud)*1.4;
 sky=mix(sky,cloudColor,cloud*0.96);
 // Slight overcast: a thin high veil softens the blue and the sun.
 sky=mix(sky,vec3(0.80,0.81,0.82),0.08*smoothstep(-0.02,0.4,up));
 float sunCloud=0.0;{float t;sunCloud=zoneCloud(sunDir,t);}
 sky+=sunCol*(2.6*smoothstep(0.99955,0.99975,dot(dir,sunDir)))*(1.0-0.9*sunCloud);
 // An ash storm (flags.w, 0-1000) closes the sky over: a low brown ceiling, the sun a smudge.
 float storm=float(lampGrid.flags.w)*0.001;
 vec3 ash=mix(vec3(0.30,0.24,0.19),vec3(0.17,0.14,0.12),clamp(up*1.5,0.0,1.0))+sunCol*0.08*pow(cosSun,8.0);
 return mix(sky,ash,storm*0.94);
}
// Overcast wasteland sky: gradient, sun disc and glow, one drifting cloud deck.
vec3 proceduralSky(vec3 dir){
 if(zoneSky())return zoneSkyColor(dir);
 vec3 sunDir=lampGrid.sun.xyz,sunCol=lampGrid.sunColor.rgb;float time=lampGrid.eye.w;
 float up=clamp(dir.z,-1.0,1.0);
 vec3 zenith=vec3(0.22,0.29,0.42),horizon=vec3(0.56,0.60,0.66),ground=vec3(0.19,0.18,0.17);
 vec3 sky=up>0.0?mix(horizon,zenith,pow(up,0.55)):mix(horizon,ground,clamp(-up*4.0,0.0,1.0));
 float cosSun=dot(dir,sunDir);
 sky+=sunCol*(0.16*pow(max(cosSun,0.0),6.0)+1.2*smoothstep(0.9985,0.9996,cosSun)+0.04*max(cosSun,0.0));
 if(dir.z>0.012){
  vec3 p=lampGrid.eye.xyz+dir*((420.0-lampGrid.eye.z)/dir.z);vec2 q=p.xy*0.0011+vec2(time*0.004,time*0.0016);
  float cover=cloudFbm(q);float cloud=smoothstep(0.40,0.70,cover);
  // Thicker toward the sun reads darker; the thin edge toward it reads lit.
  float toward=cloudFbm(q+sunDir.xy*0.035)-cover;
  vec3 cloudColor=mix(vec3(0.40,0.42,0.46),vec3(0.96,0.94,0.91),clamp(0.5-toward*4.0,0.0,1.0));
  cloudColor*=0.75+0.25*max(sunDir.z,0.0);
  sky=mix(sky,cloudColor,cloud*smoothstep(0.012,0.14,dir.z)*0.95);
 }
 return sky;
}

vec2 projectView(vec3 q){return vec2(q.x*p.camera.z/q.z,-q.y*p.camera.z*p.camera.y/q.z)*0.5+0.5;}
// World direction to view space (inverse of worldDirection).
vec3 viewDirection(vec3 w){
 vec4 b=lampGrid.basis;vec3 right=vec3(-b.y,b.x,0.0),level=vec3(b.x,b.y,0.0);
 return vec3(dot(w,right),dot(w,-b.w*level+vec3(0,0,b.z)),dot(w,b.z*level+vec3(0,0,b.w)));
}
bool skyAt(vec2 at){float d=depthAt(at);return d>=0.0&&(d<=0.02||d>=p.grade.w);}
// God rays: march from the pixel toward the sun's screen position and count
// open sky; trees, ruins and terrain cut the light into shafts.
float sunShafts(vec2 sunUV){
 const int Steps=40;vec2 delta=(sunUV-uv)*(0.92/float(Steps));
 vec2 at=uv+delta*fract(sin(dot(gl_FragCoord.xy,vec2(12.9898,78.233)))*43758.5453);
 float sum=0.0,decay=1.0,weight=0.0;
 for(int i=0;i<Steps;++i){
  at+=delta;
  if(at.x>0.0&&at.y>0.0&&at.x<1.0&&at.y<1.0&&skyAt(at))sum+=decay;
  weight+=decay;decay*=0.965;
 }
 return sum/weight;
}
// Low steam: exponential height fog over the valley floor, broken into slowly
// drifting wisps, integrated in closed form along the view ray.
float steamAmount(vec3 dir,float rayLength){
 const float base=4.2,height=1.4,density=0.006;
 vec3 eye=lampGrid.eye.xyz;float time=lampGrid.eye.w;
 float L=min(rayLength,140.0),dz=dir.z*L;
 float column=abs(dz)>0.001?height*(1.0-exp(-dz/height))/dz:1.0;
 vec2 mid=(eye+dir*min(L,18.0)).xy;
 float wisp=cloudNoise(mid*0.09+vec2(time*0.05,time*0.02))*0.7+cloudNoise(mid*0.27-vec2(time*0.03,0.0))*0.5;
 float optical=density*exp(-(eye.z-base)/height)*column*L*(0.45+wisp);
 return 1.0-exp(-max(optical,0.0));
}
float postHash(vec2 v){uvec2 q=uvec2(ivec2(v)+ivec2(65536));uint h=q.x*1664525u+q.y*1013904223u+uint(lampGrid.eye.w*60.0)*2654435761u;h^=h>>16;h*=0x7feb352du;h^=h>>15;return float(h&0xffffffu)/16777216.0;}

// Interleaved gradient noise: a per-pixel dither with no frame term, so a still shot and a test
// frame are the same every time. It staggers where each pixel's march samples, so neighbouring rows
// of a floor do not all find a wall on the same step (the hard horizontal bands an even march draws);
// the refinement below then takes every pixel's hit to the same surface whichever step found it.
float stepDither(vec2 p){return fract(52.9829189*fract(dot(p,vec2(0.06711056,0.00583715))));}
// Screen-space reflections, for finishes smooth enough to mirror (roughness under LookB.x: water, wet
// tile, chrome, glass). Rougher ones take the room's probe, filtered down its mips, instead: a blurred
// march copies what it finds side by side (a doubled image), the probe is blurred by construction.
// The hit must lie on the surface it found (a thickness test, then a check after refining), and the
// result fades where the march is unsure: near the frame's edge, far along the ray, on rays turned
// back toward the eye. Misses outdoors fall back to the sky.
vec4 screenReflection(vec3 center,vec3 normal,float roughness){
 vec3 V=normalize(-center);
 vec3 R=reflect(-V,normal);
 if(dot(R,normal)<0.0)R=reflect(R,normal);
 // A ray coming back toward the eye can only meet the backs of what is on screen.
 float facing=smoothstep(-0.15,0.2,R.z);
 const int Steps=40;const float MaxDistance=16.0;
 vec3 origin=center+normal*(0.015+0.003*center.z);
 float jitter=stepDither(gl_FragCoord.xy),previous=0.0;
 if(facing>0.0)for(int i=0;i<Steps;++i){
  float t=MaxDistance*pow((float(i)+jitter)/float(Steps),1.8);
  vec3 q=origin+R*t;if(q.z<0.05)break;
  vec2 at=projectView(q);if(any(lessThan(at,vec2(0.0)))||any(greaterThan(at,vec2(1.0))))break;
  float scene=depthAt(at),thickness=0.12+0.03*q.z;
  if(scene>0.02&&q.z>scene&&q.z-scene<thickness){
   // Refine between the last miss and this hit, staying on the surface found (behind it, within its thickness).
   float lo=previous,hi=t;
   for(int k=0;k<8;++k){float mid=(lo+hi)*0.5;vec3 m=origin+R*mid;float sd=depthAt(projectView(m));if(sd>0.02&&m.z>sd&&m.z-sd<thickness)hi=mid;else lo=mid;}
   vec3 hit=origin+R*hi;at=projectView(hit);float sd=depthAt(at);
   // Still off the surface after refining: the ray passed behind a thin edge, not into a surface.
   float onSurface=1.0-smoothstep(0.02+0.008*hit.z,0.06+0.02*hit.z,abs(hit.z-sd));
   // A plane cannot mirror itself: a hit still on the reflecting surface's own plane is the ray
   // grazing along it and meeting it again through depth's steps (the smeared copies of a wall or a
   // floor drawn along it). It must stand off the plane.
   onSurface*=smoothstep(0.03+0.006*hit.z,0.08+0.015*hit.z,dot(hit-center,normal));
   if(onSurface<=0.0){previous=t;continue;}
   vec2 edge=min(at,1.0-at);float fade=smoothstep(0.0,0.1,min(edge.x,edge.y))*(1.0-smoothstep(0.55,1.0,hi/MaxDistance))*facing*onSurface;
   return vec4(texture(sceneImage,at).rgb,fade);
  }
  previous=t;
 }
 if(lampGrid.sun.w>0.0){
  vec4 bs=lampGrid.basis;float forward=R.z*bs.z-R.y*bs.w;
  vec3 worldR=normalize(vec3(-R.x*bs.y+forward*bs.x,R.x*bs.x+forward*bs.y,R.y*bs.z+R.z*bs.w));
  if(worldR.z>0.0)return vec4(proceduralSky(worldR),0.8);
 }
 return vec4(0.0);
}

void main(){
 vec4 source=texture(sceneImage,uv);
 vec3 color=source.rgb;float depth=source.a;
 // The material views (scene.frag --detail 13-16) are measurements: shown as they are.
 if(lampGrid.flags.y>=13&&lampGrid.flags.y<=17){outColor=vec4(color,depth);return;}
 // The viewmodel writes negative depth and keeps its own lighting.
 bool viewModel=depth<0.0;
 bool surface=depth>0.02&&depth<p.grade.w;
 // Pixels that blend the viewmodel with the world after MSAA resolve keep their own lighting.
 bool worldPixel=texture(materialImage,uv).a>0.99;
 // The denoised shadow stage (visibility, shaft rgb), filtered at most once per pixel.
 vec4 shadow=vec4(1.0,0.0,0.0,0.0);bool shadowReady=false;
 if(surface&&worldPixel&&p.camera.x!=0.0){
  vec4 occlusion;denoisedBoth(depth,occlusion,shadow);shadowReady=true;
  if(p.camera.x<0.0){outColor=vec4(vec3(occlusion.a),depth);return;}
  // Occlusion and screen-space lamp shadows act on the light they model:
  // AO (squared again, about the fourth power of the raw horizon term)
  // darkens only the indirect share, lamp shadows only the direct share.
  vec4 material=texture(materialImage,uv);float directShare=clamp(material.b,0.0,1.0);
  color*=mix(max(occlusion.a*occlusion.a,0.04),1.0,directShare);
  // One bounce: nearby lit surfaces tint and fill this one (colour bleeding).
  // Bounce is low-frequency light; within arm's reach the screen-space
  // estimate degenerates into shifted texture copies, so it fades out.
  color+=occlusion.rgb*0.22*occlusion.a*smoothstep(0.35,1.1,depth);
  color*=mix(1.0,shadow.x,directShare);
  // Reflections: dielectric Fresnel, faded out for rough materials.
  // r: roughness in its high five bits, the face's axis in the low three (scene.frag outMaterial).
  float packed=floor(material.r*255.0+0.5),roughness=floor(packed/8.0)/31.0;int axis=int(packed-8.0*floor(packed/8.0));
  // g carries a metal's metalness above .5, else half the surface's wetness (scene.frag).
  float metal=material.g>0.5?(material.g-0.5)*2.0:0.0,wet=material.g>0.5?0.0:material.g*2.0;
  // Water on a surface is a film over it: smooth whatever lies under it, a mirror at a grazing angle
  // and nothing head-on (F0 .02). A damp floor carries a little of it, a puddle all of it.
  float film=smoothstep(0.0,0.6,wet);
  // --detail 12: the finish as post reads it: hue the face's axis (grey sloped, black none), brightness
  // smoothness, blue the water film.
  if(lampGrid.flags.y==12){vec3 hue=axis==1||axis==2?vec3(1.0,0.3,0.3):axis==3||axis==4?vec3(0.3,1.0,0.3):axis==5||axis==6?vec3(0.4,0.5,1.0):axis==0?vec3(0.6):vec3(0.0);
   outColor=vec4(hue*(0.25+0.75*(1.0-roughness))*(axis==2||axis==4||axis==6?0.6:1.0)+vec3(0.0,0.0,film),depth);return;}
  // scene.frag lit every finish with the room's reflection probe (the split sum). On smooth finishes
  // (under LookB.x) the screen march refines it: where it finds the room on screen it takes the place of
  // what the probe gave, by the same specular weight; the probe stays wherever the march is unsure.
  float marchShare=1.0-smoothstep(LookB.x*0.6,LookB.x,roughness);
  if(marchShare>0.0||film>0.0){
   vec3 center=viewPosition(uv,depth),normal=viewNormal(uv,center);
   // The face's own axis when the scene pass gave one: the depth-built normal steps with half-float
   // depth, worst on a wall or floor seen at a grazing angle, and cut their reflections into bands and
   // stipple. A pixel on an edge blends two faces' codes, so the axis must face the eye and not oppose
   // the depth-built normal. Sloped faces keep the depth-built one.
   if(axis>=1&&axis<=6){vec3 w=vec3(axis==1?1.0:axis==2?-1.0:0.0,axis==3?1.0:axis==4?-1.0:0.0,axis==5?1.0:axis==6?-1.0:0.0);
    vec3 exact=normalize(viewDirection(w));if(dot(exact,normal)>0.0&&dot(exact,-center)>0.0)normal=exact;}
   vec3 toEye=normalize(-center);float NoV=max(dot(normal,toEye),0.0),grazing=pow(1.0-NoV,5.0);
   // Standing water lies on what faces up; a damp wall is not a mirror.
   film*=smoothstep(0.75,0.95,worldVector(normal).z);
   float filmStrength=min((0.02+0.98*grazing)*film*LookA.x,0.9);
   vec3 R=normalize(worldVector(reflect(-toEye,normal)));vec3 at=lampGrid.eye.xyz+worldVector(center);
   vec4 sharp=vec4(0.0);
   if(filmStrength>0.004){sharp=LookC.y>1.5?vec4(0.0):screenReflection(center,normal,0.0);
    if(sharp.a<0.999){float sky;vec4 probe=probeReflection(at,R,0.03,sky);
     if(probe.a>0.0){vec3 seen=lampGrid.sun.w>0.0&&R.z>0.0?mix(probe.rgb,proceduralSky(R),sky):probe.rgb;sharp=vec4(mix(seen,sharp.rgb,sharp.a),max(sharp.a,probe.a));}}}
   if(marchShare>0.0&&LookA.x>0.0&&LookC.y<1.5){
    vec4 hit=filmStrength>0.004?sharp:screenReflection(center,normal,0.0);
    float sky;vec4 probe=probeReflection(at,R,roughness,sky);
    if(hit.a>0.0&&probe.a>0.0){
     // F0: .04 a dielectric; a metal's from its own colour (its albedo is its F0).
     float l=dot(color,vec3(0.2126,0.7152,0.0722));vec3 tint=clamp(color/max(l,1e-4),vec3(0.0),vec3(2.0));
     vec2 AB=envBrdfApprox(NoV,roughness);vec3 F0=mix(vec3(0.04),tint*0.6,metal);
     vec3 weight=(F0*AB.x+AB.y)*marchShare*hit.a*probe.a*LookA.x;
     color=max(color+weight*(hit.rgb-probe.rgb),vec3(0.0));}}
   if(filmStrength>0.004)color=mix(color,sharp.rgb,filmStrength*sharp.a);
  }
 }
 // Auto exposure scales scene light; haze and shafts below are added in
 // display terms so a dark room is not filled with brightened fog.
 float exposure=texture(exposureImage,vec2(0.5)).r;
 color*=exposure;
 vec2 ndc=uv*2.0-1.0;
 vec3 rayView=vec3(ndc.x/p.camera.z,-ndc.y/(p.camera.z*p.camera.y),1.0);
 vec3 ray=normalize(rayView);
 float rayLength=surface?depth*length(rayView):viewModel?0.5:40.0;
 if(!surface&&!viewModel&&lampGrid.sun.w>0.0)color=proceduralSky(worldDirection(rayView));
 if(surface&&p.haze.w>0.0){
  // Aerial perspective: distance lifts toward the atmosphere colour.
  // Continuous from the eye: a 2 m dead zone made the look jump when walking up to walls.
  float amount=1.0-exp(-depth*p.haze.w*0.8);
  color=mix(color,p.haze.rgb,amount*0.25);
 }
 if(zoneSky()&&!viewModel&&lampGrid.sun.w>0.0){
  vec3 dir=worldDirection(rayView),sunDir=lampGrid.sun.xyz,sunCol=lampGrid.sunColor.rgb;
  float cosSun=max(dot(dir,sunDir),0.0);
  float steam=steamAmount(dir,surface?depth*length(rayView):400.0);
  vec3 steamColor=vec3(0.70,0.72,0.71)+sunCol*(0.08+0.35*pow(cosSun,6.0));
  color=mix(color,steamColor,steam*(surface?0.8:0.35));
  vec3 sunView=viewDirection(sunDir);
  if(sunView.z>0.05){
   float t;float sunCloud=zoneCloud(sunDir,t);
   vec2 sunUV=projectView(sunView);
   float shafts=sunShafts(sunUV);
   float lobe=0.25*pow(cosSun,3.0)+0.75*pow(cosSun,14.0);
   color+=sunCol*shafts*lobe*0.38*(1.0-0.75*sunCloud)*smoothstep(0.05,0.3,sunView.z);
  }
 }
 if(p.haze.w>0.0){
  // Lamps glow through the air they light; denser air glows more.
  if(lampGrid.flags.x!=0){
   // Shadow-mapped shafts from every nearby lamp and the torch, each in its own
   // colour (shadow stage, denoised here); 0.7 keeps the old amber's luminance.
   if(!shadowReady)shadow=denoisedShadow(depth);
   color+=shadow.gba*p.haze.w*0.7;
  }else{
   float scatter=0.0;for(int i=0;i<4;++i)scatter+=lampScatter(ray,rayLength,p.lights[i]);
   color+=vec3(1.0,0.86,0.66)*scatter*p.haze.w*0.4;
  }
 }
 color+=texture(bloomImage,uv).rgb*(p.frame.z/6.0)*exposure;
 // Grade in linear light: gentle saturation, cool lifted shadows, warm highs.
 float luminance=dot(color,vec3(0.2126,0.7152,0.0722));
 color=mix(vec3(luminance),color,p.frame.w);
 float shadowMix=1.0-clamp(luminance*4.0,0.0,1.0),highMix=clamp(luminance*1.5-0.4,0.0,1.0);
 // grade.x is a black point: haze and bounce must not lift the darkest tones.
 color=max(color-vec3(p.grade.x),vec3(0.0))*(1.0+p.grade.x*4.0)+vec3(0.55,0.75,1.0)*p.grade.z*shadowMix;
 color*=mix(vec3(1.0),vec3(1.0+p.grade.y,1.0+p.grade.y*0.45,1.0-p.grade.y*0.3),highMix);
 outColor=vec4(max(color,vec3(0.0)),depth);
}
