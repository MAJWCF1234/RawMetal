#version 450
// Auto exposure, rendered into a 1x1 target. Log-average luminance of the
// scene on a 16x16 grid (geometric mean, like an eye or a camera meter),
// mapped to a partial key-value exposure and eased from last frame's value.
// Output: r exposure, g measured average luminance, b eased focus distance
// (the camera's focus puller: planar depth at the crosshair, racked smoothly).
layout(set=0,binding=0) uniform sampler2D sceneImage;
layout(set=0,binding=1) uniform sampler2D previousExposure;
layout(push_constant) uniform ExposureParams {
 vec4 settings; // x adaptation per frame (1 = instant), y key, z min, w max
 vec4 shape;    // x adaptation strength (power on key/average), y focus rate per frame (1 = instant)
} p;
layout(location=0) out vec4 outColor;
void main(){
 float sum=0.0,count=0.0;
 for(int y=0;y<16;++y)for(int x=0;x<16;++x){
  vec4 s=textureLod(sceneImage,(vec2(x,y)+0.5)/16.0,0.0);
  if(s.a<0.0)continue; // the viewmodel does not meter
  sum+=log(max(dot(s.rgb,vec3(0.2126,0.7152,0.0722)),1e-4));count+=1.0;
 }
 float average=count>0.0?exp(sum/count):p.settings.y;
 float target=clamp(pow(p.settings.y/average,p.shape.x),p.settings.z,p.settings.w);
 vec4 history=texture(previousExposure,vec2(0.5));
 float previous=history.r;
 if(!(previous>0.0)||p.settings.x>=1.0)previous=target;
 // Focus: nearest of five taps around the crosshair (the subject in front),
 // ignoring the viewmodel (negative depth) and sky.
 float focusTarget=60.0;
 for(int i=0;i<5;++i){vec2 o=vec2(i==1?0.025:i==2?-0.025:0.0,i==3?0.025:i==4?-0.025:0.0);float d=textureLod(sceneImage,vec2(0.5)+o,0.0).a;if(d>0.05)focusTarget=min(focusTarget,d);}
 focusTarget=clamp(focusTarget,0.5,60.0);
 // Rack in inverse distance (dioptres), as a lens does: near changes move fast.
 float previousFocus=history.b>0.05?history.b:focusTarget;
 float focus=1.0/mix(1.0/previousFocus,1.0/focusTarget,clamp(p.shape.y,0.0,1.0));
 outColor=vec4(clamp(mix(previous,target,clamp(p.settings.x,0.0,1.0)),p.settings.z,p.settings.w),average,focus,1.0);
}
