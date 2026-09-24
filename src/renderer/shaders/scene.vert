#version 450
layout(location=0) in vec4 position;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 lighting;
layout(location=3) in vec4 light0;
layout(location=4) in vec4 light1;
layout(location=5) in vec4 surface;
layout(location=6) in vec4 worldNormal;
layout(push_constant) uniform ViewState { vec4 eyeYaw; vec4 basis; vec4 effects; } view;
layout(location=0) out vec2 outUV;
layout(location=1) out vec4 outLighting;
layout(location=2) out vec4 outLight0;
layout(location=3) out vec4 outLight1;
layout(location=4) out vec4 outSurface;
void main(){
 vec4 adjustedLighting=lighting;
 vec4 adjustedSurface=surface;
 if(surface.w>0.5){
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
   vec3 toPoint=position.xyz-eye;float d2=dot(toPoint,toPoint);
   if(d2>0.04&&d2<196.0){float d=sqrt(d2);float along=dot(toPoint,forwardWorld)/d;
    if(along>0.80){float cone=clamp((along-0.80)/0.16,0.0,1.0);cone=cone*cone*(3.0-2.0*cone);
     float facing=0.30+0.70*abs(dot(n,toPoint)/(max(length(n),0.00001)*d));
     adjustedLighting.y+=cone*facing*1.9/(1.0+d2*0.020);
    }
   }
  }
  if(view.effects.y>0.0){
   vec3 muzzle=eye+forwardWorld*0.42-vec3(0,0,0.10);vec3 toPoint=position.xyz-muzzle;float d2=dot(toPoint,toPoint);
   if(d2<144.0){float d=max(sqrt(d2),0.001);float facing=0.35+0.65*abs(dot(n,toPoint))/d;float edge=1.0-d2/144.0;
    adjustedLighting.y+=view.effects.y*11.0*edge*edge*facing/(1.0+d2*0.045);
   }
  }
 }else gl_Position=position;
 outUV=uv;outLighting=adjustedLighting;outLight0=light0;outLight1=light1;outSurface=adjustedSurface;
}
