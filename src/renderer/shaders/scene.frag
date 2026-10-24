#version 450
layout(set=0,binding=0) uniform sampler2D colorMap;
layout(set=0,binding=1) uniform sampler2D normalMap;
layout(set=0,binding=2) uniform sampler2D emissionMap;
layout(set=0,binding=3) uniform sampler2D reliefMap;
layout(push_constant) uniform ViewState { vec4 eyeYaw; vec4 basis; vec4 effects; vec4 fogLights[4]; vec4 atmosphere; } view;
layout(set=1,binding=0) uniform StereoProjection {mat4 clip;vec4 enabled;vec4 fogIntensity;} stereo;
// Per-pixel lamps: a world-space grid lists up to 16 lamps per cell. Lamp i is
// lamps[2i] = position, range and lamps[2i+1] = intensity, shadow slot.
layout(std430,set=1,binding=2) readonly buffer LampGrid {vec4 origin;ivec4 size;ivec4 flags;vec4 eye;vec4 basis;vec4 sun;vec4 sunColor;vec4 cascades[12];vec4 lamps[512];uint cells[];} lampGrid;
// Three sun cascades (orthographic radial depth), compared in hardware.
layout(set=1,binding=4) uniform sampler2DArrayShadow sunShadows;
// Six radial-depth faces per lamp, compared in hardware with bilinear PCF.
layout(set=1,binding=3) uniform sampler2DArrayShadow lampShadows;
layout(location=0) in vec2 uv;
layout(location=1) in vec4 lighting;
layout(location=2) in vec4 light0;
layout(location=3) in vec4 light1;
layout(location=4) in vec4 surface;
layout(location=6) in vec3 worldPos;
layout(location=7) in float parallaxScale;
layout(location=0) out vec4 outColor;

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

