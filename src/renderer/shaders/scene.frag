#version 450
layout(set=0,binding=0) uniform sampler2D colorMap;
layout(set=0,binding=1) uniform sampler2D normalMap;
layout(set=0,binding=2) uniform sampler2D emissionMap;
layout(set=0,binding=3) uniform sampler2D reliefMap;
layout(push_constant) uniform ViewState { vec4 eyeYaw; vec4 basis; vec4 effects; vec4 fogLights[4]; vec4 atmosphere; } view;
layout(set=1,binding=0) uniform StereoProjection {mat4 clip;vec4 enabled;vec4 fogIntensity;vec4 darkness;vec4 fogTone[4];} stereo;
// Per-pixel lamps: a world-space grid lists up to 16 lamps per cell. Lamp i is
// lamps[2i] = position, range and lamps[2i+1] = intensity, shadow slot, tone.
// scatter.w is 1 when the shadow stage carries the lamp shafts this frame.
layout(std430,set=1,binding=2) readonly buffer LampGrid {vec4 origin;ivec4 size;ivec4 flags;vec4 eye;vec4 basis;vec4 sun;vec4 sunColor;vec4 cascades[12];vec4 flashes[4];vec4 flashTones[4];vec4 scatter;vec4 probeInfo;vec4 probeVolumes[16];vec4 reflections[144];vec4 lamps[512];uint cells[];} lampGrid;
layout(set=1,binding=5) uniform sampler3D probeAtlas;
layout(set=1,binding=6) uniform sampler2DArray reflectionAtlas;
// Soft particles: the room's planar depth (the scene target's alpha) once the room is drawn.
layout(set=1,binding=7) uniform sampler2D sceneDepth;
// The split-sum environment BRDF (Materials.h envBrdfTable): scale and bias of F0, by NoV (u) and
// roughness (v), height-correlated Smith GGX.
layout(set=1,binding=8) uniform sampler2D envBrdfMap;
// Where each resident map is wet (renderer/Wetness.h): a layer per probe slot, r wet, g puddles,
// b wall streaks, a the wet floor's height in tenths of a metre over the slot's volume origin.
layout(set=1,binding=9) uniform sampler2DArray wetAtlas;
// Three sun cascades (orthographic radial depth), compared in hardware.
layout(set=1,binding=4) uniform sampler2DArrayShadow sunShadows;
// Six radial-depth faces per lamp, compared in hardware with bilinear PCF.
layout(set=1,binding=3) uniform sampler2DArrayShadow lampShadows;
layout(location=0) in vec2 uv;
layout(location=1) in vec4 lighting;
layout(location=2) in vec4 light0;
layout(location=3) in vec4 light1;
layout(location=4) in vec4 surface;
// The open-air point that joins this face to its lamps' rooms: its height (scene.vert).
layout(location=5) in float bounceAir;
layout(location=6) in vec3 inWorldPos;
// The shaded point: the interpolated position, moved onto the recorded
// surface for Painterly Horizon LOD impostors.
vec3 worldPos;
layout(location=7) in float parallaxScale;
layout(location=8) in vec4 impostor;
layout(location=0) out vec4 outColor;
// Material target for the post pass: roughness, wetness, direct-light share.
layout(location=1) out vec4 outMaterial;

// Pixel-art filtering: snap to the texel centre and blend only across a
// one-pixel seam, so magnified PSX texels stay square without stair-stepping.
// Fades back to the original coordinate once a texel spans under a pixel.
vec2 crispTexelUV(vec2 coordinate,vec2 dx,vec2 dy){
 vec2 size=vec2(textureSize(colorMap,0));
 vec2 t=coordinate*size,footprint=max((abs(dx)+abs(dy))*size,vec2(0.00001));
 vec2 seam=floor(t+0.5);
 vec2 crisp=seam+clamp((t-seam)/footprint,-0.5,0.5);
 return mix(crisp,t,smoothstep(0.5,1.0,max(footprint.x,footprint.y)))/size;
}

const vec3 ShadowFace[6]=vec3[6](vec3(1,0,0),vec3(-1,0,0),vec3(0,1,0),vec3(0,-1,0),vec3(0,0,1),vec3(0,0,-1));
const vec3 ShadowUp[6]=vec3[6](vec3(0,0,1),vec3(0,0,1),vec3(0,0,1),vec3(0,0,1),vec3(0,1,0),vec3(0,1,0));
// d runs from the lamp to the (normal-offset) receiver. A 3x3 grid of
// bilinear comparisons gives a smooth 4x4-texel penumbra without noise.
float lampShadow(int slot,vec3 d,float range,float bias){
 vec3 a=abs(d);
 int face=a.x>=a.y&&a.x>=a.z?(d.x>0.0?0:1):a.y>=a.z?(d.y>0.0?2:3):(d.z>0.0?4:5);
 vec3 forward=ShadowFace[face],up=ShadowUp[face],right=cross(forward,up);
 float w=dot(d,forward);
 vec2 st=vec2(dot(d,right),dot(d,up))/w*0.5+0.5;
 vec2 texel=1.0/vec2(textureSize(lampShadows,0).xy);
 float reference=length(d)/range-bias,layer=float(slot*6+face),sum=0.0;
 for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)
  sum+=texture(lampShadows,vec4(clamp(st+vec2(x,y)*texel,texel*0.5,1.0-texel*0.5),layer,reference));
 return sum/9.0;
}

// One bilinear comparison: is a point in open air (no surface of its own) lit
// by this lamp? Bounce asks it of the room in front of a face.
float lampVisible(int slot,vec3 d,float range,float bias){
 vec3 a=abs(d);
 int face=a.x>=a.y&&a.x>=a.z?(d.x>0.0?0:1):a.y>=a.z?(d.y>0.0?2:3):(d.z>0.0?4:5);
 vec3 forward=ShadowFace[face],up=ShadowUp[face],right=cross(forward,up);
 vec2 st=vec2(dot(d,right),dot(d,up))/dot(d,forward)*0.5+0.5;
 vec2 texel=1.0/vec2(textureSize(lampShadows,0).xy);
 return texture(lampShadows,vec4(clamp(st,texel*0.5,1.0-texel*0.5),float(slot*6+face),min(length(d)/range-bias,0.995)));
}

// Close-up detail: a procedural grain layer in texture space, about four
// features per source texel. Like Source's detail textures it fades in once a
// texel spans several pixels, so magnified PSX art keeps micro-structure for
// the per-pixel lamps instead of collapsing into flat blocks.
// Integer lattice hash: a float-fraction hash at coordinates in the thousands
// evaluated shared corners through different rounding paths (FMA on one side)
// and tore seams into the noise at every cell edge.
float detailHash(ivec2 c){uvec2 q=uvec2(c+ivec2(65536));uint h=q.x*1664525u+q.y*1013904223u;h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;return float(h&0xffffffu)/16777216.0;}
float detailNoise(vec2 p){ivec2 i=ivec2(floor(p));vec2 f=fract(p);f=f*f*(3.0-2.0*f);
 return mix(mix(detailHash(i),detailHash(i+ivec2(1,0)),f.x),mix(detailHash(i+ivec2(0,1)),detailHash(i+ivec2(1,1)),f.x),f.y);}
// cellPixels: screen pixels per detail cell. Octaves finer than ~2 px fade out
// so the grain never sparkles in motion.
float detailField(vec2 p,float cellPixels){
 float w1=smoothstep(1.5,3.0,cellPixels/2.17),w2=smoothstep(1.5,3.0,cellPixels/4.9);
 return (detailNoise(p)*0.6+detailNoise(p*2.17+vec2(7.1,3.3))*0.28*w1+detailNoise(p*4.9+vec2(1.7,9.2))*0.12*w2)/(0.6+0.28*w1+0.12*w2);
}
// Puddle field in world metres: two octaves of lattice noise.
float puddleField(vec2 p){return detailNoise(p*0.45)*0.65+detailNoise(p*1.3+vec2(7.3,2.9))*0.35;}
// Whiteout blend of a detail normal onto a base tangent-space normal.
vec3 blendDetail(vec3 base,vec3 detail){return normalize(vec3(base.xy+detail.xy,base.z*detail.z));}

// Sun visibility from cascade c at world point w, 3x3 bilinear PCF.
float sunShadow(int c,vec3 w,float biasWorld){
 vec4 o=lampGrid.cascades[c*4],r=lampGrid.cascades[c*4+1],u=lampGrid.cascades[c*4+2],f=lampGrid.cascades[c*4+3];
 vec3 d=w-o.xyz;vec2 st=vec2(dot(d,r.xyz)*r.w,dot(d,u.xyz)*u.w)*0.5+0.5;
 if(any(lessThan(st,vec2(0.0)))||any(greaterThan(st,vec2(1.0))))return 1.0;
 float z=dot(d,f.xyz)*f.w*0.5+0.5-biasWorld*f.w*0.5;
 vec2 texel=1.0/vec2(textureSize(sunShadows,0).xy);float sum=0.0;
 for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)sum+=texture(sunShadows,vec4(st+vec2(x,y)*texel,float(c),z));
 return sum/9.0;
}

