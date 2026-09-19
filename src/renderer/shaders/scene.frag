#version 450
layout(set=0,binding=0) uniform sampler2D colorMap;
layout(set=0,binding=1) uniform sampler2D normalMap;
layout(set=0,binding=2) uniform sampler2D emissionMap;
layout(location=0) in vec2 uv;
layout(location=1) in vec4 lighting;
layout(location=2) in vec4 light0;
layout(location=3) in vec4 light1;
layout(location=4) in vec4 surface;
layout(location=0) out vec4 outColor;
void main(){
 vec4 color=texture(colorMap,uv);
 if(color.a<0.5)discard;
 float vertexLight=lighting.y;
 if(lighting.w>0.5){
  vec3 n=normalize(texture(normalMap,uv).xyz);
  float response=0.65+light0.w*max(0,dot(n,light0.xyz))+light1.w*max(0,dot(n,light1.xyz));
  vertexLight*=clamp(response/lighting.z,0.6,1.4);
 }
 if(surface.z>0.5){outColor=vec4(color.rgb*color.a*lighting.x*vec3(1,0.72,0.35),1);return;}
 vec3 result=color.rgb*lighting.x*vertexLight/(1+surface.x*0.018);
 if(surface.y>0.0)result+=texture(emissionMap,uv).rgb*1.6*surface.y;
 outColor=vec4(clamp(result,0,1),1);
}
