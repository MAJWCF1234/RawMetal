#version 450
// Shadow stage: screen-space lamp shadows from the scene's planar depth
// (alpha). Output: r = lamp visibility (1 lit, lower shadowed), gba = the
// coloured lamp and torch shafts. Jittered on the 4x4 Bayer grid; the compose
// stage's matching 4x4 filter denoises it.
layout(set=0,binding=0) uniform sampler2D sceneImage;
layout(push_constant) uniform PostParams {
 vec4 camera;   // x AO strength (negative: debug view), y aspect, z focal, w radius (m)
 vec4 haze;     // rgb linear haze colour, w density per metre
 vec4 frame;    // xy texel size, z bloom strength, w saturation
 vec4 grade;    // x shadow lift, y warm highlights, z cool shadows, w far depth
 vec4 lights[4];// view-space lamp positions (xyz) and power (w)
} p;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outColor;
// Per-pixel lamp grid and cube shadow maps (see scene.frag), for shafts.
// scatter.rgb is the torch's tone while it is on.
layout(std430,set=0,binding=1) readonly buffer LampGrid {vec4 origin;ivec4 size;ivec4 flags;vec4 eye;vec4 basis;vec4 sun;vec4 sunColor;vec4 cascades[12];vec4 flashes[4];vec4 flashTones[4];vec4 scatter;vec4 probeInfo;vec4 probeVolumes[16];vec4 reflections[144];uvec4 probeCells[288];vec4 lamps[512];uint cells[];} lampGrid;
layout(set=0,binding=2) uniform sampler2DArrayShadow lampShadows;
const vec3 ShadowFace[6]=vec3[6](vec3(1,0,0),vec3(-1,0,0),vec3(0,1,0),vec3(0,-1,0),vec3(0,0,1),vec3(0,0,-1));
const vec3 ShadowUp[6]=vec3[6](vec3(0,0,1),vec3(0,0,1),vec3(0,0,1),vec3(0,0,1),vec3(0,1,0),vec3(0,1,0));

float depthAt(vec2 at){return texture(sceneImage,clamp(at,vec2(0.0),vec2(1.0))).a;}
vec3 viewPosition(vec2 at,float depth){
 vec2 ndc=at*2.0-1.0;
 return vec3(ndc.x*depth/p.camera.z,-ndc.y*depth/(p.camera.z*p.camera.y),depth);
}
// Neighbours on the viewmodel (negative depth) or sky stand in as this surface.
float normalDepth(vec2 at,float fallback){float d=depthAt(at);return d>0.02?d:fallback;}
vec3 viewNormal(vec2 at,vec3 center){
 // Depth is stored at half precision (~11 bits): a 3-pixel baseline keeps the
 // quantisation steps from tilting normals into contour rings and stripes.
 vec2 t=p.frame.xy*3.0;
 vec3 r=viewPosition(at+vec2(t.x,0),normalDepth(at+vec2(t.x,0),center.z)),l=viewPosition(at-vec2(t.x,0),normalDepth(at-vec2(t.x,0),center.z));
 vec3 d=viewPosition(at+vec2(0,t.y),normalDepth(at+vec2(0,t.y),center.z)),u=viewPosition(at-vec2(0,t.y),normalDepth(at-vec2(0,t.y),center.z));
 vec3 dx=abs(r.z-center.z)<abs(center.z-l.z)?r-center:center-l;
 vec3 dy=abs(d.z-center.z)<abs(center.z-u.z)?d-center:center-u;
 vec3 n=normalize(cross(dy,dx));
 return dot(n,center)>0.0?-n:n;
}
// 4x4 Bayer rotation: the compose stage's 4x4 blur cancels the pattern exactly.
float bayer4(vec2 pixel){
 ivec2 q=ivec2(mod(pixel,4.0));
 const float m[16]=float[16](0.,8.,2.,10.,12.,4.,14.,6.,3.,11.,1.,9.,15.,7.,13.,5.);
 return (m[q.y*4+q.x]+0.5)/16.0;
}

