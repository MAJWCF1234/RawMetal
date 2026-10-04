#version 450
layout(set=0,binding=0) uniform sampler2D colorMap;
layout(set=0,binding=1) uniform sampler2D normalMap;
layout(set=0,binding=2) uniform sampler2D emissionMap;
layout(set=0,binding=3) uniform sampler2D reliefMap;
layout(push_constant) uniform ViewState { vec4 eyeYaw; vec4 basis; vec4 effects; vec4 fogLights[4]; vec4 atmosphere; } view;
layout(set=1,binding=0) uniform StereoProjection {mat4 clip;vec4 enabled;vec4 fogIntensity;} stereo;
layout(location=0) in vec2 uv;
layout(location=1) in vec4 lighting;
layout(location=2) in vec4 light0;
layout(location=3) in vec4 light1;
layout(location=4) in vec4 surface;
layout(location=6) in vec3 worldPos;
layout(location=7) in float parallaxScale;
layout(location=0) out vec4 outColor;

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
 float parallaxFade=1.0-smoothstep(8.0,18.0,materialDistance);
 bool parallaxMaterial=parallaxScale>0.0&&surface.z<0.5&&parallaxFade>0.001;
 bool litMaterial=lighting.w>0.5||light0.w+light1.w>0.0;
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
   vec3 toEye=normalize(view.eyeYaw.xyz-worldPos);
   vec2 direction=vec2(dot(toEye,tangent),dot(toEye,bitangent))/max(abs(dot(toEye,geometricNormal)),0.35);
   vec2 stepUV=clamp(direction,vec2(-2.0),vec2(2.0))*parallaxScale*parallaxFade/4.0;

   sampleUV=uv+stepUV*2.0;

   for(int layer=0;layer<4;++layer){
    float height=textureGrad(reliefMap,sampleUV,uvDx,uvDy).r;
    if(float(layer)/4.0>=height)break;
    sampleUV-=stepUV;
   }
 }

 vec4 color=textureGrad(colorMap,sampleUV,uvDx,uvDy);
 if(color.a<(surface.z>0.5?0.01:0.5))discard;

 float vertexLight=lighting.y;
 float specularLight=0.0;
 float waterFresnel=0.0;

 if(lighting.w>0.5||light0.w+light1.w>0.0){
  vec4 filteredNormal=lighting.w>0.5?textureGrad(normalMap,sampleUV,uvDx,uvDy):vec4(0.0,0.0,1.0,1.0);
  float normalLength=length(filteredNormal.xyz);
  vec3 n=normalLength>0.000001?filteredNormal.xyz/normalLength:vec3(0.0,0.0,1.0);
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

 float alpha=surface.z>1.5?color.a:1;
 outColor=view.effects.w>0.5?vec4(max(result,vec3(0)),alpha):vec4(toDisplay(filmic(result*0.95)),alpha);
}
