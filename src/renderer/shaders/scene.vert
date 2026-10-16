#version 450
layout(location=0) in vec4 position;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 lighting;
layout(location=3) in vec4 light0;
layout(location=4) in vec4 light1;
layout(location=5) in vec4 surface;
layout(location=6) in vec4 worldNormal;
layout(set=1,binding=0) uniform StereoProjection {mat4 clip;vec4 enabled;vec4 fogIntensity;} stereo;
struct ActorLighting {vec4 normalBrightness[6];vec4 light0[6];vec4 light1[6];vec4 surfaceNormal[6];vec4 originEye;};
layout(std430,set=1,binding=1) readonly buffer ActorLights {ActorLighting actor[];} actors;

layout(push_constant) uniform ViewState {
 vec4 eyeYaw;
 vec4 basis;
 vec4 effects;
 vec4 fogLights[4];
 vec4 atmosphere;
} view;

layout(location=0) out vec2 outUV;
layout(location=1) out vec4 outLighting;
layout(location=2) out vec4 outLight0;
layout(location=3) out vec4 outLight1;
layout(location=4) out vec4 outSurface;
layout(location=6) out vec3 outWorldPos;
layout(location=7) out float outParallax;

vec3 cameraVector(vec3 v){
 precise float forward=v.x*view.eyeYaw.w+v.y*view.basis.x;
 return vec3(-v.x*view.basis.x+v.y*view.eyeYaw.w,v.z*view.basis.y-forward*view.basis.z,forward*view.basis.y+v.z*view.basis.z);
}
vec3 stableUnit(vec3 v){
 float squared=dot(v,v);
 if(squared>1.e-30&&squared<1.e30)return v*(1.0/sqrt(squared));
 float scale=max(max(abs(v.x),abs(v.y)),abs(v.z));
 if(!(scale>0.0)||isinf(scale)||isnan(scale))return vec3(0);
 v/=scale;return v*(1.0/sqrt(dot(v,v)));
}