vec2 projectView(vec3 q){return vec2(q.x*p.camera.z/q.z,-q.y*p.camera.z*p.camera.y/q.z)*0.5+0.5;}

// Screen-space lamp shadows: march toward each nearby lamp through the depth
// buffer. Anything in front of the ray (guards, creatures, props, walls the
// vertex bake missed) removes that lamp's estimated share of the light here.
float lampVisibility(vec3 center,vec3 normal,float jitter){
 float visibility=1.0;
 for(int i=0;i<4;++i){
  vec4 lamp=p.lights[i];if(lamp.w<=0.0)continue;
  vec3 toLamp=lamp.xyz-center;float dist=length(toLamp);if(dist<0.3||dist>9.0)continue;
  vec3 dir=toLamp/dist;float facing=dot(normal,dir);if(facing<=0.05)continue;
  // Same inverse-square family as the vertex lighting; share against fill.
  float direct=lamp.w*facing*3.2/(1.0+dist*dist*0.12);float share=direct/(direct+0.35);
  float reach=min(dist-0.3,4.0);float blocked=0.0;
  // Bias grows with distance: half-precision depth steps cast stripes otherwise.
  float lift=max(0.04,center.z*0.012);
  for(int s=0;s<16;++s){
   float t=0.08+(float(s)+jitter)/16.0*reach;
   vec3 q=center+normal*lift+dir*t;float bias=max(0.06,q.z*0.02);if(q.z<0.08)break;
   vec2 at=projectView(q);if(any(lessThan(at,vec2(0.0)))||any(greaterThan(at,vec2(1.0))))break;
   float sceneDepth=depthAt(at);
   if(sceneDepth<0.0)continue; // the viewmodel (negative depth) casts no lamp shadows
   // An occluder sits in front of the ray point, within a plausible thickness.
   if(sceneDepth<q.z-bias&&sceneDepth>q.z-0.9-bias){blocked=1.0;break;}
  }
  visibility*=1.0-blocked*share*0.85;
 }
 return visibility;
}

