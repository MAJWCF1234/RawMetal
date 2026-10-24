#version 450
// Auto exposure, rendered into a 1x1 target. Log-average luminance of the
// scene on a 16x16 grid (geometric mean, like an eye or a camera meter),
// mapped to a partial key-value exposure and eased from last frame's value.
// Output: r exposure, g measured average luminance.
layout(set=0,binding=0) uniform sampler2D sceneImage;
layout(set=0,binding=1) uniform sampler2D previousExposure;
layout(push_constant) uniform ExposureParams {
 vec4 settings; // x adaptation per frame (1 = instant), y key, z min, w max
 vec4 shape;    // x adaptation strength (power on key/average)
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
 float previous=texture(previousExposure,vec2(0.5)).r;
 if(!(previous>0.0)||p.settings.x>=1.0)previous=target;
 outColor=vec4(mix(previous,target,clamp(p.settings.x,0.0,1.0)),average,0.0,1.0);
}