void main(){
 vec4 adjustedLighting=lighting;
 vec4 adjustedSurface=surface;
 vec3 worldPos=position.xyz;
 vec4 adjustedLight0=light0,adjustedLight1=light1;

 if(surface.w>=3.0){
  int index=int(surface.w)-3;
  precise vec3 n=cross(light0.xyz,light1.xyz);
  if(dot(n,lighting.xyz)<0.0)n=-n;
  int side=0;float alignment=-1.e30;
  for(int i=0;i<6;++i){float match=dot(n,actors.actor[index].normalBrightness[i].xyz);if(match>alignment){alignment=match;side=i;}}
  vec4 source0=actors.actor[index].light0[side],source1=actors.actor[index].light1[side];
  adjustedLighting=vec4(actors.actor[index].normalBrightness[side].w,position.w,0.22,0.0);
  adjustedLight0=vec4(0);adjustedLight1=vec4(0);
  bool tangentActive=worldNormal.z>=0.0&&(source0.w!=0.0||source1.w!=0.0);
  if(tangentActive){
   vec4 span=vec4(light0.w,light1.w,worldNormal.xy);
   float uvScale=max(max(abs(span.x),abs(span.y)),max(abs(span.z),abs(span.w)));
   if(uvScale>0.0&&!isinf(uvScale)&&!isnan(uvScale)){
    span/=uvScale;
    float det=span.x*span.w-span.z*span.y;
    float areaScale=dot(span.xy,span.xy)*dot(span.zw,span.zw);
    if(areaScale>0.0&&det*det>areaScale*0.00000001){
     vec3 e1=cameraVector(light0.xyz),e2=cameraVector(light1.xyz);
     float handedness=det<0.0?-1.0:1.0;
     vec3 tangent=stableUnit((e1*span.w-e2*span.y)*handedness);
     vec3 rawBitangent=(e2*span.x-e1*span.z)*handedness;
     vec3 bitangent=stableUnit(rawBitangent-tangent*dot(tangent,rawBitangent));
     vec3 geometricNormal=cross(tangent,bitangent);
     float geometricSquared=dot(geometricNormal,geometricNormal);
     if(geometricSquared>1.e-30&&geometricSquared<1.e30){
      vec3 normal=stableUnit(actors.actor[index].surfaceNormal[side].xyz);
      adjustedLight0=vec4(dot(tangent,source0.xyz),dot(bitangent,source0.xyz),dot(normal,source0.xyz),source0.w);
      adjustedLight1=vec4(dot(tangent,source1.xyz),dot(bitangent,source1.xyz),dot(normal,source1.xyz),source1.w);
      adjustedLighting.z+=source0.w*max(0.0,adjustedLight0.z)+source1.w*max(0.0,adjustedLight1.z);
      adjustedLighting.w=worldNormal.z;
     }
    }
   }
  }
  vec4 originEye=actors.actor[index].originEye;
  precise vec3 p=cameraVector(position.xyz-vec3(originEye.zw,view.eyeYaw.z));
  worldPos=position.xyz+vec3(originEye.xy,0);
  gl_Position=vec4(p.x*1.3,-p.y*1.3*view.basis.w,p.z-0.06,p.z);
  adjustedSurface=vec4(p.z,lighting.w,surface.z,0.0);
 }else if(surface.w>0.5&&surface.w<1.5){
  vec3 delta=position.xyz-view.eyeYaw.xyz;
  float right=-delta.x*view.basis.x+delta.y*view.eyeYaw.w;
  float forward=delta.x*view.eyeYaw.w+delta.y*view.basis.x;
  float cameraY=delta.z*view.basis.y-forward*view.basis.z;
  float cameraZ=forward*view.basis.y+delta.z*view.basis.z;

  gl_Position=vec4(right*1.3,-cameraY*1.3*view.basis.w,cameraZ-0.06,cameraZ);
  adjustedSurface.x=cameraZ;

  vec3 n=normalize(worldNormal.xyz);
  vec3 eye=view.eyeYaw.xyz;
  vec3 forwardWorld=normalize(vec3(view.eyeYaw.w*view.basis.y,view.basis.x*view.basis.y,view.basis.z));

  if(view.effects.x>0.5){
   vec3 toPoint=position.xyz-eye;
   float d2=dot(toPoint,toPoint);

   if(d2>0.04&&d2<196.0){
    float d=sqrt(d2);
    float along=dot(toPoint,forwardWorld)/d;

    if(along>0.80){
     float cone=clamp((along-0.80)/0.16,0.0,1.0);
     cone=cone*cone*(3.0-2.0*cone);

     float facing=0.30+0.70*abs(dot(n,toPoint)/(max(length(n),0.00001)*d));
     adjustedLighting.y+=cone*facing*1.9/(1.0+d2*0.020);
    }
   }
  }

  if(view.effects.y>0.0){
   vec3 muzzle=eye+forwardWorld*0.42-vec3(0,0,0.10);
   vec3 toPoint=position.xyz-muzzle;
   float d2=dot(toPoint,toPoint);

   if(d2<144.0){
    float d=max(sqrt(d2),0.001);
    float facing=0.35+0.65*abs(dot(n,toPoint))/d;
    float edge=1.0-d2/144.0;

    adjustedLighting.y+=view.effects.y*1.4*edge*edge*facing/(1.0+d2*0.045);
   }
  }
 }else{
  gl_Position=position;

  float right=position.x/1.3;
  float cameraY=-position.y/(1.3*view.basis.w);
  float forward=position.w*view.basis.y-cameraY*view.basis.z;

  worldPos=view.eyeYaw.xyz+vec3(
   -right*view.basis.x+forward*view.eyeYaw.w,
   right*view.eyeYaw.w+forward*view.basis.x,
   cameraY*view.basis.y+position.w*view.basis.z
  );
 }

 if(stereo.enabled.x>0.5)gl_Position=stereo.clip*vec4(worldPos,1.0);
 outUV=uv;
 outLighting=adjustedLighting;
 outLight0=adjustedLight0;
 outLight1=adjustedLight1;
 outSurface=adjustedSurface;
 outWorldPos=worldPos;
 outParallax=worldNormal.w;
}
