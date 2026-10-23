#version 450
// First post stage: screen-space ambient occlusion and one-bounce indirect
// light from the scene's linear radiance (RGB) and planar depth (alpha).
// Output: rgb indirect light, a open-sky fraction. The compose stage blurs it.
layout(set=0,binding=0) uniform sampler2D sceneImage;
layout(push_constant) uniform PostParams {
 vec4 camera;   // x AO strength (negative: debug view), y aspect, z focal, w radius (m)
 vec4 haze;     // rgb linear haze colour, w density per metre
 vec4 frame;    // xy texel size, z bloom strength, w saturation
 vec4 grade;    // x shadow lift, y warm highlights, z cool shadows, w far depth
 vec4 lights[4];// view-space lamp positions (xyz) and power (w)
} p;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outColor;

float depthAt(vec2 at){return texture(sceneImage,clamp(at,vec2(0.0),vec2(1.0))).a;}
vec3 viewPosition(vec2 at,float depth){
 vec2 ndc=at*2.0-1.0;
 return vec3(ndc.x*depth/p.camera.z,-ndc.y*depth/(p.camera.z*p.camera.y),depth);
}
vec3 viewNormal(vec2 at,vec3 center){
 vec2 t=p.frame.xy;
 vec3 r=viewPosition(at+vec2(t.x,0),depthAt(at+vec2(t.x,0))),l=viewPosition(at-vec2(t.x,0),depthAt(at-vec2(t.x,0)));
 vec3 d=viewPosition(at+vec2(0,t.y),depthAt(at+vec2(0,t.y))),u=viewPosition(at-vec2(0,t.y),depthAt(at-vec2(0,t.y)));
 vec3 dx=abs(r.z-center.z)<abs(center.z-l.z)?r-center:center-l;
 vec3 dy=abs(d.z-center.z)<abs(center.z-u.z)?d-center:center-u;
 vec3 n=normalize(cross(dy,dx));
 return dot(n,center)>0.0?-n:n;
}
// 4x4 Bayer rotation: the compose stage's 4x4 blur cancels the pattern exactly.
float bayer4(vec2 pixel){
 ivec2 q=ivec2(mod(pixel,4.0));
 const float m[16]=float[16](0.,8.,2.,10.,12.,4.,14.,6.,3.,11.,1.,9.,15.,7.,13.,5.);
 return (m[q.y*4+q.x]+0.5)/16.0;
}

void main(){
 float depth=depthAt(uv);
 if(depth<=0.02||depth>=p.grade.w){outColor=vec4(0.0,0.0,0.0,1.0);return;}
 vec3 center=viewPosition(uv,depth),normal=viewNormal(uv,center);
 const int Samples=12;
 float radius=p.camera.w,occlusion=0.0;vec3 bounce=vec3(0.0);
 vec2 screenRadius=min(vec2(radius*p.camera.z*0.5/depth,radius*p.camera.z*p.camera.y*0.5/depth),vec2(0.12));
 float rotation=bayer4(gl_FragCoord.xy)*6.2831853;
 for(int i=0;i<Samples;++i){
  float t=(float(i)+0.5)/float(Samples);
  float angle=rotation+float(i)*2.3999632;
  vec2 at=clamp(uv+vec2(cos(angle),sin(angle))*screenRadius*sqrt(t),vec2(0.0),vec2(1.0));
  vec4 sampleScene=texture(sceneImage,at);
  vec3 toSample=viewPosition(at,sampleScene.a)-center;
  float distance2=dot(toSample,toSample);
  float falloff=clamp(1.0-distance2/(radius*radius*4.0),0.0,1.0);
  float facing=dot(normal,toSample)*inversesqrt(distance2+1e-4);
  occlusion+=max(facing-0.08,0.0)*falloff;
  // Surfaces in front of this one reflect their light onto it.
  bounce+=sampleScene.rgb*max(facing,0.0)*falloff;
 }
 float open=clamp(1.0-occlusion/float(Samples)*abs(p.camera.x)*4.0,0.0,1.0);
 outColor=vec4(bounce/float(Samples),max(open*open,0.12));
}
