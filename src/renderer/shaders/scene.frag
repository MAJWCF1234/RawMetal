#version 450
layout(set=0,binding=0) uniform sampler2D colorMap;
layout(set=0,binding=1) uniform sampler2D normalMap;
layout(set=0,binding=2) uniform sampler2D emissionMap;
layout(set=0,binding=3) uniform sampler2D reliefMap;
layout(push_constant) uniform ViewState { vec4 eyeYaw; vec4 basis; vec4 effects; vec4 fogLights[4]; vec4 atmosphere; } view;
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

float shaftScattering(vec3 eye,vec3 endpoint,vec4 lamp){
 float height=lamp.z-lamp.w;
 if(height<=0.0)return 0.0;

 vec3 ray=endpoint-eye;
 float rayLength=min(length(ray),16.0);
 if(rayLength<0.01)return 0.0;

 vec3 direction=normalize(ray);
 float horizontalSpeed=max(length(direction.xy),0.001);
 float along=dot(lamp.xy-eye.xy,direction.xy)/max(dot(direction.xy,direction.xy),0.000001);
 float start=max(0.0,along-1.3/horizontalSpeed);
 float finish=min(rayLength,along+1.3/horizontalSpeed);
 if(finish<=start)return 0.0;

 float stepLength=(finish-start)/8.0;
 float jitter=fract(52.9829189*fract(dot(gl_FragCoord.xy,vec2(0.06711056,0.00583715))));
 float sum=0.0;

 for(int i=0;i<8;++i){
  vec3 p=eye+direction*(start+(float(i)+jitter)*stepLength);
  float h=(p.z-lamp.w)/height;
  if(h<=0.0||h>=1.0)continue;

  float halfWidth=mix(0.72,0.42,h);
  vec2 footprint=abs(p.xy-lamp.xy)/halfWidth;
  float edge=max(footprint.x,footprint.y);
  float core=1.0-smoothstep(0.58,1.08,edge);
  float ends=smoothstep(0.0,0.12,h)*smoothstep(0.0,0.07,1.0-h);
  float dust=0.82+0.18*sin(p.x*8.3+p.y*6.7+p.z*4.9)*sin(p.x*5.1-p.y*9.2+p.z*3.7);
  sum+=core*ends*dust;
 }

 float towardEye=max(0.0,dot(-direction,normalize(vec3(eye.xy-lamp.xy,eye.z-lamp.z))));
 float opticalDepth=sum*stepLength*(0.18+0.05*towardEye);
 return min(1.0-exp(-opticalDepth),0.055);
}

