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
layout(std430,set=0,binding=5) readonly buffer LampGrid {vec4 origin;ivec4 size;ivec4 flags;vec4 eye;vec4 basis;vec4 sun;vec4 sunColor;vec4 cascades[12];} lampGrid;
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

// Lamp visibility from the shadow stage, filtered like the occlusion.
vec2 denoisedShadow(float depth){
 vec2 sum=vec2(0.0);float weight=0.0;
 vec2 base=(floor(uv/p.frame.xy-1.5)+0.5)*p.frame.xy;
 for(int y=0;y<4;++y)for(int x=0;x<4;++x){
  vec2 at=base+vec2(x,y)*p.frame.xy;
  float w=exp(-abs(texture(sceneImage,at).a-depth)/(0.04*depth+0.02));
  sum+=texture(shadowImage,at).rg*w;weight+=w;
 }
 return weight>0.0?sum/weight:vec2(1.0,0.0);
}
// Both filters at once: the same 16 depth taps and weights feed occlusion and
// lamp visibility, so a surface pixel reads the depth once instead of twice.
void denoisedBoth(float depth,out vec4 occlusion,out vec2 shadow){
 vec4 occlusionSum=vec4(0.0);vec2 shadowSum=vec2(0.0);float weight=0.0;
 vec2 base=(floor(uv/p.frame.xy-1.5)+0.5)*p.frame.xy;
 for(int y=0;y<4;++y)for(int x=0;x<4;++x){
  vec2 at=base+vec2(x,y)*p.frame.xy;
  float sampleDepth=texture(sceneImage,at).a;
  float w=exp(-abs(sampleDepth-depth)/(0.04*depth+0.02));
  occlusionSum+=texture(occlusionImage,at)*w;shadowSum+=texture(shadowImage,at).rg*w;weight+=w;
 }
 occlusion=weight>0.0?occlusionSum/weight:texture(occlusionImage,uv);
 shadow=weight>0.0?shadowSum/weight:vec2(1.0,0.0);
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

vec3 highlight(vec2 at){vec3 c=texture(sceneImage,clamp(at,vec2(0.0),vec2(1.0))).rgb;return max(c-vec3(0.85),vec3(0.0));}
vec3 bloom(){
 vec3 sum=vec3(0.0);float weight=0.0;
 for(int ring=1;ring<=3;++ring){
  float r=float(ring*ring)*5.0;float w=1.0/float(ring);
  for(int i=0;i<8;++i){float a=float(i)*0.7853982+float(ring)*0.39;sum+=highlight(uv+vec2(cos(a),sin(a))*p.frame.xy*r)*w;weight+=w;}
 }
 return sum/weight;
}

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

// Screen-space reflections. Roughness and wetness come from the material
// target; rough surfaces jitter the ray per pixel and per frame and TAA
// resolves the blur. Misses outdoors fall back to the sky.
vec4 screenReflection(vec3 center,vec3 normal,float roughness){
 vec3 V=normalize(-center);
 vec2 xi=vec2(postHash(gl_FragCoord.xy),postHash(gl_FragCoord.yx+vec2(17.0,3.0)));
 vec3 R=reflect(-V,normal);
 vec3 a=normalize(cross(abs(R.y)<0.9?vec3(0,1,0):vec3(1,0,0),R)),b=cross(R,a);
 float spread=roughness*roughness*0.9;float ang=xi.x*6.2831853,rad=sqrt(xi.y)*spread;
 R=normalize(R+(a*cos(ang)+b*sin(ang))*rad);
 if(dot(R,normal)<0.0)R=reflect(R,normal);
 const int Steps=24;const float MaxDistance=16.0;
 float jitter=postHash(gl_FragCoord.xy+vec2(91.0,57.0)),previous=0.0;
 for(int i=0;i<Steps;++i){
  float t=MaxDistance*pow((float(i)+jitter)/float(Steps),1.7);
  vec3 q=center+normal*0.02+R*t;if(q.z<0.05)break;
  vec2 at=projectView(q);if(any(lessThan(at,vec2(0.0)))||any(greaterThan(at,vec2(1.0))))break;
  float scene=depthAt(at);
  if(scene>0.02&&q.z>scene&&q.z-scene<0.25+0.04*q.z){
   // Refine between the last miss and this hit.
   float lo=previous,hi=t;
   for(int k=0;k<5;++k){float mid=(lo+hi)*0.5;vec3 m=center+normal*0.02+R*mid;vec2 mu=projectView(m);float sd=depthAt(mu);if(sd>0.02&&m.z>sd)hi=mid;else lo=mid;}
   at=projectView(center+normal*0.02+R*hi);
   vec2 edge=min(at,1.0-at);float fade=smoothstep(0.0,0.08,min(edge.x,edge.y))*(1.0-smoothstep(0.6,1.0,hi/MaxDistance));
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
 // The viewmodel writes negative depth and keeps its own lighting.
 bool viewModel=depth<0.0;
 bool surface=depth>0.02&&depth<p.grade.w;
 // Pixels that blend the viewmodel with the world after MSAA resolve keep their own lighting.
 bool worldPixel=texture(materialImage,uv).a>0.99;
 // The denoised shadow-stage pair, filtered at most once per pixel.
 vec2 shadow=vec2(1.0,0.0);bool shadowReady=false;
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
  float roughness=material.r;
  if(roughness<0.8){
   vec3 center=viewPosition(uv,depth),normal=viewNormal(uv,center);
   float NoV=max(dot(normal,normalize(-center)),0.0);
   float fresnel=0.04+0.96*pow(1.0-NoV,5.0);
   float strength=fresnel*(1.0-smoothstep(0.15,0.8,roughness))*(0.55+0.45*material.g);
   if(strength>0.004){vec4 reflection=screenReflection(center,normal,roughness);color=mix(color,reflection.rgb,strength*reflection.a);}
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
  // The shadow stage writes b=1 everywhere exactly when lampGrid.flags.x is set.
  if(lampGrid.flags.x!=0){
   // Shadow-mapped shafts from every nearby lamp (shadow stage, denoised here).
   if(!shadowReady)shadow=denoisedShadow(depth);
   color+=vec3(1.0,0.86,0.66)*shadow.y*p.haze.w*0.8;
  }else{
   float scatter=0.0;for(int i=0;i<4;++i)scatter+=lampScatter(ray,rayLength,p.lights[i]);
   color+=vec3(1.0,0.86,0.66)*scatter*p.haze.w*0.4;
  }
 }
 color+=bloom()*p.frame.z*exposure;
 // Grade in linear light: gentle saturation, cool lifted shadows, warm highs.
 float luminance=dot(color,vec3(0.2126,0.7152,0.0722));
 color=mix(vec3(luminance),color,p.frame.w);
 float shadowMix=1.0-clamp(luminance*4.0,0.0,1.0),highMix=clamp(luminance*1.5-0.4,0.0,1.0);
 // grade.x is a black point: haze and bounce must not lift the darkest tones.
 color=max(color-vec3(p.grade.x),vec3(0.0))*(1.0+p.grade.x*4.0)+vec3(0.55,0.75,1.0)*p.grade.z*shadowMix;
 color*=mix(vec3(1.0),vec3(1.0+p.grade.y,1.0+p.grade.y*0.45,1.0-p.grade.y*0.3),highMix);
 outColor=vec4(max(color,vec3(0.0)),depth);
}