// Lamp colour from Planck's law: blackbody spectral radiance sampled at red,
// green and blue wavelengths (610, 550, 465 nm), normalised to unit luminance.
// tint pushes green (+) or magenta (-) for fluorescent and sodium tubes.
vec3 lampColour(float kelvin,float tint){
 const vec3 lambda=vec3(610e-9,550e-9,465e-9);const float c2=1.4388e-2; // hc/k in m*K
 vec3 radiance=1.0/(pow(lambda,vec3(5.0))*(exp(c2/(lambda*max(kelvin,800.0)))-1.0));
 radiance*=vec3(1.0,1.0+tint*0.35,1.0-tint*0.1);
 return radiance/max(dot(radiance,vec3(0.2126,0.7152,0.0722)),1e-30);
}

// A lamp record's final tone (blackbody x tint x gel, WorldLight::tone), packed
// on the CPU as half floats: z = red, green and w = blue, 1.
vec3 lampTone(vec4 params){vec2 rg=unpackHalf2x16(floatBitsToUint(params.z)),b=unpackHalf2x16(floatBitsToUint(params.w));return vec3(rg,b.x);}
// WorldLight::beam: the share of a lamp's light leaving toward `away` (unit,
// lamp to receiver), by the beam code packed beside its blue channel.
float lampBeam(vec4 params,vec3 away){
 // 16 and up: a switchable lamp (its power changes in play; SoftwareRenderer render).
 float code=unpackHalf2x16(floatBitsToUint(params.w)).y;if(code>=15.5)code-=16.0;
 if(code<1.5)return 1.0;
 if(code<3.0)return 0.2+0.8*smoothstep(0.0,1.0,(-away.z-0.42)/0.45);
 float yaw=code-4.0;return 0.12+0.88*smoothstep(0.0,1.0,(away.x*cos(yaw)+away.y*sin(yaw)+0.25)/0.75);
}


// Light probes (LightProbes.h, GpuRenderer probeInfo). The irradiance atlas holds each resident
// map's grid of ambient cubes: a 24-row band per map, six 24-wide face blocks (+x, -x, +y, -y,
// +z, -z) across, one layer per cell of height. rgb is irradiance over pi, a sky openness.
const float ProbeGain=3.1; // LightProbes.h ProbeGain
// The bounce's soft ceiling (LightProbes.h probeShoulder, LookTuning.h lookFillCeiling), in the look's values.
#define ProbeCeiling (LookC.x>0.0?LookC.x:0.62)
#define FillScale (lampGrid.probeInfo.w>0.0?lampGrid.probeInfo.w:1.0)
// The look's values (LookTuning.h; post.frag names them): LookB.z the fill's saturation, LookC.z a
// crystal's glow.
#define LookA lampGrid.reflections[141]
#define LookB lampGrid.reflections[142]
#define LookC lampGrid.reflections[143]
// The fill arriving at a surface at p facing n (unit), read half a cell out along n so a face never
// reads the probes behind it; a<0 where no map's grid covers p.
vec4 probeIrradiance(vec3 p,vec3 n){
 int count=int(lampGrid.probeInfo.x);
 for(int i=0;i<count;++i){
  vec4 a=lampGrid.probeVolumes[2*i],b=lampGrid.probeVolumes[2*i+1];
  vec3 local=p-a.xyz;
  if(local.x<0.0||local.y<0.0||local.x>=24.0||local.y>=24.0||local.z<-2.0||local.z>b.y*a.w+2.0)continue;
  vec3 q=local+n*vec3(0.5,0.5,0.5*a.w);
  vec3 f=clamp(vec3(q.x-0.5,q.y-0.5,q.z/a.w-0.5),vec3(0.0),vec3(23.0,23.0,b.y-1.0))+0.5;
  vec3 base=vec3(f.x/144.0,(f.y+24.0*b.x)/192.0,f.z/48.0);
  vec3 w=n*n;
  vec4 sx=texture(probeAtlas,base+vec3(n.x>=0.0?0.0:24.0/144.0,0.0,0.0));
  vec4 sy=texture(probeAtlas,base+vec3(n.y>=0.0?48.0/144.0:72.0/144.0,0.0,0.0));
  vec4 sz=texture(probeAtlas,base+vec3(n.z>=0.0?96.0/144.0:120.0/144.0,0.0,0.0));
  return sx*w.x+sy*w.y+sz*w.z;
 }
 return vec4(-1.0);
}
// The wetness mask at p (map-local metres through the probe volume's origin): wet, puddle, streak and
// the wet floor's height; dry (and no floor) where no resident map's mask covers p.
vec4 wetMask(vec3 p){
 int count=int(lampGrid.probeInfo.x);
 for(int i=0;i<count;++i){vec4 a=lampGrid.probeVolumes[2*i],b=lampGrid.probeVolumes[2*i+1];vec2 local=p.xy-a.xy;
  if(local.x<0.0||local.y<0.0||local.x>=24.0||local.y>=24.0)continue;
  vec4 t=texture(wetAtlas,vec3(local/24.0,b.x));return vec4(t.rgb,a.z+t.a*25.5);}
 return vec4(0.0,0.0,0.0,-1e9);
}
// Reflection probes: six 90-degree faces per room probe, captured by the scene pass (face order and
// orientation as GpuRenderer captureReflections draws them), a mip chain for rough surfaces.
// The mip a GGX lobe of roughness r reads: the box mip whose width matches the reflected lobe's
// half-width (about 1.3 alpha radians, alpha = r^2) on a 96-texel, 90-degree face, so the lobe's bright
// core shows (a lamp in a floor) and its tail blurs; LookA.w shifts it for a capture's A/B.
// The same in post.frag, so the screen march replaces exactly what this pass took from the probe.
float probeLod(float r){float a=r*r;return clamp(log2(max(80.0*a,1e-4))+LookA.w,0.0,lampGrid.probeInfo.z-1.0);}
const vec3 CaptureForward[6]=vec3[6](vec3(1,0,0),vec3(-1,0,0),vec3(0,1,0),vec3(0,-1,0),vec3(0,0,1),vec3(0,0,-1));
const vec3 CaptureRight[6]=vec3[6](vec3(0,1,0),vec3(0,-1,0),vec3(-1,0,0),vec3(1,0,0),vec3(0,1,0),vec3(0,1,0));
const vec3 CaptureUp[6]=vec3[6](vec3(0,0,1),vec3(0,0,1),vec3(0,0,1),vec3(0,0,1),vec3(-1,0,0),vec3(1,0,0));
// What the room's probe sees along R from p: inside the room's box the ray is carried to the box
// wall it meets first and that point looked up from the probe (parallax), so a floor reflects the
// wall across from it, not the one behind the probe. a is the probe's weight (1 inside its box,
// fading over 6 m outside it); sky is the share of the texel that saw no surface (outdoors).
int probeBest(vec3 p){
 int count=int(lampGrid.probeInfo.y);int best=-1;float bestScore=1e30;
 for(int i=0;i<count;++i){vec4 c=lampGrid.reflections[3*i],lo=lampGrid.reflections[3*i+1],hi=lampGrid.reflections[3*i+2];
  vec3 outside=max(max(lo.xyz-p,p-hi.xyz),vec3(0.0)),d=p-c.xyz;float score=dot(d,d)*0.05+dot(outside,outside)*4.0;
  if(score<bestScore){bestScore=score;best=i;}}
 return best;
}
vec4 probeSample(int best,vec3 p,vec3 R,float lod,out float sky){
 sky=0.0;
 vec4 c=lampGrid.reflections[3*best],lo=lampGrid.reflections[3*best+1],hi=lampGrid.reflections[3*best+2];
 float away=length(max(max(lo.xyz-p,p-hi.xyz),vec3(0.0)));
 // As post.frag: a point on (or a hair outside) the box's face is carried just inside it.
 vec3 dir=R;
 if(away<0.5){vec3 q=clamp(p,lo.xyz+0.02,hi.xyz-0.02);vec3 r=R+vec3(1e-5);vec3 t=max((hi.xyz-q)/r,(lo.xyz-q)/r);dir=q+R*max(min(min(t.x,t.y),t.z),0.0)-c.xyz;}
 vec3 a=abs(dir);int face=a.x>=a.y&&a.x>=a.z?(dir.x>0.0?0:1):a.y>=a.z?(dir.y>0.0?2:3):(dir.z>0.0?4:5);
 float z=max(dot(dir,CaptureForward[face]),1e-5);
 vec2 st=clamp(vec2(0.5+0.5*dot(dir,CaptureRight[face])/z,0.5-0.5*dot(dir,CaptureUp[face])/z),vec2(0.0),vec2(1.0));
 vec4 s=textureLod(reflectionAtlas,vec3(st,c.w*6.0+float(face)),lod);
 sky=smoothstep(300.0,900.0,s.a);
 return vec4(s.rgb,clamp(1.0-away/6.0,0.0,1.0));
}
vec4 probeReflection(vec3 p,vec3 R,float roughness,out float sky){sky=0.0;int best=probeBest(p);return best<0?vec4(0.0):probeSample(best,p,R,probeLod(roughness),sky);}

vec3 toLinear(vec3 c){return pow(max(c,vec3(0.0)),vec3(2.2));}
vec3 toDisplay(vec3 c){return pow(max(c,vec3(0.0)),vec3(1.0/2.2));}

vec3 filmic(vec3 c){
 c=max(c,vec3(0.0));
 return clamp((c*(2.51*c+0.03))/(c*(2.43*c+0.59)+0.14),vec3(0.0),vec3(1.0));
}

