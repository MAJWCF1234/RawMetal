#version 450
// Second post stage (compose). Reads the scene (linear RGB, planar depth in
// alpha) and the occlusion stage (rgb bounce light, a open fraction), then
// denoises occlusion with a depth-aware 4x4 filter, adds bounce light,
// aerial haze, volumetric lamp scattering, bloom and the grade. Its result
// feeds both headless readback and the window composite.
layout(set=0,binding=0) uniform sampler2D sceneImage;
layout(set=0,binding=1) uniform sampler2D occlusionImage;
layout(push_constant) uniform PostParams {
 vec4 camera;   // x AO strength (negative: debug view), y aspect, z focal, w radius (m)
 vec4 haze;     // rgb linear haze colour, w density per metre
 vec4 frame;    // xy texel size, z bloom strength, w saturation
 vec4 grade;    // x shadow lift, y warm highlights, z cool shadows, w far depth
 vec4 lights[4];// view-space lamp positions (xyz) and power (w)
} p;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outColor;

// The occlusion stage rotates its kernel on a 4x4 Bayer grid; averaging the
// 4x4 neighbourhood with similar depth removes that pattern without bleeding
// across silhouettes.
vec4 denoisedOcclusion(float depth){
 vec4 sum=vec4(0.0);float weight=0.0;
 vec2 base=(floor(uv/p.frame.xy-1.5)+0.5)*p.frame.xy;
 for(int y=0;y<4;++y)for(int x=0;x<4;++x){
  vec2 at=base+vec2(x,y)*p.frame.xy;
  float sampleDepth=texture(sceneImage,at).a;
  float w=exp(-abs(sampleDepth-depth)/(0.04*depth+0.02));
  sum+=texture(occlusionImage,at)*w;weight+=w;
 }
 return weight>0.0?sum/weight:texture(occlusionImage,uv);
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

void main(){
 vec4 source=texture(sceneImage,uv);
 vec3 color=source.rgb;float depth=source.a;
 bool surface=depth>0.02&&depth<p.grade.w;
 if(surface&&p.camera.x!=0.0){
  vec4 occlusion=denoisedOcclusion(depth);
  if(p.camera.x<0.0){outColor=vec4(vec3(occlusion.a),depth);return;}
  // Occlusion darkens bounce light more than direct light: bright direct
  // highlights keep more of their value than corners and contacts.
  float luminance=dot(color,vec3(0.2126,0.7152,0.0722));
  color*=mix(occlusion.a,1.0,clamp(luminance*1.2-0.35,0.0,0.6));
  // One bounce: nearby lit surfaces tint and fill this one (colour bleeding).
  color+=occlusion.rgb*0.3*occlusion.a;
 }
 vec2 ndc=uv*2.0-1.0;
 vec3 rayView=vec3(ndc.x/p.camera.z,-ndc.y/(p.camera.z*p.camera.y),1.0);
 vec3 ray=normalize(rayView);
 float rayLength=surface?depth*length(rayView):40.0;
 if(surface&&p.haze.w>0.0){
  // Aerial perspective: distance lifts toward the atmosphere colour.
  float amount=1.0-exp(-max(depth-2.0,0.0)*p.haze.w);
  color=mix(color,p.haze.rgb,amount*0.35);
 }
 if(p.haze.w>0.0){
  // Lamps glow through the air they light; denser air glows more.
  float scatter=0.0;for(int i=0;i<4;++i)scatter+=lampScatter(ray,rayLength,p.lights[i]);
  color+=vec3(1.0,0.86,0.66)*scatter*p.haze.w*0.55;
 }
 color+=bloom()*p.frame.z;
 // Grade in linear light: gentle saturation, cool lifted shadows, warm highs.
 float luminance=dot(color,vec3(0.2126,0.7152,0.0722));
 color=mix(vec3(luminance),color,p.frame.w);
 float shadowMix=1.0-clamp(luminance*4.0,0.0,1.0),highMix=clamp(luminance*1.5-0.4,0.0,1.0);
 color+=vec3(0.55,0.75,1.0)*p.grade.z*shadowMix+vec3(p.grade.x);
 color*=mix(vec3(1.0),vec3(1.0+p.grade.y,1.0+p.grade.y*0.45,1.0-p.grade.y*0.3),highMix);
 outColor=vec4(max(color,vec3(0.0)),depth);
}
