#version 450
// Bloom chain: the scene is halved six times (each level a 13-tap filter),
// then climbed back up, every level adding a tent-filtered copy of the one
// below. Small hot sources (lamp cores, sparks) spread into a soft glare
// whose width follows their brightness instead of a fixed ring of taps.
// The first halving takes the soft-thresholded highlights only and weights
// each tap group by 1/(1+luma) so a single hot pixel cannot flicker.
layout(set=0,binding=0) uniform sampler2D source; // the level above (down) or below (up)
layout(set=0,binding=1) uniform sampler2D level;  // up: this level's halved image
layout(push_constant) uniform BloomParams {
 vec4 step;  // xy source texel size, z mode (0 first halving, 1 halving, 2 climb), w threshold
 vec4 shape; // x knee, y climb radius in source texels
} p;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outColor;

float luma(vec3 c){return dot(c,vec3(0.2126,0.7152,0.0722));}
vec3 tap(vec2 at){return texture(source,at).rgb;}
// Soft threshold: a quadratic knee below the threshold, then linear above it.
vec3 highlights(vec3 c){
 float bright=max(c.r,max(c.g,c.b)),t=p.step.w,knee=max(p.shape.x,1e-4);
 float soft=clamp(bright-t+knee,0.0,2.0*knee);soft=soft*soft/(4.0*knee);
 return c*max(soft,bright-t)/max(bright,1e-4);
}
vec3 group(vec3 a,vec3 b,vec3 c,vec3 d,bool first){
 vec3 m=(a+b+c+d)*0.25;if(!first)return m;
 m=highlights(max(m,vec3(0.0)));return m/(1.0+luma(m));
}

void main(){
 vec2 t=p.step.xy;
 if(p.step.z<1.5){
  bool first=p.step.z<0.5;
  vec3 a=tap(uv+t*vec2(-2,-2)),b=tap(uv+t*vec2(0,-2)),c=tap(uv+t*vec2(2,-2));
  vec3 d=tap(uv+t*vec2(-1,-1)),e=tap(uv+t*vec2(1,-1));
  vec3 f=tap(uv+t*vec2(-2,0)),g=tap(uv),h=tap(uv+t*vec2(2,0));
  vec3 i=tap(uv+t*vec2(-1,1)),j=tap(uv+t*vec2(1,1));
  vec3 k=tap(uv+t*vec2(-2,2)),l=tap(uv+t*vec2(0,2)),m=tap(uv+t*vec2(2,2));
  vec3 sum=group(d,e,i,j,first)*0.5+(group(a,b,f,g,first)+group(b,c,g,h,first)+group(f,g,k,l,first)+group(g,h,l,m,first))*0.125;
  // Undo the first halving's firefly weighting so the glare keeps its energy.
  if(first)sum=sum/max(1.0-luma(sum),0.05);
  outColor=vec4(max(sum,vec3(0.0)),1.0);
  return;
 }
 vec2 r=t*p.shape.y;
 vec3 up=tap(uv)*4.0
  +(tap(uv+vec2(r.x,0))+tap(uv-vec2(r.x,0))+tap(uv+vec2(0,r.y))+tap(uv-vec2(0,r.y)))*2.0
  +tap(uv+r)+tap(uv-r)+tap(uv+vec2(r.x,-r.y))+tap(uv+vec2(-r.x,r.y));
 outColor=vec4(texture(level,uv).rgb+up/16.0,1.0);
}