// View space (right, up, forward) back to world, using the scene camera.
vec3 worldFromView(vec3 v){
 vec4 b=lampGrid.basis;float forward=v.z*b.z-v.y*b.w;
 return lampGrid.eye.xyz+vec3(-v.x*b.y+forward*b.x,v.x*b.x+forward*b.y,v.y*b.z+v.z*b.w);
}
float shadowTap(int slot,vec3 d,float range){
 vec3 a=abs(d);
 int face=a.x>=a.y&&a.x>=a.z?(d.x>0.0?0:1):a.y>=a.z?(d.y>0.0?2:3):(d.z>0.0?4:5);
 vec3 forward=ShadowFace[face],up=ShadowUp[face];
 vec2 st=vec2(dot(d,cross(forward,up)),dot(d,up))/dot(d,forward)*0.5+0.5;
 return texture(lampShadows,vec4(st,float(slot*6+face),length(d)/range-0.004));
}
// A lamp record's final tone (blackbody x tint x gel), packed as half floats.
vec3 lampTone(vec4 params){vec2 rg=unpackHalf2x16(floatBitsToUint(params.z)),b=unpackHalf2x16(floatBitsToUint(params.w));return vec3(rg,b.x);}
// WorldLight::beam (see scene.frag): a ceiling fitting's shaft is a cone.
float lampBeam(vec4 params,vec3 away){
 // 16 and up: a switchable lamp (its power changes in play; SoftwareRenderer render).
 float code=unpackHalf2x16(floatBitsToUint(params.w)).y;if(code>=15.5)code-=16.0;
 if(code<1.5)return 1.0;
 if(code<3.0)return 0.2+0.8*smoothstep(0.0,1.0,(-away.z-0.42)/0.45);
 float yaw=code-4.0;return 0.12+0.88*smoothstep(0.0,1.0,(away.x*cos(yaw)+away.y*sin(yaw)+0.25)/0.75);
}
// Shadow-mapped single scattering in each lamp's own colour: light pours
// through gaps and stops at walls. The torch scatters too, in the same march:
// its cone from the eye, unshadowed, but the march ends at the visible surface
// so anything stepping into the beam cuts it. Jittered on the Bayer grid;
// compose denoises it with the lamp shadows.
vec3 lampShafts(float depth,float jitter){
 if(lampGrid.flags.x==0)return vec3(0.0);
 float far=min(depth<0.0?-depth:depth>0.02&&depth<p.grade.w?depth:30.0,24.0);
 vec3 ray=viewPosition(uv,1.0);
 vec3 eye=lampGrid.eye.xyz,direction=normalize(worldFromView(ray)-eye),forward=normalize(worldFromView(vec3(0.0,0.0,1.0))-eye);
 bool torch=dot(lampGrid.scatter.rgb,vec3(1.0))>0.0;
 const int Steps=16;vec3 sum=vec3(0.0);
 for(int s=0;s<Steps;++s){
  float t=far*(float(s)+jitter)/float(Steps);
  vec3 world=worldFromView(ray*t);
  if(torch){
   // scene.frag's torch cone and falloff, scattered back toward the eye. The
   // first half metre is skipped so thick haze never veils the whole lens.
   vec3 toPoint=world-eye;float d2=dot(toPoint,toPoint),d=sqrt(max(d2,1e-6)),along=dot(toPoint,forward)/d;
   if(along>0.80&&d2<196.0){
    float cone=clamp((along-0.80)/0.16,0.0,1.0);cone=cone*cone*(3.0-2.0*cone);
    float c=dot(direction,-toPoint/d),phase=0.75/pow(1.09-0.6*c,1.5);
    sum+=lampGrid.scatter.rgb*(cone*1.9/(1.0+d2*0.020)*phase*0.12*smoothstep(0.5,1.5,d));
   }
  }
  if(lampGrid.size.w==0)continue;
  ivec3 cell=ivec3(floor((world-lampGrid.origin.xyz)/lampGrid.origin.w));
  if(any(lessThan(cell,ivec3(0)))||any(greaterThanEqual(cell,lampGrid.size.xyz)))continue;
  int base=((cell.z*lampGrid.size.y+cell.y)*lampGrid.size.x+cell.x)*16;
  for(int k=0;k<4;++k){
   uint entry=lampGrid.cells[base+k];if(entry==0u)break;
   int index=int(entry)-1;vec4 lamp=lampGrid.lamps[index*2],params=lampGrid.lamps[index*2+1];
   vec3 toLamp=lamp.xyz-world;float d2=dot(toLamp,toLamp),r2=lamp.w*lamp.w;if(d2>=r2)continue;
   float fade=1.0-d2/r2,falloff=params.x*fade*fade/(0.35+d2*0.6)*lampBeam(params,-toLamp*inversesqrt(max(d2,1e-4)));
   float visible=params.y>=0.0?shadowTap(int(params.y),-toLamp,lamp.w):1.0;
   // Mild forward scattering (Henyey-Greenstein, g=0.3) toward the viewer.
   float c=dot(direction,toLamp*inversesqrt(max(d2,1e-4)));float phase=0.75/pow(1.09-0.6*c,1.5);
   sum+=lampTone(params)*(falloff*visible*phase);
  }
 }
 return sum*far/float(Steps)*length(ray);
}

void main(){
 float depth=depthAt(uv);float jitter=bayer4(gl_FragCoord.xy);
 // gba: the coloured shadowed shafts (compose adds them when lampGrid.flags.x is set).
 vec3 shafts=lampShafts(depth,jitter);
 if(depth<0.02||depth>=min(p.grade.w,60.0)){outColor=vec4(1.0,shafts);return;} // negative: viewmodel
 vec3 center=viewPosition(uv,depth);
 outColor=vec4(lampVisibility(center,viewNormal(uv,center),jitter),shafts);
}
