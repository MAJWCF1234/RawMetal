#version 450
// Post pass between the scene and both outputs (window composite and headless
// readback). The scene stores linear radiance in RGB and planar camera depth
// in alpha, so this pass can add screen-space ambient occlusion, distance haze
// and a wide highlight bloom without extra attachments.
layout(set=0,binding=0) uniform sampler2D sceneImage;
layout(push_constant) uniform PostParams {
 vec4 camera;   // x AO strength, y aspect, z focal (1.3), w AO radius in metres
 vec4 haze;     // rgb linear haze colour, w density per metre
 vec4 frame;    // xy texel size, z bloom strength, w saturation
 vec4 grade;    // x shadow lift, y warm highlight tint, z cool shadow tint, w far depth
} p;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outColor;

float depthAt(vec2 at){return texture(sceneImage,clamp(at,vec2(0.0),vec2(1.0))).a;}
vec3 viewPosition(vec2 at,float depth){
 vec2 ndc=at*2.0-1.0;
 return vec3(ndc.x*depth/p.camera.z,-ndc.y*depth/(p.camera.z*p.camera.y),depth);
}
// Normal from the nearer of each pair of neighbours, so silhouettes do not
// average across a depth step.
vec3 viewNormal(vec2 at,vec3 center){
 vec2 t=p.frame.xy;
 vec3 r=viewPosition(at+vec2(t.x,0),depthAt(at+vec2(t.x,0))),l=viewPosition(at-vec2(t.x,0),depthAt(at-vec2(t.x,0)));
 vec3 d=viewPosition(at+vec2(0,t.y),depthAt(at+vec2(0,t.y))),u=viewPosition(at-vec2(0,t.y),depthAt(at-vec2(0,t.y)));
 vec3 dx=abs(r.z-center.z)<abs(center.z-l.z)?r-center:center-l;
 vec3 dy=abs(d.z-center.z)<abs(center.z-u.z)?d-center:center-u;
 vec3 n=normalize(cross(dy,dx));
 return dot(n,center)>0.0?-n:n;
}
float interleavedNoise(vec2 pixel){return fract(52.9829189*fract(dot(pixel,vec2(0.06711056,0.00583715))));}

float ambientOcclusion(vec3 center,vec3 normal){
 const int Samples=12;
 float radius=p.camera.w,occlusion=0.0;
 // Screen radius of the world-space kernel, capped so near walls stay cheap.
 vec2 screenRadius=min(vec2(radius*p.camera.z*0.5/center.z,radius*p.camera.z*p.camera.y*0.5/center.z),vec2(0.12));
 float rotation=interleavedNoise(gl_FragCoord.xy)*6.2831853;
 for(int i=0;i<Samples;++i){
  float t=(float(i)+0.5)/float(Samples);
  float angle=rotation+float(i)*2.3999632;          // golden angle spiral
  vec2 at=uv+vec2(cos(angle),sin(angle))*screenRadius*sqrt(t);
  float depth=depthAt(at);
  vec3 sampleView=viewPosition(at,depth)-center;
  float distance2=dot(sampleView,sampleView);
  float falloff=clamp(1.0-distance2/(radius*radius*4.0),0.0,1.0);
  occlusion+=max(dot(normal,sampleView)*inversesqrt(distance2+1e-4)-0.08,0.0)*falloff;
 }
 float open=clamp(1.0-occlusion/float(Samples)*abs(p.camera.x)*4.0,0.0,1.0);
 return max(open*open,0.12); // squared: contacts go dark, open walls stay clean
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
  vec3 center=viewPosition(uv,depth);
  float ao=ambientOcclusion(center,viewNormal(uv,center));
  // Negative strength is the occlusion debug view (r_post ao).
  if(p.camera.x<0.0){outColor=vec4(vec3(ao),depth);return;}
  // Occlusion darkens bounce light more than direct light: keep emitters and
  // bright direct highlights, deepen corners, contacts and creases.
  float luminance=dot(color,vec3(0.2126,0.7152,0.0722));
  color*=mix(ao,1.0,clamp(luminance*1.2-0.35,0.0,0.6));
 }
 if(surface&&p.haze.w>0.0){
  // Aerial perspective: distance gently lifts toward the scene's atmosphere,
  // separating planes so large spaces read as deep.
  float amount=1.0-exp(-max(depth-2.0,0.0)*p.haze.w);
  color=mix(color,p.haze.rgb,amount*0.55);
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