// GGX dielectric BRDF: rough paint retains a neutral grazing reflection.
float dielectricSpecular(vec3 N,vec3 V,vec3 L,float roughness){
 float NoL=max(dot(N,L),0.0),NoV=max(dot(N,V),0.001);
 if(NoL<=0.0)return 0.0;
 vec3 halfVector=L+V;
 vec3 H=halfVector*inversesqrt(max(dot(halfVector,halfVector),0.000001));
 float NoH=max(dot(N,H),0.0),VoH=max(dot(V,H),0.0);
 float a=roughness*roughness,a2=a*a;
 float denominator=NoH*NoH*(a2-1.0)+1.0;
 float D=a2/(3.14159265*denominator*denominator);
 float gv=NoL*sqrt(NoV*NoV*(1.0-a2)+a2);
 float gl=NoV*sqrt(NoL*NoL*(1.0-a2)+a2);
 float visibility=0.5/max(gv+gl,0.00001);
 float F=0.04+0.96*pow(1.0-VoH,5.0);
 return F*D*visibility*NoL;
}

// The metallic/roughness model (Karis 2013, Lagarde & de Rousiers 2014): GGX with alpha = r^2 (a2 its
// square), height-correlated Smith visibility, Schlick Fresnel from F0 (.04 a dielectric, the albedo a
// metal). Returns the specular radiance per unit light: F D Vis NoL.
vec3 specularGGX(vec3 N,vec3 V,vec3 L,float a2,vec3 F0){
 float NoL=dot(N,L);if(NoL<=0.0)return vec3(0.0);float NoV=max(dot(N,V),0.001);
 vec3 H=normalize(L+V);float NoH=max(dot(N,H),0.0),VoH=max(dot(V,H),0.0);
 float d=NoH*NoH*(a2-1.0)+1.0,D=a2/(3.14159265*d*d);
 float gv=NoL*sqrt(NoV*NoV*(1.0-a2)+a2),gl=NoV*sqrt(NoL*NoL*(1.0-a2)+a2);
 vec3 F=F0+(1.0-F0)*pow(1.0-VoH,5.0);
 return F*(D*0.5/max(gv+gl,1e-5)*NoL);
}
// The split-sum table at (NoV, roughness): F0 * x + y is the lobe's directional albedo.
vec2 envBRDF(float NoV,float r){const float n=float(textureSize(envBrdfMap,0).x);return texture(envBrdfMap,(vec2(clamp(NoV,0.0,1.0),clamp(r,0.0,1.0))*(n-1.0)+0.5)/n).rg;}
// Specular antialiasing, in alpha^2: the filtered normal map's shortening is unresolved variance
// (Toksvig), and the normal's change across the pixel footprint widens the lobe (Tokuyoshi &
// Kaplanyan 2021: 2 sigma^2 (|dN/dx|^2 + |dN/dy|^2), sigma^2 = .25, clamped at .18), so a highlight on
// fine relief or a curved edge never sparkles or streaks.
float filteredAlpha2(float r,float normalLength,vec3 N){
 float a=r*r,a2=a*a;float c=clamp(normalLength,0.05,1.0);a2+=(1.0-c)/c*0.5;
 vec3 nx=dFdx(N),ny=dFdy(N);a2+=min(0.5*(dot(nx,nx)+dot(ny,ny)),0.18);
 return clamp(a2,1e-6,1.0);
}
// Specular occlusion from ambient occlusion and roughness (Lagarde & de Rousiers 2014).
float specularOcclusion(float NoV,float ao,float r){return clamp(pow(NoV+ao,exp2(-16.0*r-1.0))-1.0+ao,0.0,1.0);}

float shaftScattering(vec3 eye,vec3 direction,float rayLength,vec4 lamp,float intensity){
 float height=lamp.z-lamp.w;
 if(height<=0.0||intensity<=0.0)return 0.0;
 float start=0.0,finish=rayLength;
 // Intersect the ray with a conservative cylinder and the actual vertical
 // extent. The old rectangular interval included empty space outside the
 // shaft, and failed completely when looking straight along a ceiling light.
 vec2 toLamp=lamp.xy-eye.xy;
 float horizontal2=dot(direction.xy,direction.xy);
 if(horizontal2>0.000001){
  float along=dot(toLamp,direction.xy)/horizontal2;
  vec2 closest=toLamp-direction.xy*along;
  float remaining=1.21-dot(closest,closest);
  if(remaining<=0.0)return 0.0;
  float halfSpan=sqrt(remaining/horizontal2);
  start=max(start,along-halfSpan);finish=min(finish,along+halfSpan);
 }else if(dot(toLamp,toLamp)>=1.21)return 0.0;
 if(abs(direction.z)>0.000001){
  float lower=(lamp.w-eye.z)/direction.z,upper=(lamp.z-eye.z)/direction.z;
  start=max(start,min(lower,upper));finish=min(finish,max(lower,upper));
 }else if(eye.z<=lamp.w||eye.z>=lamp.z)return 0.0;
 if(finish<=start)return 0.0;

 float stepLength=(finish-start)/8.0;
 float jitter=fract(52.9829189*fract(dot(gl_FragCoord.xy,vec2(0.06711056,0.00583715))));
 float sum=0.0;

 for(int i=0;i<8;++i){
  vec3 p=eye+direction*(start+(float(i)+jitter)*stepLength);
  float h=(p.z-lamp.w)/height;
  if(h<=0.0||h>=1.0)continue;

  float r=mix(1.10,0.30,h);
  float d=length(p.xy-lamp.xy);
  if(d>r)continue;

  float core=1.0-smoothstep(r*0.25,r,d);
  float ends=smoothstep(0.0,0.12,h)*smoothstep(0.0,0.06,1.0-h);
  sum+=core*ends;
 }
 vec3 lightToEye=eye-vec3(lamp.xy,lamp.z);
 float towardEye=max(0.0,dot(-direction,lightToEye*inversesqrt(max(dot(lightToEye,lightToEye),0.000001))));
 // Medium density controls extinction; source power controls radiance.
 // Neither can create a bright opaque shaft in an empty or unlit volume.
 float opticalDepth=sum*stepLength*view.atmosphere.w;
 return (1.0-exp(-opticalDepth))*(0.55+1.30*towardEye*towardEye)*intensity;
}

// Painterly Horizon LOD: the four recorded views around the eye direction
// (two azimuth columns, two elevation rows), each reprojected along the
// current view ray by its recorded depth so overlapping branches shift
// against each other (parallax) instead of cross-fading flat pictures.
// light0.xyz is the eye direction in the model frame, light1.x the radius.
vec3 impostorNormal=vec3(0.0,0.0,1.0);float impostorDepth=0.0;
vec2 impostorCell(vec2 card,vec2 index,vec2 cell){return (index+clamp(vec2(card.x*0.5+0.5,0.5-card.y*0.5),vec2(0.004),vec2(0.996)))*cell;}
// Far away (coarse) only the nearest view is read, without the relief step:
// at that size parallax is invisible and four lookups would cost more than
// the meshes they replace.
vec4 impostorColor(vec2 local,vec2 dx,vec2 dy,bool coarse){
 float packed=impostor.z,rows=mod(packed,16.0),columns=floor(packed/16.0+0.001);
 vec2 cell=vec2(1.0/columns,1.0/rows),gx=dx*cell,gy=dy*cell;
 vec3 d=normalize(light0.xyz),right=vec3(-d.y,d.x,0.0);right=length(right)>1e-4?normalize(right):vec3(1.0,0.0,0.0);
 vec3 up=cross(right,-d),q=right*(local.x*2.0-1.0)+up*(1.0-local.y*2.0);
 float a0=floor(impostor.x),fa=impostor.x-a0,e0=floor(impostor.y),fe=impostor.y-e0;
 if(coarse){fa=fa<0.5?0.0:1.0;fe=fe<0.5?0.0:1.0;}
 vec4 sum=vec4(0.0);vec3 normalSum=vec3(0.0);float depthSum=0.0,weightSum=0.0;
 for(int k=0;k<4;++k){
  float w=((k&1)!=0?fa:1.0-fa)*((k>>1)!=0?fe:1.0-fe);if(w<0.001)continue;
  vec2 index=vec2(mod(a0+float(k&1),columns),min(e0+float(k>>1),rows-1.0));
  float azimuth=index.x/columns*6.2831853,elevation=0.0698+index.y*0.4887;
  vec3 di=vec3(cos(elevation)*cos(azimuth),cos(elevation)*sin(azimuth),sin(elevation));
  vec3 ri=normalize(vec3(-di.y,di.x,0.0)),ui=cross(ri,-di);
  // One relief step: depth where the ray crosses this view's card, then the
  // point on the current ray at that depth, seen from this view.
  float h=coarse?0.0:textureGrad(reliefMap,impostorCell(vec2(dot(q,ri),dot(q,ui)),index,cell),gx,gy).r*2.0-1.0;
  vec3 p=q+d*h;vec2 at=impostorCell(vec2(dot(p,ri),dot(p,ui)),index,cell);
  vec4 c=textureGrad(colorMap,at,gx,gy);
  sum+=c*w;weightSum+=w;
  if(c.a>0.5){normalSum+=textureGrad(normalMap,at,gx,gy).xyz*w;depthSum+=(textureGrad(reliefMap,at,gx,gy).r*2.0-1.0)*w;}
 }
 sum/=max(weightSum,1e-4);
 if(dot(normalSum,normalSum)>1e-6)impostorNormal=normalize(normalSum);
 impostorDepth=depthSum/max(weightSum,1e-4);
 return sum;
}
float lodDither(vec2 pixel){return fract(52.9829189*fract(dot(floor(pixel),vec2(0.06711056,0.00583715))));}

