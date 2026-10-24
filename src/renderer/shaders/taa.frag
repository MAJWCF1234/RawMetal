#version 450
// Temporal anti-aliasing. The scene is rendered with a sub-pixel jitter each
// frame; this stage reprojects last frame's resolved image through the camera
// motion, clips it to the current 3x3 neighbourhood (variance clipping in
// YCoCg) and blends. Output alpha carries planar depth for next frame's
// disocclusion test. Runs only in the windowed path.
layout(set=0,binding=0) uniform sampler2D currentImage;
layout(set=0,binding=1) uniform sampler2D historyImage;
layout(set=0,binding=2) uniform sampler2D sceneImage;
layout(push_constant) uniform TaaParams {
 vec4 camera;    // x aspect, y focal, z far depth, w history valid
 vec4 jitter;    // xy current NDC jitter
 vec4 eye;       // xyz current eye, w history weight
 vec4 basis;     // current cy, sy, cp, sp
 vec4 prevEye;   // xyz previous eye
 vec4 prevBasis; // previous cy, sy, cp, sp
 vec4 frame;     // xy texel size
 vec4 pad;
} p;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outColor;

vec3 toYCoCg(vec3 c){return vec3(0.25*c.r+0.5*c.g+0.25*c.b,0.5*c.r-0.5*c.b,-0.25*c.r+0.5*c.g-0.25*c.b);}
vec3 fromYCoCg(vec3 c){return vec3(c.x+c.y-c.z,c.x+c.z,c.x-c.y-c.z);}

vec3 worldFromView(vec3 v,vec3 eye,vec4 b){
 float forward=v.z*b.z-v.y*b.w;
 return eye+vec3(-v.x*b.y+forward*b.x,v.x*b.x+forward*b.y,v.y*b.z+v.z*b.w);
}
vec3 viewFromWorld(vec3 w,vec3 eye,vec4 b){
 vec3 d=w-eye;float forward=d.x*b.x+d.y*b.y;
 return vec3(-d.x*b.y+d.y*b.x,d.z*b.z-forward*b.w,forward*b.z+d.z*b.w);
}

void main(){
 vec4 current=texture(currentImage,uv);
 float depth=texture(sceneImage,uv).a;
 // Neighbourhood statistics of the current frame.
 vec3 m1=vec3(0.0),m2=vec3(0.0);
 for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){
  vec3 c=toYCoCg(texture(currentImage,uv+vec2(x,y)*p.frame.xy).rgb);m1+=c;m2+=c*c;
 }
 m1/=9.0;vec3 sigma=sqrt(max(m2/9.0-m1*m1,vec3(0.0)));
 const float gamma=1.0;vec3 lo=m1-gamma*sigma,hi=m1+gamma*sigma;

 // Reproject this pixel into the previous frame.
 vec2 prevUV=uv;float weight=p.eye.w;
 bool viewModel=depth<0.0;
 if(!viewModel){
  float d=depth>0.02&&depth<p.camera.z?depth:p.camera.z;
  vec2 ndc=uv*2.0-1.0-p.jitter.xy;
  vec3 view=vec3(ndc.x*d/p.camera.y,-ndc.y*d/(p.camera.y*p.camera.x),d);
  vec3 world=worldFromView(view,p.eye.xyz,p.basis);
  vec3 prev=viewFromWorld(world,p.prevEye.xyz,p.prevBasis);
  if(prev.z<0.05)weight=0.0;
  else{
   vec2 prevNdc=vec2(prev.x*p.camera.y/prev.z,-prev.y*p.camera.y*p.camera.x/prev.z);
   prevUV=prevNdc*0.5+0.5;
   if(any(lessThan(prevUV,vec2(0.0)))||any(greaterThan(prevUV,vec2(1.0))))weight=0.0;
   // Disocclusion: the history pixel must have been about as far away.
   float prevDepth=texture(historyImage,prevUV).a;
   if(abs(prevDepth-prev.z)>0.08*prev.z+0.05)weight*=0.2;
  }
 }else weight*=0.75; // the viewmodel moves with the camera; a gentler history
 if(p.camera.w<0.5)weight=0.0;

 vec3 history=toYCoCg(texture(historyImage,prevUV).rgb);
 // Clip the history toward the current neighbourhood instead of clamping it.
 vec3 centre=0.5*(lo+hi),extent=0.5*(hi-lo)+1e-5;
 vec3 offset=history-centre;vec3 ratio=abs(offset)/extent;float t=max(ratio.x,max(ratio.y,ratio.z));
 if(t>1.0)history=centre+offset/t;
 vec3 resolved=fromYCoCg(mix(toYCoCg(current.rgb),history,weight));
 outColor=vec4(max(resolved,vec3(0.0)),depth);
}