void main(){
 vec2 sampleUV=uv;
 vec2 uvDx=dFdx(uv),uvDy=dFdy(uv);

 if(parallaxScale>0.0&&surface.z<0.5&&surface.x<6.0){
  vec3 dx=dFdx(worldPos),dy=dFdy(worldPos);
  float determinant=uvDx.x*uvDy.y-uvDx.y*uvDy.x;

  if(abs(determinant)>0.00001){
   vec3 tangent=normalize((dx*uvDy.y-dy*uvDx.y)/determinant);
   vec3 bitangent=normalize((dy*uvDx.x-dx*uvDy.x)/determinant);
   vec3 geometricNormal=normalize(cross(tangent,bitangent));
   vec3 toEye=normalize(view.eyeYaw.xyz-worldPos);
   vec2 direction=vec2(dot(toEye,tangent),dot(toEye,bitangent))/max(abs(dot(toEye,geometricNormal)),0.35);
   vec2 stepUV=clamp(direction,vec2(-2.0),vec2(2.0))*parallaxScale/4.0;

   sampleUV=uv+stepUV*2.0;

   for(int layer=0;layer<4;++layer){
    float height=textureGrad(reliefMap,sampleUV,uvDx,uvDy).r;
    if(float(layer)/4.0>=height)break;
    sampleUV-=stepUV;
   }
  }
 }

 vec4 color=textureGrad(colorMap,sampleUV,uvDx,uvDy);
 if(color.a<(surface.z>0.5?0.01:0.5))discard;

 float vertexLight=lighting.y;
 float specularLight=0.0;
 float waterFresnel=0.0;

 if(lighting.w>0.5){
  vec3 n=normalize(textureGrad(normalMap,sampleUV,uvDx,uvDy).xyz);
  float response=0.65+light0.w*max(0,dot(n,light0.xyz))+light1.w*max(0,dot(n,light1.xyz));
  vertexLight*=clamp(response/lighting.z,0.6,1.4);

  float gloss=fract(surface.z);

  if(gloss>0.001){
   vec3 dx=dFdx(worldPos),dy=dFdy(worldPos);
   float determinant=uvDx.x*uvDy.y-uvDx.y*uvDy.x;

   if(abs(determinant)>0.00001){
    vec3 tangent=normalize((dx*uvDy.y-dy*uvDx.y)/determinant);
    vec3 bitangent=normalize((dy*uvDx.x-dx*uvDy.x)/determinant);
    vec3 normal=normalize(cross(tangent,bitangent));
    vec3 eyeDirection=normalize(view.eyeYaw.xyz-worldPos);
    vec3 viewTangent=normalize(vec3(dot(eyeDirection,tangent),dot(eyeDirection,bitangent),abs(dot(eyeDirection,normal))));
    float sharpness=surface.z>1.5?64.0:30.0;

    if(light0.w>0.0){
     vec3 halfVector=light0.xyz+viewTangent;
     halfVector*=inversesqrt(max(dot(halfVector,halfVector),0.000001));
     specularLight+=light0.w*pow(max(dot(n,halfVector),0.0),sharpness);
    }

    if(light1.w>0.0){
     vec3 halfVector=light1.xyz+viewTangent;
     halfVector*=inversesqrt(max(dot(halfVector,halfVector),0.000001));
     specularLight+=light1.w*pow(max(dot(n,halfVector),0.0),sharpness);
    }

    specularLight=min(specularLight*gloss,0.32);

    if(surface.z>1.5){
     float NdotV=clamp(viewTangent.z,0.0,1.0);
     waterFresnel=0.04+0.96*pow(1.0-NdotV,5.0);
    }
   }
  }
 }

 if(surface.z>0.5&&surface.z<1.5){
  outColor=vec4(color.rgb*color.a*lighting.x*vec3(1,0.72,0.35),1);
  return;
 }

 vec3 result=toLinear(color.rgb)*lighting.x*vertexLight/(1+surface.x*0.018);

 if(surface.z<0.5&&surface.w<1.5){
  float key=clamp((vertexLight-0.24)/0.75,0.0,1.0);
  result*=mix(vec3(0.82,0.88,0.97),vec3(1.04,0.98,0.90),key);
 }

 result+=specularLight*(surface.z>1.5?vec3(0.92,0.98,1.0):vec3(1.0,0.84,0.63));

 if(surface.z>1.5){
  vec3 skySheen=toLinear(vec3(0.55,0.72,0.92));
  result+=skySheen*waterFresnel*0.22;
 }

 if(surface.y>0.0){
  result+=toLinear(textureGrad(emissionMap,sampleUV,uvDx,uvDy).rgb)*1.6*surface.y;
 }

 if(surface.z<0.5&&surface.w<1.5&&view.atmosphere.w>0.0){
  float distance=length(worldPos-view.eyeYaw.xyz);

  if(distance<75.0){
   float heightDensity=exp(-max(worldPos.z*0.06,0.0));
   float extinction=1.0-exp(-max(distance-4.0,0.0)*view.atmosphere.w*mix(0.80,1.20,heightDensity));
   vec3 rayDir=normalize(worldPos-view.eyeYaw.xyz);
   vec3 sunDir=normalize(vec3(0.5,0.7,0.5));
   float cosTheta=dot(rayDir,sunDir);
   float phase=0.90+0.10*cosTheta*cosTheta;
   result=mix(result,toLinear(view.atmosphere.rgb)*phase,min(extinction,0.58));
  }
 }

 if(surface.z<0.5&&surface.w<1.5){
  float scatter=0.0;

  for(int light=0;light<4;++light){
   scatter+=shaftScattering(view.eyeYaw.xyz,worldPos,view.fogLights[light]);
  }

  scatter=min(scatter,0.06);
  result=mix(result,toLinear(vec3(0.79,0.74,0.65)),scatter);
 }

 outColor=vec4(toDisplay(filmic(result*0.8)),surface.z>1.5?color.a:1);
}