void main(){
 worldPos=inWorldPos;
 // surface.y: emission scale; 100 and up a resonance crystal (its emission scale above 100);
 // below 0 a finish with no emission: tenths of polish (how sharply it mirrors the room) in the
 // whole part of -y, metalness in the fraction.
 bool crystal=surface.y>=99.5;float emissionScale=crystal?surface.y-100.0:max(surface.y,0.0);
 float metal=surface.y<0.0?fract(-surface.y):0.0,polish=surface.y<0.0?floor(-surface.y)*0.1:0.0;
 outMaterial=vec4(1.0,0.0,0.5,1.0);
 // Painterly Horizon LOD crossfade: the real mesh and its impostor take
 // complementary halves of one screen dither across the band, so the swap
 // neither ghosts nor doubles density.
 bool impostorQuad=parallaxScale<=-1000.0;
 float lodDistance=impostorQuad?-(parallaxScale+1000.0):parallaxScale<0.0?-parallaxScale:0.0;
 if(lodDistance>0.0){
  float eyeDistance=length(worldPos-view.eyeYaw.xyz),handover=smoothstep(lodDistance*0.85,lodDistance*1.15,eyeDistance),h=lodDither(gl_FragCoord.xy);
  if(impostorQuad){if(h>=handover||(impostor.w>0.0&&h<smoothstep(impostor.w*0.8,impostor.w,eyeDistance)))discard;}
  else if(h<handover)discard;
 }
 vec2 sampleUV=uv;
 vec2 uvDx=dFdx(uv),uvDy=dFdy(uv);
 vec3 dx=dFdx(worldPos),dy=dFdy(worldPos);

 float materialDistance=length(worldPos-view.eyeYaw.xyz);
 // Magnification: screen pixels per source texel. Past a few pixels the relief
 // map and texel snapping become flat blocks that jump at texel edges, so they
 // fade out and the detail layer fades in.
 vec2 atlasSize=vec2(textureSize(colorMap,0));
 vec2 texelFootprint=(abs(uvDx)+abs(uvDy))*atlasSize;
 float pixelsPerTexel=1.0/max(max(texelFootprint.x,texelFootprint.y),0.0001);
 float closeup=smoothstep(2.5,9.0,pixelsPerTexel);
 // Detail feeds the per-pixel lamps, so it follows the same switch.
 bool staticWorld=lampGrid.flags.x!=0&&lampGrid.flags.y!=0&&surface.z<0.5&&surface.w>0.5&&surface.w<1.5;
 float detailAmount=staticWorld?closeup:0.0;
 float parallaxFade=(1.0-smoothstep(8.0,18.0,materialDistance))*(1.0-smoothstep(4.0,9.0,pixelsPerTexel));
 bool parallaxMaterial=parallaxScale>0.0&&surface.z<0.5&&parallaxFade>0.001;
 // lighting.w: 1 a normal map, +2 a surface map (roughness, metalness, cavity in the relief slot),
 // the fraction a corpse's dissolve.
 float mapFlags=floor(lighting.w+0.25);bool normalMapped=mapFlags>0.5&&mod(mapFlags,2.0)>0.5,surfaceMapped=mapFlags>1.5&&!impostorQuad;
 bool litMaterial=normalMapped||light0.w+light1.w>0.0||detailAmount>0.001;
 vec3 tangent=vec3(0.0),bitangent=vec3(0.0),geometricNormal=vec3(0.0);
 bool tangentFrameValid=false;
 if(parallaxMaterial||litMaterial){
  float uvScale=max(max(abs(uvDx.x),abs(uvDx.y)),max(abs(uvDy.x),abs(uvDy.y)));
  vec2 frameDx=uvScale>0.0?uvDx/uvScale:vec2(0.0),frameDy=uvScale>0.0?uvDy/uvScale:vec2(0.0);
  float determinant=frameDx.x*frameDy.y-frameDx.y*frameDy.x;
  // The UV Jacobian shrinks as a surface fills more pixels. An absolute
  // epsilon disabled valid GGX/parallax frames on close walls and at higher
  // resolutions. Reject collapsed UV axes by their relative angle instead.
  float uvAreaScale=dot(frameDx,frameDx)*dot(frameDy,frameDy);
  if(uvAreaScale>0.0&&determinant*determinant>uvAreaScale*0.00000001){
   tangent=normalize((dx*frameDy.y-dy*frameDx.y)*sign(determinant));
   vec3 rawBitangent=(dy*frameDx.x-dx*frameDy.x)*sign(determinant);
   bitangent=normalize(rawBitangent-tangent*dot(tangent,rawBitangent));
   geometricNormal=normalize(cross(tangent,bitangent));
   tangentFrameValid=true;
  }
 }

 if(parallaxMaterial&&tangentFrameValid){
   // Single-sample offset mapping with offset limiting. The old 4-layer march
   // divided by the viewing angle and stacked ghost copies of the texture at
   // grazing angles, which swam as the camera turned. Fade it out at grazing
   // angles, where the derivative tangent frame is least stable.
   vec3 toEye=normalize(view.eyeYaw.xyz-worldPos);
   float grazingFade=smoothstep(0.3,0.65,abs(dot(toEye,geometricNormal)));
   vec2 direction=vec2(dot(toEye,tangent),dot(toEye,bitangent));
   float height=textureGrad(reliefMap,uv,uvDx,uvDy).r;
   sampleUV=uv+direction*(0.5-height)*parallaxScale*parallaxFade*grazingFade;
 }

 if(stereo.enabled.y>0.5)sampleUV=mix(crispTexelUV(sampleUV,uvDx,uvDy),sampleUV,smoothstep(5.0,11.0,pixelsPerTexel));
 vec4 color=impostorQuad?impostorColor(uv,uvDx,uvDy,materialDistance>lodDistance*2.0):textureGrad(colorMap,sampleUV,uvDx,uvDy);
 // The impostor's visible surface lies in front of or behind its card.
 if(impostorQuad)worldPos+=normalize(view.eyeYaw.xyz-worldPos)*impostorDepth*light1.x;
 // A particle card (Particles.cpp, surface code 5): its atlas read smooth (never snapped to texels),
 // on toward its flipbook's next frame (light1: blend, step), its own opacity (light0.x), faded where
 // it nears what was drawn behind it (over light0.y metres of the room's planar depth, when the frame
 // has it: light0.z), lit by its corners as the CPU found the room's light there, the probes' colour
 // in the share of it they carry.
 if(surface.z>=4.5&&surface.z<5.5){
  vec4 card=textureGrad(colorMap,uv,uvDx,uvDy);
  if(light1.x>0.0)card=mix(card,textureGrad(colorMap,uv+vec2(light1.y,0.0),uvDx,uvDy),light1.x);
  vec3 toCard=worldPos-view.eyeYaw.xyz;float ahead=toCard.x*view.eyeYaw.w+toCard.y*view.basis.x;
  float cardDepth=max(ahead*view.basis.y+toCard.z*view.basis.z,0.0);
  float a=card.a*light0.x;
  if(light0.z>0.5&&light0.y>0.0){float behind=texelFetch(sceneDepth,ivec2(gl_FragCoord.xy),0).a;if(behind>0.0)a*=clamp((behind-cardDepth)/light0.y,0.0,1.0);}
  if(a<0.003)discard;
  vec3 lit=card.rgb*lighting.x*lighting.y*0.96/(1.0+surface.x*0.018);
  if(lighting.x<1.5){
   vec4 fill=probeIrradiance(worldPos,normalize(-toCard));
   if(fill.w>=0.0){vec3 light=ProbeCeiling*(1.0-exp(-fill.rgb*ProbeGain*FillScale/ProbeCeiling));float luma=dot(light,vec3(0.2126,0.7152,0.0722));
    if(luma>1e-4)lit*=mix(vec3(1.0),light/luma,clamp(luma/max(lighting.y,0.02),0.0,1.0));}
  }
  outMaterial=vec4(1.0,0.0,0.5,1.0);
  outColor=view.effects.w>0.5?vec4(max(lit,vec3(0.0)),a):vec4(toDisplay(filmic(lit*0.95)),a);
  return;
 }
 if(color.a<(surface.z>0.5?0.01:0.5))discard;
 // A corpse dissolving: the fraction rides above the normal-map flag in lighting.w.
 float dissolveFade=clamp((fract(lighting.w+0.25)-0.25)/0.45,0.0,1.0);
 if(dissolveFade>0.003&&lodDither(gl_FragCoord.xy)<dissolveFade)discard;
 // Wetness: a damp sheen everywhere plus puddles on level static ground.
 // Wet porous surfaces darken and their roughness collapses toward a mirror.
 // The material: per texel from the surface map (Materials.h), else the finish's single value.
 vec4 surfaceTexel=surfaceMapped?textureGrad(reliefMap,sampleUV,uvDx,uvDy):vec4(0.5,0.0,0.0,1.0);
 float materialRoughness=surfaceMapped?max(surfaceTexel.g,0.02):clamp(1.0-fract(surface.z),0.18,1.0),wetness=0.0;
 float metalness=surfaceMapped?surfaceTexel.b:metal,cavity=surfaceMapped?surfaceTexel.a:1.0,smudge=1.0;
 // Grunge: two octaves of noise in world space (on the face's dominant plane) move every finish's
 // roughness by up to a fifth either way, smudges and wear at room and hand scale, so even a flat
 // paint or a pack's tiny palette texture never draws one even highlight.
 if(surface.z<0.5&&surface.w<1.5&&!impostorQuad){vec3 an=abs(cross(dx,dy));vec2 gp=an.z>=max(an.x,an.y)?worldPos.xy:an.x>=an.y?worldPos.yz:worldPos.xz;
  float g=detailNoise(gp*1.7)*0.65+detailNoise(gp*6.1+vec2(3.7,1.3))*0.35;materialRoughness=clamp(materialRoughness*(1.0+0.4*(g-0.5)),0.02,1.0);
  // Smudges and dust dull the sheen in patches: the whole specular, lamps' and room's alike.
  // A texture with no surface map of its own (a flat paint, a palette) takes it the harder.
  smudge=mix(surfaceMapped?0.85:0.5,1.0,smoothstep(0.25,0.75,detailNoise(gp*3.3+vec2(9.1,4.7))));}
 // Resonance crystal reads as mineral: the body's flat cyan pulled toward a deep cold grey-teal and
 // lit like polished stone; the glow is added below, concentrated in the cores.
 if(crystal){float l=dot(color.rgb,vec3(0.2126,0.7152,0.0722));color.rgb=mix(vec3(l),color.rgb,0.45)*vec3(0.5,0.7,0.8);}
 // A liquid: a blended surface with near-mirror gloss reflects like a puddle.
 bool liquid=surface.z>=3.0&&surface.z<4.0;
 if(liquid){materialRoughness=0.04;wetness=1.0;}
 // Wetness is local (renderer/Wetness.h): a floor is wet only where the map's mask says (round its
 // water, under its leaks, at its drains, its authored WET patches, a damp map's seeps), on the floor
 // the mask was made for (not a deck, a table or a crate over it), with puddles only in the low spots
 // inside a wet patch; outdoors the open sky takes the rain (sunColor.w) in patches. A wall is wet only
 // in streaks under a source. Wet porous surfaces darken; the film's mirror is post.frag's.
 float wetDebug=0.0;
 if(surface.z<0.5&&surface.w>0.5&&surface.w<1.5){
  vec3 Nw=normalize(cross(dx,dy));if(dot(Nw,view.eyeYaw.xyz-worldPos)<0.0)Nw=-Nw;
  vec4 wm=wetMask(worldPos);float rain=lampGrid.sunColor.w;
  if(Nw.z>0.8){
   float wet=wm.r*(1.0-smoothstep(0.35,0.6,abs(worldPos.z-wm.a))),pool=wm.g;
   if(rain>0.0){vec4 sky=probeIrradiance(worldPos,vec3(0.0,0.0,1.0));float open=sky.w>=0.0?sky.a:1.0;
    float patches=smoothstep(0.35,0.6,puddleField(worldPos.xy*0.6+vec2(3.1,1.7)));
    wet=max(wet,rain*smoothstep(0.55,0.85,open)*mix(0.45,1.0,patches));pool=max(pool,rain*open*0.5);}
   float puddle=smoothstep(0.7,0.78,puddleField(worldPos.xy)*0.7+pool*0.5)*smoothstep(0.35,0.7,wet);
   wetness=max(wet*0.4,puddle);wetDebug=wet;
  }else if(abs(Nw.z)<0.3&&wm.b>0.0){
   // Streaks: thin vertical runs, densest low on the wall, under the source.
   float along=dot(worldPos.xy,normalize(vec2(-Nw.y,Nw.x)));
   float run=smoothstep(0.45,0.8,detailNoise(vec2(along*9.0,worldPos.z*0.6)));
   wetness=wm.b*run*(1.0-smoothstep(0.2,3.0,worldPos.z-max(wm.a,worldPos.z-3.0)))*0.6;wetDebug=wetness;
  }
  color.rgb*=mix(1.0,0.6,wetness*0.85*(1.0-metalness));
 }
 // Water on a surface is a film over it (post.frag mirrors the room in it by its own Fresnel): the
 // material under it keeps its roughness for the room's light; the lamps' highlights sharpen on the film.
 float dryRoughness=materialRoughness;
 materialRoughness=mix(materialRoughness,0.03,wetness);
 vec3 detailNormal=vec3(0.0,0.0,1.0);float detailValue=0.5;
 if(detailAmount>0.001){
  vec2 dp=uv*atlasSize*2.5;const float e=0.3;float cellPixels=pixelsPerTexel*0.4;
  float h=detailField(dp,cellPixels),hx=detailField(dp+vec2(e,0.0),cellPixels),hy=detailField(dp+vec2(0.0,e),cellPixels);
  detailValue=h;color.rgb*=1.0+(h-0.5)*0.22*detailAmount;
  detailNormal=normalize(vec3(-(hx-h)/e*0.7*detailAmount,-(hy-h)/e*0.7*detailAmount,1.0));
 }

 float vertexLight=lighting.y;
 float specularLight=0.0;
 float waterFresnel=0.0;
 vec3 sunLight=vec3(0.0); // coloured direct sun on static geometry
 float directLight=-1.0,ambientLight=0.0; // for the post pass (AO on indirect only)
 vec3 lampOut=vec3(0.0),lampSpecularOut=vec3(0.0); // coloured lamp light on static geometry
 vec3 bounceOut=vec3(0.0); // lamp light returned by the room (indirect, under AO)
 vec3 sunDebug=vec3(0.0);
 vec3 n=vec3(0.0,0.0,1.0);float normalLength=1.0;vec3 baseNormalDebug=vec3(0.0,0.0,1.0);
 // The shading normal (world), the lobe's alpha^2 after specular antialiasing, the lamps' and sun's
 // highlights, the room's light as the fallback environment, and F0: .04 a dielectric, the albedo
 // (brought to its measured reflectance when the texture loaded) a metal.
 vec3 shadeN=vec3(0.0,0.0,1.0);bool shadeReady=false;float specAlpha2=1.0;vec3 specDirect=vec3(0.0),ambientEnv=vec3(0.0);
 vec3 F0=mix(vec3(0.04),color.rgb,metalness);

 if(normalMapped||light0.w+light1.w>0.0){
  vec4 filteredNormal=normalMapped?textureGrad(normalMap,sampleUV,uvDx,uvDy):vec4(0.0,0.0,1.0,1.0);
  normalLength=length(filteredNormal.xyz);
  n=normalLength>0.000001?filteredNormal.xyz/normalLength:vec3(0.0,0.0,1.0);baseNormalDebug=n;
  if(detailAmount>0.001)n=blendDetail(n,detailNormal);
  float response=0.22+light0.w*max(0,dot(n,light0.xyz))+light1.w*max(0,dot(n,light1.xyz));
  vertexLight*=clamp(response/lighting.z,0.35,1.8);

  float gloss=fract(surface.z);

  if(tangentFrameValid){
    vec3 normal=normalize(cross(tangent,bitangent));
    vec3 eyeDirection=normalize(view.eyeYaw.xyz-worldPos);
    vec3 viewTangent=normalize(vec3(dot(eyeDirection,tangent),dot(eyeDirection,bitangent),abs(dot(eyeDirection,normal))));
    float roughness=clamp(1.0-gloss,0.18,0.96);
    vec3 dnX=dFdx(n),dnY=dFdy(n);
    // RGB stores the unnormalized first moment at every mip. Hardware filters
    // those moments together, preserving both mean direction and unresolved
    // variance through bilinear, trilinear and anisotropic sampling.
    float concentration=clamp(normalLength,0.0,1.0);
    float normalVariance=(1.0-concentration)/max(concentration,0.05);
    roughness=sqrt(clamp(roughness*roughness+normalVariance+0.35*(dot(dnX,dnX)+dot(dnY,dnY)),0.0324,1.0));

    if(light0.w>0.0){
     specularLight+=light0.w*dielectricSpecular(n,viewTangent,light0.xyz,roughness);
    }

    if(light1.w>0.0){
     specularLight+=light1.w*dielectricSpecular(n,viewTangent,light1.xyz,roughness);
    }

    if(surface.z>1.5){
     float NdotV=clamp(viewTangent.z,0.0,1.0);
     waterFresnel=0.04+0.96*pow(1.0-NdotV,5.0);
    }
  }
 }

 // Static world surfaces: the CPU bakes ambient and contact occlusion per vertex;
 // lamps and the flashlight are lit here per pixel with shadow maps.
 if(lampGrid.flags.x!=0&&surface.w>0.5&&surface.w<1.5&&surface.z<0.5&&lighting.x<1.5){
  vec3 V=normalize(view.eyeYaw.xyz-worldPos);
  vec3 Ng=normalize(cross(dx,dy));if(dot(Ng,V)<0.0)Ng=-Ng;
  // A canopy is lit like a rounded mass, not like the flat card drawing it.
  if(impostorQuad){float c=cos(light1.y),s=sin(light1.y);Ng=normalize(vec3(impostorNormal.x*c+impostorNormal.y*s,-impostorNormal.x*s+impostorNormal.y*c,impostorNormal.z));}
  // Foliage meshes (the real side of an impostor pair): thin two-sided cards
  // light as a soft rounded mass, mostly up and a little toward the eye.
  else if(lodDistance>0.0)Ng=normalize(Ng*0.35+vec3(0.0,0.0,0.85)+V*0.3);
  vec3 N=Ng;
  if((normalMapped||detailAmount>0.001)&&tangentFrameValid){
   vec3 T=normalize(tangent-Ng*dot(Ng,tangent));
   vec3 B=bitangent-Ng*dot(Ng,bitangent);B=normalize(B-T*dot(T,B));
   N=normalize(T*n.x+B*n.y+Ng*max(n.z,0.05));
  }
  specAlpha2=filteredAlpha2(materialRoughness,normalLength,N);
  vec3 lampLight=vec3(0.0),lampSpecularRGB=vec3(0.0),bounceLight=vec3(0.0),sunSpecular=vec3(0.0);
  // The map's light probes carry the room's return when they cover this point (each lamp's flat
  // bounce below is the fallback while a streamed map's probes are still baking).
  vec4 probe=probeIrradiance(worldPos,N);bool probed=probe.w>=0.0;
  // Bounce (WorldLight::bounce): each lamp's room returns a share of its light
  // to every face in it, whichever way the face turns. The room is reached
  // through the open air in front of the face, lifted toward its ceiling on the
  // CPU (bounceAir); the lamp must see that point, so walls and decks stop it.
  vec3 bouncePoint=worldPos+Ng*0.3;if(bounceAir>-1.0e8)bouncePoint.z=bounceAir;
  ivec3 cell=ivec3(floor((worldPos-lampGrid.origin.xyz)/lampGrid.origin.w));
  if(lampGrid.size.w>0&&all(greaterThanEqual(cell,ivec3(0)))&&all(lessThan(cell,lampGrid.size.xyz))){
   int base=((cell.z*lampGrid.size.y+cell.y)*lampGrid.size.x+cell.x)*16;
   for(int k=0;k<16;++k){
    uint entry=lampGrid.cells[base+k];if(entry==0u)break;
    int index=int(entry)-1;vec4 lamp=lampGrid.lamps[index*2],params=lampGrid.lamps[index*2+1];
    vec3 toLamp=lamp.xyz-worldPos;float d2=dot(toLamp,toLamp),r2=lamp.w*lamp.w;
    if(d2>=r2||d2<0.0001)continue;
    // Same falloff as WorldLight::falloff and the CPU bake.
    float t=1.0-d2/r2,falloff=params.x*3.2*t*t/(1.0+d2*0.12);
    float bounce=params.x*0.2*t*t;
    // The probes hold the fixed lamps' return as it really falls (colour, corners, the far side of
    // a pillar), so they carry most of it; each lamp's flat share stays under them at a fifth, so a
    // big hall the bake sees little of still fills. A switchable lamp (code 16 and up) keeps its whole
    // share at this frame's power, so a breaker or a failing tube is never baked in.
    if(probed&&unpackHalf2x16(floatBitsToUint(params.w)).y<15.5)bounce*=0.2;
    if(bounce>0.002){
     vec3 q=bouncePoint-lamp.xyz;float texelQ=length(q)*2.0/float(textureSize(lampShadows,0).x);
     bounceLight+=lampTone(params)*bounce*(params.y>=0.0?lampVisible(int(params.y),q,lamp.w,texelQ*2.0/lamp.w):1.0);
    }
    float distance=sqrt(d2);vec3 L=toLamp/distance;
    float geometric=dot(Ng,L);if(geometric<=0.0)continue;
    falloff*=lampBeam(params,-L);
    float shadow=1.0;
    if(params.y>=0.0&&falloff>0.003){
     // Slope-scaled bias in shadow texels: a texel covers about 2d/256 m at
     // distance d, and a receiver at a grazing angle spans several texels of depth.
     float texelWorld=distance*2.0/float(textureSize(lampShadows,0).x);
     float slope=clamp(sqrt(max(1.0-geometric*geometric,0.0))/max(geometric,0.05),0.0,8.0);
     vec3 receiver=worldPos+Ng*texelWorld*(1.0+slope)+L*texelWorld*0.5;
     shadow=lampShadow(int(params.y),receiver-lamp.xyz,lamp.w,texelWorld*(1.0+0.5*slope)/lamp.w);
    }
    if(shadow<=0.0)continue;
    // Soft terminator: normal maps never cut a lit face to black.
    vec3 tone=lampTone(params);
    lampLight+=tone*falloff*shadow*(0.04+0.96*max(dot(N,L),0.0))*smoothstep(0.0,0.12,geometric);
    lampSpecularRGB+=tone*falloff*shadow*specularGGX(N,V,L,specAlpha2,F0)*smoothstep(0.0,0.12,geometric);
   }
  }
  if(view.effects.x>0.5){
   vec3 forwardWorld=normalize(vec3(view.eyeYaw.w*view.basis.y,view.basis.x*view.basis.y,view.basis.z));
   vec3 toPoint=worldPos-view.eyeYaw.xyz;float d2=dot(toPoint,toPoint);
   if(d2>0.04&&d2<196.0){
    float d=sqrt(d2);vec3 L=-toPoint/d;float along=dot(toPoint,forwardWorld)/d;
    if(along>0.80){
     float cone=clamp((along-0.80)/0.16,0.0,1.0);cone=cone*cone*(3.0-2.0*cone);
     float beam=cone*1.9/(1.0+d2*0.020);
     // Torch: a cool white LED.
     vec3 torch=lampColour(5600.0,0.0);
     lampLight+=torch*beam*(0.30+0.70*max(dot(N,L),0.0));
     lampSpecularRGB+=torch*beam*specularGGX(N,V,L,specAlpha2,F0);
    }
   }
  }
  // Transient lights (the player's muzzle flash, then NPC guns in darkness, sparks
  // and arcs, each in its own tone and reach), the vertex formula per pixel: a wall
  // or ceiling quad spanning the room still lights. Unshadowed, lifted 1.6x to
  // match the shoulder's low-end slope.
  // A reflection probe's capture (effects.z the capture's clock, GpuRenderer::CaptureClock) holds the
  // room, not this moment's flashes.
  for(int flash=0;flash<5&&view.effects.z>-1.0e5;++flash){
   float strength=flash==0?view.effects.y:lampGrid.flashes[flash-1].w;if(strength<=0.0)continue;
   vec3 forwardWorld=normalize(vec3(view.eyeYaw.w*view.basis.y,view.basis.x*view.basis.y,view.basis.z));
   vec3 muzzle=flash==0?view.eyeYaw.xyz+forwardWorld*0.42-vec3(0,0,0.10):lampGrid.flashes[flash-1].xyz;
   float reach=flash==0?12.0:lampGrid.flashTones[flash-1].w,r2=reach*reach;
   vec3 toPoint=worldPos-muzzle;float d2=dot(toPoint,toPoint);if(d2>=r2)continue;
   float d=max(sqrt(d2),0.001),edge=1.0-d2/r2;
   lampLight+=(flash==0?lampColour(3000.0,0.0):lampGrid.flashTones[flash-1].rgb)*strength*1.6*1.4*edge*edge*(0.35+0.65*abs(dot(Ng,toPoint))/d)/(1.0+d2*stereo.darkness.y);
  }
  float hemisphere=1.0;
  if(lampGrid.sun.w>0.0){
   vec3 L=lampGrid.sun.xyz;float geometric=dot(Ng,L);
   if(geometric>0.0){
    int cascade=surface.x<7.0?0:surface.x<22.0?1:2;float texelWorld=lampGrid.cascades[cascade*4].w;
    float slope=clamp(sqrt(max(1.0-geometric*geometric,0.0))/max(geometric,0.05),0.0,6.0);
    float visible=sunShadow(cascade,worldPos+Ng*texelWorld*(1.5+slope)+L*texelWorld,texelWorld*(1.0+0.5*slope));
    float terminator=smoothstep(0.0,0.1,geometric);
    sunLight=lampGrid.sunColor.rgb*lampGrid.sun.w*visible*(0.04+0.96*max(dot(N,L),0.0))*terminator;
    sunDebug=vec3(visible,max(dot(N,L),0.0),float(cascade)/3.0+0.2);
    sunSpecular+=lampGrid.sunColor.rgb*lampGrid.sun.w*visible*specularGGX(N,V,L,specAlpha2,F0)*terminator;
   }
   // Sky ambient: the flat outdoor bake becomes a hemisphere term.
   hemisphere=0.62+0.38*max(N.z,0.0);
  }
  // Ambient keeps a little of the normal map's relief in unlit areas.
  float relief=mix(1.0,clamp(dot(N,Ng),0.0,1.0),0.6)*hemisphere;
  // Exposure-matched to the old bake (which clamped at 1.4): a soft shoulder
  // keeps hot spots under lamps without flattening the falloff into the room.
  // Soft shoulder at 2.4: maps authored for the old 1.4-clamped bake overlap
  // many lamps (Intake sums to ~10x ambient); unbounded, they flattened rooms
  // and drowned each lamp's shadows in its neighbours' light. Cores stay hot.
  lampOut=2.4*(1.0-exp(-lampLight*0.62/2.4));
  // Its own soft shoulder: a room of many lamps fills, it never floods.
  // A crystal stands in its own glow: the probes round it carry that glow, which must not light
  // the crystal itself flat; it takes only a little of it.
  if(probed){vec3 fill=probe.rgb*ProbeGain*FillScale*(crystal?0.12:1.0);float fl=dot(fill,vec3(0.2126,0.7152,0.0722));bounceLight+=max(vec3(fl)+(fill-vec3(fl))*LookB.z,vec3(0.0));}
  bounceOut=ProbeCeiling*(1.0-exp(-bounceLight/ProbeCeiling))*relief;
  // Outdoors the sky's fill follows how much sky the probes saw (under a roof, in a ruin).
  vertexLight=lighting.y*relief*(probed&&lampGrid.sun.w>0.0?0.45+0.55*probe.a:1.0);
  // The lamps' highlights go through the same soft shoulder as their diffuse light (its slope here).
  float lampIn=dot(lampLight,vec3(0.2126,0.7152,0.0722)),shoulder=lampIn>1e-4?dot(lampOut,vec3(0.2126,0.7152,0.0722))/lampIn:0.62;
  specDirect=lampSpecularRGB*shoulder+sunSpecular*0.96;specularLight=0.0;
  shadeN=N;shadeReady=true;ambientEnv=vertexLight*vec3(0.78,0.88,1.12)+bounceOut;
  float lampLuma=dot(lampOut,vec3(0.2126,0.7152,0.0722));
  directLight=lampLuma+dot(sunLight,vec3(0.333));ambientLight=lighting.y*relief+dot(bounceOut,vec3(0.2126,0.7152,0.0722));
 }

 if(surface.z>0.5&&surface.z<1.5){
  vec3 additive=color.rgb*color.a*lighting.x*vec3(1,0.72,0.35);
  outColor=vec4(view.effects.w>0.5?toLinear(additive):additive,1);
  return;
 }

 // Image-based lighting (the split sum, Karis 2013): the room's reflection probe along R at the
 // lobe's mip, times the table's F0 * scale + bias; the multiple scattering a single lobe loses restored
 // (Turquin 2019) so a rough surface keeps its energy; occluded in pits and seams (Lagarde's specular
 // occlusion from the surface map's cavity) and below the surface's horizon. The diffuse takes what
 // the lobe does not reflect (1 - E); a metal has none.
 bool shadedOpaque=surface.z<0.5&&surface.w<1.5&&lighting.x<1.5&&!crystal;
 vec3 specIBL=vec3(0.0);float diffuseShare=1.0,iblRoughness=dryRoughness;
 // (A blended surface conserves what it lets through: white in the furnace.)
 float debugSpecular=0.0,debugFurnace=surface.z>1.5?1.0:0.0,debugReflectance=0.0;
 if(shadedOpaque){
  vec3 V=normalize(view.eyeYaw.xyz-worldPos);vec3 Ng=normalize(cross(dx,dy));if(dot(Ng,V)<0.0)Ng=-Ng;
  vec3 N=shadeN;
  if(!shadeReady){N=Ng;
   if(normalMapped&&tangentFrameValid){vec3 T=normalize(tangent-Ng*dot(Ng,tangent));vec3 B=bitangent-Ng*dot(Ng,bitangent);B=normalize(B-T*dot(T,B));N=normalize(T*n.x+B*n.y+Ng*max(n.z,0.05));}
   specAlpha2=filteredAlpha2(materialRoughness,normalLength,N);ambientEnv=vec3(vertexLight);}
  // The room's light takes the dry finish (the water film's mirror is post.frag's), with the same
  // antialiasing width added.
  float a2=clamp(specAlpha2-pow(materialRoughness,4.0)+pow(dryRoughness,4.0),1e-6,1.0);iblRoughness=sqrt(sqrt(a2));
  float NoV=max(dot(N,V),1e-3);vec2 AB=envBRDF(NoV,iblRoughness);
  float E1=max(AB.x+AB.y,0.01);vec3 comp=1.0+F0*(1.0/E1-1.0);
  vec3 R=reflect(-V,N);float horizon=clamp(1.0+dot(R,Ng),0.0,1.0);horizon*=horizon;
  vec3 weight=(F0*AB.x+AB.y)*comp*specularOcclusion(NoV,cavity,iblRoughness)*horizon*smudge;
  // What the lobe sees. A probe's capture never reads the atlas it is filling (no feedback, no order).
  vec3 env;
  // The room's reflection probe, parallax-corrected to its box, at the lobe's mip. Where no probe
  // covers a point the lobe sees the room's ambient times a room's mean albedo (LookA.z): the ambient
  // is light arriving, what a lobe sees is light leaving the room's surfaces.
  // (A probe's first capture reads none; its second, CaptureBounce, reads the first round.)
  {float sky=0.0;vec4 refl=(view.effects.z>-1.0e5||view.effects.z<-1.5e6)&&LookC.w>0.0?probeReflection(worldPos,R,iblRoughness,sky):vec4(0.0);
   vec3 fill=ambientEnv*LookA.z;
   env=mix(fill,mix(refl.rgb,fill,sky),refl.a);}
  specIBL=env*weight*LookC.w;
  diffuseShare=(1.0-metalness)*(1.0-(0.04*AB.x+AB.y));
  // The lamps' lobes lose the same multiple scattering.
  specDirect*=comp*smudge;
  debugSpecular=dot(specDirect+specIBL,vec3(0.2126,0.7152,0.0722));
  // White furnace: a white material in a uniform white room, no lamps (metal F0 = 1).
  {vec3 Fw=mix(vec3(0.04),vec3(1.0),metalness);debugFurnace=dot((1.0-metalness)*(1.0-(0.04*AB.x+AB.y))+(Fw*AB.x+AB.y)*(1.0+Fw*(1.0/E1-1.0)),vec3(0.2126,0.7152,0.0722));}
  debugReflectance=metalness<0.1&&NoV>0.9?dot((F0*AB.x+AB.y)*comp,vec3(0.2126,0.7152,0.0722)):0.0;
 }

 // Dielectrics return approximately four percent through the specular lobe.
 // Diffuse atlases use sRGB images: hardware decodes each source texel before
 // bilinear/trilinear filtering. Decoding the already-filtered sample twice
 // would crush both midtones and minified material detail.
 // Per-pixel scenes: ambient is a cool dim fill, lamps carry their own colour.
 vec3 ambientTone=lampGrid.flags.x!=0&&directLight>=0.0&&lampGrid.sun.w<=0.0?vec3(0.78,0.88,1.12):vec3(1.0);
 vec3 albedo=color.rgb*diffuseShare;
 vec3 result=albedo*lighting.x*(vertexLight*ambientTone+lampOut+bounceOut)*0.96/(1+surface.x*0.018);
 result+=albedo*lighting.x*sunLight*0.96;

 if(surface.z<0.5&&surface.w<1.5&&directLight<0.0){
  float key=clamp((vertexLight-0.24)/0.75,0.0,1.0);
  result*=mix(vec3(0.82,0.88,0.97),vec3(1.04,0.98,0.90),key);
 }
 // Lit on the CPU (actors, the hands, a vertex-lit bake): its brightness already holds the probes'
 // fill; the share of it that came from them takes their colour (crystal cyan, an emergency red).
 if(directLight<0.0&&surface.z<0.5&&lighting.x<1.5){
  vec3 facing=normalize(cross(dx,dy));if(dot(facing,view.eyeYaw.xyz-worldPos)<0.0)facing=-facing;
  vec4 fill=probeIrradiance(worldPos,facing);
  if(fill.w>=0.0){vec3 light=ProbeCeiling*(1.0-exp(-fill.rgb*ProbeGain*FillScale/ProbeCeiling));float luma=dot(light,vec3(0.2126,0.7152,0.0722));
   if(luma>1e-4)result*=mix(vec3(1.0),light/luma,clamp(luma/max(lighting.x*vertexLight,0.02),0.0,1.0));}
 }

 // The vertex-lit highlights (two baked lamp directions) carry a metal's F0 for the dielectric's .04.
 result+=vec3(specularLight*lighting.x)*mix(vec3(1.0),F0*25.0,metalness)+lampSpecularOut*lighting.x;
 result+=specDirect*lighting.x+specIBL*0.96/(1.0+surface.x*0.018);

 if(surface.z>1.5){
  vec3 skySheen=toLinear(vec3(0.55,0.72,0.92));
  result+=skySheen*waterFresnel*0.22;
 }
 // Glass mirrors the room: its reflection probe, by Fresnel, makes a pane read even where nothing
 // lies behind it to see.
 float reflectAlpha=0.0;
 if(surface.z>1.5&&!liquid){vec3 Ng=normalize(cross(dx,dy)),V=normalize(view.eyeYaw.xyz-worldPos);if(dot(Ng,V)<0.0)Ng=-Ng;
  float sky;vec4 mirror=probeReflection(worldPos,reflect(-V,Ng),0.06,sky);
  if(mirror.a>0.0){float F=0.04+0.96*pow(1.0-max(dot(Ng,V),0.0),5.0);reflectAlpha=clamp(F*1.1+0.08,0.0,0.85)*mirror.a;result=mix(result,mirror.rgb,reflectAlpha);}
 }

 if(emissionScale>0.0){
  // Emission atlases and their mip chains already contain linear radiance.
  vec3 glow=textureGrad(emissionMap,sampleUV,uvDx,uvDy).rgb*1.6*emissionScale;
  if(crystal){
   // The light is inside the stone: strongest looking into a prism's core, falling off toward its
   // edges, where the cold rim and the room's reflection take over; the hottest cores go toward
   // white. A few facets catch the eye and glint as it moves.
   vec3 Ng=normalize(cross(dx,dy)),V=normalize(view.eyeYaw.xyz-worldPos);if(dot(Ng,V)<0.0)Ng=-Ng;
   float NoV=max(dot(Ng,V),0.0),l=dot(glow,vec3(0.2126,0.7152,0.0722));
   float core=NoV*NoV;glow=(mix(vec3(l),glow,0.85)*mix(0.1,1.0,core)+vec3(l*l*0.3)*core*core)*0.72*LookC.z;
   float rim=pow(1.0-NoV,3.0);result+=vec3(0.16,0.42,0.56)*rim*(0.12+0.5*l);
   vec2 cell=floor(sampleUV*vec2(textureSize(colorMap,0))/6.0);float glint=fract(sin(dot(cell,vec2(12.9898,78.233))+floor(dot(V,vec3(7.1,5.3,3.7))*3.0))*43758.5453);
   result+=vec3(0.75,0.95,1.0)*smoothstep(0.985,1.0,glint)*(0.4+l)*NoV;
  }
  result+=glow;
 }

 // Fixture lighting is already visibility-tested in vertexLight and the two
 // tangent-space lights. A second unshadowed lamp loop leaked through walls
 // and incorrectly dotted tangent-space normals with world-space light vectors.

 if(surface.z<0.5&&surface.w<1.5&&view.atmosphere.w>0.0){
  float distance=length(worldPos-view.eyeYaw.xyz);
  float density=view.atmosphere.w;
  float heightDensity=exp(-max(worldPos.z*0.06,0.0));
  float nearWeight=smoothstep(0.75,7.0,distance);
  float extinction=(1.0-exp(-distance*density*mix(0.80,1.20,heightDensity)))*nearWeight;
  vec3 rayDir=(worldPos-view.eyeYaw.xyz)/max(distance,0.001);
  // Under a real sun the air scatters a little more toward and away from it;
  // indoors there is none, so the phase is even (the old curve's mean).
  float cosTheta=dot(rayDir,lampGrid.sun.xyz);
  float phase=lampGrid.sun.w>0.0?0.90+0.10*cosTheta*cosTheta:0.9333;
  result=mix(result,toLinear(view.atmosphere.rgb)*phase,min(extinction,0.35));
 }

 // Analytic shafts: only the fallback (VR, no post stage, or per-pixel lamps off).
 // Otherwise the shadow stage's shadow-mapped shafts carry them, never both.
 if(surface.z<0.5&&surface.w<1.5&&view.atmosphere.w>0.0&&materialDistance>=0.05&&(lampGrid.flags.x==0||lampGrid.scatter.w<0.5)){
  float scatter=0.0;vec3 tinted=vec3(0.0);
  vec3 direction=(worldPos-view.eyeYaw.xyz)/materialDistance;
  for(int light=0;light<4;++light){
   float s=shaftScattering(view.eyeYaw.xyz,direction,materialDistance,view.fogLights[light],stereo.fogIntensity[light]);
   scatter+=s;tinted+=stereo.fogTone[light].rgb*s;
  }
  // Keep shafts subordinate to surface lighting and architectural silhouettes.
  // A smooth shoulder preserves gradients instead of creating a flat plateau.
  // Each lamp hangs its own colour in the air, at the old warm glow's luminance (0.81).
  float shaped=0.025*(1.0-exp(-scatter/0.025));
  result+=tinted*(scatter>0.0?shaped/scatter:0.0)*0.81;
 }

 // Material views (post.frag shows them as they are): 13 roughness, metalness, cavity; 14 the specular
 // term alone; 15 the white furnace; 16 a dielectric's reflectance where it faces the eye.
 if(lampGrid.flags.y==13)result=shadedOpaque?vec3(iblRoughness,metalness,cavity):vec3(0.0);
 if(lampGrid.flags.y==14)result=vec3(debugSpecular*8.0); // (x8: the lab reads it in 8 bits)
 if(lampGrid.flags.y==15)result=vec3(debugFurnace*0.25); // (x.25: off the display curve's shoulder)
 if(lampGrid.flags.y==16)result=vec3(debugReflectance);
 // 17: the wetness as the floor and walls take it (red wet, blue puddle or streak).
 // 18: what the room's probe saw at this point (looking at it from the probe, sharp); 19 this pass's own
 // light here: their ratio says whether the captures hold the room as it is drawn.
 if(lampGrid.flags.y==18){int b=probeBest(worldPos);float sky;result=b>=0?probeSample(b,lampGrid.reflections[3*b].xyz,normalize(worldPos-lampGrid.reflections[3*b].xyz),0.0,sky).rgb:vec3(0.0);}
 if(lampGrid.flags.y==17)result=vec3(wetDebug,0.0,wetness>wetDebug*0.4+0.01?wetness:0.0)+color.rgb*0.15;
 if(lampGrid.flags.y==2&&detailAmount>0.001)result=vec3(detailValue*0.5);
 if(lampGrid.flags.y==3&&detailAmount>0.001)result=detailNormal*0.5+0.5;
 if(lampGrid.flags.y==8)result=sunDebug;
 if(lampGrid.flags.y==10)result=bounceOut*color.rgb*2.0;
 if(lampGrid.flags.y==11)result=crystal?vec3(1.0,0.0,0.0):metalness>0.0?vec3(0.0,0.0,metalness):vec3(1.0-materialRoughness);
 if(lampGrid.flags.y==9)result=vec3(ambientLight,max(directLight,0.0),directLight<0.0?0.5:0.0)*0.25;
 if(lampGrid.flags.y==7)result=vec3(clamp(pixelsPerTexel/64.0,0.0,1.0),fract(uv.y*atlasSize.y*2.5),fract(uv.x*atlasSize.x*2.5));
 if(lampGrid.flags.y==6)result=baseNormalDebug*0.5+0.5;
 if(lampGrid.flags.y==5)result=vec3(clamp(abs(uv.x)/64.0,0.0,1.0),clamp(abs(uv.y)/64.0,0.0,1.0),surface.w*0.25);
 if(lampGrid.flags.y==4)result=vec3(fract(uv.x*atlasSize.x),fract(uv.y*atlasSize.y*4.0),fract(worldPos.z*256.0));
 float alpha=surface.z>1.5?mix(color.a,1.0,reflectAlpha):1;
 // HDR opaque surfaces store planar camera depth in alpha for the post pass;
 // blended pipelines keep the destination alpha, so water stays a blend factor.
 vec3 toPoint=worldPos-view.eyeYaw.xyz;float forward=toPoint.x*view.eyeYaw.w+toPoint.y*view.basis.x;
 float planarDepth=max(forward*view.basis.y+toPoint.z*view.basis.z,0.0);
 // The first-person viewmodel stores a large negative depth so post stages
 // skip it exactly. Large, so an MSAA edge pixel that resolves the hand with
 // the wall behind it stays negative; a small value averaged to a phantom
 // surface in mid-air that cast shadow and AO halos around the hands.
 if(surface.w>1.5&&surface.w<2.5)planarDepth=-1000.0;
 float directShare=directLight>=0.0?directLight/(directLight+ambientLight+1e-4):0.5;
 // Alpha 1 marks world pixels; the viewmodel writes 0 so MSAA edge pixels
 // (fractional after resolve) skip world-only post effects.
 bool viewModelPixel=surface.w>1.5&&surface.w<2.5;
 // g: a metal's metalness above .5 (0.5 + metal/2), else half the wetness (post.frag).
 // r: roughness in 32 steps (the high five bits) and, in the low three, the axis the face looks along
 // (1-6: +x -x +y -y +z -z; 0 sloped; 7 none): post.frag reflects off that exact normal instead of
 // one rebuilt from half-float depth, which steps at a distance and cut floors' reflections into bands.
 float roughOut=(surface.z<0.5||liquid)&&surface.w<1.5?(liquid?0.04:shadedOpaque?iblRoughness:materialRoughness):1.0;
 float axisCode=7.0;
 if(roughOut<0.99){vec3 Nf=normalize(cross(dx,dy));if(dot(Nf,view.eyeYaw.xyz-worldPos)<0.0)Nf=-Nf;vec3 a=abs(Nf);
  axisCode=a.z>0.985?(Nf.z>0.0?5.0:6.0):a.x>0.985?(Nf.x>0.0?1.0:2.0):a.y>0.985?(Nf.y>0.0?3.0:4.0):0.0;}
 outMaterial=vec4((floor(clamp(roughOut,0.0,1.0)*31.0+0.5)*8.0+axisCode)/255.0,metalness>0.02?0.5+0.5*metalness:wetness*0.5,viewModelPixel?1.0:directShare,viewModelPixel?0.0:1.0);
 outColor=view.effects.w>0.5?vec4(max(result,vec3(0)),surface.z>1.5&&!liquid?alpha:planarDepth):vec4(toDisplay(filmic(result*0.95)),alpha);
}