void main(){
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
 bool litMaterial=lighting.w>0.5||light0.w+light1.w>0.0||detailAmount>0.001;
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
 vec4 color=textureGrad(colorMap,sampleUV,uvDx,uvDy);
 if(color.a<(surface.z>0.5?0.01:0.5))discard;
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
 vec3 sunDebug=vec3(0.0);
 vec3 n=vec3(0.0,0.0,1.0);float normalLength=1.0;vec3 baseNormalDebug=vec3(0.0,0.0,1.0);

 if(lighting.w>0.5||light0.w+light1.w>0.0){
  vec4 filteredNormal=lighting.w>0.5?textureGrad(normalMap,sampleUV,uvDx,uvDy):vec4(0.0,0.0,1.0,1.0);
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
  vec3 N=Ng;
  if((lighting.w>0.5||detailAmount>0.001)&&tangentFrameValid){
   vec3 T=normalize(tangent-Ng*dot(Ng,tangent));
   vec3 B=bitangent-Ng*dot(Ng,bitangent);B=normalize(B-T*dot(T,B));
   N=normalize(T*n.x+B*n.y+Ng*max(n.z,0.05));
  }
  float concentration=clamp(normalLength,0.0,1.0);
  float roughness=clamp(1.0-fract(surface.z),0.18,0.96);
  roughness=sqrt(clamp(roughness*roughness+(1.0-concentration)/max(concentration,0.05),0.0324,1.0));
  float lampLight=0.0,lampSpecular=0.0;
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
    float distance=sqrt(d2);vec3 L=toLamp/distance;
    float geometric=dot(Ng,L);if(geometric<=0.0)continue;
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
    lampLight+=falloff*shadow*(0.04+0.96*max(dot(N,L),0.0))*smoothstep(0.0,0.12,geometric);
    lampSpecular+=falloff*shadow*dielectricSpecular(N,V,L,roughness);
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
     lampLight+=beam*(0.30+0.70*max(dot(N,L),0.0));
     lampSpecular+=beam*dielectricSpecular(N,V,L,roughness);
    }
   }
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
    lampSpecular+=lampGrid.sun.w*visible*dielectricSpecular(N,V,L,roughness)*terminator;
   }
   // Sky ambient: the flat outdoor bake becomes a hemisphere term.
   hemisphere=0.62+0.38*max(N.z,0.0);
  }
  // Ambient keeps a little of the normal map's relief in unlit areas.
  float relief=mix(1.0,clamp(dot(N,Ng),0.0,1.0),0.6)*hemisphere;
  // Exposure-matched to the old bake (which clamped at 1.4): a soft shoulder
  // keeps hot spots under lamps without flattening the falloff into the room.
  vertexLight=lighting.y*relief+1.25*(1.0-exp(-lampLight*0.62/1.25));
  specularLight=lampSpecular*0.7;
 }

 if(surface.z>0.5&&surface.z<1.5){
  vec3 additive=color.rgb*color.a*lighting.x*vec3(1,0.72,0.35);
  outColor=vec4(view.effects.w>0.5?toLinear(additive):additive,1);
  return;
 }

 // Dielectrics return approximately four percent through the specular lobe.
 // Diffuse atlases use sRGB images: hardware decodes each source texel before
 // bilinear/trilinear filtering. Decoding the already-filtered sample twice
 // would crush both midtones and minified material detail.
 vec3 result=color.rgb*lighting.x*vertexLight*0.96/(1+surface.x*0.018);
 result+=color.rgb*lighting.x*sunLight*0.96;

 if(surface.z<0.5&&surface.w<1.5){
  float key=clamp((vertexLight-0.24)/0.75,0.0,1.0);
  result*=mix(vec3(0.82,0.88,0.97),vec3(1.04,0.98,0.90),key);
 }

 result+=vec3(specularLight*lighting.x);

 if(surface.z>1.5){
  vec3 skySheen=toLinear(vec3(0.55,0.72,0.92));
  result+=skySheen*waterFresnel*0.22;
 }

 if(surface.y>0.0){
  // Emission atlases and their mip chains already contain linear radiance.
  result+=textureGrad(emissionMap,sampleUV,uvDx,uvDy).rgb*1.6*surface.y;
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
  vec3 sunDir=normalize(vec3(0.5,0.7,0.5));
  float cosTheta=dot(rayDir,sunDir);
  float phase=0.90+0.10*cosTheta*cosTheta;
  result=mix(result,toLinear(view.atmosphere.rgb)*phase,min(extinction,0.35));
 }

 if(surface.z<0.5&&surface.w<1.5&&view.atmosphere.w>0.0&&materialDistance>=0.05){
  float scatter=0.0;
  vec3 direction=(worldPos-view.eyeYaw.xyz)/materialDistance;
  for(int light=0;light<4;++light){
   scatter+=shaftScattering(view.eyeYaw.xyz,direction,materialDistance,view.fogLights[light],stereo.fogIntensity[light]);
  }
  // Keep shafts subordinate to surface lighting and architectural silhouettes.
  // A smooth shoulder preserves gradients instead of creating a flat plateau.
  scatter=0.025*(1.0-exp(-scatter/0.025));
  result+=toLinear(vec3(0.98,0.90,0.78))*scatter*1.0;
 }

 if(lampGrid.flags.y==2&&detailAmount>0.001)result=vec3(detailValue*0.5);
 if(lampGrid.flags.y==3&&detailAmount>0.001)result=detailNormal*0.5+0.5;
 if(lampGrid.flags.y==8)result=sunDebug;
 if(lampGrid.flags.y==7)result=vec3(clamp(pixelsPerTexel/64.0,0.0,1.0),fract(uv.y*atlasSize.y*2.5),fract(uv.x*atlasSize.x*2.5));
 if(lampGrid.flags.y==6)result=baseNormalDebug*0.5+0.5;
 if(lampGrid.flags.y==5)result=vec3(clamp(abs(uv.x)/64.0,0.0,1.0),clamp(abs(uv.y)/64.0,0.0,1.0),surface.w*0.25);
 if(lampGrid.flags.y==4)result=vec3(fract(uv.x*atlasSize.x),fract(uv.y*atlasSize.y*4.0),fract(worldPos.z*256.0));
 float alpha=surface.z>1.5?color.a:1;
 // HDR opaque surfaces store planar camera depth in alpha for the post pass;
 // blended pipelines keep the destination alpha, so water stays a blend factor.
 vec3 toPoint=worldPos-view.eyeYaw.xyz;float forward=toPoint.x*view.eyeYaw.w+toPoint.y*view.basis.x;
 float planarDepth=max(forward*view.basis.y+toPoint.z*view.basis.z,0.0);
 // The first-person viewmodel stores negative depth so post stages skip it
 // exactly, at any distance from walls.
 if(surface.w>1.5&&surface.w<2.5)planarDepth=-max(planarDepth,0.01);
 outColor=view.effects.w>0.5?vec4(max(result,vec3(0)),surface.z>1.5?alpha:planarDepth):vec4(toDisplay(filmic(result*0.95)),alpha);
}
